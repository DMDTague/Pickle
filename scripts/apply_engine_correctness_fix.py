from pathlib import Path


def replace_required(path, old, new):
    p = Path(path)
    text = p.read_text()
    if new in text:
        return
    if old not in text:
        raise SystemExit(f"expected block not found in {path}")
    p.write_text(text.replace(old, new, 1))


# Critical invariant: after unmaking a move, the board hash must be exactly the
# pre-move hash. Reconstructing pieces mutates the incremental key, so restore
# the authoritative historical key only after reconstruction is complete.
replace_required(
    "board.cpp",
    "    PieceType cap_piece = history[ply].captured_piece;\n    hash_key = history[ply].hash_key;\n\n    // Handle Promotion Reverse\n",
    "    PieceType cap_piece = history[ply].captured_piece;\n    U64 restored_hash = history[ply].hash_key;\n\n    // Handle Promotion Reverse\n",
)
replace_required(
    "board.cpp",
    "    if (castling) {\n        if (to == G1) move_piece(F1, H1, us, ROOK);\n        else if (to == C1) move_piece(D1, A1, us, ROOK);\n        else if (to == G8) move_piece(F8, H8, us, ROOK);\n        else if (to == C8) move_piece(D8, A8, us, ROOK);\n    }\n}\n",
    "    if (castling) {\n        if (to == G1) move_piece(F1, H1, us, ROOK);\n        else if (to == C1) move_piece(D1, A1, us, ROOK);\n        else if (to == G8) move_piece(F8, H8, us, ROOK);\n        else if (to == C8) move_piece(D8, A8, us, ROOK);\n    }\n\n    hash_key = restored_hash;\n}\n",
)

# Search correctness: immediate checkmate is lexicographically dominant, mate
# distance survives quiescence/TT use, and root fail-highs retain their move.
replace_required(
    "search.cpp",
    "void add_history(Board& board, Move move, int bonus) {\n    int piece = history_index(board, move);\n    int target = get_move_target(move);\n    int& value = history_moves[piece][target];\n    value += bonus - (value * std::abs(bonus)) / 200000;\n    value = std::max(-200000, std::min(200000, value));\n}\n\n} // namespace\n",
    "void add_history(Board& board, Move move, int bonus) {\n    int piece = history_index(board, move);\n    int target = get_move_target(move);\n    int& value = history_moves[piece][target];\n    value += bonus - (value * std::abs(bonus)) / 200000;\n    value = std::max(-200000, std::min(200000, value));\n}\n\nbool has_legal_move(Board& board) {\n    MoveList replies;\n    generate_moves(board, replies);\n    for (int i = 0; i < replies.count; ++i) {\n        Move reply = replies.moves[i];\n        if (board.make_move(reply)) {\n            board.unmake_move(reply);\n            return true;\n        }\n    }\n    return false;\n}\n\nMove find_mate_in_one(Board& board) {\n    MoveList list;\n    generate_moves(board, list);\n    Move best = 0;\n    int best_tiebreak = -1;\n\n    for (int i = 0; i < list.count; ++i) {\n        Move move = list.moves[i];\n        if (!board.make_move(move)) continue;\n        bool mate = in_check(board) && !has_legal_move(board);\n        board.unmake_move(move);\n        if (!mate) continue;\n\n        int promoted = get_move_promoted(move);\n        int tiebreak = promoted ? MATERIAL_VALUES[promoted] : 0;\n        if (!best || tiebreak > best_tiebreak) {\n            best = move;\n            best_tiebreak = tiebreak;\n        }\n    }\n    return best;\n}\n\n} // namespace\n",
)
replace_required(
    "search.cpp",
    "int quiescence(int alpha, int beta, Board& board, int qs_ply) {\n",
    "int quiescence(int alpha, int beta, Board& board, int search_ply, int qs_ply) {\n",
)
replace_required(
    "search.cpp",
    "        int score = -quiescence(-beta, -alpha, board, qs_ply + 1);\n",
    "        int score = -quiescence(-beta, -alpha, board, search_ply + 1, qs_ply + 1);\n",
)
replace_required(
    "search.cpp",
    "    if (checked && legal == 0) return -MATE_SCORE + qs_ply;\n",
    "    if (checked && legal == 0) return -MATE_SCORE + search_ply;\n",
)
replace_required(
    "search.cpp",
    "    if (depth <= 0) return quiescence(alpha, beta, board, 0);\n",
    "    if (depth <= 0) return quiescence(alpha, beta, board, search_ply, 0);\n",
)
replace_required(
    "search.cpp",
    "    int tt_score = probe_tt(board.get_hash_key(), depth, alpha, beta, tt_move);\n",
    "    int tt_score = probe_tt(board.get_hash_key(), depth, alpha, beta, tt_move, search_ply);\n",
)
replace_required(
    "search.cpp",
    "        if (score >= beta) {\n            if (quiet && search_ply < MAX_PLY) {\n",
    "        if (score >= beta) {\n            if (search_ply == 0) best_move = move;\n            if (quiet && search_ply < MAX_PLY) {\n",
)
replace_required(
    "search.cpp",
    "            record_tt(board.get_hash_key(), depth, TT_BETA, beta, move);\n",
    "            record_tt(board.get_hash_key(), depth, TT_BETA, beta, move, search_ply);\n",
)
replace_required(
    "search.cpp",
    "    record_tt(board.get_hash_key(), depth, tt_flag, alpha, node_best);\n",
    "    record_tt(board.get_hash_key(), depth, tt_flag, alpha, node_best, search_ply);\n",
)
replace_required(
    "search.cpp",
    "    clear_heuristics();\n\n    int target_depth = tm.depth_limit > 0 ? tm.depth_limit : depth;\n",
    "    clear_heuristics();\n\n    Move mate_in_one = find_mate_in_one(board);\n    if (mate_in_one) {\n        best_move = mate_in_one;\n        previous_best_move = mate_in_one;\n        last_search_score = MATE_SCORE - 1;\n        last_search_depth = 1;\n        if (print_info) {\n            std::cout << \"info depth 1 nodes \" << nodes_searched\n                      << \" time 0 nps 0 score mate 1 pv \"\n                      << move_to_string(mate_in_one) << std::endl;\n        }\n        return mate_in_one;\n    }\n\n    int target_depth = tm.depth_limit > 0 ? tm.depth_limit : depth;\n",
)

# Native invariant regression target.
test = Path("tests/hash_roundtrip.cpp")
test.parent.mkdir(exist_ok=True)
test.write_text('''#include "board.h"\n#include "attacks.h"\n#include "magics.h"\n#include "movegen.h"\n#include "zobrist.h"\n#include <iostream>\n#include <string>\n#include <vector>\n\nstatic bool verify(const std::string& fen) {\n    Board board;\n    board.parse_fen(fen);\n    const std::string original_fen = board.get_fen();\n    const U64 original_hash = board.get_hash_key();\n    MoveList list;\n    generate_moves(board, list);\n    int legal = 0;\n    for (int i = 0; i < list.count; ++i) {\n        Move move = list.moves[i];\n        bool made = board.make_move(move);\n        if (made) { ++legal; board.unmake_move(move); }\n        if (board.get_hash_key() != original_hash || board.get_fen() != original_fen) {\n            std::cerr << "state/hash corruption after make/unmake: " << fen << "\\n";\n            return false;\n        }\n    }\n    return legal > 0;\n}\n\nint main() {\n    init_leapers();\n    init_sliders();\n    init_zobrist();\n    const std::vector<std::string> fens = {\n        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",\n        "r3k2r/ppp2ppp/2n5/3pp3/3PP3/2N5/PPP2PPP/R3K2R w KQkq - 0 1",\n        "8/8/8/6k1/7p/8/3nrp2/7K b - - 0 1",\n        "b7/8/8/6k1/8/8/r6p/1n3K2 b - - 0 1"\n    };\n    for (const auto& fen : fens) if (!verify(fen)) return 1;\n    std::cout << "hash round-trip ok\\n";\n    return 0;\n}\n''')

replace_required(
    "CMakeLists.txt",
    "else()\n    add_executable(pickle ${PICKLE_CORE} datagen.cpp main.cpp)\n    target_compile_options(pickle PRIVATE -O3)\nendif()\n",
    "else()\n    add_executable(pickle ${PICKLE_CORE} datagen.cpp main.cpp)\n    target_compile_options(pickle PRIVATE -O3)\n\n    add_executable(pickle_regression attacks.cpp board.cpp magics.cpp movegen.cpp zobrist.cpp tests/hash_roundtrip.cpp)\n    target_include_directories(pickle_regression PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})\n    target_compile_options(pickle_regression PRIVATE -O3)\nendif()\n",
)
