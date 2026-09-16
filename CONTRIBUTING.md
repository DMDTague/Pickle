# Contributing to Pickle

Pickle is a from-scratch C++ chess engine with a browser build. Contributions are welcome when they make the engine stronger, easier to verify, or easier to use without replacing the project with code copied from another engine.

## Build the native engine

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/pickle
```

Pickle speaks UCI, so a useful smoke test is:

```text
uci
isready
position startpos
go depth 8
```

## Good contribution areas

Small, reviewable changes are preferred. Useful areas include:

- move-generation and perft regression tests
- search correctness and performance measurements
- transposition-table and move-ordering improvements
- evaluation terms backed by tests or before/after positions
- UCI compatibility and time-management fixes
- WebAssembly/browser integration fixes
- documentation that makes the engine easier to build, test, or understand

For search or evaluation changes, include a short explanation of the idea and whatever evidence you used to judge the change. Avoid importing Stockfish source, NNUE weights, or code from another engine unless the licensing and attribution are explicitly compatible with this repository and the change has been discussed first.

## Pull requests

1. Create a focused branch.
2. Keep the change narrow enough to review independently.
3. Build the engine locally and include the commands you ran in the PR description.
4. For move-generation changes, include perft results where relevant.
5. For search/evaluation changes, include a few representative positions, node counts, or match/benchmark results when practical.
6. Explain any tradeoffs instead of only reporting that the code is faster or stronger.

If you want a small first contribution, check the open issues for tasks intentionally scoped for a first PR.
