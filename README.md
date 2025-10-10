
<h1 align="center">SugaR</h1>

  ### License

SugaR is based on the Stockfish engine and is distributed under the GNU General Public License v3.0.
See the [LICENSE](./LICENSE) file for details.


  ### SugaR Overview


SugaR is a free and strong UCI chess engine derived from Stockfish 
that analyzes chess positions and computes the optimal moves.

SugaR does not include a graphical user interface (GUI) that is required 
to display a chessboard and to make it easy to input moves. These GUIs are 
developed independently from SugaR and are available online.

  ### Acknowledgements

This project is built upon the  [Stockfish](https://github.com/official-stockfish/Stockfish)  and would not have been possible without the exceptional work of the Stockfish developers.  
While SugaR has diverged from the latest upstream version due to structural differences and the integration of a custom learning system, the core foundation, ideas, and architecture remain deeply rooted in Stockfish.  
I am sincerely grateful to the entire Stockfish team for making such an outstanding engine openly available to the community.

* Andrew Grant for the [OpenBench](https://github.com/AndyGrant/OpenBench) platform.

SugaR development is currently supported through the OpenBench framework.  
OpenBench, created by Andrew Grant, is an open-source Sequential Probability Ratio Testing (SPRT) framework designed for self-play testing of chess engines.  
It leverages distributed computing, allowing anyone to contribute CPU time to support the development of some of the world’s strongest chess engines.


  #### UCI options
  

### Book1

Default: False  
If activated, the engine will use the external book defined in **Book1 File**.

### Book1 File

Default: `""` (disabled)  
Path to the Polyglot opening book used as Book1.

### Book1 BestBookMove

Default: False  
If enabled, only the single best move from the book will be played.

### Book1 Depth

Default: 255 (range: 1–350)  
Maximum search depth within the opening book.

### Book1 Width

Default: 1 (range: 1–10)  
Number of candidate moves considered from the book at the same position.

---

### Book2

Default: False  
If activated, the engine will use the external book defined in **Book2 File**.

### Book2 File

Default: `""` (disabled)  
Path to the Polyglot opening book used as Book2.

### Book2 BestBookMove

Default: False  
If enabled, only the single best move from the book will be played.

### Book2 Depth

Default: 255 (range: 1–350)  
Maximum search depth within the opening book.

### Book2 Width

Default: 1 (range: 1–10)  
Number of candidate moves considered from the book at the same position.
	
  ### Self-Learning

*	### Experience file structure:

1. e4 (from start position)
1. c4 (from start position)
1. Nf3 (from start position)
1 .. c5 (after 1. e4)
1 .. d6 (after 1. e4)

2 positions and a total of 5 moves in those positions

Now imagine SugaR plays 1. e4 again, it will store this move in the experience file, but it will be duplicate because 1. e4 is already stored. The experience file will now contain the following:
1. e4 (from start position)
1. c4 (from start position)
1. Nf3 (from start position)
1 .. c5 (after 1. e4)
1 .. d6 (after 1. e4)
1. e4 (from start position)

Now we have 2 positions, 6 moves, and 1 duplicate move (so effectively the total unique moves is 5)

Duplicate moves are a problem and should be removed by merging with existing moves. The merge operation will take the move with the highst depth and ignore the other ones. However, when the engine loads the experience file it will only merge duplicate moves in memory without saving the experience file (to make startup and loading experience file faster)

At this point, the experience file is considered fragmented because it contains duplicate moves. The fragmentation percentage is simply: (total duplicate moves) / (total unique moves) * 100
In this example we have a fragmentation level of: 1/6 * 100 = 16.67%


  ### Experience Readonly

  Default: False If activated, the experience file is only read.
  
  ### Experience Book

  SugaR play using the moves stored in the experience file as if it were a book

  ### Experience Book Width

The number of moves to consider from the book for the same position.

  ### Experience Book Eval Importance

The quality of experience book moves has been revised heavily based on feedback from users. The new logic relies on a new parameter called (Experience Book Eval Importance) which defines how much weight to assign to experience move evaluation vs. count.

The maximum value for this new parameter is: 10, which means the experience move quality will be 100% based on evaluation, and 0% based on count

The minimum value for this new parameter is: 0, which means the experience move quality will be 0% based on evaluation, and 100% based on count

The default value for this new parameter is: 5, which means the experience move quality will be 50% based on evaluation, and 50% based on count	

  ### Experience Book Min Depth

Type: Integer
Default Value: 27
Range: Max to 64
This option sets the minimum depth threshold for moves to be included in the engine's Experience Book. Only moves that have been searched at least to the specified depth will be considered for inclusion.

  ### Experience Book Max Moves

Type: Integer
Default Value: 16
Range: 1 to 100
	This is a setup to limit the number of moves that can be played by the experience book.
	If you configure 16, the engine will only play 16 moves (if available).

  ### Variety

Enables randomization of move selection in balanced positions not covered by the opening book.  
A higher value increases the probability of deviating from the mainline, potentially at the cost of Elo.

This option is mainly intended for testing, analysis, or generating varied self-play games.

Set to `0` for fully deterministic behavior.  
Typical useful range: `10–20` for light variety in early game.

 Note: Variety works in combination with `Variety Max Score` and `Variety Max Moves`, which control the conditions under which randomness can apply.
  ### Variety Max Score

Maximum score threshold (in centipawns) below which randomization of the best move is allowed.  
If the absolute evaluation is below this value, the engine may apply a small, controlled random bonus  
to the best move score in order to increase variability in balanced positions.

- A value of `0` disables the feature (fully deterministic behavior).
- Typical values range from `10` to `30`.
- The hard maximum is `50`, beyond which the randomness could affect clearly winning or losing positions and is not recommended.

This feature is primarily intended for testing purposes, to introduce diversity in games that would otherwise be repetitive.

  ### Variety Max Moves

Maximum game ply (half-moves) under which the variety bonus can be applied.  
Once the game progresses beyond this ply count, the randomization feature is disabled.

- `0` disables the feature entirely.
- Values between `10` and `30` are typical for introducing diversity early in the game.
- The hard cap is `40`, since variety in late-game scenarios is generally undesirable.

This setting prevents randomness from affecting important endgame decisions. 

## Dynamic/Pressing branch

  ### AttackInclination
- **Type:** Integer (0–100) — **Default:** 0  
- **Effect:** Lightens **LMR** on forcing moves (checks/captures).  
  - `20…49` ≈ half step (−512)  
  - `≥50` ≈ full step (−1024)  
- **Notes:** No effect on non-forcing moves; neutral at default.

  ### CheckSacrificeToleranceCp
- **Type:** Integer (0–80 cp) — **Default:** 0  
- **Effect:** When a move gives **check**, increases SEE margin by **+cp** (allows slightly more negative SEE to pass).  
- **Notes:** Applied only in SEE-based pruning; neutral at default.setoption name CheckSacrificeToleranceCp value 35

  ### NNUE Dynamic Weights

Type: Boolean — Default: true

Description: enables dynamic blending of NNUE weights wMat/wPos using tapered game phase (0..24 → t 0..1024) and a small complexity boost when |psqt - positional| is large. No Shashin.

Interaction: if NNUE ManualWeights = true, Dynamic is bypassed (Manual has priority).

Note: the Dyn Open/Endgame … options below define the profiles used by Dynamic.

  ### NNUE ManualWeights

Type: Boolean — Default: false

Description: forces manual weights:

wMat = 125 + NNUE StrategyMaterialWeight

wPos = 131 + NNUE StrategyPositionalWeight
and ignores all Dyn … options.

  ### NNUE StrategyMaterialWeight

Type: Integer (-12..12) — Default: 0

Description: delta over 125 (Manual mode only).

  ### NNUE StrategyPositionalWeight

Type: Integer (-12..12) — Default: 0

Description: delta over 131 (Manual mode only).

  ### (Debug) NNUE Log Weights

Type: Boolean — Default: false

Description: prints one line per search at root: wMat/wPos, phase t, small/big net, scaled threshold.


  ### Fail-High/Low Info (UCI)

What it is
Controls how often the engine prints info/PV lines when a fail-high/low occurs, to avoid GUI spam while keeping feedback timely.

  ### Options

FailInfo Enabled (bool, default true) — master switch.

FailInfo First ms (int, 0..60000, default 4000) — time gate (ms from search start) for the first update.

FailInfo Min Nodes (int, 0..1000000000, default 10000000) — alternative nodes gate for the first update.

FailInfo Rate ms (int, 0..10000, default 400) — minimal interval between consecutive updates.

  ### Behavior

First update is allowed when (elapsed ≥ First ms) OR (nodes ≥ Min Nodes).

Subsequent updates: at most one every Rate ms.

Active only for main thread, MultiPV=1, and fail-high/low.

The rate limiter resets at the start of each new root iteration (rootDepth==1).

Changes apply from the next search (after ucinewgame or new game).

  ### Recommended presets

Bullet/Blitz: First 800–1500, MinNodes 5–10M, Rate 300–400.

Rapid: First 3000–6000, MinNodes 10–20M, Rate 400–800.

Classical: First 4000–8000, MinNodes 20–40M, Rate 600–1000.