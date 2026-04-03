#include "uci.h"
#include "movegen.h"
#include "search.h"
#include "time_manager.h"
#include <iostream>
#include <sstream>
#include <vector>

// Helper to convert square index to algebraic notation
static std::string sq_to_alg(int square) {
    if (square < 0 || square >= 64) return "--";
    std::string s = "";
    s += (char)('a' + (square % 8));
    s += (char)('1' + (square / 8));
    return s;
}

std::string move_to_string(Move move) {
    if (move == 0) return "0000";
    std::string text = sq_to_alg(get_move_source(move)) + sq_to_alg(get_move_target(move));
    int promoted = get_move_promoted(move);
    if (promoted) {
        if (promoted == QUEEN) text += "q";
        else if (promoted == ROOK) text += "r";
        else if (promoted == BISHOP) text += "b";
        else if (promoted == KNIGHT) text += "n";
    }
    return text;
}

Move parse_move(const std::string& move_str, const Board& board) {
    MoveList move_list;
    generate_moves((Board&)board, move_list);

    for (int i = 0; i < move_list.count; ++i) {
        Move m = move_list.moves[i];
        if (move_to_string(m) == move_str) {
            // Found the move in the pseudo-legal generation list
            return m;
        }
    }
    return 0; // Invalid or illegal string
}

void uci_loop(Board& board) {
    std::string line;
    std::cout << "Pickle UCI Engine initialized." << std::endl;

    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string command;
        iss >> command;

        if (command == "uci") {
            std::cout << "id name Pickle\n";
            std::cout << "id author Dylan Tague (AI Assistant)\n";
            std::cout << "uciok\n" << std::flush;
        } 
        else if (command == "isready") {
            std::cout << "readyok\n" << std::flush;
        } 
        else if (command == "ucinewgame") {
            board.init_start_position();
        } 
        else if (command == "position") {
            std::string token;
            iss >> token;

            if (token == "startpos") {
                board.init_start_position();
                iss >> token; // consume 'moves' if present
            } else if (token == "fen") {
                std::string fen = "";
                // FEN has 6 parts, gather them
                for (int i = 0; i < 6; ++i) {
                    iss >> token;
                    fen += token + " ";
                }
                board.parse_fen(fen);
                iss >> token; // consume 'moves' if present
            }

            // Now parse subsequent moves to update board dynamically
            while (iss >> token) {
                Move m = parse_move(token, board);
                if (m) {
                    board.make_move(m);
                }
            }
        } 
        else if (command == "go") {
            int depth = -1;
            long long time_left = -1;
            long long inc = 0;
            long long movetime = -1;
            
            std::string token;
            while (iss >> token) {
                if (token == "depth") iss >> depth;
                else if (token == "wtime" && board.get_side_to_move() == WHITE) iss >> time_left;
                else if (token == "btime" && board.get_side_to_move() == BLACK) iss >> time_left;
                else if (token == "winc" && board.get_side_to_move() == WHITE) iss >> inc;
                else if (token == "binc" && board.get_side_to_move() == BLACK) iss >> inc;
                else if (token == "movetime") iss >> movetime;
            }

            // Route safe baselines
            set_time_limits(time_left, inc, movetime, depth);
            
            search_position(board, depth);
        } 
        else if (command == "quit") {
            break;
        }
    }
}
