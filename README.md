# Pickle

A chess engine I built from scratch in C++, now playable in the browser.

**Play Pickle:** https://pickle-dmdtagues-projects.vercel.app

I got bored one day while working on Lean formalization and came back to this engine to take my mind off the existential horrors of theoretical mathematics with some good old-fashioned alpha-beta pruning.

Pickle is a chess engine I built from scratch in C++. It uses bitboards, magic-bitboard attacks, handcrafted evaluation, alpha-beta/negamax search, principal variation search, transposition tables, move ordering, pruning and reduction techniques, its own opening book, and a UCI interface, with the same engine also compiled to WebAssembly for browser play.

I specifically wanted to build my own engine rather than start with Stockfish or another open-source engine and progressively inherit its architecture. Pickle uses established computer-chess ideas, of course, but the point of the project has been to implement, test, break, repair, and tune those ideas myself and see how far I can push an engine whose search and evaluation are actually mine.

## Engine

Pickle currently includes:

- bitboard board representation
- magic-bitboard sliding attacks
- legal move generation, castling, promotion, and en passant
- Zobrist hashing and a transposition table
- iterative deepening
- alpha-beta negamax with principal variation search
- aspiration windows
- null-move pruning
- adaptive late-move reductions
- killer moves and history ordering
- check-aware quiescence search
- mate-distance-aware scoring and transposition-table handling
- exact mate-in-one selection before normal search
- built-in opening-book support with a UCI `OwnBook` switch
- UCI support and configurable hash size
- clock-aware search limits

### PickleBook

Pickle's opening repertoire is **PickleBook**, a compact book derived from strong non-bullet games in the Lichess Elite Database rather than from a generic engine-testing book. The current seed tree comes from a pinned Lichess-Elite-derived repertoire in PyCheckmate, whose book builder samples up to 80,000 elite games through the first 16 plies. Pickle pins both the upstream repository revision and the exact source blob used for regeneration.

The upstream human frequencies are only a prior. `scripts/generate_opening_book.py` replays the tree into Pickle's own Zobrist format and reweights each candidate for the limitations of Pickle's shallow browser search. The filter rewards direct development and castling, penalizes repeated early queen moves and exposed kings, penalizes new pawn defects, and downweights positions with unusually high immediate branching or forcing-move density. Low-fit alternatives are removed instead of being kept merely because they are theoretically playable.

This is deliberately different from asking a depth-11 engine to understand why a long-term structural concession may pay off dozens of moves later. The book is intended to hand Pickle positions where the important features are visible inside its horizon. The observed `Qxd5` Scandinavian tempo-loss line is part of the generator's regression audit: the development-first `...Nf6` continuation must outrank the early queen capture before a book can be produced.

The generated result is compiled into `opening_book_data.inc`, so native and browser Pickle have no opening-time network dependency. The source repertoire is data only; Pickle does not run PyCheckmate or another chess engine for book moves.

### How Pickle evaluates a position

I originally hoped Pickle would play like his namesake: the titular satanic Border Collie from Adult Swim's *Mr. Pickles*, happily killing and mutilating its unfortunate human chess opponents through wildly aggressive play.

At one point I considered making the Colle System a central part of Pickle's repertoire, but decided against it until I was confident I could program the engine to be aggressive enough to make playing the Colle feel morally defensible.

Unfortunately, every time I turned the aggression dial too far, Pickle's Elo began falling off a cliff.

That changed the project somewhat. Watching Pickle's Elo fall off a cliff every time I tried to force more aggression into it made something obvious to me: I did not yet have the fundamental understanding of computer-chess analysis needed to build an engine strong enough to achieve the thing I actually wanted to make. So the project stopped being just a destination and the progress toward that destination became the project itself.

For now, the goal is to push Pickle's strength as far as I can independently, without simply looking up the answers and copying the strongest known solutions, until I can develop an engine capable of consistently beating opposition at or above the level of the best human players in the world, with a long-term target around 3000+ on the engine-calibration scale. I want to get there by actually learning why stronger search, evaluation, pruning, move ordering, and positional understanding work rather than treating Stockfish or another open engine as an answer key.

Its evaluation is entirely handcrafted and considers material, piece-square placement, mobility, bishop pair, pawn structure, passed and connected pawns, knight outposts, rook activity, king shelter, open files around the king, endgame king activity, pawn storms, and coordinated pressure on the enemy king.

Once Pickle reaches a strength range I'm satisfied with, I want to investigate the more interesting questions: what does this engine value differently from Stockfish and stronger open-source engines such as Ethereal, Berserk, and Koivisto? Which of those differences are weaknesses, which are merely stylistic, and how much aggression can be deliberately reintroduced before playing strength starts collapsing again?

I do not expect to work on Pickle on any fixed schedule. I will probably keep returning to it whenever the prospect of creating a satanic Border Collie chess nightmare powered by cold machinery, good mathematics, and increasingly competent computer science becomes too entertaining to ignore.

Eventually, the goal is still the same: make Pickle aggressive and strong enough to make Magnus shake in his boots.

## Play against Pickle

The `web/` directory contains a React interface for playing directly against the C++ engine.

**Live:** https://pickle-dmdtagues-projects.vercel.app

The engine is compiled to WebAssembly and runs inside a Web Worker, so the browser is running **Pickle itself** rather than replacing it with another chess engine. `chess.js` handles browser-side game state and legal interaction, while `react-chessboard` provides the board component.

The UI includes:

- play as White or Black
- fixed depth-11 engine play
- live engine evaluation during normal search
- search depth and node count
- move history
- drag-and-drop or click-to-move controls
- legal-move destination highlights and capture rings
- last-move and check highlighting
- opening-book source indication
- seven-piece-or-fewer endgame tablebase probing
- a tablebase panel that shows legal moves by win/draw/loss outcome instead of inventing a centipawn evaluation
- undo and board flip controls
- responsive desktop/mobile layout

For eligible standard-chess positions with seven pieces or fewer, the browser probes the public Lichess tablebase service before falling back to Pickle's normal search. Each probe gets multiple attempts with timeouts and short backoff; a transient timeout does not mark the service unavailable for the rest of the game. Tablebase results are treated as game-theoretic win/draw/loss information, not as engine evaluations.

The board uses the MIT-licensed [`react-chessboard`](https://github.com/Clariity/react-chessboard) component as its rendering/interaction foundation. The surrounding interface and Pickle integration are specific to this project; it does not copy Chess.com or Lichess assets or source code.

## Build the native engine

Generate PickleBook first:

```bash
python -m pip install python-chess==1.999
python scripts/generate_opening_book.py --output opening_book_data.inc
```

Then build Pickle:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/pickle
```

Pickle speaks UCI, so it can also be loaded into a compatible chess GUI.

For example:

```text
uci
isready
position startpos
go depth 8
```

To force normal search instead of using the built-in opening book:

```text
setoption name OwnBook value false
```

## Build the WebAssembly engine

Install the Emscripten SDK, generate PickleBook, then run:

```bash
bash scripts/build_wasm.sh
```

This writes the browser engine to:

```text
web/public/engine/pickle.js
web/public/engine/pickle_web.wasm
```

## Run the web app

```bash
cd web
npm install
npm run dev
```

For a production build:

```bash
npm run build
```

The repository includes a root `vercel.json`. The production Vercel project is linked to this repository and deploys from `main`, so new pushes to `main` trigger the production deployment pipeline.

## Local Elo tournament

`tournament/` contains a Windows FastChess harness for estimating Pickle's fixed-D11 strength against official Stockfish `UCI_Elo` anchors. It builds/downloads the required binaries, uses the neutral `UHO_Lichess_4852_v1.epd` opening suite with paired colors, runs a quick rating bracket, then supports a longer 120+1 calibration run with a fitted Elo estimate and bootstrap confidence interval.

See [`tournament/README.md`](tournament/README.md) for the complete workflow.

## Project layout

```text
Pickle/
├── board.*                 board state, FEN, move execution
├── movegen.*               move generation
├── attacks.*               leaper attacks
├── magics.*                sliding-piece attack tables
├── evaluate.*              static evaluation
├── opening_book.*          compiled PickleBook lookup
├── opening_book_data.inc   generated PickleBook data
├── search.*                search and move ordering
├── tt.*                    transposition table
├── time_manager.*          search time allocation
├── uci.*                   UCI protocol
├── zobrist.*               position hashing
├── wasm_api.cpp            browser-facing C++ API
├── scripts/                build/book-generation helpers
├── tournament/             local FastChess Elo harness
└── web/                    playable React interface
```

## Contributing

Focused contributions are welcome, especially around perft/regression testing, search correctness, engine benchmarking, UCI behavior, and the browser build. See [`CONTRIBUTING.md`](CONTRIBUTING.md) for the contribution workflow and suggested areas to work on.

## A note on engine code

Pickle is its own chess engine. It uses established chess-engine techniques and a derived strong-human opening dataset, but its search, evaluation, move generation, hashing, time management, UCI implementation, opening-book policy, and browser engine are Pickle's own codebase.
