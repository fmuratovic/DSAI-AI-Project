#pragma once
#include "MoveBB.h"
#include "VisualPiece.h"
#include <array>
#include <vector>
#include <cstdint>
#include <cassert>

#ifdef _MSC_VER
#include <intrin.h>
inline int popcount64(uint64_t x) {
    return (int)__popcnt64(x);  // MSVC intrinsic
}
#else
inline int popcount64(uint64_t x) {
    return __builtin_popcountll(x); // GCC/Clang
}
#endif

class ChessBoard {
public:

    // --- Piece enum ---
    /*enum Piece {
        EMPTY, WPAWN, WKNIGHT, WBISHOP, WROOK, WQUEEN, WKING,
        BPAWN, BKNIGHT, BBISHOP, BROOK, BQUEEN, BKING
    };*/

    // --- Board representation ---

    // Constructor
    ChessBoard();

    // --- Printing ---
    void printBitboards() const;

    // --- Move generation ---
    bool isKingInCheckBB(bool isWhite);
    bool isSquareAttacked(int square, bool byWhite) const;


    // --- Bitboard helpers ---
    uint64_t getWhitePieces() const;
    uint64_t getBlackPieces() const;
    uint64_t getAllPieces() const;

    // --- Pawn moves ---
    uint64_t getPawnMovesB(uint64_t pawns, uint64_t emptySquares, bool isWhite) const;
    uint64_t getWhitePawnCaptures(uint64_t pawns, uint64_t blackPieces) const;
    uint64_t getBlackPawnCaptures(uint64_t pawns, uint64_t whitePieces) const;

    // --- Knight moves ---
    void initKnightAttacks();
    uint64_t getKnightMovesB(uint64_t knights, uint64_t ownPieces) const;
    static uint64_t knightAttacks[64];

    // --- Sliding piece moves ---
    uint64_t getBishopMovesB(uint64_t bishops, uint64_t ownPieces, uint64_t enemyPieces) const;
    uint64_t getRookMovesB(uint64_t rooks, uint64_t ownPieces, uint64_t enemyPieces) const;
    uint64_t getQueenMovesB(uint64_t queens, uint64_t ownPieces, uint64_t enemyPieces) const;

    // --- King moves ---
    void initKingAttacks();
    uint64_t getKingMovesB(uint64_t kings, uint64_t ownPieces) const;
    static uint64_t kingAttacks[64];

    // --- Access bitboards ---
    uint64_t& getPieceBitboard(Piece p);

    // --- Bitboard operations ---
    void makeMoveBB(const MoveBB& m);
    void undoMoveBB(const MoveBB& m);
    inline Piece getCapturedPieceAt(int to, bool isWhite) const;
    void fillCastlingState(MoveBB& m);
    std::vector<MoveBB> getAllMovesBB(bool isWhite);
    std::vector<MoveBB> getLegalMovesBB(bool isWhite);
    bool pawnAttacksKingBB(int kingSquare, bool isWhite) const;

    // --- Evaluation & search ---
    int evaluateBoardBB() const;
    int negamaxBB(int depth, int alpha, int beta, bool isWhite);
    MoveBB findBestMoveBB(int depth, bool isWhite);


    bool getSideToMove() const { return sideToMove; };

    Piece pieceAt(int sq) const;

	// --- Visualization ---
    //std::vector<VisualPiece> getVisualPieces() const;
    //std::vector<VisualPiece> makeVisualPieces(gui::Image* images[12]) const;

    static int popLSB(uint64_t& bb);

private:
    static constexpr uint64_t FILE_A = 0x0101010101010101ULL;
    static constexpr uint64_t FILE_B = 0x0202020202020202ULL;
    static constexpr uint64_t FILE_G = 0x4040404040404040ULL;
    static constexpr uint64_t FILE_H = 0x8080808080808080ULL;
    static constexpr uint64_t NOT_FILE_A = ~FILE_A;
    static constexpr uint64_t NOT_FILE_AB = ~(FILE_A | FILE_B);
    static constexpr uint64_t NOT_FILE_H = ~FILE_H;
    static constexpr uint64_t NOT_FILE_GH = ~(FILE_G | FILE_H);
    static constexpr uint64_t NOT_FILE_B = ~FILE_B;
    static constexpr uint64_t NOT_FILE_G = ~FILE_G;
    static constexpr uint64_t RANK_1 = 0x00000000000000FFULL;
    static constexpr uint64_t RANK_2 = 0x000000000000FF00ULL;
    static constexpr uint64_t RANK_7 = 0x00FF000000000000ULL;
    static constexpr uint64_t RANK_8 = 0xFF00000000000000ULL;

    bool canWhiteCastleKingSide = true;
    bool canWhiteCastleQueenSide = true;
    bool canBlackCastleKingSide = true;
    bool canBlackCastleQueenSide = true;
 


    // --- Bitboards ---
    uint64_t whitePawns, whiteKnights, whiteBishops, whiteRooks, whiteQueen, whiteKing;
    uint64_t blackPawns, blackKnights, blackBishops, blackRooks, blackQueen, blackKing;

    int whiteKingSquare;
    int blackKingSquare;

    bool sideToMove = true;
};
