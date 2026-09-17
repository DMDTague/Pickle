#!/usr/bin/env python3
"""Generate PickleBook from a pinned Lichess-Elite-derived repertoire.

The upstream repertoire is the compact book committed by PyCheckmate. Its
builder derives that data from the Lichess Elite Database (strong non-bullet
Lichess games), using up to 80,000 games and the first 16 plies. We pin both
that repository revision and the exact Git blob so regeneration is stable.

Pickle does not copy the upstream weights blindly. Human frequency is only the
prior. Each candidate is reweighted for Pickle's deliberately shallow browser
search: early development and castling are rewarded, repeated early queen
moves and king exposure are strongly penalized, new pawn defects are penalized,
and positions with unusually high immediate reply/forcing-move complexity are
downweighted. Rare low-fit alternatives are removed entirely.

The result is a book designed to hand depth-11 Pickle a position whose important
features are visible inside its horizon rather than merely a theoretically
respectable position that requires long-range strategic understanding.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import random
import urllib.request
from pathlib import Path

import chess

UPSTREAM_COMMIT = "edd4049bb314147402f638db9a92f52150c94162"
UPSTREAM_BLOB_SHA1 = "a7fb509a3080f733521142b9b8b6c4b2aef35e9d"
UPSTREAM_URL = (
    "https://raw.githubusercontent.com/pdloc06/PyCheckmate/"
    f"{UPSTREAM_COMMIT}/books/book.json"
)
UPSTREAM_BUILDER_URL = (
    "https://github.com/pdloc06/PyCheckmate/blob/"
    f"{UPSTREAM_COMMIT}/engine/tools/build_book.py"
)
LICHESS_ELITE_URL = "https://database.nikonoel.fr/"

MAX_PLIES = 16
MAX_MOVES_PER_POSITION = 4
RELATIVE_CUTOFF = 0.30
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


PICKLE_PIECE_KEYS, PICKLE_EP_KEYS, PICKLE_CASTLE_KEYS, PICKLE_SIDE_KEY = _pickle_zobrist_tables()


def pickle_hash(board: chess.Board) -> int:
    key = 0
    for square, piece in board.piece_map().items():
        color_index = 0 if piece.color == chess.WHITE else 1
        type_index = piece.piece_type - 1
        key ^= PICKLE_PIECE_KEYS[color_index * 6 + type_index][square]

    if board.ep_square is not None:
        key ^= PICKLE_EP_KEYS[chess.square_file(board.ep_square)]

    rights = 0
    if board.has_kingside_castling_rights(chess.WHITE):
        rights |= 1
    if board.has_queenside_castling_rights(chess.WHITE):
        rights |= 2
    if board.has_kingside_castling_rights(chess.BLACK):
        rights |= 4
    if board.has_queenside_castling_rights(chess.BLACK):
        rights |= 8
    key ^= PICKLE_CASTLE_KEYS[rights]

    if board.turn == chess.BLACK:
        key ^= PICKLE_SIDE_KEY
    return key


# PyCheckmate's committed book uses its own deterministic Zobrist layout. We
# reproduce only the key schedule so its already-pruned Lichess Elite tree can
# be traversed and re-keyed for Pickle.
_py_rng = random.Random(20260716)
PY_PIECE_KEYS = {
    piece: [[_py_rng.getrandbits(64) for _ in range(8)] for _ in range(8)]
    for piece in range(1, 13)
}
PY_SIDE_KEY = _py_rng.getrandbits(64)
PY_CASTLE_KEYS = [_py_rng.getrandbits(64) for _ in range(16)]
PY_EP_KEYS = [_py_rng.getrandbits(64) for _ in range(8)]


def pycheckmate_hash(board: chess.Board) -> int:
    key = 0
    for square, piece in board.piece_map().items():
        row = 7 - chess.square_rank(square)
        col = chess.square_file(square)
        code = piece.piece_type + (0 if piece.color == chess.WHITE else 6)
        key ^= PY_PIECE_KEYS[code][row][col]

    if board.turn == chess.BLACK:
        key ^= PY_SIDE_KEY

    rights = 0
    if board.has_kingside_castling_rights(chess.WHITE):
        rights |= 8
    if board.has_queenside_castling_rights(chess.WHITE):
        rights |= 4
    if board.has_kingside_castling_rights(chess.BLACK):
        rights |= 2
    if board.has_queenside_castling_rights(chess.BLACK):
        rights |= 1
    key ^= PY_CASTLE_KEYS[rights]

    if board.ep_square is not None:
        key ^= PY_EP_KEYS[chess.square_file(board.ep_square)]
    return key


def pack_move(move: chess.Move) -> int:
    promotion = 0 if move.promotion is None else move.promotion - 1
    return move.from_square | (move.to_square << 6) | (promotion << 12)


def git_blob_sha1(payload: bytes) -> str:
    header = f"blob {len(payload)}\0".encode("ascii")
    return hashlib.sha1(header + payload).hexdigest()


def download_source() -> dict[str, list[list[object]]]:
    request = urllib.request.Request(
        UPSTREAM_URL,
        headers={"User-Agent": "PickleBook-generator/1.0"},
    )
    with urllib.request.urlopen(request, timeout=60) as response:
        payload = response.read()

    digest = git_blob_sha1(payload)
    if digest != UPSTREAM_BLOB_SHA1:
        raise RuntimeError(
            "Lichess-Elite-derived source blob mismatch: "
            f"expected {UPSTREAM_BLOB_SHA1}, got {digest}"
        )

    source = json.loads(payload)
    start_key = str(pycheckmate_hash(chess.Board()))
    if start_key != "11774829771777683484" or start_key not in source:
        raise RuntimeError("PyCheckmate Zobrist compatibility check failed")
    return source


def home_minor_squares(color: chess.Color) -> tuple[int, ...]:
    return (
        chess.B1, chess.C1, chess.F1, chess.G1
        if color == chess.WHITE
        else chess.B8, chess.C8, chess.F8, chess.G8
    )


def undeveloped_minors(board: chess.Board, color: chess.Color) -> int:
    if color == chess.WHITE:
        homes = ((chess.B1, chess.KNIGHT), (chess.G1, chess.KNIGHT),
                 (chess.C1, chess.BISHOP), (chess.F1, chess.BISHOP))
    else:
        homes = ((chess.B8, chess.KNIGHT), (chess.G8, chess.KNIGHT),
                 (chess.C8, chess.BISHOP), (chess.F8, chess.BISHOP))
    return sum(
        1 for square, piece_type in homes
        if (piece := board.piece_at(square))
        and piece.color == color and piece.piece_type == piece_type
    )


def pawn_defects(board: chess.Board, color: chess.Color) -> int:
    pawns = list(board.pieces(chess.PAWN, color))
    files = [0] * 8
    for square in pawns:
        files[chess.square_file(square)] += 1
    doubled = sum(max(0, count - 1) for count in files)
    isolated = 0
    for square in pawns:
        file = chess.square_file(square)
        left = file > 0 and files[file - 1] > 0
        right = file < 7 and files[file + 1] > 0
        if not left and not right:
            isolated += 1
    return doubled + isolated


def king_is_developed(board: chess.Board, color: chess.Color) -> bool:
    square = board.king(color)
    if color == chess.WHITE:
        return square in (chess.G1, chess.C1)
    return square in (chess.G8, chess.C8)


def horizon_fit(board: chess.Board, move: chess.Move, ply: int) -> float:
    """Return a multiplicative compatibility score for depth-11 Pickle."""
    us = board.turn
    piece = board.piece_at(move.from_square)
    if piece is None:
        return 0.01

    undeveloped_before = undeveloped_minors(board, us)
    defects_before = pawn_defects(board, us)
    factor = 1.0

    # Direct development is valuable because it reduces the number of future
    # moves whose merit depends on a long strategic horizon.
    if piece.piece_type in (chess.KNIGHT, chess.BISHOP):
        if us == chess.WHITE:
            home = move.from_square in (chess.B1, chess.C1, chess.F1, chess.G1)
        else:
            home = move.from_square in (chess.B8, chess.C8, chess.F8, chess.G8)
        if home:
            factor *= 1.22

    if board.is_castling(move):
        factor *= 1.32

    # Repeated early queen moves are exactly the sort of tempo debt that left
    # Pickle calculating an open Scandinavian from behind in development.
    if piece.piece_type == chess.QUEEN and ply < 12:
        factor *= 0.30 if undeveloped_before >= 2 else 0.62
        queen_home = chess.D1 if us == chess.WHITE else chess.D8
        if move.from_square != queen_home:
            factor *= 0.42
        if move.to_square == queen_home:
            factor *= 0.45

    if piece.piece_type == chess.KING and not board.is_castling(move) and ply < 14:
        factor *= 0.58

    # Early flank-pawn commitments tend to create plans whose value is not
    # visible to a short search unless they have an immediate tactical reason.
    if piece.piece_type == chess.PAWN and ply < 10 and undeveloped_before >= 2:
        file = chess.square_file(move.from_square)
        if file in (0, 5, 6, 7) and not board.is_capture(move):
            factor *= 0.80

    # Central pawn moves and simple piece development make the eventual book
    # exit easier for Pickle to understand.
    if piece.piece_type == chess.PAWN and ply < 6:
        file = chess.square_file(move.from_square)
        if file in (2, 3, 4) and not board.is_capture(move):
            factor *= 1.08

    board.push(move)
    try:
        defects_after = pawn_defects(board, us)
        new_defects = max(0, defects_after - defects_before)
        factor *= 0.80 ** new_defects

        pawns_left = len(board.pieces(chess.PAWN, chess.WHITE)) + len(board.pieces(chess.PAWN, chess.BLACK))
        if ply < 12:
            if pawns_left <= 10:
                factor *= 0.74
            elif pawns_left <= 12:
                factor *= 0.87

        replies = list(board.legal_moves)
        reply_count = len(replies)
        forcing = 0
        checks = 0
        for reply in replies:
            is_check = board.gives_check(reply)
            if board.is_capture(reply) or reply.promotion is not None or is_check:
                forcing += 1
            if is_check:
                checks += 1

        # Branching/forcing density is a proxy for how much tactical work D11
        # must do immediately after leaving book.
        if reply_count > 38:
            factor *= 0.78
        elif reply_count > 32:
            factor *= 0.89
        elif reply_count <= 28 and forcing <= 5:
            factor *= 1.07

        if forcing >= 11:
            factor *= 0.72
        elif forcing >= 8:
            factor *= 0.84
        if checks >= 4:
            factor *= 0.82

        if ply >= 8 and not king_is_developed(board, us):
            factor *= 0.90
    finally:
        board.pop()

    return max(0.04, min(2.25, factor))


def scored_choices(source, board: chess.Board, ply: int):
    raw = source.get(str(pycheckmate_hash(board)), [])
    legal = set(board.legal_moves)
    candidates = []
    total = sum(int(count) for _, count in raw) or 1

    for uci, count_obj in raw:
        try:
            move = chess.Move.from_uci(str(uci))
        except ValueError:
            continue
        if move not in legal:
            continue
        count = int(count_obj)
        share = count / total
        # Flatten the human frequency distribution so a safer second-choice
        # line can beat a fashionable but horizon-hostile main line.
        prior = share ** 0.55
        fit = horizon_fit(board, move, ply)
        candidates.append((move, count, prior * fit, fit))

    candidates.sort(key=lambda item: (-item[2], -item[1], item[0].uci()))
    return candidates


def retained_choices(source, board: chess.Board, ply: int):
    scored = scored_choices(source, board, ply)
    if not scored:
        return []
    best = scored[0][2]
    kept = [item for item in scored if item[2] >= best * RELATIVE_CUTOFF]
    kept = kept[:MAX_MOVES_PER_POSITION]
    if not kept:
        kept = [scored[0]]

    max_score = kept[0][2]
    result = []
    for move, count, score, fit in kept:
        weight = max(1, min(65535, round(10000 * score / max_score)))
        result.append((move, weight, count, fit))
    return result


def compile_book(source):
    visited: set[int] = set()
    compiled: dict[int, list[tuple[int, int]]] = {}
    stack: list[tuple[chess.Board, int]] = [(chess.Board(), 0)]

    while stack:
        board, ply = stack.pop()
        if ply >= MAX_PLIES:
            continue
        key = pickle_hash(board)
        if key in visited:
            continue
        visited.add(key)

        choices = retained_choices(source, board, ply)
        if not choices:
            continue
        compiled[key] = [(pack_move(move), weight) for move, weight, _, _ in choices]

        for move, _, _, _ in choices:
            child = board.copy(stack=True)
            child.push(move)
            stack.append((child, ply + 1))

    entries = []
    flat_moves = []
    for key in sorted(compiled):
        offset = len(flat_moves)
        moves = compiled[key]
        flat_moves.extend(moves)
        entries.append((key, offset, len(moves)))
    return entries, flat_moves, compiled


def audit_scandinavian(source) -> None:
    board = chess.Board()
    for uci in ("e2e4", "d7d5", "e4d5"):
        board.push_uci(uci)
    choices = scored_choices(source, board, 3)
    score_map = {move.uci(): (score, fit, count) for move, count, score, fit in choices}
    q = score_map.get("d8d5")
    n = score_map.get("g8f6")
    if q and n:
        print(
            "Scandinavian horizon audit: "
            f"Nf6 score={n[0]:.4f} fit={n[1]:.3f} count={n[2]}; "
            f"Qxd5 score={q[0]:.4f} fit={q[1]:.3f} count={q[2]}"
        )
        if n[0] <= q[0]:
            raise RuntimeError("PickleBook still prefers the early Qxd5 Scandinavian over Nf6")


def write_include(path: Path, entries, moves) -> None:
    lines = [
        "// Generated by scripts/generate_opening_book.py. Do not edit by hand.",
        "// Source repertoire: PyCheckmate Lichess-Elite-derived book.",
        f"// Upstream revision: {UPSTREAM_COMMIT}",
        f"// Upstream builder: {UPSTREAM_BUILDER_URL}",
        f"// Lichess Elite Database: {LICHESS_ELITE_URL}",
        "// Weights are re-filtered for Pickle's shallow-search horizon.",
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
        f"static constexpr std::size_t PICKLE_BOOK_ENTRY_COUNT = {len(entries)};",
        f"static constexpr std::size_t PICKLE_BOOK_MOVE_COUNT = {len(moves)};",
        "",
        "static constexpr GeneratedBookEntry PICKLE_BOOK_ENTRIES[] = {",
    ]
    if entries:
        lines.extend(
            f"    {{0x{key:016x}ULL, {offset}u, {count}u}},"
            for key, offset, count in entries
        )
    else:
        lines.append("    {0ULL, 0u, 0u},")
    lines.extend(["};", "", "static constexpr GeneratedBookMove PICKLE_BOOK_MOVES[] = {"])
    if moves:
        lines.extend(f"    {{{packed}u, {weight}u}}," for packed, weight in moves)
    else:
        lines.append("    {0u, 0u},")
    lines.extend(["};", ""])
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", default="opening_book_data.inc")
    args = parser.parse_args()

    source = download_source()
    audit_scandinavian(source)
    entries, moves, compiled = compile_book(source)
    if len(entries) < 1000:
        raise RuntimeError(f"unexpectedly small PickleBook: only {len(entries)} positions")

    output = Path(args.output)
    write_include(output, entries, moves)

    start = compiled.get(pickle_hash(chess.Board()), [])
    print(
        f"generated {output}: {len(entries)} Pickle-fit positions, "
        f"{len(moves)} weighted moves; start choices={len(start)}"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"PickleBook generation failed: {exc}", file=sys.stderr)
        raise
