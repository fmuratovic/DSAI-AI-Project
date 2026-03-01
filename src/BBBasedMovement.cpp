
#include "ChessBoard.h"



uint64_t ChessBoard::getPawnMovesB(uint64_t pawns, uint64_t emptySquares, bool isWhite) const {
	uint64_t moves = 0ULL;
	if (isWhite) {
		// Single move forward
		uint64_t singleMove = (pawns << 8) & emptySquares;
		moves |= singleMove;

		// Double move from starting rank
		uint64_t doubleMove = ((pawns & RANK_2) << 8 & emptySquares) << 8 & emptySquares;
		moves |= doubleMove;
	}
	else {
		// Single move forward
		uint64_t singleMove = (pawns >> 8) & emptySquares;
		moves |= singleMove;

		// Double move from starting rank
		uint64_t doubleMove = ((pawns & RANK_7) >> 8 & emptySquares) >> 8 & emptySquares;
		moves |= doubleMove;
	}
	return moves;
}

uint64_t ChessBoard::getWhitePawnCaptures(uint64_t pawns, uint64_t blackPieces) const {
	uint64_t leftCaptures = ((pawns & NOT_FILE_A) << 7) & blackPieces; // capture to the left
	uint64_t rightCaptures = ((pawns & NOT_FILE_H) << 9) & blackPieces; // capture to the right
	return leftCaptures | rightCaptures;
}

uint64_t ChessBoard::getBlackPawnCaptures(uint64_t pawns, uint64_t whitePieces) const {
	uint64_t leftCaptures = ((pawns & NOT_FILE_A) >> 9) & whitePieces; // capture to the left
	uint64_t rightCaptures = ((pawns & NOT_FILE_H) >> 7) & whitePieces; // capture to the right
	return leftCaptures | rightCaptures;
}

void ChessBoard::initKnightAttacks() {
	for (int sq = 0; sq < 64; sq++) {
		uint64_t knight = 1ULL << sq;
		uint64_t attacks = 0;

		attacks |= (knight & NOT_FILE_A & NOT_FILE_B) << 6;
		attacks |= (knight & NOT_FILE_A & NOT_FILE_B) >> 10;

		attacks |= (knight & NOT_FILE_H & NOT_FILE_G) << 10;
		attacks |= (knight & NOT_FILE_H & NOT_FILE_G) >> 6;

		attacks |= (knight & NOT_FILE_A) << 15;
		attacks |= (knight & NOT_FILE_A) >> 17;

		attacks |= (knight & NOT_FILE_H) << 17;
		attacks |= (knight & NOT_FILE_H) >> 15;

		knightAttacks[sq] = attacks;
	}
}

uint64_t ChessBoard::getKnightMovesB(uint64_t knights, uint64_t ownPieces) const {
	uint64_t moves = 0ULL;
	
	while (knights) {
		int sq = popLSB(knights);
		moves |= knightAttacks[sq] & ~ownPieces;
	}
	return moves;
}

void ChessBoard::initKingAttacks() {
	for (int sq = 0; sq < 64; sq++) {
		uint64_t king = 1ULL << sq;
		uint64_t attacks = 0;

		attacks |= (king & NOT_FILE_H) << 1;
		attacks |= (king & NOT_FILE_A) << 7;
		attacks |= king << 8;
		attacks |= (king & NOT_FILE_H) << 9;

		attacks |= (king & NOT_FILE_A) >> 1;
		attacks |= (king & NOT_FILE_H) >> 7;
		attacks |= king >> 8;
		attacks |= (king & NOT_FILE_A) >> 9;

		kingAttacks[sq] = attacks;
	}
}

uint64_t ChessBoard::getKingMovesB(uint64_t kings, uint64_t ownPieces) const {
	uint64_t moves = 0ULL;

	while (kings) {
		int sq = popLSB(kings);
		moves |= kingAttacks[sq] & ~ownPieces;
	}
	return moves;
}

uint64_t ChessBoard::getRookMovesB(uint64_t rooks, uint64_t ownPieces, uint64_t enemyPieces) const {
	uint64_t moves = 0ULL;

	while (rooks) {
		int sq = popLSB(rooks);
		// NORTH
		for (int s = sq + 8; s < 64; s += 8) {
			uint64_t bb = 1ULL << s;
			if (bb & ownPieces) break;
			moves |= bb;
			if (bb & enemyPieces) break;
		}

		// SOUTH
		for (int s = sq - 8; s >= 0; s -= 8) {
			uint64_t bb = 1ULL << s;
			if (bb & ownPieces) break;
			moves |= bb;
			if (bb & enemyPieces) break;
		}

		// EAST
		for (int s = sq + 1; s < 64 && (s % 8) != 0; s++) {
			uint64_t bb = 1ULL << s;
			if (bb & ownPieces) break;
			moves |= bb;
			if (bb & enemyPieces) break;
		}

		// WEST
		for (int s = sq - 1; s >= 0 && (s % 8) != 7; s--) {
			uint64_t bb = 1ULL << s;
			if (bb & ownPieces) break;
			moves |= bb;
			if (bb & enemyPieces) break;
		}
	}
	return moves;
}

uint64_t ChessBoard::getBishopMovesB(uint64_t bishops, uint64_t ownPieces, uint64_t enemyPieces) const {
	uint64_t moves = 0ULL;

	while (bishops) {
		int sq = popLSB(bishops);
		// NORTHEAST
		for (int s = sq + 9; s < 64 && (s % 8) != 0; s += 9) {
			uint64_t bb = 1ULL << s;
			if (bb & ownPieces) break;
			moves |= bb;
			if (bb & enemyPieces) break;
		}

		// NORTHWEST
		for (int s = sq + 7; s < 64 && (s % 8) != 7; s += 7) {
			uint64_t bb = 1ULL << s;
			if (bb & ownPieces) break;
			moves |= bb;
			if (bb & enemyPieces) break;
		}

		// SOUTHEAST
		for (int s = sq - 7; s >=0 && (s % 8) != 0; s -= 7) {
			uint64_t bb = 1ULL << s;
			if (bb & ownPieces) break;
			moves |= bb;
			if (bb & enemyPieces) break;
		}

		// SOUTHWEST
		for (int s = sq - 9; s >= 0 && (s % 8) != 7; s -= 9) {
			uint64_t bb = 1ULL << s;
			if (bb & ownPieces) break;
			moves |= bb;
			if (bb & enemyPieces) break;
		}
	}
	return moves;
}

uint64_t ChessBoard::getQueenMovesB(uint64_t queens, uint64_t ownPieces, uint64_t enemyPieces) const {
	uint64_t moves = 0ULL;

	moves |= getRookMovesB(queens, ownPieces, enemyPieces);

	moves |= getBishopMovesB(queens, ownPieces, enemyPieces);
	
	return moves;
}

