#pragma once
#include <cstdint>

enum Piece : uint8_t {
    EMPTY = 0,
    WPAWN, WKNIGHT, WBISHOP, WROOK, WQUEEN, WKING,
    BPAWN, BKNIGHT, BBISHOP, BROOK, BQUEEN, BKING
};

enum MoveFlags : uint8_t {
    QUIET = 0,
    CAPTURE = 1 << 0,
    DOUBLE_PAWN = 1 << 1,
    EN_PASSANT = 1 << 2,
    CASTLING = 1 << 3,
    PROMOTION = 1 << 4
};

struct MoveBB {
    uint8_t from = 0;           // 0–63
    uint8_t to = 0;             // 0–63
    Piece piece = EMPTY;            // moving piece
    Piece captured = EMPTY;         // captured piece or EMPTY
    bool promotion = false;         // is this a promotion
    Piece promotionPiece = EMPTY;   // piece promoted to
    uint8_t flags = QUIET;  // MoveFlags

    bool prevWCK = false;
    bool prevWCQ = false;
    bool prevBCK = false;
    bool prevBCQ = false;

    MoveBB() = default;

    MoveBB(uint8_t f, uint8_t t,
        Piece p,
        Piece c = EMPTY,
        bool promo = false,
        Piece promoPiece = EMPTY,
        uint8_t fl = QUIET)
        : from(f), to(t), piece(p),
        captured(c), promotion(promo),
        promotionPiece(promoPiece), flags(fl) {}

	bool isValid() const {
		return piece != EMPTY;
	}
};
