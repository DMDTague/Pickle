# Pickle local Elo tournament

This directory turns Pickle into a reproducible local UCI engine tournament using [FastChess](https://github.com/Disservin/fastchess), official Stockfish `UCI_Elo` anchors, and the Stockfish project's neutral `UHO_Lichess_4852_v1.epd` opening suite.

The goal is an **approximate engine Elo for Pickle at fixed depth 11**, not a Chess.com bot rating and not an official CCRL submission.

## What is being measured

The tournament profile is deliberately simple:

- Pickle searches to **depth 11** on every normal move.
- `OwnBook=false` for Pickle so every engine receives the same neutral starting positions.
- Stockfish uses one thread, 32 MB hash, `UCI_LimitStrength=true`, and named `UCI_Elo` anchors.
- Openings come from `UHO_Lichess_4852_v1.epd` and are repeated with colors reversed.
- FastChess uses standard paired-game reporting and common engine-test adjudication thresholds.
- The final calibration uses Stockfish at **120+1.0**, matching the current time-control basis used for Stockfish's `UCI_Elo` calibration.

This makes the number much more defensible than trying to infer Elo from one or two games against website bots.

It is intentionally a **core-engine D11 rating**. The browser's Lichess Syzygy network probe is not used in local native tournaments, and PickleBook is disabled for the rating run because a common opening suite is the fair comparison method used by engine-testing communities.

## 1. Prepare the Windows tournament environment

Open PowerShell in this directory and run:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\setup_windows.ps1
```

The setup script will:

1. build `pickle.exe` in Release mode;
2. download the latest Windows x86-64 FastChess release;
3. download the latest official Windows x64 Stockfish release;
4. download a pinned copy of `UHO_Lichess_4852_v1.epd` from `official-stockfish/books`;
5. place the runnable files under `tournament/bin` and `tournament/books`;
6. write exact source/release information to `environment.txt`.

Requirements: CMake and a Windows C++ compiler/toolchain. Python is only required for the automatic Elo estimator after the games finish.

If you already placed a freshly built `pickle.exe` at `tournament/bin/pickle.exe`, setup can skip the Pickle build:

```powershell
.\setup_windows.ps1 -SkipBuild
```

## 2. Verify UCI compatibility

```powershell
.\run_compliance.ps1
```

FastChess will exercise Pickle's UCI interface before you spend time on tournament games.

## 3. Find Pickle's rough rating band

Run the quick bracket:

```powershell
.\run_bracket.ps1
```

Defaults:

- Stockfish anchors: 1600, 1800, 2000, 2200, 2400, 2600, 2800, 3000
- 12 paired openings per anchor
- Stockfish time control: 10+0.1
- Pickle: fixed D11

The short time control makes this a **locator**, not the final number. At the end, `estimate_elo.py` reads the PGN and prints an approximate center.

Examples:

```powershell
# More games per anchor
.\run_bracket.ps1 -PairsPerAnchor 25

# Restrict the first pass to a plausible band
.\run_bracket.ps1 -Anchors 2000,2200,2400,2600,2800

# Reduce CPU load
.\run_bracket.ps1 -Concurrency 2
```

Every `PairsPerAnchor=12` means 24 games against each anchor because each opening is played with colors reversed.

## 4. Run the final calibration

Once the quick bracket tells you the neighborhood, center the long run there. For example, if the bracket points near 2500:

```powershell
.\run_calibration.ps1 -Center 2500
```

By default this uses five anchors at `Center - 200`, `Center - 100`, `Center`, `Center + 100`, and `Center + 200`, with 20 paired openings per anchor. That is 200 games total.

For a tighter estimate, increase the pairs:

```powershell
.\run_calibration.ps1 -Center 2500 -PairsPerAnchor 50
```

That produces 500 games. For a serious long run, 100 pairs per anchor gives 1000 games.

The final mode uses Stockfish `120+1.0`, because Stockfish's limited-strength Elo values are calibrated around that time-control regime. Pickle itself remains capped at depth 11, which is the strength profile we are trying to measure.

## 5. Read the result

Each run creates its own timestamped directory under:

```text
results/
```

A completed calibration contains:

```text
games.pgn
fastchess.log
elo-summary.txt
```

The estimator reports results per anchor, total W/D/L, Pickle's fitted rating, and a stratified bootstrap 95% interval. For example:

```text
Estimated Pickle D11 Elo: 2470
95% bootstrap interval:   2415 to 2524
```

That should be reported as something like:

> Pickle D11: approximately 2470 Stockfish-UCI-Elo-calibrated, 95% local bootstrap interval 2415-2524, 500 games.

Do **not** call it an official CCRL rating. Elo depends on the opponent pool, time control, hardware/search limits, opening suite, and rating anchor.

## Reproducibility

`setup_windows.ps1` writes the exact Pickle commit, FastChess release, Stockfish release/asset, and opening-suite revision to `environment.txt`. Keep that file with any result you publish.

The opening positions are paired and color-reversed so a favorable opening is not credited to only one engine. The final rating script fits one Pickle rating simultaneously against every Stockfish anchor instead of simply averaging a handful of individual performance ratings.

## Files

```text
tournament/
├── setup_windows.ps1       build/download the tournament environment
├── run_compliance.ps1      FastChess UCI protocol check
├── run_bracket.ps1         quick broad Elo locator
├── run_calibration.ps1     longer 120+1 Stockfish-anchor gauntlet
├── estimate_elo.py         PGN -> fitted Elo + bootstrap interval
├── bin/                    generated local executables (ignored by git)
├── books/                  downloaded opening suite (ignored by git)
└── results/                local PGNs/logs/summaries (ignored by git)
```
