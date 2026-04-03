#ifndef UCI_H
#define UCI_H

#include "board.h"
#include "move.h"
#include <string>

// Converts an internal Move integer to an algebraic string (e.g., e2e4, e7e8q)
std::string move_to_string(Move move);

// Parses an algebraic string (e.g., e2e4) and returns the corresponding pseudo-legal Move
// Returns 0 if the move is invalid or illegal.
Move parse_move(const std::string& move_str, const Board& board);

// Starts the infinite Universal Chess Interface loop
void uci_loop(Board& board);

#endif // UCI_H
