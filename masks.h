#ifndef MASKS_H
#define MASKS_H

#include <cstdint>

// -----------------------------------------------------------------------------
// File Boundaries and Wrapping Masks
// -----------------------------------------------------------------------------

// Used to prevent piece attacks from wrapping around from the H-file to the A-file.
// NOT_A_FILE has 0s on the A-file and 1s everywhere else.
constexpr std::uint64_t NOT_A_FILE = 0xFEFEFEFEFEFEFEFEULL;

// NOT_H_FILE has 0s on the H-file and 1s everywhere else.
constexpr std::uint64_t NOT_H_FILE = 0x7F7F7F7F7F7F7F7FULL;

// Used to prevent piece attacks from wrapping across TWO files (e.g., Knights).
// NOT_AB_FILE has 0s on the A and B files and 1s everywhere else.
constexpr std::uint64_t NOT_AB_FILE = 0xFCFCFCFCFCFCFCFCULL;

// NOT_GH_FILE has 0s on the G and H files and 1s everywhere else.
constexpr std::uint64_t NOT_GH_FILE = 0x3F3F3F3F3F3F3F3FULL;

#endif // MASKS_H
