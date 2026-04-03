#include "search.h"
#include "evaluate.h"
#include "movegen.h"
#include "bit_utils.h"
#include "uci.h"
#include "time_manager.h"
#include "tt.h"
#include <iostream>
#include <string>
#include <chrono>

// Global Search Variables
U64 nodes_searched = 0;
Move best_move = 0;
Move previous_best_move = 0;

Move killer_moves[2][MAX_PLY];
int history_moves[12][64];

// MVV-LVA [Victim][Attacker]
int mvv_lva[6][6];

void init_mvv_lva() {
    int piece_values[6] = { 100, 300, 300, 500, 900, 10000 };
    for (int victim = 0; victim < 6; victim++) {
        for (int attacker = 0; attacker < 6; attacker++) {
            mvv_lva[victim][attacker] = 1000000 + piece_values[victim] - piece_values[attacker];
        }
    }
}

void clear_heuristics() {
    for (int i = 0; i < MAX_PLY; i++) {
        killer_moves[0][i] = 0;
        killer_moves[1][i] = 0;
    }
    for (int i = 0; i < 12; i++) {
        for (int j = 0; j < 64; j++) {
            history_moves[i][j] = 0;
        }
    }
}

int score_move(Board& board, Move move, Move tt_move, int search_ply) {
    if (move == tt_move) {
        return 10000000;
    }
    
    if (get_move_capture(move)) {
        int attacker = get_move_piece(move);
        int victim = get_move_captured_piece(move);
        return mvv_lva[victim][attacker];
    }
    
    // Safety boundary for ply to prevent out-of-bounds on super deep lines
    if (search_ply < MAX_PLY) {
        if (move == killer_moves[0][search_ply]) {
            return 900000;
        }
        if (move == killer_moves[1][search_ply]) {
            return 800000;
        }
    }
    
    int piece = get_move_piece(move);
    if (board.get_side_to_move() == BLACK) piece += 6; // Differentiate black pieces in history if needed, or simply map it 0-11
    
    return history_moves[piece][get_move_target(move)];
}

// -----------------------------------------------------------------------------
// Quiescence Search
// -----------------------------------------------------------------------------
int quiescence(int alpha, int beta, Board& board) {
    if ((nodes_searched & 2047) == 0) {
        check_time();
    }
    if (tm.time_is_up) return 0;
    nodes_searched++;

    // Evaluate static position
    int stand_pat = evaluate(board);

    // Fail-hard beta cutoff
    if (stand_pat >= beta) {
        return beta;
    }

    // Alpha update
    if (stand_pat > alpha) {
        alpha = stand_pat;
    }

    MoveList move_list;
    generate_moves(board, move_list);

    int scores[256];
    for (int i = 0; i < move_list.count; i++) {
        scores[i] = score_move(board, move_list.moves[i], 0, 0);
    }

    for (int i = 0; i < move_list.count; i++) {
        // Selection Sort
        int best_score = -1;
        int best_index = i;
        for (int j = i; j < move_list.count; j++) {
            if (scores[j] > best_score) {
                best_score = scores[j];
                best_index = j;
            }
        }
        std::swap(move_list.moves[i], move_list.moves[best_index]);
        std::swap(scores[i], scores[best_index]);

        Move move = move_list.moves[i];

        // Process only tactical moves (captures or promotions)
        if (!get_move_capture(move) && !get_move_promoted(move)) {
            continue;
        }

        if (board.make_move(move)) {
            int score = -quiescence(-beta, -alpha, board);
            board.unmake_move(move);

            if (score >= beta) {
                return beta;
            }
            if (score > alpha) {
                alpha = score;
            }
        }
    }

    return alpha;
}

// -----------------------------------------------------------------------------
// Negamax (Alpha-Beta)
// -----------------------------------------------------------------------------
int negamax(int depth, int alpha, int beta, Board& board, int search_ply, bool can_null_move) {
    if ((nodes_searched & 2047) == 0) {
        check_time();
    }
    if (tm.time_is_up) return 0;
    nodes_searched++;

    if (depth == 0) {
        return quiescence(alpha, beta, board);
    }

    // Check status
    Color us = board.get_side_to_move();
    Color them = (Color)(1 - us);
    Bitboard our_king_bb = board.get_piece_bitboard(KING) & board.get_color_bitboard(us);
    Square our_king_sq = (Square)lsb(our_king_bb);
    bool in_check = board.is_square_attacked(our_king_sq, them);

    if (in_check) depth++; // Check Extension

    // Null Move Pruning
    if (depth >= 3 && can_null_move && !in_check && board.has_non_pawn_material(us)) {
        board.make_null_move();
        int null_score = -negamax(depth - 3, -beta, -beta + 1, board, search_ply + 1, false);
        board.unmake_null_move();
        
        if (null_score >= beta) {
            return beta;
        }
    }

    // Transposition Table Probe
    Move tt_move = 0;
    int tt_score = probe_tt(board.get_hash_key(), depth, alpha, beta, tt_move);
    if (tt_score != TT_UNKNOWN) {
        if (search_ply == 0) best_move = tt_move;
        return tt_score;
    }

    MoveList move_list;
    generate_moves(board, move_list);

    int scores[256];
    for (int i = 0; i < move_list.count; i++) {
        scores[i] = score_move(board, move_list.moves[i], tt_move, search_ply);
    }

    int legal_moves_played = 0;
    int old_alpha = alpha;
    Move current_best_move = 0;
    int tt_flag = TT_ALPHA;

    for (int i = 0; i < move_list.count; i++) {
        // Selection Sort
        int best_score = -1;
        int best_index = i;
        for (int j = i; j < move_list.count; j++) {
            if (scores[j] > best_score) {
                best_score = scores[j];
                best_index = j;
            }
        }
        std::swap(move_list.moves[i], move_list.moves[best_index]);
        std::swap(scores[i], scores[best_index]);

        Move move = move_list.moves[i];

        if (board.make_move(move)) {
            legal_moves_played++;
            
            int score;
            
            // Late Move Reductions (LMR)
            if (legal_moves_played >= 4 && depth >= 3 && !in_check && 
                !get_move_capture(move) && !get_move_promoted(move)) {
                
                score = -negamax(depth - 2, -beta, -alpha, board, search_ply + 1, true);
                
                // Re-Search if the move was surprisingly good
                if (score > alpha) {
                    score = -negamax(depth - 1, -beta, -alpha, board, search_ply + 1, true);
                }
            } else {
                // Full Depth Search
                score = -negamax(depth - 1, -beta, -alpha, board, search_ply + 1, true);
            }
            
            board.unmake_move(move);

            if (score >= beta) {
                if (!get_move_capture(move) && search_ply < MAX_PLY) {
                    killer_moves[1][search_ply] = killer_moves[0][search_ply];
                    killer_moves[0][search_ply] = move;
                    
                    int piece = get_move_piece(move);
                    if (board.get_side_to_move() == BLACK) piece += 6;
                    history_moves[piece][get_move_target(move)] += depth * depth;
                }
                record_tt(board.get_hash_key(), depth, TT_BETA, beta, move);
                return beta; // Fail-hard beta cutoff
            }
            if (score > alpha) {
                alpha = score;
                tt_flag = TT_EXACT;
                current_best_move = move;
                if (search_ply == 0) {
                    best_move = move;
                }
            }
        }
    }

    // Checkmate and Stalemate detection
    if (legal_moves_played == 0) {
        if (in_check) {
            return -49000 + search_ply;
        }
        return 0;
    }

    record_tt(board.get_hash_key(), depth, tt_flag, alpha, current_best_move);
    return alpha;
}


// -----------------------------------------------------------------------------
// Search Wrapper
// -----------------------------------------------------------------------------
void search_position(Board& board, int depth) {
    nodes_searched = 0;
    best_move = 0;
    previous_best_move = 0;
    clear_heuristics();
    
    // Default search limits if un-initialized (safe fallback)
    int target_depth = (tm.depth_limit > 0) ? tm.depth_limit : depth;
    if (target_depth <= 0) target_depth = 64; // MAX depth loop

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int current_depth = 1; current_depth <= target_depth; current_depth++) {
        
        int score = negamax(current_depth, -50000, 50000, board, 0, true);

        // If time was up during the calculation of this depth, 
        // the results are severely tainted. Discard them.
        if (tm.time_is_up) {
            break; 
        }

        // Successfully completed depth calculation!
        previous_best_move = best_move;

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        long long nps = 0;
        if (duration.count() > 0) {
            nps = (nodes_searched * 1000) / duration.count();
        }
        
        // Print standard UCI info line for the completely searched depth
        std::cout << "info depth " << current_depth 
                  << " nodes " << nodes_searched 
                  << " time " << duration.count() 
                  << " nps " << nps;

        if (score > 48000) {
            std::cout << " score mate " << (49000 - score + 1) / 2 << std::endl;
        } else if (score < -48000) {
            std::cout << " score mate " << -(score + 49000 + 1) / 2 << std::endl;
        } else {
            std::cout << " score cp " << score << std::endl;
        }
    }

    // Safety fallback if time triggers before depth 1 even completes:
    // Just grab the first pseudo-legal move generated
    if (previous_best_move == 0) {
        MoveList list;
        generate_moves(board, list);
        if (list.count > 0) previous_best_move = list.moves[0];
    }
    
    // CRITICAL: UCI GUI strictly waits for this exact string
    std::cout << "bestmove " << move_to_string(previous_best_move) << std::endl;
}
