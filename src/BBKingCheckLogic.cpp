#include "ChessBoard.h"


bool ChessBoard::pawnAttacksKingBB(int kingSquare, bool isWhite) const {
	uint64_t king = 1ULL << kingSquare;


	if (isWhite) {
		uint64_t pawnAttacks = (king & NOT_FILE_H) >> 7; // from left
		pawnAttacks |= (king & NOT_FILE_A) >> 9;// from right
		return pawnAttacks & blackPawns;
	}
	else {
		uint64_t pawnAttacks = (king & NOT_FILE_H) << 9; // from left
		pawnAttacks |= (king & NOT_FILE_A) << 7; // from right
		return pawnAttacks & whitePawns;
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
