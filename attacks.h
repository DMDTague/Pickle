#ifndef ATTACKS_H
#define ATTACKS_H

#include <cstdint>

// -----------------------------------------------------------------------------
// Leaper Pre-Calculated Attack Tables
// -----------------------------------------------------------------------------

// Look-up tables corresponding to every square on the board for leaping pieces
extern std::uint64_t knight_attacks[64];
extern std::uint64_t king_attacks[64];

// Masking functions that compute the valid attack bitboard for a piece placed on a square
std::uint64_t mask_knight_attacks(int square);
std::uint64_t mask_king_attacks(int square);

// Runs on start up to fill the global jump table arrays
void init_leapers();

#endif // ATTACKS_H
