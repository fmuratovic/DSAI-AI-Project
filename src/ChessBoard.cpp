#include "ChessBoard.h"
#include <iostream>
#include <cassert>
#define NOMINMAX

#include <sstream>

// =====================
// Static tables
// =====================
uint64_t ChessBoard::knightAttacks[64] = {};
uint64_t ChessBoard::kingAttacks[64] = {};
int _enPassantSq = -1;  // -1 means no en passant available

// =====================
// Constructor
// =====================
ChessBoard::ChessBoard() {

    sideToMove = true;

    whitePawns = 0x000000000000FF00ULL;
    blackPawns = 0x00FF000000000000ULL;

    whiteRooks = 0x0000000000000081ULL;
    blackRooks = 0x8100000000000000ULL;

    whiteKnights = 0x0000000000000042ULL;
    blackKnights = 0x4200000000000000ULL;

    whiteBishops = 0x0000000000000024ULL;
    blackBishops = 0x2400000000000000ULL;

    whiteQueen = 0x0000000000000008ULL;
    blackQueen = 0x0800000000000000ULL;

    whiteKing = 0x0000000000000010ULL;
    blackKing = 0x1000000000000000ULL;

    whiteKingSquare = 4;
    blackKingSquare = 60;

    initKnightAttacks();
    initKingAttacks();
}

// =====================
// Debug printing
// =====================
void ChessBoard::printBitboards() const {
    auto printBB = [](uint64_t bb) {
        for (int i = 0; i < 64; i++) {
            std::cout << ((bb >> i) & 1) << " ";
            if ((i + 1) % 8 == 0) std::cout << "\n";
        }
        std::cout << "\n";
        };

    std::cout << "White Pawns:\n"; printBB(whitePawns);
    std::cout << "Black Pawns:\n"; printBB(blackPawns);
}

// =====================
// Bitboard getters
// =====================
uint64_t ChessBoard::getWhitePieces() const {
    return whitePawns | whiteKnights | whiteBishops |
        whiteRooks | whiteQueen | whiteKing;
}

uint64_t ChessBoard::getBlackPieces() const {
    return blackPawns | blackKnights | blackBishops |
        blackRooks | blackQueen | blackKing;
}

uint64_t ChessBoard::getAllPieces() const {
    return getWhitePieces() | getBlackPieces();
}

// =====================
// popLSB (SAFE)
// =====================
int ChessBoard::popLSB(uint64_t& bb) {
    if (bb == 0) return -1;

#ifdef _MSC_VER
    unsigned long index;
    _BitScanForward64(&index, bb);
    bb &= bb - 1;
    return (int)index;
#else
    int index = __builtin_ctzll(bb);
    bb &= bb - 1;
    return index;
#endif
}

// =====================
// Piece → bitboard mapping
// =====================
uint64_t& ChessBoard::getPieceBitboard(Piece p) {
    switch (p) {
    case WPAWN:   return whitePawns;
    case WKNIGHT: return whiteKnights;
    case WBISHOP: return whiteBishops;
    case WROOK:   return whiteRooks;
    case WQUEEN:  return whiteQueen;
    case WKING:   return whiteKing;

    case BPAWN:   return blackPawns;
    case BKNIGHT: return blackKnights;
    case BBISHOP: return blackBishops;
    case BROOK:   return blackRooks;
    case BQUEEN:  return blackQueen;
    case BKING:   return blackKing;

    default:
        //assert(false && "Invalid Piece enum");
        return whitePawns; // never reached
    }
}

// =====================
// Make / undo move
// =====================
void ChessBoard::makeMoveBB(const MoveBB& m)
{


    uint64_t fromBB = 1ULL << m.from;
    uint64_t toBB = 1ULL << m.to;

    Piece piece = pieceAt(m.from);



    assert(piece != EMPTY);

    bool isWhite = (m.piece <= WKING);

    uint64_t& bb = getPieceBitboard(m.piece);

    assert((bb & fromBB) && "makeMoveBB: piece not on FROM square");

    // 0️⃣ Save castling rights in MoveBB (already done in getAllMovesBB)
    // m.prevWCK, m.prevWCQ, m.prevBCK, m.prevBCQ

    // 1️⃣ Remove moving piece from source
    getPieceBitboard(m.piece) &= ~fromBB;

    // 2️⃣ Handle captures
    if (m.flags & EN_PASSANT)
    {
        if (isWhite)
            blackPawns &= ~(1ULL << (m.to - 8));
        else
            whitePawns &= ~(1ULL << (m.to + 8));
    }
    else if (m.captured != EMPTY)
    {
        getPieceBitboard(m.captured) &= ~toBB;
    }

    // 3️⃣ Promotion or normal move
    if (m.promotion)
        getPieceBitboard(m.promotionPiece) |= toBB;
    else
        getPieceBitboard(m.piece) |= toBB;

    // 4️⃣ Update king square
    if (m.piece == WKING) whiteKingSquare = m.to;
    if (m.piece == BKING) blackKingSquare = m.to;

    // 5️⃣ Castling rook move
    if (m.flags & CASTLING)
    {
        // White
        if (m.to == 6) { whiteRooks &= ~0x80ULL; whiteRooks |= 0x20ULL; } // h1 → f1
        if (m.to == 2) { whiteRooks &= ~0x01ULL; whiteRooks |= 0x08ULL; } // a1 → d1
        // Black
        if (m.to == 62) { blackRooks &= ~(1ULL << 63); blackRooks |= (1ULL << 61); }
        if (m.to == 58) { blackRooks &= ~(1ULL << 56); blackRooks |= (1ULL << 59); }
    }

    // 6️⃣ Update castling rights
    // 6️⃣ Update castling rights
    if (m.piece == WKING) {
        canWhiteCastleKingSide = false;
        canWhiteCastleQueenSide = false;
    }
    if (m.piece == BKING) {
        canBlackCastleKingSide = false;
        canBlackCastleQueenSide = false;
    }
    if (m.piece == WROOK) {
        if (m.from == 0) canWhiteCastleQueenSide = false; // a1 rook
        if (m.from == 7) canWhiteCastleKingSide = false;  // h1 rook
    }
    if (m.piece == BROOK) {
        if (m.from == 56) canBlackCastleQueenSide = false; // a8 rook
        if (m.from == 63) canBlackCastleKingSide = false;  // h8 rook
    }

    // Also if a rook is captured
    if (m.captured == WROOK) {
        if (m.to == 0) canWhiteCastleQueenSide = false;
        if (m.to == 7) canWhiteCastleKingSide = false;
    }
    if (m.captured == BROOK) {
        if (m.to == 56) canBlackCastleQueenSide = false;
        if (m.to == 63) canBlackCastleKingSide = false;
    }

    // 8️⃣ Update en passant square
    _enPassantSq = -1;
    if (m.flags & DOUBLE_PAWN)
        _enPassantSq = (m.from + m.to) / 2;

    // 7️⃣ Switch side
    sideToMove = !sideToMove;
}


void ChessBoard::undoMoveBB(const MoveBB& m)
{
    uint64_t fromBB = 1ULL << m.from;
    uint64_t toBB = 1ULL << m.to;

    bool isWhite = (m.piece <= WKING);

    // 0️⃣ Restore side to move
    sideToMove = !sideToMove;

    // 1️⃣ Undo promotion or normal move
    if (m.promotion)
    {
        getPieceBitboard(m.promotionPiece) &= ~toBB;
        getPieceBitboard(m.piece) |= fromBB;
    }
    else
    {
        getPieceBitboard(m.piece) &= ~toBB;
        getPieceBitboard(m.piece) |= fromBB;
    }

    // 2️⃣ Restore captured piece
    if (m.flags & EN_PASSANT)
    {
        if (isWhite)
            blackPawns |= (1ULL << (m.to - 8));
        else
            whitePawns |= (1ULL << (m.to + 8));
    }
    else if (m.captured != EMPTY)
    {
        getPieceBitboard(m.captured) |= toBB;
    }

    // 3️⃣ Restore king square
    if (m.piece == WKING) whiteKingSquare = m.from;
    if (m.piece == BKING) blackKingSquare = m.from;

    // 4️⃣ Undo castling rook move
    if (m.flags & CASTLING)
    {
        // White
        if (m.to == 6) { whiteRooks &= ~(1ULL << 5); whiteRooks |= (1ULL << 7); } // f1 → h1
        if (m.to == 2) { whiteRooks &= ~(1ULL << 3); whiteRooks |= (1ULL << 0); } // d1 → a1
        // Black
        if (m.to == 62) { blackRooks &= ~(1ULL << 61); blackRooks |= (1ULL << 63); }
        if (m.to == 58) { blackRooks &= ~(1ULL << 59); blackRooks |= (1ULL << 56); }
    }

    // 5️⃣ Restore castling rights
    canWhiteCastleKingSide = m.prevWCK;
    canWhiteCastleQueenSide = m.prevWCQ;
    canBlackCastleKingSide = m.prevBCK;
    canBlackCastleQueenSide = m.prevBCQ;
}


// =====================
// Move generation
// =====================


inline Piece ChessBoard::getCapturedPieceAt(int to, bool isWhite) const {
    uint64_t toBB = 1ULL << to;

    if (toBB & (isWhite ? blackPawns : whitePawns))
        return isWhite ? BPAWN : WPAWN;
    if (toBB & (isWhite ? blackKnights : whiteKnights))
        return isWhite ? BKNIGHT : WKNIGHT;
    if (toBB & (isWhite ? blackBishops : whiteBishops))
        return isWhite ? BBISHOP : WBISHOP;
    if (toBB & (isWhite ? blackRooks : whiteRooks))
        return isWhite ? BROOK : WROOK;
    if (toBB & (isWhite ? blackQueen : whiteQueen))
        return isWhite ? BQUEEN : WQUEEN;

    return EMPTY;
}

void ChessBoard::fillCastlingState(MoveBB& m) {
    m.prevWCK = canWhiteCastleKingSide;
    m.prevWCQ = canWhiteCastleQueenSide;
    m.prevBCK = canBlackCastleKingSide;
    m.prevBCQ = canBlackCastleQueenSide;
}

std::vector<MoveBB> ChessBoard::getAllMovesBB(bool isWhite) {
    std::vector<MoveBB> moves;

    uint64_t own = isWhite ? getWhitePieces() : getBlackPieces();
    uint64_t enemy = isWhite ? getBlackPieces() : getWhitePieces();
    uint64_t all = own | enemy;
    uint64_t empty = ~all;

    uint64_t pawns = isWhite ? whitePawns : blackPawns;
    uint64_t pawnsCopy = pawns;

    // --- PAWN LOGIC (Fixed to avoid guessing 'from' square) ---
    while (pawnsCopy) {
        int from = popLSB(pawnsCopy);
        uint64_t fromBB = 1ULL << from;

        // 1. Single Push
        uint64_t push = isWhite ? (fromBB << 8) & empty : (fromBB >> 8) & empty;
        if (push) {
            int to = isWhite ? from + 8 : from - 8;
            MoveBB m(from, to, isWhite ? WPAWN : BPAWN);
            fillCastlingState(m); // Helper to avoid repetition
            moves.push_back(m);
        }
        // 2. Double Push (only from starting rank)
        if (push) {  // single push must be possible first
            if (isWhite && (from >= 8 && from <= 15)) {  // rank 2
                uint64_t doublePush = (fromBB << 16) & empty;
                if (doublePush) {
                    MoveBB m(from, from + 16, WPAWN, EMPTY, false, EMPTY, DOUBLE_PAWN);
                    fillCastlingState(m);
                    moves.push_back(m);
                }
            }
            else if (!isWhite && (from >= 48 && from <= 55)) {  // rank 7
                uint64_t doublePush = (fromBB >> 16) & empty;
                if (doublePush) {
                    MoveBB m(from, from - 16, BPAWN, EMPTY, false, EMPTY, DOUBLE_PAWN);
                    fillCastlingState(m);
                    moves.push_back(m);
                }
            }
        }

        // 2. Captures
        uint64_t captures = isWhite ? getWhitePawnCaptures(fromBB, enemy)
            : getBlackPawnCaptures(fromBB, enemy);
        while (captures) {
            int to = popLSB(captures);
            Piece captured = getCapturedPieceAt(to, isWhite);
            MoveBB m(from, to, isWhite ? WPAWN : BPAWN, captured);
            fillCastlingState(m);
            moves.push_back(m);
        }

        // 3. En Passant
        if (_enPassantSq != -1) {
            uint64_t epBB = 1ULL << _enPassantSq;
            uint64_t epCaptures = isWhite ? getWhitePawnCaptures(fromBB, epBB)
                : getBlackPawnCaptures(fromBB, epBB);
            while (epCaptures) {
                int to = popLSB(epCaptures);
                Piece captured = isWhite ? BPAWN : WPAWN;
                MoveBB m(from, to, isWhite ? WPAWN : BPAWN, captured, false, EMPTY, EN_PASSANT);
                fillCastlingState(m);
                moves.push_back(m);
            }
        }
    }


    // --- KNIGHTS ---
    uint64_t knightsCopy = isWhite ? whiteKnights : blackKnights;
    while (knightsCopy) {
        int from = popLSB(knightsCopy);
        uint64_t attacks = knightAttacks[from] & ~own;
        while (attacks) {
            int to = popLSB(attacks);
            Piece captured = ((1ULL << to) & enemy) ? getCapturedPieceAt(to, isWhite) : EMPTY;
            MoveBB m(from, to, isWhite ? WKNIGHT : BKNIGHT, captured);
            fillCastlingState(m);
            moves.push_back(m);
        }
    }

    // --- BISHOPS / QUEENS / ROOKS (Sliding Pieces) ---
    // Note: You should add Rooks here too if they aren't in another section
    auto processSliders = [&](uint64_t bitboard, Piece pType, auto moveFunc) {
        while (bitboard) {
            int from = popLSB(bitboard);
            uint64_t attacks = moveFunc(1ULL << from, own, enemy);
            while (attacks) {
                int to = popLSB(attacks);
                Piece captured = ((1ULL << to) & enemy) ? getCapturedPieceAt(to, isWhite) : EMPTY;
                MoveBB m(from, to, pType, captured);
                fillCastlingState(m);
                moves.push_back(m);
            }
        }
        };

    processSliders(isWhite ? whiteBishops : blackBishops, isWhite ? WBISHOP : BBISHOP,
        [this](uint64_t b, uint64_t o, uint64_t e) { return getBishopMovesB(b, o, e); });

    processSliders(isWhite ? whiteQueen : blackQueen, isWhite ? WQUEEN : BQUEEN,
        [this](uint64_t b, uint64_t o, uint64_t e) { return getQueenMovesB(b, o, e); });

    // --- KING ---
    uint64_t kingBit = isWhite ? whiteKing : blackKing;
    if (kingBit) {
        int from = popLSB(kingBit);
        uint64_t attacks = getKingMovesB(1ULL << from, own);
        while (attacks) {
            int to = popLSB(attacks);
            Piece captured = ((1ULL << to) & enemy) ? getCapturedPieceAt(to, isWhite) : EMPTY;
            MoveBB m(from, to, isWhite ? WKING : BKING, captured);
            fillCastlingState(m);
            moves.push_back(m);
        }

        // --- CASTLING (Standard Squares) ---
        if (isWhite) {
            if (canWhiteCastleKingSide && !(all & 0x60ULL) && !isSquareAttacked(4, false) && !isSquareAttacked(5, false) && !isSquareAttacked(6, false)) {
                MoveBB m(4, 6, WKING, EMPTY, false, EMPTY, CASTLING);
                fillCastlingState(m);
                moves.push_back(m);
            }
            if (canWhiteCastleQueenSide && !(all & 0x0EULL) && !isSquareAttacked(4, false) && !isSquareAttacked(3, false) && !isSquareAttacked(2, false)) {
                MoveBB m(4, 2, WKING, EMPTY, false, EMPTY, CASTLING);
                fillCastlingState(m);
                moves.push_back(m);
            }
        }
        else {
            if (canBlackCastleKingSide && !(all & 0x6000000000000000ULL) && !isSquareAttacked(60, true) && !isSquareAttacked(61, true) && !isSquareAttacked(62, true)) {
                MoveBB m(60, 62, BKING, EMPTY, false, EMPTY, CASTLING);
                fillCastlingState(m);
                moves.push_back(m);
            }
            if (canBlackCastleQueenSide && !(all & 0x0E00000000000000ULL) && !isSquareAttacked(60, true) && !isSquareAttacked(59, true) && !isSquareAttacked(58, true)) {
                MoveBB m(60, 58, BKING, EMPTY, false, EMPTY, CASTLING);
                fillCastlingState(m);
                moves.push_back(m);
            }
        }
    }

    return moves;
}


// =====================
// Legal moves
// =====================
std::vector<MoveBB> ChessBoard::getLegalMovesBB(bool isWhite) {
    std::vector<MoveBB> legal;
    auto moves = getAllMovesBB(isWhite);

    for (const auto& m : moves) {
        makeMoveBB(m);
        if (!isKingInCheckBB(isWhite))
            legal.push_back(m);
        undoMoveBB(m);
    }
    return legal;
}

// =====================
// Evaluation
// =====================
int ChessBoard::evaluateBoardBB() const {
    uint64_t whitePieces = getWhitePieces();
    uint64_t blackPieces = getBlackPieces();
    //assert((whiteBishops & blackBishops) == 0);
    //assert((whitePawns & blackPawns) == 0);
    //assert((whitePieces & blackPieces) == 0);

    return
        100 * (popcount64(whitePawns) - popcount64(blackPawns)) +
        320 * (popcount64(whiteKnights) - popcount64(blackKnights)) +
        330 * (popcount64(whiteBishops) - popcount64(blackBishops)) +
        500 * (popcount64(whiteRooks) - popcount64(blackRooks)) +
        900 * (popcount64(whiteQueen) - popcount64(blackQueen));
}

// =====================
// Negamax
// =====================
int ChessBoard::negamaxBB(int depth, int alpha, int beta, bool isWhite) {
    if (depth == 0)
        return isWhite ? evaluateBoardBB() : -evaluateBoardBB();

    int best = -1000000;
    auto moves = getLegalMovesBB(isWhite);

    for (const auto& m : moves) {
        makeMoveBB(m);
        int score = -negamaxBB(depth - 1, -beta, -alpha, !isWhite);
        undoMoveBB(m);

        best = std::max(best, score);
        alpha = std::max(alpha, score);
        if (alpha >= beta) break;
    }

    return best;
}

// =====================
// Best move
// =====================
MoveBB ChessBoard::findBestMoveBB(int depth, bool isWhite) {
    MoveBB best;
    bool found = false;
    int bestScore = -1000000;

    auto moves = getLegalMovesBB(isWhite);

    // Debug: Print number of legal moves
    std::cout << "findBestMoveBB: " << moves.size() << " legal moves" << std::endl;

    if (moves.empty()) {
        std::cout << "WARNING: No legal moves found!" << std::endl;
        // Return a dummy move instead of asserting
        return MoveBB(0, 0, isWhite ? WPAWN : BPAWN);
    }

    for (const auto& m : moves) {
        makeMoveBB(m);
        int score = -negamaxBB(depth - 1, -1000000, 1000000, !isWhite);
        undoMoveBB(m);

        if (!found || score > bestScore) {
            bestScore = score;
            best = m;
            found = true;
        }
    }

    // Comment out the assertion
    // assert(found && "No legal moves found");
    return best;
}

Piece ChessBoard::pieceAt(int sq) const
{
    uint64_t b = 1ULL << sq;

    if (b & whitePawns)   return WPAWN;
    if (b & whiteKnights) return WKNIGHT;
    if (b & whiteBishops) return WBISHOP;
    if (b & whiteRooks)   return WROOK;
    if (b & whiteQueen)   return WQUEEN;
    if (b & whiteKing)    return WKING;

    if (b & blackPawns)   return BPAWN;
    if (b & blackKnights) return BKNIGHT;
    if (b & blackBishops) return BBISHOP;
    if (b & blackRooks)   return BROOK;
    if (b & blackQueen)   return BQUEEN;
    if (b & blackKing)    return BKING;

    return EMPTY;
}


// =====================
// Visualisation
// =====================

//std::vector<VisualPiece> ChessBoard::getVisualPieces() const {
//    std::vector<VisualPiece> visuals;
//
//    for (int sq = 0; sq < 64; sq++) {
//        uint64_t bb = 1ULL << sq;
//
//        if (bb & whitePawns)   visuals.push_back({ WPAWN, sq });
//        if (bb & whiteKnights) visuals.push_back({ WKNIGHT, sq });
//        if (bb & whiteBishops) visuals.push_back({ WBISHOP, sq });
//        if (bb & whiteRooks)   visuals.push_back({ WROOK, sq });
//        if (bb & whiteQueen)   visuals.push_back({ WQUEEN, sq });
//        if (bb & whiteKing)    visuals.push_back({ WKING, sq });
//
//        if (bb & blackPawns)   visuals.push_back({ BPAWN, sq });
//        if (bb & blackKnights) visuals.push_back({ BKNIGHT, sq });
//        if (bb & blackBishops) visuals.push_back({ BBISHOP, sq });
//        if (bb & blackRooks)   visuals.push_back({ BROOK, sq });
//        if (bb & blackQueen)   visuals.push_back({ BQUEEN, sq });
//        if (bb & blackKing)    visuals.push_back({ BKING, sq });
//    }
//
//    return visuals;
//}

// ChessBoard.cpp
/*std::vector<VisualPiece> ChessBoard::makeVisualPieces(gui::Image* images[12]) const {
    std::vector<VisualPiece> visuals;
    const gui::CoordType SQUARE_SIZE = 80.0;

    for (int sq = 0; sq < 64; sq++) {
        uint64_t bb = 1ULL << sq;

        auto addPiece = [&](Piece p) {
            gui::Point pos((sq % 8) * SQUARE_SIZE, (7 - sq / 8) * SQUARE_SIZE);
            visuals.emplace_back(this, p, sq, images[p], pos);
            };

        if (bb & whitePawns)   addPiece(WPAWN);
        if (bb & whiteKnights) addPiece(WKNIGHT);
        if (bb & whiteBishops) addPiece(WBISHOP);
        if (bb & whiteRooks)   addPiece(WROOK);
        if (bb & whiteQueen)   addPiece(WQUEEN);
        if (bb & whiteKing)    addPiece(WKING);

        if (bb & blackPawns)   addPiece(BPAWN);
        if (bb & blackKnights) addPiece(BKNIGHT);
        if (bb & blackBishops) addPiece(BBISHOP);
        if (bb & blackRooks)   addPiece(BROOK);
        if (bb & blackQueen)   addPiece(BQUEEN);
        if (bb & blackKing)    addPiece(BKING);
    }

    return visuals;
}*/




