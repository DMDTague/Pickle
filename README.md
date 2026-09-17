# Pickle

A chess engine I built from scratch in C++, now playable in the browser.

**Play Pickle:** https://pickle-dmdtagues-projects.vercel.app

Pickle started as an excuse to learn what actually sits underneath a chess engine: bitboards, move generation, evaluation, search, hashing, time management, and all the small decisions that turn legal moves into good ones. It has gradually become a real engine project rather than a toy move picker.

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

I want Pickle to play actively without confusing aggression with correctness. Its evaluation therefore rewards pressure that actually exists on the board rather than artificially discounting its own material.

The current evaluator blends middlegame and endgame terms and considers material, piece-square placement, mobility, bishop pair, pawn structure, passed and connected pawns, knight outposts, rook files and seventh-rank activity, king shelter, open files around the king, endgame king activity, pawn storms, and coordinated attacks on the enemy king zone.

That gives the engine a bias toward active positions while still requiring compensation to be real.

## Play against Pickle

The `web/` directory contains a React interface for playing directly against the C++ engine.

**Live:** https://pickle-dmdtagues-projects.vercel.app

The engine is compiled to WebAssembly and runs inside a Web Worker, so the browser is running **Pickle itself** rather than replacing it with another chess engine. `chess.js` handles browser-side game state and legal interaction, while `react-chessboard` provides the board component.

The UI includes:

- play as White or Black
- multiple search-strength presets
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
└── web/                    playable React interface
```

## Contributing

Focused contributions are welcome, especially around perft/regression testing, search correctness, engine benchmarking, UCI behavior, and the browser build. See [`CONTRIBUTING.md`](CONTRIBUTING.md) for the contribution workflow and suggested areas to work on.

## A note on engine code

Pickle is its own chess engine. It uses established chess-engine techniques and a derived strong-human opening dataset, but its search, evaluation, move generation, hashing, time management, UCI implementation, opening-book policy, and browser engine are Pickle's own codebase.
