#include "ChessBoard.h"


bool ChessBoard::pawnAttacksKingBB(int kingSquare, bool isWhite) const {
	uint64_t king = 1ULL << kingSquare;


	// Instead of asking "which pawns attack the king?", we ask "from which
	// squares COULD a pawn attack the king?" and intersect that with the
	// enemy pawn bitboard -- one shift-and-mask instead of a loop.
	//
	// Black pawns capture downwards, so a black pawn attacking a white king
	// on square k must sit ABOVE it, on k+7 or k+9. White pawns capture
	// upwards, so a white pawn attacking a black king sits BELOW it, on
	// k-7 or k-9. (These two cases were previously swapped, which meant
	// pawn checks against the king were never detected at all -- the king
	// could legally walk onto a square guarded by an enemy pawn. Caught by
	// perft on Position 3, where Ka5-b6 was generated despite the c7 pawn
	// covering b6.)
	if (isWhite) {
		// Look up-right (+9) and up-left (+7) for black pawns.
		uint64_t pawnAttacks = (king & NOT_FILE_H) << 9;
		pawnAttacks |= (king & NOT_FILE_A) << 7;
		return (pawnAttacks & blackPawns) != 0;
	}
	else {
		// Look down-left (-9) and down-right (-7) for white pawns.
		uint64_t pawnAttacks = (king & NOT_FILE_A) >> 9;
		pawnAttacks |= (king & NOT_FILE_H) >> 7;
		return (pawnAttacks & whitePawns) != 0;
	}
}

bool ChessBoard::isKingInCheckBB(bool isWhite) {
    // King bitboard & square
    uint64_t kingBB = isWhite ? whiteKing : blackKing;
    if (kingBB == 0) return false; // king missing, should not happen
    int kingSq = popLSB(kingBB);

    uint64_t kingMask = 1ULL << kingSq;

    // Own & enemy pieces
    uint64_t ownPieces = isWhite ? getWhitePieces() : getBlackPieces();
    uint64_t enemyPieces = isWhite ? getBlackPieces() : getWhitePieces();

    // Pawn attacks
    if (pawnAttacksKingBB(kingSq, isWhite)) return true;

    // Knight attacks
    if (knightAttacks[kingSq] & (isWhite ? blackKnights : whiteKnights)) return true;

    // King attacks
    if (kingAttacks[kingSq] & (isWhite ? blackKing : whiteKing)) return true;

    // Bishop/Queen diagonal attacks
    uint64_t bishopSliding = getBishopMovesB(kingMask, ownPieces, enemyPieces);
    if (bishopSliding & (isWhite ? (blackBishops | blackQueen) : (whiteBishops | whiteQueen)))
        return true;

    // Rook/Queen straight attacks
    uint64_t rookSliding = getRookMovesB(kingMask, ownPieces, enemyPieces);
    if (rookSliding & (isWhite ? (blackRooks | blackQueen) : (whiteRooks | whiteQueen)))
        return true;

    return false; // no check detected
}

bool ChessBoard::isSquareAttacked(int square, bool byWhite) const {
    uint64_t sqBB = 1ULL << square;
    uint64_t enemyPawns = byWhite ? whitePawns : blackPawns;
    uint64_t enemyKnights = byWhite ? whiteKnights : blackKnights;
    uint64_t enemyBishops = byWhite ? whiteBishops : blackBishops;
    uint64_t enemyRooks = byWhite ? whiteRooks : blackRooks;
    uint64_t enemyQueens = byWhite ? whiteQueen : blackQueen;
    uint64_t enemyKing = byWhite ? whiteKing : blackKing;

    uint64_t ownPieces = byWhite ? getWhitePieces() : getBlackPieces();
    uint64_t oppPieces = byWhite ? getBlackPieces() : getWhitePieces();

    // Pawns
    if (byWhite) {
        if ((sqBB & ((enemyPawns & ~0x0101010101010101ULL) << 7)) != 0) return true; // capture from left
        if ((sqBB & ((enemyPawns & ~0x8080808080808080ULL) << 9)) != 0) return true; // capture from right
    }
    else {
        if ((sqBB & ((enemyPawns & ~0x0101010101010101ULL) >> 9)) != 0) return true; // capture from left
        if ((sqBB & ((enemyPawns & ~0x8080808080808080ULL) >> 7)) != 0) return true; // capture from right
    }

    // Knights
    for (int from = 0; from < 64; from++) {
        if (enemyKnights & (1ULL << from)) {
            if (knightAttacks[from] & sqBB) return true;
        }
    }

    // Kings
    for (int from = 0; from < 64; from++) {
        if (enemyKing & (1ULL << from)) {
            if (kingAttacks[from] & sqBB) return true;
        }
    }

    // Bishops / Queens (diagonals)
    uint64_t bishopsQueens = enemyBishops | enemyQueens;
    for (int from = 0; from < 64; from++) {
        if (bishopsQueens & (1ULL << from)) {
            uint64_t attacks = getBishopMovesB(1ULL << from, ownPieces, oppPieces);
            if (attacks & sqBB) return true;
        }
    }

    // Rooks / Queens (lines)
    uint64_t rooksQueens = enemyRooks | enemyQueens;
    for (int from = 0; from < 64; from++) {
        if (rooksQueens & (1ULL << from)) {
            uint64_t attacks = getRookMovesB(1ULL << from, ownPieces, oppPieces);
            if (attacks & sqBB) return true;
        }
    }

    return false;
}
