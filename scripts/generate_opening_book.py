#!/usr/bin/env python3
"""Generate Pickle's compact opening book from the official Stockfish books repo.

Source: official-stockfish/books, 8moves_v3.pgn.zip (CC0-1.0)
Pinned revision and raw archive SHA-384 digest make regeneration deterministic.
"""

from __future__ import annotations

import argparse
import base64
import hashlib
import io
import sys
import urllib.request
import zipfile
from collections import Counter, defaultdict
from pathlib import Path

import chess
import chess.pgn

STOCKFISH_BOOKS_COMMIT = "65815ccdbc7727cd4f6aee252ba8f67fb740e92f"
BOOK_NAME = "8moves_v3.pgn"
BOOK_ARCHIVE = f"{BOOK_NAME}.zip"
BOOK_URL = (
    "https://raw.githubusercontent.com/official-stockfish/books/"
    f"{STOCKFISH_BOOKS_COMMIT}/{BOOK_ARCHIVE}"
)
# SHA-384 of the raw ZIP bytes at the pinned commit above.
EXPECTED_SHA384_B64 = "AoBfMaMZlFdkNLJ6nX7DnL0t1YR+PHAmIFO1+DIH12sexBWTep6t76DxXe05BU5f"
MAX_PLIES = 16
MIN_OCCURRENCES = 2
MAX_MOVES_PER_POSITION = 4
MASK32 = 0xFFFFFFFF


def _xorshift32(state: int) -> int:
    state ^= (state << 13) & MASK32
    state &= MASK32
    state ^= state >> 17
    state &= MASK32
    state ^= (state << 5) & MASK32
    return state & MASK32


def _pickle_zobrist_tables():
    state = 1804289383

    def u32() -> int:
        nonlocal state
        state = _xorshift32(state)
        return state

    def u64() -> int:
        n1 = u32() & 0xFFFF
        n2 = u32() & 0xFFFF
        n3 = u32() & 0xFFFF
        n4 = u32() & 0xFFFF
        return n1 | (n2 << 16) | (n3 << 32) | (n4 << 48)

    piece_keys = [[u64() for _ in range(64)] for _ in range(12)]
    ep_keys = [u64() for _ in range(8)]
    castle_keys = [u64() for _ in range(16)]
    side_key = u64()
    return piece_keys, ep_keys, castle_keys, side_key


PIECE_KEYS, EP_KEYS, CASTLE_KEYS, SIDE_KEY = _pickle_zobrist_tables()


def pickle_hash(board: chess.Board) -> int:
    key = 0
    for square, piece in board.piece_map().items():
        color_index = 0 if piece.color == chess.WHITE else 1
        type_index = piece.piece_type - 1
        key ^= PIECE_KEYS[color_index * 6 + type_index][square]

    if board.ep_square is not None:
        key ^= EP_KEYS[chess.square_file(board.ep_square)]

    rights = 0
    if board.has_kingside_castling_rights(chess.WHITE):
        rights |= 1
    if board.has_queenside_castling_rights(chess.WHITE):
        rights |= 2
    if board.has_kingside_castling_rights(chess.BLACK):
        rights |= 4
    if board.has_queenside_castling_rights(chess.BLACK):
        rights |= 8
    key ^= CASTLE_KEYS[rights]

    if board.turn == chess.BLACK:
        key ^= SIDE_KEY
    return key


def pack_move(move: chess.Move) -> int:
    # Pickle PieceType values are PAWN=0, KNIGHT=1, BISHOP=2, ROOK=3, QUEEN=4.
    promotion = 0 if move.promotion is None else move.promotion - 1
    return move.from_square | (move.to_square << 6) | (promotion << 12)


def download_source() -> str:
    request = urllib.request.Request(
        BOOK_URL,
        headers={"User-Agent": "Pickle-opening-book-generator/1.0"},
    )
    with urllib.request.urlopen(request, timeout=60) as response:
        archive = response.read()

    digest = base64.b64encode(hashlib.sha384(archive).digest()).decode("ascii")
    if digest != EXPECTED_SHA384_B64:
        raise RuntimeError(
            "Stockfish book digest mismatch: "
            f"expected {EXPECTED_SHA384_B64}, got {digest}"
        )

    with zipfile.ZipFile(io.BytesIO(archive)) as zf:
        pgn_names = [name for name in zf.namelist() if name.lower().endswith(".pgn")]
        if len(pgn_names) != 1:
            raise RuntimeError(f"expected exactly one PGN in archive, found {pgn_names}")
        return zf.read(pgn_names[0]).decode("utf-8", errors="replace")


def compile_book(pgn_text: str):
    choices: dict[int, Counter[int]] = defaultdict(Counter)
    occurrences: Counter[int] = Counter()
    min_ply: dict[int, int] = {}
    stream = io.StringIO(pgn_text)
    games = 0
    plies = 0

    while True:
        game = chess.pgn.read_game(stream)
        if game is None:
            break
        games += 1
        board = game.board()
        for ply, move in enumerate(game.mainline_moves()):
            if ply >= MAX_PLIES:
                break
            key = pickle_hash(board)
            packed = pack_move(move)
            choices[key][packed] += 1
            occurrences[key] += 1
            min_ply[key] = min(min_ply.get(key, ply), ply)
            board.push(move)
            plies += 1

    entries = []
    flat_moves = []
    for key in sorted(choices):
        total = occurrences[key]
        if total < MIN_OCCURRENCES and min_ply[key] > 4:
            continue
        ranked = sorted(
            choices[key].items(),
            key=lambda item: (-item[1], item[0]),
        )[:MAX_MOVES_PER_POSITION]
        offset = len(flat_moves)
        for packed, weight in ranked:
            flat_moves.append((packed, min(weight, 65535)))
        entries.append((key, offset, len(ranked)))

    return games, plies, entries, flat_moves


def write_include(path: Path, games: int, plies: int, entries, moves) -> None:
    lines = [
        "// Generated by scripts/generate_opening_book.py. Do not edit by hand.",
        "// Upstream: https://github.com/official-stockfish/books",
        f"// Revision: {STOCKFISH_BOOKS_COMMIT}",
        f"// Source: {BOOK_ARCHIVE} (CC0-1.0)",
        f"// Parsed games: {games}; plies: {plies}; retained positions: {len(entries)}",
        "",
        "struct GeneratedBookEntry {",
        "    std::uint64_t key;",
        "    std::uint32_t move_offset;",
        "    std::uint8_t move_count;",
        "};",
        "",
        "struct GeneratedBookMove {",
        "    std::uint16_t packed;",
        "    std::uint16_t weight;",
        "};",
        "",
        f"static constexpr std::size_t STOCKFISH_BOOK_ENTRY_COUNT = {len(entries)};",
        f"static constexpr std::size_t STOCKFISH_BOOK_MOVE_COUNT = {len(moves)};",
        "",
        "static constexpr GeneratedBookEntry STOCKFISH_BOOK_ENTRIES[] = {",
    ]
    if entries:
        lines.extend(
            f"    {{0x{key:016x}ULL, {offset}u, {count}u}},"
            for key, offset, count in entries
        )
    else:
        lines.append("    {0ULL, 0u, 0u},")
    lines.extend([
        "};",
        "",
        "static constexpr GeneratedBookMove STOCKFISH_BOOK_MOVES[] = {",
    ])
    if moves:
        lines.extend(
            f"    {{{packed}u, {weight}u}}," for packed, weight in moves
        )
    else:
        lines.append("    {0u, 0u},")
    lines.extend(["};", ""])
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", default="opening_book_data.inc")
    args = parser.parse_args()

    pgn_text = download_source()
    games, plies, entries, moves = compile_book(pgn_text)
    if games < 30000:
        raise RuntimeError(f"unexpectedly small Stockfish book: only {games} games")
    if len(entries) < 500:
        raise RuntimeError(f"unexpectedly small compiled book: only {len(entries)} positions")

    output = Path(args.output)
    write_include(output, games, plies, entries, moves)
    print(
        f"generated {output}: {games} games, {plies} plies, "
        f"{len(entries)} positions, {len(moves)} weighted moves"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"opening book generation failed: {exc}", file=sys.stderr)
        raise
