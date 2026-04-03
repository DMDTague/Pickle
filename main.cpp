#include <iostream>
#include "board.h"
#include "attacks.h"
#include "magics.h"
#include "zobrist.h"
#include "tt.h"
#include "search.h"
#include "uci.h"
#include "datagen.h"
#include <string>

int main(int argc, char* argv[]) {
    // Pickle relies on these tables mathematically mapping before booting
    init_leapers();
    init_sliders();
    init_mvv_lva();
    init_zobrist();
    init_tt(32); // 32MB standard TT size

    // Check for datagen CLI command
    if (argc >= 4 && std::string(argv[1]) == "datagen") {
        int games = std::stoi(argv[2]);
        int depth = std::stoi(argv[3]);
        std::string filename = (argc >= 5) ? argv[4] : "dataset.txt";
        run_datagen(games, depth, filename);
        return 0;
    }

    Board board;
    
    // Hand over control to the Universal Chess Interface loop
    uci_loop(board);

    return 0;
}
