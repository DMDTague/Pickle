#ifndef ZOBRIST_H
#define ZOBRIST_H

#include "board.h"
#include <cstdint>

// Global keys
extern U64 piece_keys[12][64]; // [Color * 6 + PieceType][Square]
extern U64 en_passant_keys[8]; // One for each file (A-H)
extern U64 castle_keys[16];    // 4 bits = 16 possible states
extern U64 side_key;           // XORed when it is Black's turn

// Initialize the pseudo-random numbers
void init_zobrist();

// Generate the initial hash for a board state from scratch
U64 generate_hash(const Board& board);

#endif // ZOBRIST_H
