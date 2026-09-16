#include "board.h"
#include "attacks.h"
#include "magics.h"
#include "movegen.h"
#include "zobrist.h"
#include <iostream>
#include <string>
#include <vector>

static bool verify(const std::string& fen) {
    Board board;
    board.parse_fen(fen);
    const std::string original_fen = board.get_fen();
    const U64 original_hash = board.get_hash_key();
    MoveList list;
    generate_moves(board, list);
    int legal = 0;
    for (int i = 0; i < list.count; ++i) {
        Move move = list.moves[i];
        bool made = board.make_move(move);
        if (made) { ++legal; board.unmake_move(move); }
        if (board.get_hash_key() != original_hash || board.get_fen() != original_fen) {
            std::cerr << "state/hash corruption after make/unmake: " << fen << "\n";
            return false;
        }
    }
    return legal > 0;
}

int main() {
    init_leapers();
    init_sliders();
    init_zobrist();
    const std::vector<std::string> fens = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/ppp2ppp/2n5/3pp3/3PP3/2N5/PPP2PPP/R3K2R w KQkq - 0 1",
        "8/8/8/6k1/7p/8/3nrp2/7K b - - 0 1",
        "b7/8/8/6k1/8/8/r6p/1n3K2 b - - 0 1"
    };
    for (const auto& fen : fens) if (!verify(fen)) return 1;
    std::cout << "hash round-trip ok\n";
    return 0;
}
