#include "opening_book.h"
#include "movegen.h"
#include <cstddef>
#include <cstdint>
#include <vector>

#include "opening_book_data.inc"

namespace {

struct LegalChoice {
    Move move = 0;
    int weight = 0;
};

bool book_enabled = true;
U64 probe_counter = 0;

U64 mix64(U64 x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

std::uint16_t compact_move(Move move) {
    return static_cast<std::uint16_t>(
        get_move_source(move)
        | (get_move_target(move) << 6)
        | (get_move_promoted(move) << 12));
}

const GeneratedBookEntry* find_entry(U64 key) {
    std::size_t lo = 0;
    std::size_t hi = PICKLE_BOOK_ENTRY_COUNT;
    while (lo < hi) {
        const std::size_t mid = lo + (hi - lo) / 2;
        if (PICKLE_BOOK_ENTRIES[mid].key < key) lo = mid + 1;
        else hi = mid;
    }
    if (lo < PICKLE_BOOK_ENTRY_COUNT && PICKLE_BOOK_ENTRIES[lo].key == key) {
        return &PICKLE_BOOK_ENTRIES[lo];
    }
    return nullptr;
}

} // namespace

void init_opening_book() {
    // PickleBook is generated at build time into opening_book_data.inc. There
    // is no runtime parsing or network dependency for native or WASM Pickle.
}

Move probe_opening_book(Board& board) {
    if (!book_enabled || PICKLE_BOOK_ENTRY_COUNT == 0) return 0;

    const GeneratedBookEntry* entry = find_entry(board.get_hash_key());
    if (!entry || entry->move_count == 0) return 0;

    MoveList legal_moves;
    generate_moves(board, legal_moves);

    std::vector<LegalChoice> choices;
    choices.reserve(entry->move_count);
    int total_weight = 0;

    for (std::uint32_t i = 0; i < entry->move_count; ++i) {
        const GeneratedBookMove& generated = PICKLE_BOOK_MOVES[entry->move_offset + i];
        for (int j = 0; j < legal_moves.count; ++j) {
            Move move = legal_moves.moves[j];
            if (compact_move(move) != generated.packed) continue;
            if (!board.make_move(move)) break;
            board.unmake_move(move);
            const int weight = generated.weight > 0 ? generated.weight : 1;
            choices.push_back({move, weight});
            total_weight += weight;
            break;
        }
    }

    if (choices.empty() || total_weight <= 0) return 0;

    // Variation remains weighted, but the generated weights already combine
    // strong-human frequency with Pickle's shallow-horizon compatibility.
    U64 pick = mix64(board.get_hash_key() ^ (++probe_counter * 0x9e3779b97f4a7c15ULL))
             % static_cast<U64>(total_weight);
    for (const auto& choice : choices) {
        if (pick < static_cast<U64>(choice.weight)) return choice.move;
        pick -= static_cast<U64>(choice.weight);
    }
    return choices.front().move;
}

void set_opening_book_enabled(bool enabled) {
    book_enabled = enabled;
}

bool opening_book_enabled() {
    return book_enabled;
}

int opening_book_position_count() {
    return static_cast<int>(PICKLE_BOOK_ENTRY_COUNT);
}
