#include "search.h"
#include "bit_utils.h"
#include "evaluate.h"
#include "movegen.h"

namespace {

bool probe_in_check(const Board& board) {
    Color us = board.get_side_to_move();
    U64 king = board.get_piece_bitboard(KING) & board.get_color_bitboard(us);
    int king_sq = lsb(king);
    return king_sq >= 0 && king_sq < 64
        && board.is_square_attacked((Square)king_sq, (Color)(1 - us));
}

bool probe_has_legal_move(Board& board) {
    MoveList replies;
    generate_moves(board, replies);
    for (int i = 0; i < replies.count; ++i) {
        Move reply = replies.moves[i];
        if (board.make_move(reply)) {
            board.unmake_move(reply);
            return true;
        }
    }
    return false;
}

} // namespace

Move find_immediate_mate(Board& board) {
    MoveList list;
    generate_moves(board, list);
    Move best = 0;
    int best_tiebreak = -1;

    for (int i = 0; i < list.count; ++i) {
        Move move = list.moves[i];
        if (!board.make_move(move)) continue;
        const bool mate = probe_in_check(board) && !probe_has_legal_move(board);
        board.unmake_move(move);
        if (!mate) continue;

        const int promoted = get_move_promoted(move);
        const int tiebreak = promoted ? MATERIAL_VALUES[promoted] : 0;
        if (!best || tiebreak > best_tiebreak) {
            best = move;
            best_tiebreak = tiebreak;
        }
    }

    return best;
}
