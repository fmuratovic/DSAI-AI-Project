#include "ChessBoard.h"
#include <iostream>
#include <cassert>
#define NOMINMAX

#include <sstream>
#include <thread>
#include <atomic>
#include <algorithm>
#include <random>
#include <chrono>
#include <cstdlib>

// =====================
// Static tables
// =====================
uint64_t ChessBoard::knightAttacks[64] = {};
uint64_t ChessBoard::kingAttacks[64] = {};
// NOTE: there used to be a file-scope `int _enPassantSq = -1;` here with
// the same name as the ChessBoard member. Inside member functions the
// member always won name lookup, so this global was dead -- but it had
// external linkage and would collide with any other translation unit
// defining the same symbol. Removed; the real state lives in
// ChessBoard::_enPassantSq (see ChessBoard.h).

// =====================
// Zobrist hashing (for the transposition table)
// =====================
// A Zobrist hash is a 64-bit fingerprint of the position: one random
// number per (piece, square) combination, XORed together for every piece
// on the board, plus random numbers for side-to-move, castling rights and
// the en-passant file. XOR is its own inverse, which is what makes this
// scheme cheap to maintain incrementally: makeMoveBB updates the hash with
// a handful of XORs as it changes state, and undoMoveBB reverses those
// same XORs (applying an XOR twice cancels it out), so there is no need
// to recompute the hash from scratch on every node.
namespace {
    uint64_t g_pieceKeys[13][64]; // index 0 (EMPTY) unused
    uint64_t g_sideKey;
    uint64_t g_castlingKeys[4]; // WK, WQ, BK, BQ
    uint64_t g_enPassantFileKeys[8];

    void initZobristKeys() {
        std::mt19937_64 rng(0x9E3779B97F4A7C15ULL); // fixed seed: reproducible runs
        for (int p = 0; p < 13; p++)
            for (int sq = 0; sq < 64; sq++)
                g_pieceKeys[p][sq] = rng();

        g_sideKey = rng();
        for (int i = 0; i < 4; i++) g_castlingKeys[i] = rng();
        for (int i = 0; i < 8; i++) g_enPassantFileKeys[i] = rng();
    }
}

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

    zobristHash = computeZobristHash(); // one-time full computation

    // Seed repetition history with the initial position.
    _posHistoryCount = 0;
    _undoTop = 0;
    _halfmoveClock = 0;
    _posHistory[_posHistoryCount++] = zobristHash;
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

    // Same portability situation as popcount64: _BitScanForward64 is MSVC
    // x86-64/ARM64 only, so guard it properly and keep a plain-C++
    // fallback (de Bruijn multiplication) for every other target.
#if defined(__GNUC__) || defined(__clang__)
    int index = __builtin_ctzll(bb);
    bb &= bb - 1;
    return index;
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_AMD64) || defined(_M_ARM64))
    unsigned long index;
    _BitScanForward64(&index, bb);
    bb &= bb - 1;
    return (int)index;
#else
    static const int deBruijnTable[64] = {
         0,  1, 48,  2, 57, 49, 28,  3, 61, 58, 50, 42, 38, 29, 17,  4,
        62, 55, 59, 36, 53, 51, 43, 22, 45, 39, 33, 30, 24, 18, 12,  5,
        63, 47, 56, 27, 60, 41, 37, 16, 54, 35, 52, 21, 44, 32, 23, 11,
        46, 26, 40, 15, 34, 20, 31, 10, 25, 14, 19,  9, 13,  8,  7,  6
    };
    uint64_t lsb = bb & (~bb + 1); // isolate lowest set bit
    int index = deBruijnTable[(lsb * 0x03F79D71B4CB0A89ULL) >> 58];
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
    // C++11 magic statics guarantee the Zobrist key tables are ready
    // before this runs, even on the very first call.
    static bool zobristReady = (initZobristKeys(), true);
    (void)zobristReady;

    uint64_t delta = 0; // accumulates every hash change this move makes

    uint64_t fromBB = 1ULL << m.from;
    uint64_t toBB = 1ULL << m.to;

    Piece piece = pieceAt(m.from);




    assert(piece != EMPTY);

    bool isWhite = (m.piece <= WKING);

    uint64_t& bb = getPieceBitboard(m.piece);

    assert((bb & fromBB) && "makeMoveBB: piece not on FROM square");

    // 0️⃣ Save castling rights in MoveBB (already done in getAllMovesBB)
    // m.prevWCK, m.prevWCQ, m.prevBCK, m.prevBCQ

    // Snapshot castling rights & en passant BEFORE this move changes them,
    // so we can compute exactly what flipped for the hash delta below.
    bool oldWCK = canWhiteCastleKingSide, oldWCQ = canWhiteCastleQueenSide;
    bool oldBCK = canBlackCastleKingSide, oldBCQ = canBlackCastleQueenSide;
    int  oldEnPassantSq = _enPassantSq;

    // Record the true previous en-passant square so undoMoveBB can
    // restore it correctly. (Previously undoMoveBB just hardcoded -1
    // here, which silently discarded en-passant availability whenever a
    // move was undone -- a pre-existing correctness bug independent of
    // the hash work below, caught because it also desynced the hash.)
    m.prevEnPassantSq = static_cast<int8_t>(oldEnPassantSq);

    // 1️⃣ Remove moving piece from source
    getPieceBitboard(m.piece) &= ~fromBB;
    delta ^= g_pieceKeys[m.piece][m.from];

    // 2️⃣ Handle captures
    if (m.flags & EN_PASSANT)
    {
        if (isWhite) {
            int capSq = m.to - 8;
            blackPawns &= ~(1ULL << capSq);
            delta ^= g_pieceKeys[BPAWN][capSq];
        } else {
            int capSq = m.to + 8;
            whitePawns &= ~(1ULL << capSq);
            delta ^= g_pieceKeys[WPAWN][capSq];
        }
    }
    else if (m.captured != EMPTY)
    {
        getPieceBitboard(m.captured) &= ~toBB;
        delta ^= g_pieceKeys[m.captured][m.to];
    }

    // 3️⃣ Promotion or normal move
    if (m.promotion) {
        getPieceBitboard(m.promotionPiece) |= toBB;
        delta ^= g_pieceKeys[m.promotionPiece][m.to];
    } else {
        getPieceBitboard(m.piece) |= toBB;
        delta ^= g_pieceKeys[m.piece][m.to];
    }

    // 4️⃣ Update king square
    if (m.piece == WKING) whiteKingSquare = m.to;
    if (m.piece == BKING) blackKingSquare = m.to;

    // 5️⃣ Castling rook move
    if (m.flags & CASTLING)
    {
        // White
        if (m.to == 6) {
            whiteRooks &= ~0x80ULL; whiteRooks |= 0x20ULL; // h1 → f1
            delta ^= g_pieceKeys[WROOK][7] ^ g_pieceKeys[WROOK][5];
        }
        if (m.to == 2) {
            whiteRooks &= ~0x01ULL; whiteRooks |= 0x08ULL; // a1 → d1
            delta ^= g_pieceKeys[WROOK][0] ^ g_pieceKeys[WROOK][3];
        }
        // Black
        if (m.to == 62) {
            blackRooks &= ~(1ULL << 63); blackRooks |= (1ULL << 61);
            delta ^= g_pieceKeys[BROOK][63] ^ g_pieceKeys[BROOK][61];
        }
        if (m.to == 58) {
            blackRooks &= ~(1ULL << 56); blackRooks |= (1ULL << 59);
            delta ^= g_pieceKeys[BROOK][56] ^ g_pieceKeys[BROOK][59];
        }
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

    // Fold castling-rights changes into the hash: XOR out any right that
    // was true and is now false. A right that stays the same contributes
    // nothing (XORing it out then back in cancels out).
    if (oldWCK && !canWhiteCastleKingSide)  delta ^= g_castlingKeys[0];
    if (oldWCQ && !canWhiteCastleQueenSide) delta ^= g_castlingKeys[1];
    if (oldBCK && !canBlackCastleKingSide)  delta ^= g_castlingKeys[2];
    if (oldBCQ && !canBlackCastleQueenSide) delta ^= g_castlingKeys[3];

    // 8️⃣ Update en passant square

    _enPassantSq = -1;
    if (m.flags & DOUBLE_PAWN)
        _enPassantSq = (m.from + m.to) / 2;

    // Fold en-passant-square change into the hash.
    if (oldEnPassantSq != -1) delta ^= g_enPassantFileKeys[oldEnPassantSq & 7];
    if (_enPassantSq   != -1) delta ^= g_enPassantFileKeys[_enPassantSq & 7];

    // 7️⃣ Switch side
    sideToMove = !sideToMove;
    delta ^= g_sideKey; // side toggles on every move, so this key always flips

    zobristHash ^= delta;

    // --- Draw-rule bookkeeping ---
    // Save the previous values so undoMoveBB can restore them exactly.
    if (_undoTop < MAX_HISTORY)
        _undoStack[_undoTop++] = { (int16_t)_halfmoveClock, (int16_t)_posHistoryCount };

    // A capture or any pawn move is IRREVERSIBLE: it resets the fifty-move
    // counter, and no earlier position can ever recur afterwards, so the
    // repetition history is cleared too (which also keeps isRepetition()
    // cheap, since it only ever scans back to the last such move).
    const bool isPawnMove = (m.piece == WPAWN || m.piece == BPAWN);
    const bool isCapture = (m.captured != EMPTY) || (m.flags & EN_PASSANT);
    if (isPawnMove || isCapture) {
        _halfmoveClock = 0;
        _posHistoryCount = 0;
    } else {
        _halfmoveClock++;
    }

    // Record the position we just arrived at.
    if (_posHistoryCount < MAX_HISTORY)
        _posHistory[_posHistoryCount++] = zobristHash;
}


void ChessBoard::undoMoveBB(const MoveBB& m)
{
    // Recompute the same hash delta that makeMoveBB applied, then XOR it
    // in again to cancel it out. Every term below pairs the exact same
    // two Zobrist keys as the matching step in makeMoveBB, so the value
    // computed here is numerically identical to what makeMoveBB computed
    // -- no need to cache it on the move object.
    uint64_t delta = 0;

    uint64_t fromBB = 1ULL << m.from;
    uint64_t toBB = 1ULL << m.to;

    bool isWhite = (m.piece <= WKING);

    // Castling rights & en passant as they stand right now (i.e. the
    // "after move" state we're about to revert away from).
    bool beforeWCK = canWhiteCastleKingSide, beforeWCQ = canWhiteCastleQueenSide;
    bool beforeBCK = canBlackCastleKingSide, beforeBCQ = canBlackCastleQueenSide;
    int  beforeEnPassantSq = _enPassantSq;

    delta ^= g_pieceKeys[m.piece][m.from];

    if (m.flags & EN_PASSANT) {
        int capSq = isWhite ? (m.to - 8) : (m.to + 8);
        delta ^= g_pieceKeys[isWhite ? BPAWN : WPAWN][capSq];
    } else if (m.captured != EMPTY) {
        delta ^= g_pieceKeys[m.captured][m.to];
    }

    delta ^= g_pieceKeys[m.promotion ? m.promotionPiece : m.piece][m.to];

    if (m.flags & CASTLING) {
        if (m.to == 6)  delta ^= g_pieceKeys[WROOK][7]  ^ g_pieceKeys[WROOK][5];
        if (m.to == 2)  delta ^= g_pieceKeys[WROOK][0]  ^ g_pieceKeys[WROOK][3];
        if (m.to == 62) delta ^= g_pieceKeys[BROOK][63] ^ g_pieceKeys[BROOK][61];
        if (m.to == 58) delta ^= g_pieceKeys[BROOK][56] ^ g_pieceKeys[BROOK][59];
    }

    // 0️⃣ Restore side to move
    sideToMove = !sideToMove;
    delta ^= g_sideKey; // side toggles on every move, in either direction

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

    // Restore the actual previous en passant square (was previously
    // hardcoded to -1 here, discarding real en-passant availability).
    _enPassantSq = m.prevEnPassantSq;

    // Restore fifty-move counter and repetition history.
    if (_undoTop > 0) {
        const DrawUndo u = _undoStack[--_undoTop];
        _halfmoveClock = u.prevHalfmoveClock;
        _posHistoryCount = u.prevHistoryCount;
    }

    // Fold the castling-rights and en-passant restorations into the hash.
    // Using inequality (rather than the one-directional "was true, now
    // false" check in makeMoveBB) because undo can flip a right from
    // false back to true -- the opposite direction of normal play.
    if (beforeWCK != canWhiteCastleKingSide)  delta ^= g_castlingKeys[0];
    if (beforeWCQ != canWhiteCastleQueenSide) delta ^= g_castlingKeys[1];
    if (beforeBCK != canBlackCastleKingSide)  delta ^= g_castlingKeys[2];
    if (beforeBCQ != canBlackCastleQueenSide) delta ^= g_castlingKeys[3];

    if (beforeEnPassantSq != -1) delta ^= g_enPassantFileKeys[beforeEnPassantSq & 7];
    if (_enPassantSq       != -1) delta ^= g_enPassantFileKeys[_enPassantSq & 7];

    zobristHash ^= delta;
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

    // Rank a pawn promotes on: rank 8 (squares 56-63) for white,
    // rank 1 (squares 0-7) for black.
    const int promoRankLo = isWhite ? 56 : 0;
    const int promoRankHi = isWhite ? 63 : 7;
    auto isPromoSquare = [&](int sq) { return sq >= promoRankLo && sq <= promoRankHi; };

    // A pawn arriving on the last rank MUST promote, and each choice of
    // promotion piece is a distinct legal move -- so one pawn push can
    // yield four moves. Queen is listed first so move ordering sees the
    // strongest option immediately.
    auto addPawnMove = [&](int from, int to, Piece captured) {
        Piece pawn = isWhite ? WPAWN : BPAWN;
        if (isPromoSquare(to)) {
            const Piece promoPieces[4] = {
                isWhite ? WQUEEN : BQUEEN,
                isWhite ? WROOK : BROOK,
                isWhite ? WBISHOP : BBISHOP,
                isWhite ? WKNIGHT : BKNIGHT
            };
            for (Piece pp : promoPieces) {
                MoveBB m(from, to, pawn, captured, true, pp);
                fillCastlingState(m);
                moves.push_back(m);
            }
        } else {
            MoveBB m(from, to, pawn, captured);
            fillCastlingState(m);
            moves.push_back(m);
        }
    };

    // --- PAWN LOGIC (Fixed to avoid guessing 'from' square) ---
    while (pawnsCopy) {
        int from = popLSB(pawnsCopy);
        uint64_t fromBB = 1ULL << from;

        // 1. Single Push
        uint64_t push = isWhite ? (fromBB << 8) & empty : (fromBB >> 8) & empty;
        if (push) {
            int to = isWhite ? from + 8 : from - 8;
            addPawnMove(from, to, EMPTY);
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
            addPawnMove(from, to, captured); // may promote (capture on last rank)
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

    // --- BISHOPS / ROOKS / QUEENS (Sliding Pieces) ---
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

    // Rooks were previously missing entirely from move generation -- the
    // engine simply never considered any rook move. Caught by perft.
    processSliders(isWhite ? whiteRooks : blackRooks, isWhite ? WROOK : BROOK,
        [this](uint64_t b, uint64_t o, uint64_t e) { return getRookMovesB(b, o, e); });

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
// Score is always from WHITE's point of view (positive = white better);
// negamaxBB negates it for black. Units are centipawns (100 = one pawn).
int ChessBoard::evaluateBoardBB() const {

    // Piece-square tables, written in VISUAL order: the first row is rank 8
    // (black's back rank) and the last row is rank 1 (white's back rank).
    // Because square indices run a1=0..h8=63, a white piece on square `sq`
    // must look up index `sq ^ 56` to flip the rank into this layout, and a
    // black piece looks up `sq` directly (which also mirrors the table for
    // black, exactly as intended).
    static const int pawnTable[64] = {
         0,  0,  0,  0,  0,  0,  0,  0,
        50, 50, 50, 50, 50, 50, 50, 50,
        10, 10, 20, 30, 30, 20, 10, 10,
         5,  5, 10, 25, 25, 10,  5,  5,
         0,  0,  0, 20, 20,  0,  0,  0,
         5, -5,-10,  0,  0,-10, -5,  5,
         5, 10, 10,-20,-20, 10, 10,  5,
         0,  0,  0,  0,  0,  0,  0,  0
    };

    static const int knightTable[64] = {
        -50,-40,-30,-30,-30,-30,-40,-50,
        -40,-20,  0,  0,  0,  0,-20,-40,
        -30,  0, 10, 15, 15, 10,  0,-30,
        -30,  5, 15, 20, 20, 15,  5,-30,
        -30,  0, 15, 20, 20, 15,  0,-30,
        -30,  5, 10, 15, 15, 10,  5,-30,
        -40,-20,  0,  5,  5,  0,-20,-40,
        -50,-40,-30,-30,-30,-30,-40,-50
    };

    static const int bishopTable[64] = {
        -20,-10,-10,-10,-10,-10,-10,-20,
        -10,  0,  0,  0,  0,  0,  0,-10,
        -10,  0,  5, 10, 10,  5,  0,-10,
        -10,  5,  5, 10, 10,  5,  5,-10,
        -10,  0, 10, 10, 10, 10,  0,-10,
        -10, 10, 10, 10, 10, 10, 10,-10,
        -10,  5,  0,  0,  0,  0,  5,-10,
        -20,-10,-10,-10,-10,-10,-10,-20
    };

    static const int rookTable[64] = {
         0,  0,  0,  0,  0,  0,  0,  0,
         5, 10, 10, 10, 10, 10, 10,  5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
         0,  0,  0,  5,  5,  0,  0,  0
    };

    static const int queenTable[64] = {
        -20,-10,-10, -5, -5,-10,-10,-20,
        -10,  0,  0,  0,  0,  0,  0,-10,
        -10,  0,  5,  5,  5,  5,  0,-10,
         -5,  0,  5,  5,  5,  5,  0, -5,
          0,  0,  5,  5,  5,  5,  0, -5,
        -10,  5,  5,  5,  5,  5,  0,-10,
        -10,  0,  5,  0,  0,  0,  0,-10,
        -20,-10,-10, -5, -5,-10,-10,-20
    };

    // Two king tables: in the middlegame the king wants to hide behind its
    // pawns on the back rank, but in the endgame (few pieces left, little
    // mating danger) it becomes a strong attacker and wants to march to the
    // centre. Blending between them by remaining material is called a
    // "tapered" evaluation, and it prevents the engine from cowering in the
    // corner during endgames it should be trying to win.
    static const int kingTableMid[64] = {
        -30,-40,-40,-50,-50,-40,-40,-30,
        -30,-40,-40,-50,-50,-40,-40,-30,
        -30,-40,-40,-50,-50,-40,-40,-30,
        -30,-40,-40,-50,-50,-40,-40,-30,
        -20,-30,-30,-40,-40,-30,-30,-20,
        -10,-20,-20,-20,-20,-20,-20,-10,
         20, 20,  0,  0,  0,  0, 20, 20,
         20, 30, 10,  0,  0, 10, 30, 20
    };

    static const int kingTableEnd[64] = {
        -50,-40,-30,-20,-20,-30,-40,-50,
        -30,-20,-10,  0,  0,-10,-20,-30,
        -30,-10, 20, 30, 30, 20,-10,-30,
        -30,-10, 30, 40, 40, 30,-10,-30,
        -30,-10, 30, 40, 40, 30,-10,-30,
        -30,-10, 20, 30, 30, 20,-10,-30,
        -30,-30,  0,  0,  0,  0,-30,-30,
        -50,-30,-30,-30,-30,-30,-30,-50
    };

    // File masks, used by the pawn-structure and open-file terms.
    static const uint64_t FILE_MASK[8] = {
        0x0101010101010101ULL, 0x0202020202020202ULL,
        0x0404040404040404ULL, 0x0808080808080808ULL,
        0x1010101010101010ULL, 0x2020202020202020ULL,
        0x4040404040404040ULL, 0x8080808080808080ULL
    };

    const uint64_t whitePieces = getWhitePieces();
    const uint64_t blackPieces = getBlackPieces();

    // -----------------------------------------------------------------
    // 1) Material
    // -----------------------------------------------------------------
    const int wPawnCount = popcount64(whitePawns);
    const int bPawnCount = popcount64(blackPawns);
    const int wKnightCount = popcount64(whiteKnights);
    const int bKnightCount = popcount64(blackKnights);
    const int wBishopCount = popcount64(whiteBishops);
    const int bBishopCount = popcount64(blackBishops);
    const int wRookCount = popcount64(whiteRooks);
    const int bRookCount = popcount64(blackRooks);
    const int wQueenCount = popcount64(whiteQueen);
    const int bQueenCount = popcount64(blackQueen);

    int material =
        100 * (wPawnCount - bPawnCount) +
        320 * (wKnightCount - bKnightCount) +
        330 * (wBishopCount - bBishopCount) +
        500 * (wRookCount - bRookCount) +
        900 * (wQueenCount - bQueenCount);

    // -----------------------------------------------------------------
    // 2) Game phase (for tapering the king table)
    // -----------------------------------------------------------------
    // 0 = pure endgame, 256 = full middlegame. Pawns are excluded because
    // they stay on the board in endgames; it's the heavy pieces that decide
    // whether the king is in danger.
    int phaseMaterial =
        (wKnightCount + bKnightCount) * 320 +
        (wBishopCount + bBishopCount) * 330 +
        (wRookCount + bRookCount) * 500 +
        (wQueenCount + bQueenCount) * 900;
    const int PHASE_MAX = 2 * (2 * 320 + 2 * 330 + 2 * 500 + 900); // full starting set
    int phase = phaseMaterial > PHASE_MAX ? 256 : (phaseMaterial * 256) / PHASE_MAX;

    // -----------------------------------------------------------------
    // 3) Piece-square tables
    // -----------------------------------------------------------------
    auto evalPieces = [](uint64_t bb, const int* table, bool isWhite) {
        int score = 0;
        while (bb) {
            int sq = popLSB(bb);
            int idx = isWhite ? (sq ^ 56) : sq; // see table-orientation note above
            score += table[idx];
        }
        return score;
    };

    int positional =
        evalPieces(whitePawns, pawnTable, true) - evalPieces(blackPawns, pawnTable, false) +
        evalPieces(whiteKnights, knightTable, true) - evalPieces(blackKnights, knightTable, false) +
        evalPieces(whiteBishops, bishopTable, true) - evalPieces(blackBishops, bishopTable, false) +
        evalPieces(whiteRooks, rookTable, true) - evalPieces(blackRooks, rookTable, false) +
        evalPieces(whiteQueen, queenTable, true) - evalPieces(blackQueen, queenTable, false);

    // King: interpolate between the middlegame and endgame tables by phase.
    int wKingMid = evalPieces(whiteKing, kingTableMid, true);
    int wKingEnd = evalPieces(whiteKing, kingTableEnd, true);
    int bKingMid = evalPieces(blackKing, kingTableMid, false);
    int bKingEnd = evalPieces(blackKing, kingTableEnd, false);
    positional += ((wKingMid - bKingMid) * phase + (wKingEnd - bKingEnd) * (256 - phase)) / 256;

    // -----------------------------------------------------------------
    // 4) Bishop pair
    // -----------------------------------------------------------------
    // Two bishops cover both colour complexes and work well together, which
    // is worth more than the sum of the individual pieces.
    int bishopPair = 0;
    if (wBishopCount >= 2) bishopPair += 30;
    if (bBishopCount >= 2) bishopPair -= 30;

    // -----------------------------------------------------------------
    // 5) Pawn structure: doubled, isolated, passed
    // -----------------------------------------------------------------
    int pawnStructure = 0;
    for (int f = 0; f < 8; f++) {
        int wOnFile = popcount64(whitePawns & FILE_MASK[f]);
        int bOnFile = popcount64(blackPawns & FILE_MASK[f]);

        // Doubled: pawns stacked on one file can't defend each other and
        // the rear one is often permanently passive.
        if (wOnFile > 1) pawnStructure -= 12 * (wOnFile - 1);
        if (bOnFile > 1) pawnStructure += 12 * (bOnFile - 1);

        // Isolated: no friendly pawn on either adjacent file, so it can
        // never be defended by another pawn.
        uint64_t adjacent = 0;
        if (f > 0) adjacent |= FILE_MASK[f - 1];
        if (f < 7) adjacent |= FILE_MASK[f + 1];
        if (wOnFile > 0 && !(whitePawns & adjacent)) pawnStructure -= 18;
        if (bOnFile > 0 && !(blackPawns & adjacent)) pawnStructure += 18;
    }

    // Passed pawns: no enemy pawn ahead on its own or adjacent files, so
    // nothing can stop it from running. Value grows sharply as it advances.
    static const int passedBonus[8] = { 0, 10, 20, 35, 60, 100, 150, 0 };
    {
        uint64_t wp = whitePawns;
        while (wp) {
            int sq = popLSB(wp);
            int f = sq & 7, r = sq >> 3;
            uint64_t blockers = FILE_MASK[f];
            if (f > 0) blockers |= FILE_MASK[f - 1];
            if (f < 7) blockers |= FILE_MASK[f + 1];
            // Mask to ranks strictly ahead of this pawn (towards rank 8).
            uint64_t ahead = ~0ULL << ((r + 1) * 8);
            if (!(blackPawns & blockers & ahead))
                pawnStructure += passedBonus[r];
        }
        uint64_t bp = blackPawns;
        while (bp) {
            int sq = popLSB(bp);
            int f = sq & 7, r = sq >> 3;
            uint64_t blockers = FILE_MASK[f];
            if (f > 0) blockers |= FILE_MASK[f - 1];
            if (f < 7) blockers |= FILE_MASK[f + 1];
            // Black advances towards rank 1, so "ahead" means lower ranks.
            uint64_t ahead = (r == 0) ? 0ULL : (~0ULL >> ((8 - r) * 8));
            if (!(whitePawns & blockers & ahead))
                pawnStructure += -passedBonus[7 - r];
        }
    }

    // -----------------------------------------------------------------
    // 6) Mobility
    // -----------------------------------------------------------------
    // Counts squares the pieces can reach (pseudo-legal, ignoring pins and
    // check -- full legality here would mean running move generation inside
    // the evaluation, which is far too slow for something called at every
    // leaf). A boxed-in piece is worth less than the same piece with
    // options, which is exactly the distinction plain material misses.
    auto mobilityFor = [&](bool white) {
        uint64_t own = white ? whitePieces : blackPieces;
        uint64_t enemy = white ? blackPieces : whitePieces;
        int mob = 0;

        uint64_t knights = white ? whiteKnights : blackKnights;
        mob += 4 * popcount64(getKnightMovesB(knights, own));

        uint64_t bishops = white ? whiteBishops : blackBishops;
        while (bishops) {
            int sq = popLSB(bishops);
            mob += 3 * popcount64(getBishopMovesB(1ULL << sq, own, enemy));
        }

        uint64_t rooks = white ? whiteRooks : blackRooks;
        while (rooks) {
            int sq = popLSB(rooks);
            mob += 2 * popcount64(getRookMovesB(1ULL << sq, own, enemy));
        }

        uint64_t queens = white ? whiteQueen : blackQueen;
        while (queens) {
            int sq = popLSB(queens);
            mob += 1 * popcount64(getQueenMovesB(1ULL << sq, own, enemy));
        }
        return mob;
    };
    int mobility = mobilityFor(true) - mobilityFor(false);

    // -----------------------------------------------------------------
    // 7) Rooks on open / semi-open files
    // -----------------------------------------------------------------
    int rookFiles = 0;
    {
        uint64_t wr = whiteRooks;
        while (wr) {
            int f = popLSB(wr) & 7;
            bool ownPawn = (whitePawns & FILE_MASK[f]) != 0;
            bool enemyPawn = (blackPawns & FILE_MASK[f]) != 0;
            if (!ownPawn && !enemyPawn) rookFiles += 20;   // fully open
            else if (!ownPawn)          rookFiles += 10;   // semi-open
        }
        uint64_t br = blackRooks;
        while (br) {
            int f = popLSB(br) & 7;
            bool ownPawn = (blackPawns & FILE_MASK[f]) != 0;
            bool enemyPawn = (whitePawns & FILE_MASK[f]) != 0;
            if (!ownPawn && !enemyPawn) rookFiles -= 20;
            else if (!ownPawn)          rookFiles -= 10;
        }
    }

    // -----------------------------------------------------------------
    // 8) King safety: pawn shield
    // -----------------------------------------------------------------
    // Rewards keeping friendly pawns directly in front of the king, and
    // penalises an exposed king on an open file. Scaled by game phase, since
    // none of this matters once the heavy pieces are gone.
    int kingSafety = 0;
    if (phase > 40) {
        auto shieldFor = [&](bool white) {
            int kingSq = white ? whiteKingSquare : blackKingSquare;
            if (kingSq < 0 || kingSq > 63) return 0;
            int f = kingSq & 7, r = kingSq >> 3;
            uint64_t ownPawns = white ? whitePawns : blackPawns;

            int s = 0;
            // The three files around the king (clamped at the board edge).
            for (int df = -1; df <= 1; df++) {
                int nf = f + df;
                if (nf < 0 || nf > 7) continue;

                // Is there a friendly pawn on this file, ahead of the king?
                uint64_t fileMask = FILE_MASK[nf];
                uint64_t aheadMask;
                if (white)
                    aheadMask = (r >= 7) ? 0ULL : (~0ULL << ((r + 1) * 8));
                else
                    aheadMask = (r == 0) ? 0ULL : (~0ULL >> ((8 - r) * 8));

                if (ownPawns & fileMask & aheadMask) s += 12;
                else                                 s -= 14; // missing shield pawn
            }
            return s;
        };
        kingSafety = (shieldFor(true) - shieldFor(false)) * phase / 256;
    }

    return material + positional + bishopPair + pawnStructure
         + mobility + rookFiles + kingSafety;
}

// =====================
// Move ordering (MVV-LVA)
// =====================
// Searching promising moves first lets alpha-beta prune far more branches.
// "Most Valuable Victim, Least Valuable Attacker": a capture is scored by
// (value of the captured piece) minus a small penalty for using a valuable
// attacker, so e.g. "pawn takes queen" ranks above "queen takes pawn".
// Promotions are also pushed to the front. Quiet moves keep their original
// order after all captures/promotions.
static int piecePointValue(Piece p) {
    switch (p) {
    case WPAWN: case BPAWN:     return 100;
    case WKNIGHT: case BKNIGHT: return 320;
    case WBISHOP: case BBISHOP: return 330;
    case WROOK: case BROOK:     return 500;
    case WQUEEN: case BQUEEN:   return 900;
    case WKING: case BKING:     return 20000;
    default: return 0;
    }
}

static int moveOrderScore(const MoveBB& m) {
    int score = 0;
    if (m.captured != EMPTY)
        score += 10 * piecePointValue(m.captured) - piecePointValue(m.piece);
    if (m.promotion)
        score += 800;
    return score;
}

void ChessBoard::orderMoves(std::vector<MoveBB>& moves) const {
    std::stable_sort(moves.begin(), moves.end(),
        [](const MoveBB& a, const MoveBB& b) {
            return moveOrderScore(a) > moveOrderScore(b);
        });
}

// Extends moveOrderScore with killer/history info for quiet moves. Captures
// and promotions keep their MVV-LVA score exactly as before (always large
// enough to stay ahead of every quiet move); among quiet moves, a killer
// for this ply is tried next, then the rest are ranked by history score.
static int moveOrderScoreWithHeuristics(const MoveBB& m, const SearchHeuristics& heur, int ply) {
    int score = moveOrderScore(m);
    if (m.captured == EMPTY && !m.promotion) {
        if (heur.isKiller(ply, m))
            score += 500; // below the smallest real capture score, above plain quiets
        score += heur.historyScore(m) / 100; // tie-breaker among non-killer quiets
    }
    return score;
}

static void orderMovesWithHeuristics(std::vector<MoveBB>& moves, const SearchHeuristics& heur, int ply) {
    std::stable_sort(moves.begin(), moves.end(),
        [&](const MoveBB& a, const MoveBB& b) {
            return moveOrderScoreWithHeuristics(a, heur, ply) > moveOrderScoreWithHeuristics(b, heur, ply);
        });
}

// =====================
// Zobrist hashing (for the transposition table)
// =====================
// computeZobristHash() recomputes the hash from scratch by scanning all 64
// squares. It's used once, in the constructor, to establish the initial
// hash. During search, makeMoveBB/undoMoveBB keep `zobristHash` in sync
// incrementally instead (see below) -- this full recompute is too slow to
// call on every node, which is exactly what made the first version of
// this transposition table slower than no table at all.
uint64_t ChessBoard::computeZobristHash() const {
    // C++11 "magic statics" guarantee this runs exactly once even if
    // multiple threads call it for the first time concurrently.
    static bool zobristReady = (initZobristKeys(), true);
    (void)zobristReady;

    uint64_t h = 0;
    for (int sq = 0; sq < 64; sq++) {
        Piece p = pieceAt(sq);
        if (p != EMPTY)
            h ^= g_pieceKeys[p][sq];
    }

    if (!sideToMove) h ^= g_sideKey;

    if (canWhiteCastleKingSide)  h ^= g_castlingKeys[0];
    if (canWhiteCastleQueenSide) h ^= g_castlingKeys[1];
    if (canBlackCastleKingSide)  h ^= g_castlingKeys[2];
    if (canBlackCastleQueenSide) h ^= g_castlingKeys[3];

    if (_enPassantSq != -1) h ^= g_enPassantFileKeys[_enPassantSq & 7];

    return h;
}

// =====================
// Quiescence search
// =====================
// At depth 0, negamax used to evaluate the position immediately -- even if
// that position was mid-capture-sequence (e.g. right after grabbing a
// pawn that's about to be recaptured by a bigger piece). That's the
// "horizon effect": the engine's view stops exactly at the search limit,
// blind to whatever happens one move later. Quiescence search fixes this
// by continuing the search past the nominal depth limit, but *only*
// through captures (and promotions), until the position is "quiet" (no
// more captures worth considering). This is far cheaper than extending
// the full search, because most positions have far fewer captures than
// total legal moves, and it terminates naturally since captures remove
// material from a finite supply of pieces.
//
// qDepth is a safety cap on how many extra plies quiescence can recurse
// through. In a legal game this recursion already terminates on its own
// (you can only capture the pieces that exist), but the cap guards
// against any edge case blowing up the recursion unexpectedly.
int ChessBoard::quiescenceBB(int alpha, int beta, bool isWhite, int qDepth) {
    nodeCount++;

    // "Stand pat": the score if we simply stop here and don't force any
    // more captures. This matters because a capture is not always
    // forced/good -- sometimes the quiet evaluation is already the best
    // available outcome (e.g. all available captures lose material).
    int standPat = isWhite ? evaluateBoardBB() : -evaluateBoardBB();

    if (qDepth <= 0) return standPat;

    if (standPat >= beta) return beta;
    if (standPat > alpha) alpha = standPat;

    auto moves = getLegalMovesBB(isWhite);

    // Terminal check here too: without it, a position that is actually
    // checkmate would just be scored by material ("I'm only a pawn down")
    // because quiescence returns the stand-pat score when no captures
    // remain. Mate and stalemate have to short-circuit.
    if (moves.empty())
        return isKingInCheckBB(isWhite) ? (-MATE_SCORE + 64) : 0;

    // Only continue through captures/promotions -- quiet moves are
    // exactly what "quiescence" (a quiet position) means we stop at.
    std::vector<MoveBB> captures;
    captures.reserve(moves.size());
    for (const auto& m : moves)
        if (m.captured != EMPTY || (m.flags & EN_PASSANT) || m.promotion)
            captures.push_back(m);

    orderMoves(captures); // MVV-LVA: search the most consequential captures first

    for (const auto& m : captures) {
        makeMoveBB(m);
        int score = -quiescenceBB(-beta, -alpha, !isWhite, qDepth - 1);
        undoMoveBB(m);

        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }

    return alpha;
}

// =====================
// FEN loading
// =====================
bool ChessBoard::loadFEN(const std::string& fen) {
    // Parse into locals first, then commit -- so a malformed FEN leaves
    // the existing position untouched rather than half-overwritten.
    uint64_t wp = 0, wn = 0, wb = 0, wr = 0, wq = 0, wk = 0;
    uint64_t bp = 0, bn = 0, bbi = 0, br = 0, bq = 0, bk = 0;
    int wkSq = -1, bkSq = -1;

    std::istringstream ss(fen);
    std::string placement, sideStr, castlingStr, epStr;
    if (!(ss >> placement >> sideStr >> castlingStr >> epStr))
        return false;
    // Fields 5 and 6 (halfmove clock, fullmove number) are optional in
    // practice -- many FEN strings in test suites omit them.
    std::string hm, fm;
    ss >> hm >> fm;

    // --- Field 1: piece placement, given from rank 8 down to rank 1 ---
    int rank = 7; // FEN starts at rank 8, which is index 7 here
    int file = 0;
    for (char c : placement) {
        if (c == '/') {
            if (file != 8) return false; // rank didn't add up to 8 squares
            rank--;
            file = 0;
            if (rank < 0) return false;
            continue;
        }
        if (c >= '1' && c <= '8') {
            file += (c - '0'); // run of empty squares
            if (file > 8) return false;
            continue;
        }
        if (file > 7 || rank < 0) return false;

        int sq = rank * 8 + file;
        uint64_t bit = 1ULL << sq;
        switch (c) {
        case 'P': wp  |= bit; break;
        case 'N': wn  |= bit; break;
        case 'B': wb  |= bit; break;
        case 'R': wr  |= bit; break;
        case 'Q': wq  |= bit; break;
        case 'K': wk  |= bit; wkSq = sq; break;
        case 'p': bp  |= bit; break;
        case 'n': bn  |= bit; break;
        case 'b': bbi |= bit; break;
        case 'r': br  |= bit; break;
        case 'q': bq  |= bit; break;
        case 'k': bk  |= bit; bkSq = sq; break;
        default: return false; // unrecognized piece character
        }
        file++;
    }
    if (rank != 0 || file != 8) return false; // didn't end on a complete rank 1
    if (wkSq < 0 || bkSq < 0) return false;   // both kings must be present

    // --- Field 2: side to move ---
    bool side;
    if (sideStr == "w") side = true;
    else if (sideStr == "b") side = false;
    else return false;

    // --- Field 3: castling rights ---
    bool wck = false, wcq = false, bck = false, bcq = false;
    if (castlingStr != "-") {
        for (char c : castlingStr) {
            switch (c) {
            case 'K': wck = true; break;
            case 'Q': wcq = true; break;
            case 'k': bck = true; break;
            case 'q': bcq = true; break;
            default: return false;
            }
        }
    }

    // --- Field 4: en passant target square ---
    int ep = -1;
    if (epStr != "-") {
        if (epStr.size() < 2) return false;
        int epFile = epStr[0] - 'a';
        int epRank = epStr[1] - '1';
        if (epFile < 0 || epFile > 7 || epRank < 0 || epRank > 7) return false;
        ep = epRank * 8 + epFile;
    }

    // --- Commit ---
    whitePawns = wp; whiteKnights = wn; whiteBishops = wb;
    whiteRooks = wr; whiteQueen = wq;  whiteKing = wk;
    blackPawns = bp; blackKnights = bn; blackBishops = bbi;
    blackRooks = br; blackQueen = bq;  blackKing = bk;

    whiteKingSquare = wkSq;
    blackKingSquare = bkSq;
    sideToMove = side;

    canWhiteCastleKingSide = wck;
    canWhiteCastleQueenSide = wcq;
    canBlackCastleKingSide = bck;
    canBlackCastleQueenSide = bcq;

    _enPassantSq = ep;

    // Halfmove clock (field 5). Absent or unparsable -> 0.
    _halfmoveClock = 0;
    if (!hm.empty()) {
        char* endp = nullptr;
        long v = std::strtol(hm.c_str(), &endp, 10);
        if (endp != hm.c_str() && v >= 0 && v < 1000)
            _halfmoveClock = (int)v;
    }

    // A new position means a fresh game as far as repetition is concerned.
    _posHistoryCount = 0;
    _undoTop = 0;

    // The incremental hash must be rebuilt from scratch for a new position.
    zobristHash = computeZobristHash();

    // Seed the history with the starting position so a later return to it
    // counts as a repetition.
    _posHistory[_posHistoryCount++] = zobristHash;

    return true;
}

// =====================
// Negamax
// =====================
int ChessBoard::negamaxBB(int depth, int alpha, int beta, bool isWhite, TranspositionTable& tt,
                           SearchHeuristics& heur, int ply) {
    nodeCount++;

    uint64_t hash = zobristHash; // maintained incrementally by makeMoveBB/undoMoveBB
    int ttScore;
    if (tt.probe(hash, depth, alpha, beta, ttScore))
        return ttScore;

    // Automatic draws end the game regardless of material, so they must be
    // scored 0 rather than evaluated. Guarded by ply > 0 so the root
    // position itself is still searched normally -- returning "draw" at the
    // root would leave the caller with no move to play.
    if (ply > 0 && isDrawByRule())
        return 0;

    if (depth == 0) {
        int alphaOrig = alpha;
        int qScore = quiescenceBB(alpha, beta, isWhite, 8); // capped extra depth
        TranspositionTable::Flag flag;
        if (qScore <= alphaOrig)    flag = TranspositionTable::UPPERBOUND;
        else if (qScore >= beta)    flag = TranspositionTable::LOWERBOUND;
        else                        flag = TranspositionTable::EXACT;
        tt.store(hash, depth, qScore, flag);
        return qScore;
    }

    int alphaOrig = alpha;
    int best = -1000000;
    auto moves = getLegalMovesBB(isWhite);

    // Terminal position: no legal moves means the game is over RIGHT HERE,
    // and which outcome it is depends entirely on whether we're in check.
    //   - in check + no moves  = checkmate -> a loss for the side to move
    //   - not in check, no moves = STALEMATE -> a draw, score 0
    // Previously this function just fell through the empty loop and
    // returned its -1000000 initial value for both cases, which told the
    // engine that stalemate is as bad as being mated. That's wrong in both
    // directions: a losing engine would not try to save itself with a
    // stalemate, and a winning engine would happily force stalemate
    // believing it had won.
    //
    // Mate scores also encode DISTANCE: MATE_SCORE is reduced by `ply`, so
    // a mate found closer to the root scores higher than a deeper one. With
    // a flat score, every mate looks identical and the engine has no reason
    // to prefer mate-in-1 over mate-in-5 (and can shuffle indefinitely
    // without ever landing the mate).
    if (moves.empty()) {
        int terminal = isKingInCheckBB(isWhite) ? (-MATE_SCORE + ply) : 0;
        tt.store(hash, depth, terminal, TranspositionTable::EXACT);
        return terminal;
    }

    orderMovesWithHeuristics(moves, heur, ply);

    for (const auto& m : moves) {
        makeMoveBB(m);
        int score = -negamaxBB(depth - 1, -beta, -alpha, !isWhite, tt, heur, ply + 1);
        undoMoveBB(m);

        best = std::max(best, score);
        alpha = std::max(alpha, score);
        if (alpha >= beta) {
            // Beta cutoff from a quiet move: remember it as a killer for
            // this ply, and reward it in the history table (weighted by
            // depth -- a cutoff found deeper in the tree is a stronger
            // signal than one found near the leaves). Captures already
            // have MVV-LVA, so there's nothing to learn from them here.
            if (m.captured == EMPTY && !m.promotion) {
                heur.recordKiller(ply, m);
                heur.recordHistory(m, depth);
            }
            break;
        }
    }

    TranspositionTable::Flag flag;
    if (best <= alphaOrig)      flag = TranspositionTable::UPPERBOUND;
    else if (best >= beta)      flag = TranspositionTable::LOWERBOUND;
    else                        flag = TranspositionTable::EXACT;
    tt.store(hash, depth, best, flag);

    return best;
}

// =====================
// Best move
// =====================
MoveBB ChessBoard::findBestMoveBB(int depth, bool isWhite) {
    MoveBB best;
    bool found = false;
    int bestScore = -1000000;

    // One transposition table for this whole search call. It's a local
    // object (not shared/global), so there's no thread-safety concern here.
    TranspositionTable tt(16); // 16MB
    SearchHeuristics heur;

    auto moves = getLegalMovesBB(isWhite);
    orderMoves(moves);

    if (moves.empty()) {
        // Return a dummy move instead of asserting
        return MoveBB(0, 0, isWhite ? WPAWN : BPAWN);
    }

    for (const auto& m : moves) {
        nodeCount++;
        makeMoveBB(m);
        int score = -negamaxBB(depth - 1, -1000000, 1000000, !isWhite, tt, heur, 1);
        undoMoveBB(m);

        if (!found || score > bestScore) {
            bestScore = score;
            best = m;
            found = true;
        }
    }

    return best;
}

// =====================
// Root-split multithreaded best move
// =====================
// Splits the root's legal moves across a thread pool. Each thread works on
// its own copy of the board (ChessBoard has no pointers, so copying is cheap
// and safe) and runs the normal single-threaded negamax from there. Threads
// pull the next unassigned move from a shared atomic counter, so faster
// threads pick up more work automatically (no static/even split).
//
// Trade-off: each thread starts its own alpha-beta window from scratch
// instead of sharing a tightening window like the sequential version does,
// so pruning is somewhat weaker per-thread -- but overall throughput scales
// close to the number of cores used.
MoveBB ChessBoard::findBestMoveBB_MT(int depth, bool isWhite, unsigned numThreads,
                                       const MoveBB* preferredMove) {
    auto moves = getLegalMovesBB(isWhite);
    orderMoves(moves);
    if (moves.empty())
        return MoveBB(0, 0, isWhite ? WPAWN : BPAWN);

    // If iterative deepening already found a good move at a shallower
    // depth, search it first at the root. It doesn't strengthen pruning
    // *within* each thread's independent subtree the way it would in a
    // single sequential search, but the work-stealing queue still hands
    // it out first, so if it's actually the best move its score becomes
    // available earliest -- which matters if this is ever wired up to a
    // time-limited search that can be cut off between root moves.
    if (preferredMove) {
        for (size_t i = 0; i < moves.size(); i++) {
            if (moves[i].from == preferredMove->from &&
                moves[i].to == preferredMove->to &&
                moves[i].promotionPiece == preferredMove->promotionPiece) {
                std::swap(moves[0], moves[i]);
                break;
            }
        }
    }

    size_t n = moves.size();
    std::vector<int> scores(n);
    std::vector<uint64_t> perThreadNodes;

    if (numThreads == 0) {
        numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 4;
    }
    numThreads = std::min<unsigned>(numThreads, (unsigned)n);

    perThreadNodes.assign(numThreads, 0);

    std::atomic<size_t> nextIndex{0};
    std::vector<std::thread> pool;

    auto worker = [&](unsigned threadSlot) {
        // One table per thread, created once and reused across every root
        // move this thread ends up handling. Each thread's table is its
        // own object -- no sharing, no locks, no data races -- so this is
        // safe even though several threads run concurrently.
        TranspositionTable tt(16); // 16MB per thread
        SearchHeuristics heur;     // also per-thread, reused across its root moves

        while (true) {
            size_t i = nextIndex.fetch_add(1);
            if (i >= n) break;

            ChessBoard localBoard = *this;   // independent copy per thread
            localBoard.resetNodeCount();
            localBoard.makeMoveBB(moves[i]);
            scores[i] = -localBoard.negamaxBB(depth - 1, -1000000, 1000000, !isWhite, tt, heur, 1);
            perThreadNodes[threadSlot] += localBoard.getNodeCount() + 1; // +1 for root move
        }
    };

    for (unsigned t = 0; t < numThreads; t++)
        pool.emplace_back(worker, t);
    for (auto& th : pool)
        th.join();

    uint64_t totalNodes = 0;
    for (auto c : perThreadNodes) totalNodes += c;
    nodeCount = totalNodes; // total work done across all threads combined

    int bestScore = -1000000;
    MoveBB best = moves[0];
    for (size_t i = 0; i < n; i++) {
        if (scores[i] > bestScore) {
            bestScore = scores[i];
            best = moves[i];
        }
    }
    return best;
}

// =====================
// Time-budgeted search
// =====================
MoveBB ChessBoard::findBestMoveBB_Timed(int budgetMs, bool isWhite,
                                        unsigned numThreads, int maxDepth)
{
    const auto t0 = std::chrono::steady_clock::now();
    auto elapsedMs = [&]() {
        return (int)std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0).count();
    };

    _lastCompletedDepth = 0;

    auto rootMoves = getLegalMovesBB(isWhite);
    if (rootMoves.empty())
        return MoveBB(0, 0, isWhite ? WPAWN : BPAWN);

    // Depth 1 always runs, so there is always a legal move to return even
    // if the budget is tiny.
    int iterStart = elapsedMs();
    MoveBB best = findBestMoveBB_MT(1, isWhite, numThreads);
    int lastIterMs = elapsedMs() - iterStart;
    _lastCompletedDepth = 1;
    uint64_t nodesTotal = nodeCount;

    if (maxDepth < 1) maxDepth = 1;
    if (budgetMs <= 0) return best;

    for (int depth = 2; depth <= maxDepth; depth++)
    {
        const int used = elapsedMs();
        if (used >= budgetMs) break;

        // Don't START an iteration that probably won't finish in time --
        // the clock is only checked BETWEEN iterations, since stopping
        // mid-iteration would mean threading a deadline through the whole
        // recursion.
        //
        // Estimate the next iteration from how long the LAST one took,
        // times the observed per-ply growth. An earlier version compared
        // TOTAL elapsed time against a flat 6x factor, which was far too
        // pessimistic: a middlegame that had finished depth 4 in 269ms
        // refused to attempt depth 5 even with 930ms of a 1200ms budget
        // still unspent, so the Hard and Expert levels behaved identically.
        //
        // A floor of 1ms keeps the very first (sub-millisecond) iterations
        // from making the estimate zero and looping to maxDepth.
        const int predictedNext = (lastIterMs < 1 ? 1 : lastIterMs) * kPlyGrowthFactor;
        if (used + predictedNext > budgetMs) break;

        iterStart = elapsedMs();
        MoveBB candidate = findBestMoveBB_MT(depth, isWhite, numThreads, &best);
        lastIterMs = elapsedMs() - iterStart;
        nodesTotal += nodeCount;

        // The iteration completed, so its result supersedes the shallower
        // one. (Nothing is abandoned part-way here, so there's no risk of
        // adopting a half-searched result.)
        best = candidate;
        _lastCompletedDepth = depth;
    }

    nodeCount = nodesTotal;
    return best;
}

// =====================
// Iterative deepening
// =====================
// Searches depth 1, then 2, then 3, ... up to maxDepth, instead of jumping
// straight there. This looks wasteful (the shallow searches get thrown
// away), but it isn't: each shallow search's branching factor is
// exponentially smaller than the final depth's, so the total cost of all
// the earlier iterations combined is usually a modest fraction of the
// final iteration's cost. In exchange, the best move found at depth N-1
// is tried FIRST at depth N -- a far stronger ordering signal than static
// MVV-LVA alone, since it comes from an actual (if shallower) search
// rather than a heuristic guess. Better ordering means more alpha-beta
// cutoffs, which can make the final iteration cheaper than searching
// straight to maxDepth would have been on its own.
MoveBB ChessBoard::findBestMoveBB_ID(int maxDepth, bool isWhite) {
    MoveBB bestMove;
    bool found = false;

    // One table shared across all depths of this call. Later iterations
    // can hit entries left behind by earlier ones (a position reached at
    // depth 3 this iteration may have already been scored during the
    // depth-2 pass), and there's no thread-safety concern since this
    // whole call is sequential.
    TranspositionTable tt(16);

    // Also shared across iterations: since `ply` always means "distance
    // from the true root" regardless of which iteration's depth we're on,
    // killers/history learned during a shallow iteration remain valid and
    // useful for ordering in the next, deeper iteration.
    SearchHeuristics heur;

    for (int depth = 1; depth <= maxDepth; depth++) {
        auto moves = getLegalMovesBB(isWhite);
        orderMoves(moves);

        if (moves.empty()) {
            return MoveBB(0, 0, isWhite ? WPAWN : BPAWN);
        }

        // Move last iteration's best move to the front of this
        // iteration's list, ahead of even the static MVV-LVA ordering.
        if (found) {
            for (size_t i = 0; i < moves.size(); i++) {
                if (moves[i].from == bestMove.from &&
                    moves[i].to == bestMove.to &&
                    moves[i].promotionPiece == bestMove.promotionPiece) {
                    std::swap(moves[0], moves[i]);
                    break;
                }
            }
        }

        int bestScoreThisDepth = -1000000;
        MoveBB bestThisDepth;
        bool foundThisDepth = false;

        for (const auto& m : moves) {
            nodeCount++;
            makeMoveBB(m);
            int score = -negamaxBB(depth - 1, -1000000, 1000000, !isWhite, tt, heur, 1);
            undoMoveBB(m);

            if (!foundThisDepth || score > bestScoreThisDepth) {
                bestScoreThisDepth = score;
                bestThisDepth = m;
                foundThisDepth = true;
            }
        }

        if (foundThisDepth) {
            bestMove = bestThisDepth;
            found = true;
        }
    }

    return bestMove;
}

// Combines iterative deepening with the multithreaded root search: the
// cheap shallow iterations (1..maxDepth-1) run single-threaded to build up
// a best-move hint quickly, then the expensive final iteration -- the one
// actually worth parallelizing -- runs via findBestMoveBB_MT, seeded with
// that hint so its root move ordering benefits too.
MoveBB ChessBoard::findBestMoveBB_ID_MT(int maxDepth, bool isWhite, unsigned numThreads) {
    if (maxDepth <= 1)
        return findBestMoveBB_MT(maxDepth, isWhite, numThreads);

    MoveBB bestMove;
    bool found = false;
    TranspositionTable tt(16);
    SearchHeuristics heur;

    for (int depth = 1; depth < maxDepth; depth++) {
        auto moves = getLegalMovesBB(isWhite);
        orderMoves(moves);

        if (moves.empty())
            return MoveBB(0, 0, isWhite ? WPAWN : BPAWN);

        if (found) {
            for (size_t i = 0; i < moves.size(); i++) {
                if (moves[i].from == bestMove.from &&
                    moves[i].to == bestMove.to &&
                    moves[i].promotionPiece == bestMove.promotionPiece) {
                    std::swap(moves[0], moves[i]);
                    break;
                }
            }
        }

        int bestScoreThisDepth = -1000000;
        MoveBB bestThisDepth;
        bool foundThisDepth = false;

        for (const auto& m : moves) {
            nodeCount++;
            makeMoveBB(m);
            int score = -negamaxBB(depth - 1, -1000000, 1000000, !isWhite, tt, heur, 1);
            undoMoveBB(m);

            if (!foundThisDepth || score > bestScoreThisDepth) {
                bestScoreThisDepth = score;
                bestThisDepth = m;
                foundThisDepth = true;
            }
        }

        if (foundThisDepth) {
            bestMove = bestThisDepth;
            found = true;
        }
    }

    // Final, deepest iteration: parallel, seeded with the best move found
    // by the cheap shallow passes above. findBestMoveBB_MT overwrites
    // nodeCount with just its own count, so preserve the shallow passes'
    // count and add it back in for accurate total reporting.
    uint64_t shallowNodes = nodeCount;
    MoveBB result = findBestMoveBB_MT(maxDepth, isWhite, numThreads, found ? &bestMove : nullptr);
    nodeCount += shallowNodes;
    return result;
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




