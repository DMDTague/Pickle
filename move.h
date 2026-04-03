#ifndef MOVE_H
#define MOVE_H

#include <cstdint>
#include <string>

// -----------------------------------------------------------------------------
// Move Encoding (32-bit for clarity and piece storage)
// -----------------------------------------------------------------------------
/*
    Move bitboard encoding:
    0-5:   From square (0-63)
    6-11:  To square (0-63)
    12-15: Piece type (0-5)
    16-19: Promoted piece type (0-5, or 0 if none)
    20:    Capture flag
    21:    Double pawn push flag
    22:    En passant flag
    23:    Castling flag
    24-27: Captured Piece type (0-5, or 0 if none)
*/

typedef std::uint32_t Move;

// Macros / Inline functions for move encoding
inline Move encode_move(int from, int to, int piece, int promoted, int capture, int double_push, int enpassant, int castling, int captured_piece) {
    return (Move)from | 
           ((Move)to << 6) | 
           ((Move)piece << 12) | 
           ((Move)promoted << 16) | 
           ((Move)capture << 20) | 
           ((Move)double_push << 21) | 
           ((Move)enpassant << 22) | 
           ((Move)castling << 23) |
           ((Move)captured_piece << 24);
}

// Extraction helpers
inline int get_move_source(Move move)    { return move & 0x3F; }
inline int get_move_target(Move move)    { return (move >> 6) & 0x3F; }
inline int get_move_piece(Move move)     { return (move >> 12) & 0xF; }
inline int get_move_promoted(Move move)  { return (move >> 16) & 0xF; }
inline int get_move_capture(Move move)   { return (move >> 20) & 0x1; }
inline int get_move_double(Move move)    { return (move >> 21) & 0x1; }
inline int get_move_enpassant(Move move) { return (move >> 22) & 0x1; }
inline int get_move_castling(Move move)  { return (move >> 23) & 0x1; }
inline int get_move_captured_piece(Move move) { return (move >> 24) & 0xF; }

// -----------------------------------------------------------------------------
// Move List (Pre-allocated for performance)
// -----------------------------------------------------------------------------

struct MoveList {
    Move moves[256];
    int count;

    MoveList() : count(0) {}

    void add_move(Move move) {
        if (count < 256) {
            moves[count++] = move;
        }
    }
};

#endif // MOVE_H
