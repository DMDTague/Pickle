#ifndef OPENING_BOOK_H
#define OPENING_BOOK_H

#include "board.h"
#include "move.h"

// PickleBook is generated at build time from a pinned Lichess-Elite-derived
// repertoire, then reweighted for Pickle's shallow-search horizon.
void init_opening_book();

// Returns a legal weighted book move for the current position, or 0 when out of book.
Move probe_opening_book(Board& board);

void set_opening_book_enabled(bool enabled);
bool opening_book_enabled();
int opening_book_position_count();

#endif // OPENING_BOOK_H
