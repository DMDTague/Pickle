#include "perft.h"
#include "movegen.h"
#include <iostream>
#include <string>

// Helper directly duplicated for quick formatting here (or include it from main/utils if desired)
static std::string sq_to_algebraic(int square) {
    if (square < 0 || square >= 64) return "--";
    std::string s = "";
    s += (char)('a' + (square % 8));
    s += (char)('1' + (square / 8));
    return s;
}

static std::string format_move(Move move) {
    std::string text = sq_to_algebraic(get_move_source(move)) + sq_to_algebraic(get_move_target(move));
    int promoted = get_move_promoted(move);
    if (promoted) {
        if (promoted == QUEEN) text += "q";
        else if (promoted == ROOK) text += "r";
        else if (promoted == BISHOP) text += "b";
        else if (promoted == KNIGHT) text += "n";
    }
    return text;
}

U64 perft(Board& board, int depth) {
    if (depth == 0) return 1ULL;

    MoveList move_list;
    generate_moves(board, move_list);

    U64 nodes = 0;
    for (int i = 0; i < move_list.count; i++) {
        Move m = move_list.moves[i];
        if (board.make_move(m)) {
            nodes += perft(board, depth - 1);
            board.unmake_move(m);
        }
    }
    return nodes;
}

void perft_divide(Board& board, int depth) {
    if (depth == 0) {
        std::cout << "Depth 0, Nodes = 1" << std::endl;
        return;
    }

    std::cout << "Perft Divide (Depth " << depth << "):\n";
    MoveList move_list;
    generate_moves(board, move_list);

    U64 total_nodes = 0;
    for (int i = 0; i < move_list.count; i++) {
        Move m = move_list.moves[i];
        if (board.make_move(m)) {
            U64 nodes = perft(board, depth - 1);
            std::cout << format_move(m) << ": " << nodes << "\n";
            total_nodes += nodes;
            board.unmake_move(m);
        }
    }
    
    std::cout << "Total Nodes: " << total_nodes << std::endl;
}
