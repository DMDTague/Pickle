#include "search.h"
#include "evaluate.h"
#include "attacks.h"
#include "movegen.h"
#include "bit_utils.h"
#include "uci.h"
#include "time_manager.h"
#include "tt.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>

namespace {
constexpr int INF = 50000;
constexpr int MATE_SCORE = 49000;
constexpr int MAX_QS_PLY = 32;
constexpr int MAX_SEARCH_PLY = 128;

bool in_check(const Board& board) {
    Color us = board.get_side_to_move();
    U64 king = board.get_piece_bitboard(KING) & board.get_color_bitboard(us);
    int king_sq = lsb(king);
    return king_sq >= 0 && king_sq < 64 && board.is_square_attacked((Square)king_sq, (Color)(1 - us));
}

bool is_quiet(Move move) {
    return !get_move_capture(move) && !get_move_promoted(move);
}

int history_index(const Board& board, Move move) {
    int piece = get_move_piece(move);
    if (board.get_side_to_move() == BLACK) piece += 6;
    return piece;
}

void add_history(Board& board, Move move, int bonus) {
    int piece = history_index(board, move);
    int target = get_move_target(move);
    int& value = history_moves[piece][target];
    value += bonus - (value * std::abs(bonus)) / 200000;
    value = std::max(-200000, std::min(200000, value));
}

} // namespace

U64 nodes_searched = 0;
Move best_move = 0;
Move previous_best_move = 0;
int last_search_score = 0;
int last_search_depth = 0;

Move killer_moves[2][MAX_PLY];
int history_moves[12][64];
int mvv_lva[6][6];

void init_mvv_lva() {
    for (int victim = 0; victim < 6; ++victim) {
        for (int attacker = 0; attacker < 6; ++attacker) {
            mvv_lva[victim][attacker] =
                1000000 + MATERIAL_VALUES[victim] * 16 - MATERIAL_VALUES[attacker];
        }
    }
}

void clear_heuristics() {
    for (int ply = 0; ply < MAX_PLY; ++ply) {
        killer_moves[0][ply] = 0;
        killer_moves[1][ply] = 0;
    }
    for (auto& row : history_moves) {
        for (int& value : row) value = 0;
    }
}

int score_move(Board& board, Move move, Move tt_move, int search_ply) {
    if (move == tt_move) return 10000000;

    if (get_move_capture(move)) {
        int victim = get_move_captured_piece(move);
        int attacker = get_move_piece(move);
        int score = mvv_lva[victim][attacker];
        if (get_move_promoted(move)) score += 180000;
        if (get_move_enpassant(move)) score += 2000;
        return score;
    }

    if (get_move_promoted(move)) {
        return 950000 + MATERIAL_VALUES[get_move_promoted(move)];
    }

    if (search_ply < MAX_PLY) {
        if (move == killer_moves[0][search_ply]) return 900000;
        if (move == killer_moves[1][search_ply]) return 800000;
    }

    return history_moves[history_index(board, move)][get_move_target(move)];
}

int quiescence(int alpha, int beta, Board& board, int qs_ply) {
    if ((nodes_searched & 511ULL) == 0) check_time();
    if (tm.time_is_up) return 0;
    ++nodes_searched;

    if (qs_ply >= MAX_QS_PLY) return evaluate(board);

    bool checked = in_check(board);
    int stand_pat = evaluate(board);

    if (!checked) {
        if (stand_pat >= beta) return beta;
        if (stand_pat > alpha) alpha = stand_pat;
    }

    MoveList list;
    generate_moves(board, list);

    int scores[256];
    for (int i = 0; i < list.count; ++i) {
        scores[i] = score_move(board, list.moves[i], 0, 0);
    }

    int legal = 0;
    for (int i = 0; i < list.count; ++i) {
        int best_index = i;
        for (int j = i + 1; j < list.count; ++j) {
            if (scores[j] > scores[best_index]) best_index = j;
        }
        std::swap(list.moves[i], list.moves[best_index]);
        std::swap(scores[i], scores[best_index]);

        Move move = list.moves[i];
        bool tactical = get_move_capture(move) || get_move_promoted(move);
        if (!checked && !tactical) continue;

        if (!board.make_move(move)) continue;
        ++legal;
        int score = -quiescence(-beta, -alpha, board, qs_ply + 1);
        board.unmake_move(move);

        if (tm.time_is_up) return 0;
        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }

    if (checked && legal == 0) return -MATE_SCORE + qs_ply;
    return alpha;
}

int negamax(int depth, int alpha, int beta, Board& board, int search_ply, bool can_null_move) {
    if ((nodes_searched & 511ULL) == 0) check_time();
    if (tm.time_is_up) return 0;
    ++nodes_searched;

    if (search_ply >= MAX_SEARCH_PLY) return evaluate(board);
    if (search_ply > 0 && board.is_draw()) return -CONTEMPT_FACTOR;

    bool checked = in_check(board);
    if (depth <= 0) return quiescence(alpha, beta, board, 0);

    bool pv_node = (beta - alpha) > 1;
    int old_alpha = alpha;

    Move tt_move = 0;
    int tt_score = probe_tt(board.get_hash_key(), depth, alpha, beta, tt_move);
    if (tt_score != TT_UNKNOWN) {
        if (search_ply == 0 && tt_move) best_move = tt_move;
        return tt_score;
    }

    int static_eval = evaluate(board);

    // Keep null-move conservative. It is useful, but this engine is still
    // hand-tuned and should prefer tactical correctness over maximum pruning.
    if (!pv_node && can_null_move && !checked && depth >= 4
        && static_eval >= beta && board.has_non_pawn_material(board.get_side_to_move())) {
        int reduction = 2 + depth / 5;
        reduction = std::min(reduction, 3);
        board.make_null_move();
        int score = -negamax(depth - 1 - reduction, -beta, -beta + 1,
                             board, search_ply + 1, false);
        board.unmake_null_move();
        if (tm.time_is_up) return 0;
        if (score >= beta) return beta;
    }

    MoveList list;
    generate_moves(board, list);
    int scores[256];
    for (int i = 0; i < list.count; ++i) {
        scores[i] = score_move(board, list.moves[i], tt_move, search_ply);
    }

    int legal_moves = 0;
    Move node_best = 0;
    int tt_flag = TT_ALPHA;

    for (int i = 0; i < list.count; ++i) {
        int best_index = i;
        for (int j = i + 1; j < list.count; ++j) {
            if (scores[j] > scores[best_index]) best_index = j;
        }
        std::swap(list.moves[i], list.moves[best_index]);
        std::swap(scores[i], scores[best_index]);

        Move move = list.moves[i];
        bool quiet = is_quiet(move);

        if (!board.make_move(move)) continue;
        ++legal_moves;

        bool gives_check = in_check(board);

        // Old Pickle extended every checking move by a full ply. A chain of
        // checks could therefore keep depth from decreasing and explode the
        // browser search tree. Checks are already handled tactically by the
        // check-aware quiescence search, so normal search depth now always
        // decreases by one ply.
        int full_depth = depth - 1;

        int score;
        if (legal_moves == 1) {
            score = -negamax(full_depth, -beta, -alpha, board, search_ply + 1, true);
        } else {
            int reduction = 0;
            if (quiet && !checked && !gives_check && depth >= 4 && legal_moves >= 5) {
                reduction = 1;
                if (depth >= 7 && legal_moves >= 10) ++reduction;
                reduction = std::min(reduction, std::min(2, std::max(0, full_depth - 1)));
            }

            score = -negamax(full_depth - reduction, -alpha - 1, -alpha,
                             board, search_ply + 1, true);

            if (reduction > 0 && score > alpha) {
                score = -negamax(full_depth, -alpha - 1, -alpha,
                                 board, search_ply + 1, true);
            }
            if (score > alpha && score < beta) {
                score = -negamax(full_depth, -beta, -alpha,
                                 board, search_ply + 1, true);
            }
        }

        board.unmake_move(move);
        if (tm.time_is_up) return 0;

        if (score >= beta) {
            if (quiet && search_ply < MAX_PLY) {
                if (killer_moves[0][search_ply] != move) {
                    killer_moves[1][search_ply] = killer_moves[0][search_ply];
                    killer_moves[0][search_ply] = move;
                }
                add_history(board, move, depth * depth * 32);
            }
            record_tt(board.get_hash_key(), depth, TT_BETA, beta, move);
            return beta;
        }

        if (score > alpha) {
            alpha = score;
            node_best = move;
            tt_flag = TT_EXACT;
            if (search_ply == 0) best_move = move;
            if (quiet) add_history(board, move, depth * depth * 4);
        } else if (quiet) {
            add_history(board, move, -(depth * depth));
        }
    }

    if (legal_moves == 0) {
        return checked ? (-MATE_SCORE + search_ply) : -CONTEMPT_FACTOR;
    }

    if (alpha == old_alpha) tt_flag = TT_ALPHA;
    record_tt(board.get_hash_key(), depth, tt_flag, alpha, node_best);
    return alpha;
}

Move search_best_move(Board& board, int depth, bool print_info) {
    nodes_searched = 0;
    best_move = 0;
    previous_best_move = 0;
    last_search_score = 0;
    last_search_depth = 0;
    clear_heuristics();

    int target_depth = tm.depth_limit > 0 ? tm.depth_limit : depth;
    if (target_depth <= 0) target_depth = 64;

    auto wall_start = std::chrono::high_resolution_clock::now();
    int previous_score = 0;
    bool have_previous_score = false;
    Move completed_move = 0;
    int stable_best_count = 0;

    for (int current_depth = 1; current_depth <= target_depth; ++current_depth) {
        int alpha = -INF;
        int beta = INF;
        int window = 32;

        if (have_previous_score && current_depth >= 4) {
            alpha = std::max(-INF, previous_score - window);
            beta = std::min(INF, previous_score + window);
        }

        int score = 0;
        while (true) {
            best_move = completed_move;
            score = negamax(current_depth, alpha, beta, board, 0, true);
            if (tm.time_is_up || tm.stopped) break;

            if (score <= alpha && alpha > -INF) {
                window *= 2;
                alpha = std::max(-INF, score - window);
                beta = std::min(INF, score + window / 2);
                continue;
            }
            if (score >= beta && beta < INF) {
                window *= 2;
                alpha = std::max(-INF, score - window / 2);
                beta = std::min(INF, score + window);
                continue;
            }
            break;
        }

        if (tm.time_is_up || tm.stopped) break;

        Move iteration_move = best_move ? best_move : completed_move;
        if (iteration_move == completed_move && iteration_move != 0) ++stable_best_count;
        else stable_best_count = 0;

        completed_move = iteration_move;
        previous_best_move = completed_move;
        last_search_score = score;
        last_search_depth = current_depth;

        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - wall_start).count();
        long long nps = elapsed > 0 ? (nodes_searched * 1000ULL) / elapsed : 0;

        if (print_info) {
            std::cout << "info depth " << current_depth
                      << " nodes " << nodes_searched
                      << " time " << elapsed
                      << " nps " << nps;
            if (score > 48000) {
                std::cout << " score mate " << (MATE_SCORE - score + 1) / 2;
            } else if (score < -48000) {
                std::cout << " score mate " << -(score + MATE_SCORE + 1) / 2;
            } else {
                std::cout << " score cp " << score;
            }
            if (completed_move) std::cout << " pv " << move_to_string(completed_move);
            std::cout << std::endl;
        }

        if (tm.optimum_time != -1) {
            long long soft_limit = tm.optimum_time;
            if (have_previous_score && std::abs(score - previous_score) >= 65) {
                soft_limit = std::min(tm.max_time, tm.optimum_time + tm.optimum_time / 2);
            } else if (stable_best_count >= 3) {
                soft_limit = std::max(1LL, (tm.optimum_time * 3) / 4);
            }
            if (elapsed >= soft_limit) break;
        }

        previous_score = score;
        have_previous_score = true;
    }

    if (!completed_move) {
        MoveList list;
        generate_moves(board, list);
        for (int i = 0; i < list.count; ++i) {
            Move move = list.moves[i];
            if (board.make_move(move)) {
                board.unmake_move(move);
                completed_move = move;
                break;
            }
        }
    }

    previous_best_move = completed_move;
    return completed_move;
}

void search_position(Board& board, int depth) {
    Move move = search_best_move(board, depth, true);
    std::cout << "bestmove " << move_to_string(move) << std::endl;
}
