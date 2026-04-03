#ifndef PERFT_H
#define PERFT_H

#include "board.h"
#include <cstdint>

// Recursive Perft function
U64 perft(Board& board, int depth);

// Perft divide for debugging - prints root moves and their node counts
void perft_divide(Board& board, int depth);

#endif // PERFT_H
