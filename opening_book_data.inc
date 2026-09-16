// Placeholder generated-book data. CI replaces this from official-stockfish/books.
// See scripts/generate_opening_book.py.

struct GeneratedBookEntry {
    std::uint64_t key;
    std::uint32_t move_offset;
    std::uint8_t move_count;
};

struct GeneratedBookMove {
    std::uint16_t packed;
    std::uint16_t weight;
};

static constexpr std::size_t STOCKFISH_BOOK_ENTRY_COUNT = 0;
static constexpr std::size_t STOCKFISH_BOOK_MOVE_COUNT = 0;

static constexpr GeneratedBookEntry STOCKFISH_BOOK_ENTRIES[] = {
    {0ULL, 0u, 0u},
};

static constexpr GeneratedBookMove STOCKFISH_BOOK_MOVES[] = {
    {0u, 0u},
};
