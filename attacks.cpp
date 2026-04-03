#include "attacks.h"
#include "bit_utils.h"
#include "masks.h"

// Initialize the global lookup tables
std::uint64_t knight_attacks[64];
std::uint64_t king_attacks[64];

// Given a square index, calculate the pseudo-legal moves for a Knight masking wrapping
std::uint64_t mask_knight_attacks(int square) {
    std::uint64_t attacks = 0ULL;
    std::uint64_t bitboard = 0ULL;

    // Place a piece on the specified square
    set_bit(bitboard, square);

    // Calculate all 8 possible knight offsets.
    // If an offset pushes the piece off the side of the board and it wraps,
    // the respective NOT_FILES mask will eliminate the illegal wrapped bits.
    
    // Shifts upwards (North)
    if ((bitboard << 17) & NOT_A_FILE) attacks |= (bitboard << 17); // North-North-East
    if ((bitboard << 15) & NOT_H_FILE) attacks |= (bitboard << 15); // North-North-West
    if ((bitboard << 10) & NOT_AB_FILE) attacks |= (bitboard << 10); // East-East-North
    if ((bitboard <<  6) & NOT_GH_FILE) attacks |= (bitboard <<  6); // West-West-North

    // Shifts downwards (South)
    if ((bitboard >> 15) & NOT_A_FILE) attacks |= (bitboard >> 15); // South-South-East
    if ((bitboard >> 17) & NOT_H_FILE) attacks |= (bitboard >> 17); // South-South-West
    if ((bitboard >>  6) & NOT_AB_FILE) attacks |= (bitboard >>  6); // East-East-South
    if ((bitboard >> 10) & NOT_GH_FILE) attacks |= (bitboard >> 10); // West-West-South

    return attacks;
}

// Given a square index, calculate the pseudo-legal moves for a King masking wrapping
std::uint64_t mask_king_attacks(int square) {
    std::uint64_t attacks = 0ULL;
    std::uint64_t bitboard = 0ULL;

    // Place a piece on the specified square
    set_bit(bitboard, square);

    // Lateral / Vertical offsets
    if (bitboard << 8) attacks |= (bitboard << 8); // North
    if (bitboard >> 8) attacks |= (bitboard >> 8); // South
    if ((bitboard << 1) & NOT_A_FILE) attacks |= (bitboard << 1); // East
    if ((bitboard >> 1) & NOT_H_FILE) attacks |= (bitboard >> 1); // West

    // Diagonal offsets
    if ((bitboard << 9) & NOT_A_FILE) attacks |= (bitboard << 9); // North-East
    if ((bitboard << 7) & NOT_H_FILE) attacks |= (bitboard << 7); // North-West
    if ((bitboard >> 7) & NOT_A_FILE) attacks |= (bitboard >> 7); // South-East
    if ((bitboard >> 9) & NOT_H_FILE) attacks |= (bitboard >> 9); // South-West

    return attacks;
}

// Populate the static jump tables arrays using our masking functions
void init_leapers() {
    for (int square = 0; square < 64; square++) {
        knight_attacks[square] = mask_knight_attacks(square);
        king_attacks[square] = mask_king_attacks(square);
    }
}
