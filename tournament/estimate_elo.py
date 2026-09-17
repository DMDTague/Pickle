#!/usr/bin/env python3
"""Estimate Pickle's Elo from a FastChess PGN against Stockfish UCI_Elo anchors.

The estimate solves the standard logistic Elo score equation across all anchors.
The confidence interval is a stratified non-parametric bootstrap over game results.
No third-party Python packages are required.
"""

from __future__ import annotations

import argparse
import math
import random
import re
from collections import defaultdict
from pathlib import Path

TAG_RE = re.compile(r'^\[([A-Za-z0-9_]+)\s+"(.*)"\]$')
ANCHOR_RE = re.compile(r'^Stockfish_(\d+)$')
PICKLE_NAME = 'Pickle_D11'


def read_games(path: Path):
    games = []
    headers = {}

    def flush():
        nonlocal headers
        if {'White', 'Black', 'Result'} <= headers.keys():
            games.append(headers)
        headers = {}

    with path.open('r', encoding='utf-8-sig', errors='replace') as handle:
        for raw in handle:
            line = raw.strip()
            if line.startswith('['):
                match = TAG_RE.match(line)
                if match:
                    key, value = match.groups()
                    if key == 'Event' and headers.get('Result'):
                        flush()
                    headers[key] = value
            elif not line and headers.get('Result'):
                flush()
        if headers.get('Result'):
            flush()

    return games


def pickle_score(game):
    white = game['White']
    black = game['Black']
    result = game['Result']

    if white == PICKLE_NAME:
        opponent = black
        if result == '1-0':
            score = 1.0
        elif result == '0-1':
            score = 0.0
        elif result == '1/2-1/2':
            score = 0.5
        else:
            return None
    elif black == PICKLE_NAME:
        opponent = white
        if result == '0-1':
            score = 1.0
        elif result == '1-0':
            score = 0.0
        elif result == '1/2-1/2':
            score = 0.5
        else:
            return None
    else:
        return None

    match = ANCHOR_RE.match(opponent)
    if not match:
        return None
    return int(match.group(1)), score


def expectation(rating: float, opponent: float) -> float:
    exponent = (opponent - rating) / 400.0
    if exponent > 20:
        return 0.0
    if exponent < -20:
        return 1.0
    return 1.0 / (1.0 + 10.0 ** exponent)


def solve_rating(samples, lo=400.0, hi=4000.0):
    if not samples:
        raise ValueError('No usable games were found.')

    def residual(rating):
        return sum(score - expectation(rating, opponent) for opponent, score in samples)

    if residual(lo) <= 0:
        return lo
    if residual(hi) >= 0:
        return hi

    for _ in range(80):
        mid = (lo + hi) / 2.0
        if residual(mid) > 0:
            lo = mid
        else:
            hi = mid
    return (lo + hi) / 2.0


def percentile(sorted_values, p):
    if not sorted_values:
        return float('nan')
    position = (len(sorted_values) - 1) * p
    lower = int(math.floor(position))
    upper = int(math.ceil(position))
    if lower == upper:
        return sorted_values[lower]
    fraction = position - lower
    return sorted_values[lower] * (1.0 - fraction) + sorted_values[upper] * fraction


def bootstrap_interval(grouped_scores, iterations=5000, seed=0x5049434B4C45):
    rng = random.Random(seed)
    ratings = []
    groups = sorted(grouped_scores.items())

    for _ in range(iterations):
        sample = []
        for anchor, scores in groups:
            for _ in range(len(scores)):
                sample.append((anchor, rng.choice(scores)))
        ratings.append(solve_rating(sample))

    ratings.sort()
    return percentile(ratings, 0.025), percentile(ratings, 0.975)


def performance_rating(anchor, scores):
    # Half-game smoothing avoids infinite performance ratings after a sweep.
    points = sum(scores)
    smoothed = (points + 0.5) / (len(scores) + 1.0)
    return anchor + 400.0 * math.log10(smoothed / (1.0 - smoothed))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('pgn', type=Path)
    parser.add_argument('--label', default='Pickle Elo calibration')
    parser.add_argument('--bootstrap', type=int, default=5000)
    args = parser.parse_args()

    games = read_games(args.pgn)
    samples = []
    grouped = defaultdict(list)

    for game in games:
        item = pickle_score(game)
        if item is None:
            continue
        anchor, score = item
        samples.append((anchor, score))
        grouped[anchor].append(score)

    if not samples:
        raise SystemExit(
            'No Pickle_D11 vs Stockfish_<Elo> games were found in the PGN. '
            'Use the supplied FastChess scripts so engine names match the estimator.'
        )

    print(args.label)
    print('=' * len(args.label))
    print(f'PGN: {args.pgn}')
    print()
    print(f'{"Anchor":>8} {"Games":>7} {"W":>5} {"D":>5} {"L":>5} {"Score":>8} {"Perf Elo":>10}')

    total_wins = total_draws = total_losses = 0
    for anchor in sorted(grouped):
        scores = grouped[anchor]
        wins = sum(score == 1.0 for score in scores)
        draws = sum(score == 0.5 for score in scores)
        losses = sum(score == 0.0 for score in scores)
        pct = 100.0 * sum(scores) / len(scores)
        perf = performance_rating(anchor, scores)
        total_wins += wins
        total_draws += draws
        total_losses += losses
        print(f'{anchor:8d} {len(scores):7d} {wins:5d} {draws:5d} {losses:5d} {pct:7.2f}% {perf:10.0f}')

    rating = solve_rating(samples)
    low, high = bootstrap_interval(grouped, iterations=max(200, args.bootstrap))
    total = len(samples)
    total_score = 100.0 * sum(score for _, score in samples) / total

    print()
    print(f'Total games: {total}  W/D/L: {total_wins}/{total_draws}/{total_losses}  Score: {total_score:.2f}%')
    print(f'Estimated Pickle D11 Elo: {rating:.0f}')
    print(f'95% bootstrap interval:   {low:.0f} to {high:.0f}')
    print()
    print('Interpretation: this is a local Stockfish-UCI_Elo-calibrated estimate, not an official CCRL rating.')
    print('The interval measures sampling uncertainty in these games; it does not include uncertainty in Stockfish\'s Elo calibration.')


if __name__ == '__main__':
    main()
