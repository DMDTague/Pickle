#ifndef BIT_UTILS_H
#define BIT_UTILS_H

#include <cstdint>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

// -----------------------------------------------------------------------------
// Bit Manipulation Utilities
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
inline int pop_count(std::uint64_t bitboard) {
#if defined(_MSC_VER)
    // Portable SWAR popcount avoids requiring a particular MSVC /arch setting
    // or CPU POPCNT feature while remaining branch-free.
    bitboard -= (bitboard >> 1) & 0x5555555555555555ULL;
    bitboard = (bitboard & 0x3333333333333333ULL)
             + ((bitboard >> 2) & 0x3333333333333333ULL);
    bitboard = (bitboard + (bitboard >> 4)) & 0x0F0F0F0F0F0F0F0FULL;
    return static_cast<int>((bitboard * 0x0101010101010101ULL) >> 56);
#else
    return __builtin_popcountll(bitboard);
#endif
}

// Get the index of the Least Significant 1-Bit (LSB).
inline int lsb(std::uint64_t bitboard) {
    if (bitboard == 0) return -1;
#if defined(_MSC_VER) && defined(_M_X64)
    unsigned long index = 0;
    _BitScanForward64(&index, bitboard);
    return static_cast<int>(index);
#elif defined(_MSC_VER)
    unsigned long index = 0;
    const std::uint32_t low = static_cast<std::uint32_t>(bitboard);
    if (low != 0) {
        _BitScanForward(&index, low);
        return static_cast<int>(index);
    }
    _BitScanForward(&index, static_cast<std::uint32_t>(bitboard >> 32));
    return static_cast<int>(index + 32);
#else
    return __builtin_ctzll(bitboard);
#endif
}

#endif // BIT_UTILS_H
