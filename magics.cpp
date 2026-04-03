#include "magics.h"
#include <iostream>

// -----------------------------------------------------------------------------
// Magic Bitboard move generation for sliding pieces (Rooks and Bishops)
// -----------------------------------------------------------------------------

// Attack lookup tables
U64 bishop_attacks[64][512];
U64 rook_attacks[64][4096];

// Relevant bits for bishop occupancy (from corners and edges)
const int bishop_relevant_bits[64] = {
    6, 5, 5, 5, 5, 5, 5, 6,
    5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 7, 7, 7, 7, 5, 5,
    5, 5, 7, 9, 9, 7, 5, 5,
    5, 5, 7, 9, 9, 7, 5, 5,
    5, 5, 7, 7, 7, 7, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5,
    6, 5, 5, 5, 5, 5, 5, 6
};

// Relevant bits for rook occupancy (from corners and edges)
const int rook_relevant_bits[64] = {
    12, 11, 11, 11, 11, 11, 11, 12,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    12, 11, 11, 11, 11, 11, 11, 12
};

// Valid Magic numbers generated for this specific implementation
const U64 bishop_magic_numbers[64] = {
    0x004081082c818880ULL, 0x0048818802004020ULL, 0x1004011212000101ULL, 0x04c2408100000460ULL,
    0x0001114000005301ULL, 0x010e111028000428ULL, 0x0292008404400100ULL, 0x0111228444104000ULL,
    0x0000046802540400ULL, 0x0401341060821c80ULL, 0x4400040c00921000ULL, 0x0000082240404020ULL,
    0x0008240420008830ULL, 0x8014809004200059ULL, 0x0a0020420210424aULL, 0x0003409409011008ULL,
    0x0044000809080810ULL, 0x2004801001081910ULL, 0x08100e220403400aULL, 0x0008000402182005ULL,
    0x2044000200a26040ULL, 0x0002040308010400ULL, 0x0400502608040400ULL, 0x0616002508a20111ULL,
    0x0009590040100100ULL, 0x0850088030012902ULL, 0x0040248008080100ULL, 0x01a0080109004088ULL,
    0x0010030050200802ULL, 0x0151014002005022ULL, 0x0001004441041000ULL, 0x86270020044c0404ULL,
    0x1102082000842000ULL, 0x05050110209810a4ULL, 0x2283840400405041ULL, 0x4002004040dc0100ULL,
    0x80080a0400801100ULL, 0x5000900080010080ULL, 0xa9020204200200a3ULL, 0x0025822a02008480ULL,
    0x1018240220080800ULL, 0x2004021310480d00ULL, 0x0a88084048001004ULL, 0x0148202024208800ULL,
    0x2104200410400400ULL, 0x0110601081002020ULL, 0x0002022202000400ULL, 0x4401014400800901ULL,
    0x0652825030040000ULL, 0x2000470090101829ULL, 0x8008010488044404ULL, 0x0480400a84241048ULL,
    0x04064020052c0040ULL, 0x0030410822142080ULL, 0x0104200401220000ULL, 0x111104012409a040ULL,
    0x0007008200e00422ULL, 0x3001010098410808ULL, 0x8900101900809002ULL, 0x4003881316411080ULL,
    0x00010110200b4400ULL, 0x0004804202042102ULL, 0x0000042004112210ULL, 0x0022100228004488ULL
};

const U64 rook_magic_numbers[64] = {
    0x0c80028010400422ULL, 0x0240004010042008ULL, 0x8a0019c022001080ULL, 0x0100210008100004ULL,
    0x4200102002000804ULL, 0x0880040002008061ULL, 0x0200020021042088ULL, 0x0080192080044100ULL,
    0x4800800080204002ULL, 0x0505002100884000ULL, 0x0a01002001004010ULL, 0x0020801000800800ULL,
    0x0241000508010010ULL, 0x1010800200040081ULL, 0x5181000100040200ULL, 0x0841800300214080ULL,
    0x0200218000824001ULL, 0x5040010029044080ULL, 0x100017002000c100ULL, 0x0601010020100008ULL,
    0x0010050008010010ULL, 0x0200080104201040ULL, 0x0000040010224128ULL, 0x02460a0030804504ULL,
    0x0000400080008020ULL, 0x0020002240005000ULL, 0x0800100080200080ULL, 0x0028080080801000ULL,
    0x2001000500080010ULL, 0xc002001600041028ULL, 0x0210680400010210ULL, 0x0804004200109104ULL,
    0x0080400020800088ULL, 0x9040810022004200ULL, 0x601120008c801000ULL, 0x0088100021000900ULL,
    0x0001000801000410ULL, 0x520060c008010410ULL, 0x0000120104001830ULL, 0x88000040a2000104ULL,
    0x2000802040008004ULL, 0x0040400020008080ULL, 0x0001004020010012ULL, 0x1482000820120041ULL,
    0x1021001008010006ULL, 0x0004000402008080ULL, 0x0848020810040001ULL, 0x0440010044860004ULL,
    0x6680800040002080ULL, 0x0207810042002a00ULL, 0x01441044a3820200ULL, 0x0010002089029100ULL,
    0xa000080080040080ULL, 0x24a4020080040080ULL, 0x4800412208108400ULL, 0x0202010400408200ULL,
    0x0d05020040601a82ULL, 0x090d011080400925ULL, 0x8005004008200011ULL, 0x0424041000210109ULL,
    0x0202001008042002ULL, 0x0003000400080201ULL, 0x8800011008020084ULL, 0x1000004401003082ULL
};

// -----------------------------------------------------------------------------
// Occupancy Masking
// -----------------------------------------------------------------------------

U64 mask_bishop_occupancy(int square) {
    U64 occupancy = 0ULL;
    int r, f;
    int tr = square / 8;
    int tf = square % 8;

    for (r = tr + 1, f = tf + 1; r <= 6 && f <= 6; r++, f++) occupancy |= (1ULL << (r * 8 + f));
    for (r = tr + 1, f = tf - 1; r <= 6 && f >= 1; r++, f--) occupancy |= (1ULL << (r * 8 + f));
    for (r = tr - 1, f = tf + 1; r >= 1 && f <= 6; r--, f++) occupancy |= (1ULL << (r * 8 + f));
    for (r = tr - 1, f = tf - 1; r >= 1 && f >= 1; r--, f--) occupancy |= (1ULL << (r * 8 + f));

    return occupancy;
}

U64 mask_rook_occupancy(int square) {
    U64 occupancy = 0ULL;
    int r, f;
    int tr = square / 8;
    int tf = square % 8;

    for (r = tr + 1; r <= 6; r++) occupancy |= (1ULL << (r * 8 + tf));
    for (r = tr - 1; r >= 1; r--) occupancy |= (1ULL << (r * 8 + tf));
    for (f = tf + 1; f <= 6; f++) occupancy |= (1ULL << (tr * 8 + f));
    for (f = tf - 1; f >= 1; f--) occupancy |= (1ULL << (tr * 8 + f));

    return occupancy;
}

// -----------------------------------------------------------------------------
// Attack Generation on the Fly
// -----------------------------------------------------------------------------

U64 bishop_attacks_on_the_fly(int square, U64 block) {
    U64 attacks = 0ULL;
    int r, f;
    int tr = square / 8;
    int tf = square % 8;

    for (r = tr + 1, f = tf + 1; r <= 7 && f <= 7; r++, f++) {
        attacks |= (1ULL << (r * 8 + f));
        if ((1ULL << (r * 8 + f)) & block) break;
    }
    for (r = tr + 1, f = tf - 1; r <= 7 && f >= 0; r++, f--) {
        attacks |= (1ULL << (r * 8 + f));
        if ((1ULL << (r * 8 + f)) & block) break;
    }
    for (r = tr - 1, f = tf + 1; r >= 0 && f <= 7; r--, f++) {
        attacks |= (1ULL << (r * 8 + f));
        if ((1ULL << (r * 8 + f)) & block) break;
    }
    for (r = tr - 1, f = tf - 1; r >= 0 && f >= 0; r--, f--) {
        attacks |= (1ULL << (r * 8 + f));
        if ((1ULL << (r * 8 + f)) & block) break;
    }

    return attacks;
}

U64 rook_attacks_on_the_fly(int square, U64 block) {
    U64 attacks = 0ULL;
    int r, f;
    int tr = square / 8;
    int tf = square % 8;

    for (r = tr + 1; r <= 7; r++) {
        attacks |= (1ULL << (r * 8 + tf));
        if ((1ULL << (r * 8 + tf)) & block) break;
    }
    for (r = tr - 1; r >= 0; r--) {
        attacks |= (1ULL << (r * 8 + tf));
        if ((1ULL << (r * 8 + tf)) & block) break;
    }
    for (f = tf + 1; f <= 7; f++) {
        attacks |= (1ULL << (tr * 8 + f));
        if ((1ULL << (tr * 8 + f)) & block) break;
    }
    for (f = tf - 1; f >= 0; f--) {
        attacks |= (1ULL << (tr * 8 + f));
        if ((1ULL << (tr * 8 + f)) & block) break;
    }

    return attacks;
}

// -----------------------------------------------------------------------------
// Permutation Generator
// -----------------------------------------------------------------------------

// Utility to set occupancy for a specific index. 
// Standard LSB-based permutation generator.
static U64 set_occupancy(int index, int bits_in_mask, U64 attack_mask) {
    U64 occupancy = 0ULL;
    for (int count = 0; count < bits_in_mask; count++) {
        int square = __builtin_ctzll(attack_mask);
        attack_mask &= ~(1ULL << square);
        if (index & (1 << count)) {
            occupancy |= (1ULL << square);
        }
    }
    return occupancy;
}

// -----------------------------------------------------------------------------
// Initialization
// -----------------------------------------------------------------------------

void init_sliders() {
    for (int sq = 0; sq < 64; sq++) {
        // Init bishop attack table
        U64 bishop_mask = mask_bishop_occupancy(sq);
        int bishop_bits = bishop_relevant_bits[sq];
        int bishop_occupancy_count = (1 << bishop_bits);

        for (int i = 0; i < bishop_occupancy_count; i++) {
            U64 occupancy = set_occupancy(i, bishop_bits, bishop_mask);
            int magic_index = (int)((occupancy * bishop_magic_numbers[sq]) >> (64 - bishop_bits));
            bishop_attacks[sq][magic_index] = bishop_attacks_on_the_fly(sq, occupancy);
        }

        // Init rook attack table
        U64 rook_mask = mask_rook_occupancy(sq);
        int rook_bits = rook_relevant_bits[sq];
        int rook_occupancy_count = (1 << rook_bits);

        for (int i = 0; i < rook_occupancy_count; i++) {
            U64 occupancy = set_occupancy(i, rook_bits, rook_mask);
            int magic_index = (int)((occupancy * rook_magic_numbers[sq]) >> (64 - rook_bits));
            rook_attacks[sq][magic_index] = rook_attacks_on_the_fly(sq, occupancy);
        }
    }
}

// -----------------------------------------------------------------------------
// Attack Accessors
// -----------------------------------------------------------------------------

U64 get_bishop_attacks(int square, U64 occupancy) {
    U64 mask = mask_bishop_occupancy(square);
    occupancy &= mask;
    occupancy *= bishop_magic_numbers[square];
    occupancy >>= (64 - bishop_relevant_bits[square]);
    return bishop_attacks[square][(int)occupancy];
}

U64 get_rook_attacks(int square, U64 occupancy) {
    U64 mask = mask_rook_occupancy(square);
    occupancy &= mask;
    occupancy *= rook_magic_numbers[square];
    occupancy >>= (64 - rook_relevant_bits[square]);
    return rook_attacks[square][(int)occupancy];
}

U64 get_queen_attacks(int square, U64 occupancy) {
    return get_bishop_attacks(square, occupancy) | get_rook_attacks(square, occupancy);
}
