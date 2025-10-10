/*
  SugaR, a UCI chess playing engine derived from Stockfish
  Copyright (C) 2004-2025 The Stockfish developers (see AUTHORS file)

  SugaR is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  SugaR is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "evaluate.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <tuple>
#include <cmath>

#include "nnue/network.h"
#include "nnue/nnue_misc.h"
#include "position.h"
#include "types.h"
#include "uci.h"
#include "nnue/nnue_accumulator.h"
#include "eval_weights.h"
#include "dyn_gate.h"

namespace Sugar {

// Returns a static, purely materialistic evaluation of the position from
// the point of view of the side to move. It can be divided by PawnValue to get
// an approximation of the material advantage on the board in terms of pawns.
int Eval::simple_eval(const Position& pos) {
    Color c = pos.side_to_move();
    return PawnValue * (pos.count<PAWN>(c) - pos.count<PAWN>(~c))
         + (pos.non_pawn_material(c) - pos.non_pawn_material(~c));
}

bool Eval::use_smallnet(const Position& pos) { return std::abs(simple_eval(pos)) > 962; }

// Evaluate is the evaluator for the outer world. It returns a static evaluation
// of the position from the point of view of the side to move.
Value Eval::evaluate(const Eval::NNUE::Networks&    networks,
                     const Position&                pos,
                     Eval::NNUE::AccumulatorStack&  accumulators,
                     Eval::NNUE::AccumulatorCaches& caches,
                     int                            optimism) {

    assert(!pos.checkers());

    // Precompute material once (used for Dynamic weights and final blend)
    int material = 535 * pos.count<PAWN>() + pos.non_pawn_material();

    // --- NNUE weights defaults (function-scope) ---
    int wMat = 125;
    int wPos = 131;

    bool smallNet           = use_smallnet(pos);
    auto [psqt, positional] = smallNet ? networks.small.evaluate(pos, accumulators, &caches.small)
                                       : networks.big.evaluate(pos, accumulators, &caches.big);

    // --- NNUE weights selection (Default / Manual / Dynamic) ---
    switch (static_cast<Sugar::Eval::WeightsMode>(Sugar::Eval::gEvalWeights.mode.load())) {
    case Sugar::Eval::WeightsMode::Manual: {
        // Manual fixed weights from UCI options
        wMat = Sugar::Eval::gEvalWeights.manualMat.load();
        wPos = Sugar::Eval::gEvalWeights.manualPos.load();
        break;
    }
    case Sugar::Eval::WeightsMode::Dynamic: {
        // Game-phase estimate (tapered): 0..24 -> 0..1024
        int gamePhase = 0;
        gamePhase += pos.count<KNIGHT>() + pos.count<BISHOP>(); // minor pieces
        gamePhase += 2 * pos.count<ROOK>();
        gamePhase += 4 * pos.count<QUEEN>();
        if (gamePhase < 0) gamePhase = 0;
        if (gamePhase > 24) gamePhase = 24;
        const int t = (gamePhase * 1024) / 24; // 0..1024

        const int oM = Sugar::Eval::gEvalWeights.dynOpenMat.load();
        const int oP = Sugar::Eval::gEvalWeights.dynOpenPos.load();
        const int eM = Sugar::Eval::gEvalWeights.dynEgMat.load();
        const int eP = Sugar::Eval::gEvalWeights.dynEgPos.load();

        wMat = (eM * (1024 - t) + oM * t) / 1024;
        wPos = (eP * (1024 - t) + oP * t) / 1024;

        // Internal base weights (UCI ignored): Open 126/134, End 134/126
        {
            // Game-phase estimate (Stockfish style): minors + 2*rooks + 4*queens
            int gp = 0;
            gp += pos.count<KNIGHT>() + pos.count<BISHOP>(); // minor pieces
            gp += 2 * pos.count<ROOK>();
            gp += 4 * pos.count<QUEEN>();
            if (gp < 0)  gp = 0;
            if (gp > 24) gp = 24;
            const int t1024 = (gp * 1024) / 24; // 0..1024

            const int openMat = 126, openPos = 134;
            const int endMat  = 134, endPos  = 126;

            // Linear interpolation in 0..1024 space
            wMat = (endMat * (1024 - t1024) + openMat * t1024) / 1024;
            wPos = (endPos * (1024 - t1024) + openPos * t1024) / 1024;
        }

        // Dynamic complexity boost (gated, smoothed, clamped)
        const int complexity = std::abs(psqt - positional);
        const int cg         = 10; // internal constant (UCI ignored)

        if (DynGate::enabled) {
            // normalize complexity to [0,1] and squash it (smoothstep)
            const float c   = std::min(800, complexity) / 800.0f;   // [0..1]
            const float c01 = c * (3.0f - 2.0f * c);                 // smoothstep

            // endgame quench: scale by game phase (based on non-pawn material)
            const int   npm    = pos.non_pawn_material(WHITE) + pos.non_pawn_material(BLACK);
            const float phase  = std::min(1.0f, npm / 6200.0f);      // ~initial NPM ≈ 6200 cp
            const float quench = phase * phase;                      // stronger damping in EG

            // cap the fraction of the raw gain (more conservative)
            const float alpha_max = 0.10f;
            const float d_now     = DynGate::strength * quench * alpha_max * (wPos * cg * c01 / 100.0f);

            // EMA smoothing (lambda = 0.45), per-thread
            static thread_local float s_dyn_prev_eval = 0.0f;
            const float d_sm = (1.0f - 0.45f) * s_dyn_prev_eval + 0.45f * d_now;
            s_dyn_prev_eval  = d_sm;

            // clamp to small integer step in weight domain
            int delta_i = (int)((d_sm >= 0.0f) ? (d_sm + 0.5f) : (d_sm - 0.5f));
            if (delta_i >  4) delta_i =  4;
            if (delta_i < -4) delta_i = -4;

            wPos += delta_i;
        }
        break;

    }
    case Sugar::Eval::WeightsMode::Default:
    default:
        // Keep original behavior (125/131)
        break;
    }
    // --- end of NNUE weights selection ---

    // Sanity clamp to keep weights in a reasonable range
    wMat = std::min(200, std::max(50, wMat));
    wPos = std::min(200, std::max(50, wPos));

    // Scale the small->big switch threshold with current weights (baseline 125+131)
    const int baseThreshold   = 236;
    const int scaledThreshold = baseThreshold * (wMat + wPos) / (125 + 131);

    Value nnue = (wMat * psqt + wPos * positional) / 128;

    // Re-evaluate the position when higher eval accuracy is worth the time spent
    if (smallNet && (std::abs(nnue) < scaledThreshold))
    {
        std::tie(psqt, positional) = networks.big.evaluate(pos, accumulators, &caches.big);
        nnue                       = (wMat * psqt + wPos * positional) / 128;
        smallNet                   = false;
    }

    // Blend optimism and eval with nnue complexity
    int nnueComplexity = std::abs(psqt - positional);
    optimism += optimism * nnueComplexity / 468;
    nnue -= nnue * nnueComplexity / 18000;

    // 'material' already computed above
    int v = (nnue * (77777 + material) + optimism * (7777 + material)) / 77777;

    // Damp down the evaluation linearly when shuffling
    v -= v * pos.rule50_count() / 212;

    // Guarantee evaluation does not hit the tablebase range
    v = std::clamp(v, VALUE_TB_LOSS_IN_MAX_PLY + 1, VALUE_TB_WIN_IN_MAX_PLY - 1);

    return v;
}

// Like evaluate(), but instead of returning a value, it returns
// a string (suitable for outputting to stdout) that contains the detailed
// descriptions and values of each evaluation term. Useful for debugging.
// Trace scores are from white's point of view
std::string Eval::trace(Position& pos, const Eval::NNUE::Networks& networks) {

    if (pos.checkers())
        return "Final evaluation: none (in check)";

    Eval::NNUE::AccumulatorStack accumulators;
    auto                         caches = std::make_unique<Eval::NNUE::AccumulatorCaches>(networks);

    std::stringstream ss;
    ss << std::showpoint << std::noshowpos << std::fixed << std::setprecision(2);
    ss << '\n' << NNUE::trace(pos, networks, *caches) << '\n';

    ss << std::showpoint << std::showpos << std::fixed << std::setprecision(2) << std::setw(15);

    auto [psqt, positional] = networks.big.evaluate(pos, accumulators, &caches->big);
    Value v                 = psqt + positional;
    v                       = pos.side_to_move() == WHITE ? v : -v;
    ss << "NNUE evaluation        " << 0.01 * UCIEngine::to_cp(v, pos) << " (white side)\n";

    v = evaluate(networks, pos, accumulators, *caches, VALUE_ZERO);
    v = pos.side_to_move() == WHITE ? v : -v;
    ss << "Final evaluation       " << 0.01 * UCIEngine::to_cp(v, pos) << " (white side)";
    ss << " [with scaled NNUE, ...]";
    ss << "\n";

    return ss.str();
}

}  // namespace Sugar
