#include "datagen.h"
#include "board.h"
#include "search.h"
#include "movegen.h"
#include "time_manager.h"
#include "bit_utils.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <random>

struct DataPoint {
    std::string fen;
    int score;
};

void run_datagen(int games, int target_depth, const std::string& filename) {
    std::ofstream file(filename, std::ios::app);
    if (!file.is_open()) {
        std::cout << "Error: Could not open " << filename << std::endl;
        return;
    }

    std::random_device rd;
    std::mt19937 gen(rd());

    std::cout << "Starting Data Generation... (" << games << " games at depth " << target_depth << ")\n";

    // Disable time management for raw algorithmic depth mapping
    tm.depth_limit = target_depth;
    tm.time_is_up = false;

    for (int g = 0; g < games; g++) {
        Board board;
        board.init_start_position();

        std::vector<DataPoint> current_game_data;
        bool valid_opening = true;

        // 1. Play 8 completely random legal moves
        for (int i = 0; i < 8; i++) {
            MoveList ml;
            generate_moves(board, ml);

            MoveList legal;
            for(int j = 0; j < ml.count; j++) {
                if(board.make_move(ml.moves[j])) {
                    board.unmake_move(ml.moves[j]);
                    legal.add_move(ml.moves[j]);
                }
            }

            if (legal.count == 0) {
                valid_opening = false;
                break;
            }

            std::uniform_int_distribution<> dist(0, legal.count - 1);
            board.make_move(legal.moves[dist(gen)]);
        }

        if (!valid_opening) {
            g--; // Retry
            continue;
        }

        // 2. Play out the rest of the game
        int result = -1; // 0=Black win, 1=White win, 2=Draw
        
        while (true) {
            if (board.is_draw()) {
                result = 2;
                break;
            }

            MoveList ml;
            generate_moves(board, ml);
            int legal_count = 0;
            for(int j = 0; j < ml.count; j++) {
                if(board.make_move(ml.moves[j])) {
                    legal_count++;
                    board.unmake_move(ml.moves[j]);
                }
            }

            if (legal_count == 0) {
                Color us = board.get_side_to_move();
                Color them = (Color)(1 - us);
                Bitboard our_king = board.get_piece_bitboard(KING) & board.get_color_bitboard(us);
                Square k_sq = (Square)lsb(our_king);
                if (board.is_square_attacked(k_sq, them)) {
                    result = (us == WHITE) ? 0 : 1; 
                } else {
                    result = 2; // Stalemate
                }
                break;
            }

            // Engine calculation
            int final_score = 0;
            Move best = 0;
            clear_heuristics();
            nodes_searched = 0;
            tm.time_is_up = false;

            for (int d = 1; d <= target_depth; d++) {
                final_score = negamax(d, -50000, 50000, board, 0, true);
                if (!tm.time_is_up) best = best_move;
            }
            
            // To prevent mating scores distorting NN evaluations heavily, cap mate scores
            if (final_score > 48000) final_score = 10000;
            if (final_score < -48000) final_score = -10000;

            DataPoint dp;
            dp.fen = board.get_fen();
            // Score must be explicitly mapped to White's perspective for standard NNUE sets
            dp.score = (board.get_side_to_move() == WHITE) ? final_score : -final_score;
            current_game_data.push_back(dp);

            board.make_move(best);
        }

        // 3. Dump labeled data
        std::string res_str = "0.5";
        if (result == 1) res_str = "1.0";
        if (result == 0) res_str = "0.0";

        for (const auto& dp : current_game_data) {
            file << dp.fen << " | " << dp.score << " | " << res_str << "\n";
        }

        if ((g + 1) % 10 == 0) {
            std::cout << "Generated " << (g + 1) << " / " << games << " games...\n";
            file.flush();
        }
    }

    std::cout << "Data generation complete.\n";
    file.close();
}
