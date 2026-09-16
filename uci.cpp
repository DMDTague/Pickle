#include "uci.h"
#include "movegen.h"
#include "opening_book.h"
#include "search.h"
#include "time_manager.h"
#include "tt.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <vector>

static std::string sq_to_alg(int square) {
    if (square < 0 || square >= 64) return "--";
    std::string s;
    s += static_cast<char>('a' + (square % 8));
    s += static_cast<char>('1' + (square / 8));
    return s;
}

std::string move_to_string(Move move) {
    if (move == 0) return "0000";
    std::string text = sq_to_alg(get_move_source(move)) + sq_to_alg(get_move_target(move));
    int promoted = get_move_promoted(move);
    if (promoted == QUEEN) text += "q";
    else if (promoted == ROOK) text += "r";
    else if (promoted == BISHOP) text += "b";
    else if (promoted == KNIGHT) text += "n";
    return text;
}

Move parse_move(const std::string& move_str, const Board& board) {
    MoveList move_list;
    generate_moves((Board&)board, move_list);

    for (int i = 0; i < move_list.count; ++i) {
        Move m = move_list.moves[i];
        if (move_to_string(m) != move_str) continue;

        Board& mutable_board = (Board&)board;
        if (mutable_board.make_move(m)) {
            mutable_board.unmake_move(m);
            return m;
        }
    }
    return 0;
}

void uci_loop(Board& board) {
    std::string line;

    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string command;
        iss >> command;

        if (command == "uci") {
            std::cout << "id name Pickle\n";
            std::cout << "id author Dylan Tague\n";
            std::cout << "option name Hash type spin default 32 min 1 max 512\n";
            std::cout << "option name Clear Hash type button\n";
            std::cout << "option name OwnBook type check default true\n";
            std::cout << "uciok\n" << std::flush;
        }
        else if (command == "isready") {
            std::cout << "readyok\n" << std::flush;
        }
        else if (command == "setoption") {
            std::string token;
            iss >> token; // name
            if (token != "name") continue;

            std::string name;
            iss >> name;
            if (name == "Clear") {
                iss >> token; // Hash
                clear_tt();
            } else if (name == "Hash") {
                int mb = 32;
                iss >> token; // value
                if (token == "value") iss >> mb;
                init_tt(std::clamp(mb, 1, 512));
            } else if (name == "OwnBook") {
                std::string value = "true";
                iss >> token; // value
                if (token == "value") iss >> value;
                std::transform(value.begin(), value.end(), value.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                set_opening_book_enabled(value != "false" && value != "0" && value != "off");
            }
        }
        else if (command == "ucinewgame") {
            board.init_start_position();
            clear_tt();
            clear_heuristics();
        }
        else if (command == "position") {
            std::string token;
            iss >> token;

            if (token == "startpos") {
                board.init_start_position();
                if (iss >> token) {
                    if (token != "moves") continue;
                } else {
                    continue;
                }
            } else if (token == "fen") {
                std::string fen;
                for (int i = 0; i < 6; ++i) {
                    if (!(iss >> token)) break;
                    if (!fen.empty()) fen += ' ';
                    fen += token;
                }
                board.parse_fen(fen);
                if (iss >> token) {
                    if (token != "moves") continue;
                } else {
                    continue;
                }
            } else {
                continue;
            }

            while (iss >> token) {
                Move m = parse_move(token, board);
                if (!m || !board.make_move(m)) break;
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

            set_time_limits(time_left, inc, movetime, depth);
            search_position(board, depth);
        }
        else if (command == "stop") {
            tm.stopped = true;
        }
        else if (command == "quit") {
            break;
        }
    }
}
