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
- check extensions
- killer moves and history ordering
- check-aware quiescence search
- shallow futility and reverse-futility pruning
- UCI support and configurable hash size
- clock-aware search limits

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
- live engine evaluation
- search depth and node count
- move history
- undo and board flip controls
- responsive desktop/mobile layout

The board uses the MIT-licensed [`react-chessboard`](https://github.com/Clariity/react-chessboard) component as its rendering/interaction foundation. The surrounding interface and Pickle integration are specific to this project; it does not copy Chess.com assets or source code.

## Build the native engine

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

## Build the WebAssembly engine

Install the Emscripten SDK, then run:

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

The repository includes a root `vercel.json`, so the web app can be deployed from the repository root once the generated WebAssembly files are present.

## Project layout

```text
Pickle/
├── board.*             board state, FEN, move execution
├── movegen.*           move generation
├── attacks.*           leaper attacks
├── magics.*            sliding-piece attack tables
├── evaluate.*          static evaluation
├── search.*            search and move ordering
├── tt.*                transposition table
├── time_manager.*      search time allocation
├── uci.*               UCI protocol
├── zobrist.*           position hashing
├── wasm_api.cpp        browser-facing C++ API
├── scripts/            build helpers
└── web/                playable React interface
```

## Contributing

Focused contributions are welcome, especially around perft/regression testing, search correctness, engine benchmarking, UCI behavior, and the browser build. See [`CONTRIBUTING.md`](CONTRIBUTING.md) for the contribution workflow and suggested areas to work on.

## A note on engine code

Pickle is not Stockfish with a different name. It does not bundle Stockfish code, weights, or an NNUE network. It uses established chess-engine techniques, but the implementation and evaluation in this repository are built around Pickle's own codebase.
