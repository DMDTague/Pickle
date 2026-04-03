#ifndef BIT_UTILS_H
#define BIT_UTILS_H

#include <cstdint>

// -----------------------------------------------------------------------------
// Bit Manipulation Utilites
// -----------------------------------------------------------------------------

// Set the bit at the given square index (0-63) to 1.
inline void set_bit(std::uint64_t& bitboard, int square) {
    bitboard |= (1ULL << square);
}

// Get the value of the bit at the given square index (returns true if 1).
inline bool get_bit(std::uint64_t bitboard, int square) {
    return (bitboard & (1ULL << square)) != 0;
}

// Clear the bit at the given square index (set to 0).
inline void clear_bit(std::uint64_t& bitboard, int square) {
    bitboard &= ~(1ULL << square);
}

// Count the number of set bits (population count) in the bitboard.
// Uses highly optimized compiler intrinsics.
inline int pop_count(std::uint64_t bitboard) {
    return __builtin_popcountll(bitboard);
}

// Get the index of the Least Significant 1-Bit (LSB).
// Undefined behavior for passing 0 to __builtin_ctzll natively, so we check.
inline int lsb(std::uint64_t bitboard) {
    if (bitboard == 0) return -1; // Or return 64; handled conventionally here
    return __builtin_ctzll(bitboard);
}

#endif // BIT_UTILS_H
