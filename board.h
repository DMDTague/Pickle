#ifndef BOARD_H
#define BOARD_H

#include <cstdint>
#include <string>
#include "move.h"

// -----------------------------------------------------------------------------
// Constants
// -----------------------------------------------------------------------------

#define MAX_PLY 2048

// -----------------------------------------------------------------------------
// Core Data Types
// -----------------------------------------------------------------------------

// A Bitboard is a 64-bit unsigned integer representing the 64 squares of a chessboard.
// Each bit corresponds to a specific square on the board (0-63).
// 1 = piece present on that square, 0 = no piece present.
using U64 = std::uint64_t;
using Bitboard = std::uint64_t;

// -----------------------------------------------------------------------------
// Enumerations
// -----------------------------------------------------------------------------

// Colors: Represents the two sides in chess.
enum Color {
    WHITE = 0,
    BLACK = 1,
    COLOR_COUNT = 2
};

// Pieces: Represents the six types of chess pieces.
// We keep piece types separate from color to allow independent queries.
enum PieceType {
    PAWN   = 0,
    KNIGHT = 1,
    BISHOP = 2,
    ROOK   = 3,
    QUEEN  = 4,
    KING   = 5,
    PIECE_TYPE_COUNT = 6
};

// Squares: The 64 squares on a chessboard.
// The mapping follows little-endian rank-file configuration:
// A1 is 0, B1 is 1 ... H1 is 7 ... A8 is 56 ... H8 is 63.
enum Square {
    A1 = 0,  B1, C1, D1, E1, F1, G1, H1,
    A2 = 8,  B2, C2, D2, E2, F2, G2, H2,
    A3 = 16, B3, C3, D3, E3, F3, G3, H3,
    A4 = 24, B4, C4, D4, E4, F4, G4, H4,
    A5 = 32, B5, C5, D5, E5, F5, G5, H5,
    A6 = 40, B6, C6, D6, E6, F6, G6, H6,
    A7 = 48, B7, C7, D7, E7, F7, G7, H7,
    A8 = 56, B8, C8, D8, E8, F8, G8, H8,
    SQUARE_COUNT = 64
};

// -----------------------------------------------------------------------------
// Board Representation
// -----------------------------------------------------------------------------

// The foundational physical universe of the chess board.
// Designed purely with bitboards for extreme performance and bitwise operations.
class Board {
public:
    // Initializes an empty board with all bitboards set to 0.
    Board();

    // Clears the board completely
    void clear_board();

    // Parses a FEN string to set up the board state
    void parse_fen(const std::string& fen);

    // Initializes the standard chess starting position
    void init_start_position();

    // Prints the board in an easy-to-read ASCII 8x8 grid
    void print_board() const;

    // Returns the full FEN string for the current board state
    std::string get_fen() const;

    // Adjudicates draws (50-move rule & repetition)
    bool is_draw() const;

    // Avoid adding logic yet, just establish the physical state arrays.

    // Getters for bitboards and state
    Bitboard get_piece_bitboard(PieceType p) const { return pieces[p]; }
    Bitboard get_color_bitboard(Color c) const { return colors[c]; }
    Bitboard get_occupancy() const { return colors[WHITE] | colors[BLACK]; }
    
    Color get_side_to_move() const { return side_to_move; }
    Square get_en_passant_sq() const { return en_passant_sq; }
    int get_castling_rights() const { return castling_rights; }
    U64 get_hash_key() const { return hash_key; }
    
    // Helper for Zugzwang checks
    bool has_non_pawn_material(Color color) const;

    void set_side_to_move(Color color) { side_to_move = color; }
    void set_en_passant_sq(Square sq) { en_passant_sq = sq; }
    void set_castling_rights(int rights) { castling_rights = rights; }

    // -------------------------------------------------------------------------
    // Move Execution & Undo
    // -------------------------------------------------------------------------
    bool make_move(Move move);
    void unmake_move(Move move);
    
    // Null Move Pruning support
    void make_null_move();
    void unmake_null_move();
    bool is_square_attacked(Square sq, Color by_color) const;

private:
    // Internal bitboard manipulation helpers
    void remove_piece(Square sq, Color c, PieceType p);
    void add_piece(Square sq, Color c, PieceType p);
    void move_piece(Square from, Square to, Color c, PieceType p);

    // bitboards for piece types, independent of color.
    // Index corresponds to PieceType enum.
    // pieces[PAWN] will have 1s on squares with either White or Black pawns.
    Bitboard pieces[PIECE_TYPE_COUNT];

    // bitboards for colors, independent of piece type.
    // Index corresponds to Color enum.
    // colors[WHITE] will have 1s on squares with any White piece.
    Bitboard colors[COLOR_COUNT];
    
    // Game state
    Color side_to_move;
    Square en_passant_sq;
    int castling_rights;
    int half_move_clock;
    U64 hash_key;

    // State History
    struct GameState {
        int castling_rights;
        Square en_passant_sq;
        int half_move_clock;
        PieceType captured_piece;
        U64 hash_key;
    };
    GameState history[MAX_PLY];
    int ply;

    // Note: To find a specific piece of a specific color, use bitwise AND:
    // Bitboard white_pawns = pieces[PAWN] & colors[WHITE];
};

#endif // BOARD_H
