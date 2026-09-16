#include "opening_book.h"
#include "uci.h"
#include <algorithm>
#include <cstdint>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

struct BookChoice {
    Move move = 0;
    int weight = 0;
};

std::unordered_map<U64, std::vector<BookChoice>> book;
bool book_initialized = false;
bool book_enabled = true;
U64 probe_counter = 0;

U64 mix64(U64 x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

void add_choice(U64 key, Move move, int weight) {
    auto& choices = book[key];
    for (auto& choice : choices) {
        if (choice.move == move) {
            choice.weight += weight;
            return;
        }
    }
    choices.push_back({move, weight});
}

void add_line(const char* line, int weight) {
    Board board;
    board.init_start_position();

    std::istringstream moves(line);
    std::string token;
    while (moves >> token) {
        Move move = parse_move(token, board);
        if (!move) return;
        add_choice(board.get_hash_key(), move, weight);
        if (!board.make_move(move)) return;
    }
}

} // namespace

void init_opening_book() {
    if (book_initialized) return;
    book_initialized = true;
    book.clear();

    // Open games / 1.e4 e5
    add_line("e2e4 e7e5 g1f3 b8c6 f1b5 a7a6 b5a4 g8f6 e1g1 f8e7", 10); // Ruy Lopez
    add_line("e2e4 e7e5 g1f3 b8c6 f1b5 g8f6 e1g1 f6e4 d2d4 e4d6", 7);    // Berlin
    add_line("e2e4 e7e5 g1f3 b8c6 f1c4 f8c5 c2c3 g8f6 d2d4 e5d4 c3d4 c5b4", 9); // Italian
    add_line("e2e4 e7e5 g1f3 b8c6 d2d4 e5d4 f3d4 g8f6 b1c3 f8b4", 8);    // Scotch
    add_line("e2e4 e7e5 f2f4 e5f4 g1f3 g7g5 h2h4 g5g4 f3e5", 2);          // King's Gambit

    // Sicilian family
    add_line("e2e4 c7c5 g1f3 d7d6 d2d4 c5d4 f3d4 g8f6 b1c3 a7a6", 10); // Najdorf
    add_line("e2e4 c7c5 g1f3 d7d6 d2d4 c5d4 f3d4 g8f6 b1c3 g7g6", 7);  // Dragon
    add_line("e2e4 c7c5 g1f3 b8c6 d2d4 c5d4 f3d4 g8f6 b1c3 d7d6", 8);  // Classical
    add_line("e2e4 c7c5 g1f3 e7e6 d2d4 c5d4 f3d4 b8c6 b1c3 d8c7", 7);  // Taimanov

    // French / Caro-Kann / other e4 defenses
    add_line("e2e4 e7e6 d2d4 d7d5 b1c3 f8b4 e4e5 c7c5 a2a3 b4c3 b2c3", 7);
    add_line("e2e4 e7e6 d2d4 d7d5 e4e5 c7c5 c2c3 b8c6 g1f3 d8b6", 6);
    add_line("e2e4 c7c6 d2d4 d7d5 b1c3 d5e4 c3e4 c8f5 e4g3 f5g6", 8);
    add_line("e2e4 c7c6 d2d4 d7d5 e4e5 c8f5 g1f3 e7e6 f1e2", 5);
    add_line("e2e4 d7d6 d2d4 g8f6 b1c3 g7g6 f2f4 f8g7 g1f3", 5);       // Pirc
    add_line("e2e4 d7d5 e4d5 d8d5 b1c3 d5d8 d2d4 g8f6", 4);           // Scandinavian
    add_line("e2e4 g8f6 e4e5 f6d5 d2d4 d7d6 g1f3", 3);                // Alekhine

    // Queen's pawn openings
    add_line("d2d4 d7d5 c2c4 e7e6 b1c3 g8f6 c1g5 f8e7 e2e3 e8g8", 10); // QGD
    add_line("d2d4 d7d5 c2c4 c7c6 g1f3 g8f6 b1c3 d5c4 a2a4", 8);        // Slav
    add_line("d2d4 d7d5 c2c4 e7e6 b1c3 g8f6 g1f3 c7c6", 7);              // Semi-Slav
    add_line("d2d4 g8f6 c2c4 g7g6 b1c3 f8g7 e2e4 d7d6 g1f3 e8g8", 10);  // King's Indian
    add_line("d2d4 g8f6 c2c4 e7e6 b1c3 f8b4 e2e3 e8g8 f1d3 d7d5", 8);    // Nimzo-Indian
    add_line("d2d4 g8f6 c2c4 g7g6 b1c3 d7d5 c4d5 f6d5 e2e4 d5c3", 7);    // Grunfeld
    add_line("d2d4 g8f6 c2c4 e7e6 g1f3 b7b6 g2g3 c8b7 f1g2", 6);         // Queen's Indian
    add_line("d2d4 g8f6 c2c4 c7c5 d4d5 e7e6 b1c3 e6d5 c4d5 d7d6", 5);    // Benoni
    add_line("d2d4 g8f6 c2c4 c7c5 d4d5 b7b5", 3);                         // Benko
    add_line("d2d4 f7f5 g2g3 g8f6 f1g2 g7g6", 4);                         // Dutch
    add_line("d2d4 d7d5 g1f3 g8f6 c1f4 e7e6 e2e3 f8d6", 6);              // London

    // English / Reti
    add_line("c2c4 e7e5 b1c3 g8f6 g2g3 d7d5 c4d5 f6d5 f1g2", 7);
    add_line("c2c4 c7c5 b1c3 b8c6 g2g3 g7g6 f1g2 f8g7", 6);
    add_line("g1f3 d7d5 g2g3 g8f6 f1g2 g7g6 e1g1 f8g7", 6);
}

Move probe_opening_book(Board& board) {
    if (!book_enabled) return 0;
    if (!book_initialized) init_opening_book();

    auto it = book.find(board.get_hash_key());
    if (it == book.end()) return 0;

    std::vector<BookChoice> legal;
    int total_weight = 0;
    legal.reserve(it->second.size());

    for (const auto& choice : it->second) {
        if (!choice.move || choice.weight <= 0) continue;
        if (board.make_move(choice.move)) {
            board.unmake_move(choice.move);
            legal.push_back(choice);
            total_weight += choice.weight;
        }
    }

    if (legal.empty() || total_weight <= 0) return 0;

    U64 pick = mix64(board.get_hash_key() ^ (++probe_counter * 0x9e3779b97f4a7c15ULL))
             % static_cast<U64>(total_weight);
    for (const auto& choice : legal) {
        if (pick < static_cast<U64>(choice.weight)) return choice.move;
        pick -= static_cast<U64>(choice.weight);
    }
    return legal.front().move;
}

void set_opening_book_enabled(bool enabled) {
    book_enabled = enabled;
}

bool opening_book_enabled() {
    return book_enabled;
}

int opening_book_position_count() {
    if (!book_initialized) init_opening_book();
    return static_cast<int>(book.size());
}
