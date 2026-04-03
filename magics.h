#ifndef MAGICS_H
#define MAGICS_H

#include <cstdint>

// -----------------------------------------------------------------------------
// Magic Bitboard move generation for sliding pieces (Bishops, Rooks, and Queens)
// -----------------------------------------------------------------------------

using U64 = std::uint64_t;

// Attack lookup tables
extern U64 bishop_attacks[64][512];
extern U64 rook_attacks[64][4096];

// Initialization: must be called once at program start
void init_sliders();

// Main sliding move accessors
U64 get_bishop_attacks(int square, U64 occupancy);
U64 get_rook_attacks(int square, U64 occupancy);
U64 get_queen_attacks(int square, U64 occupancy);

// Internal Generation Helper (Shared for initialization and move generation)
U64 mask_bishop_occupancy(int square);
U64 mask_rook_occupancy(int square);
U64 bishop_attacks_on_the_fly(int square, U64 block);
U64 rook_attacks_on_the_fly(int square, U64 block);

#endif // MAGICS_H
