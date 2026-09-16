#ifndef OPENING_BOOK_H
#define OPENING_BOOK_H

#include "board.h"
#include "move.h"

// Builds Pickle's small built-in opening book from curated UCI lines.
// Call after attacks/sliders/Zobrist initialization.
void init_opening_book();

// Returns a legal book move for the current position, or 0 when out of book.
Move probe_opening_book(Board& board);

void set_opening_book_enabled(bool enabled);
bool opening_book_enabled();
int opening_book_position_count();

#endif // OPENING_BOOK_H
