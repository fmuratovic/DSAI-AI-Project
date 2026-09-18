#pragma once
#include "MoveBB.h"
#include "VisualPiece.h"
#include <array>
#include <vector>
#include <cstdint>
#include <cassert>
#include <string>

// ---------------------------------------------------------------------
// popcount64 -- portable population count (number of set bits)
// ---------------------------------------------------------------------
// The fast paths use compiler intrinsics, but each is only available on
// certain compiler/architecture combinations:
//   - __popcnt64      MSVC, x86-64 only (NOT 32-bit x86, NOT ARM64)
//   - __builtin_popcountll  GCC/Clang, all architectures
// The previous version called __popcnt64 for any _MSC_VER, which fails to
// compile on 32-bit MSVC targets and on Windows-on-ARM. The plain C++
// fallback below keeps the code building everywhere; it is slower, but
// only used where no intrinsic exists.
#if defined(__GNUC__) || defined(__clang__)
inline int popcount64(uint64_t x) {
    return __builtin_popcountll(x);
}
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_AMD64))
#include <intrin.h>
inline int popcount64(uint64_t x) {
    return (int)__popcnt64(x);
}
#elif defined(_MSC_VER) && defined(_M_ARM64)
#include <intrin.h>
inline int popcount64(uint64_t x) {
    return (int)_CountOneBits64(x);
}
#else
// Portable SWAR fallback -- no intrinsics, works on any C++17 compiler.
inline int popcount64(uint64_t x) {
    x = x - ((x >> 1) & 0x5555555555555555ULL);
    x = (x & 0x3333333333333333ULL) + ((x >> 2) & 0x3333333333333333ULL);
    x = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0FULL;
    return (int)((x * 0x0101010101010101ULL) >> 56);
}
#endif

// =====================
// Transposition table
// =====================
// Caches previously-searched positions (keyed by a Zobrist hash) so the
// search can skip re-analyzing a position it has already seen via a
// different move order. One table is created per search call (or per
// worker thread in the multithreaded search) and lives only for the
// duration of that search -- it is NOT a shared/global structure, so no
// locking is needed and there is no risk of a data race between threads.
class TranspositionTable {
public:
    enum Flag : uint8_t { EXACT, LOWERBOUND, UPPERBOUND };

    struct Entry {
        uint64_t key = 0;
        int depth = -1;
        int score = 0;
        Flag flag = EXACT;
        bool valid = false;
    };

    // sizeMB controls how much memory this table uses. Table size is
    // rounded down to a power of two so lookups can use a fast bitmask
    // instead of a modulo.
    explicit TranspositionTable(size_t sizeMB = 16) {
        size_t numEntries = (sizeMB * 1024ULL * 1024ULL) / sizeof(Entry);
        size_t pow2 = 1;
        while (pow2 * 2 <= numEntries) pow2 *= 2;
        if (pow2 == 0) pow2 = 1;
        indexMask = pow2 - 1;
        table.assign(pow2, Entry{});
    }

    // Returns true and fills outScore if this position (at >= the
    // requested depth) already tells us the answer for this alpha/beta
    // window. Returns false if the search must still be run.
    bool probe(uint64_t key, int depth, int alpha, int beta, int& outScore) const {
        const Entry& e = table[key & indexMask];
        if (!e.valid || e.key != key || e.depth < depth) return false;

        if (e.flag == EXACT) { outScore = e.score; return true; }
        if (e.flag == LOWERBOUND && e.score >= beta) { outScore = e.score; return true; }
        if (e.flag == UPPERBOUND && e.score <= alpha) { outScore = e.score; return true; }
        return false;
    }

    void store(uint64_t key, int depth, int score, Flag flag) {
        Entry& e = table[key & indexMask];
        // Always-replace: simple and effective enough for this project.
        // A depth-preferred scheme could be added later if needed.
        e.key = key;
        e.depth = depth;
        e.score = score;
        e.flag = flag;
        e.valid = true;
    }

private:
    std::vector<Entry> table;
    size_t indexMask = 0;
};

// =====================
// Killer moves + history heuristic
// =====================
// MVV-LVA already orders captures/promotions well, but quiet moves are
// otherwise searched in arbitrary order. This fills that gap with two
// cheap, well-known heuristics:
//
// - Killer moves: if a quiet move caused a beta cutoff at a given ply
//   (distance from the root), it's remembered as a "killer" for that ply.
//   The same tactical idea (e.g. a defensive retreat, a blocking move)
//   often works again in a sibling branch at the same ply, so trying
//   killers early there tends to find cutoffs faster.
// - History heuristic: a running score per (piece, destination square),
//   bumped whenever a quiet move causes a cutoff, weighted by how much
//   depth was left (deeper cutoffs are stronger signals). Used to rank
//   the remaining quiet moves that aren't killers.
//
// One instance is created per top-level search call and passed down by
// reference through the recursion -- like TranspositionTable, it's local
// to that call, so there's no thread-safety concern.
struct SearchHeuristics {
    static constexpr int MAX_PLY = 64;

    MoveBB killers[MAX_PLY][2];
    int history[13][64] = {}; // index 0 (EMPTY) unused

    void recordKiller(int ply, const MoveBB& m) {
        if (ply < 0 || ply >= MAX_PLY) return;
        if (killers[ply][0].from == m.from && killers[ply][0].to == m.to)
            return; // already the top killer here
        killers[ply][1] = killers[ply][0];
        killers[ply][0] = m;
    }

    bool isKiller(int ply, const MoveBB& m) const {
        if (ply < 0 || ply >= MAX_PLY) return false;
        return (killers[ply][0].from == m.from && killers[ply][0].to == m.to) ||
               (killers[ply][1].from == m.from && killers[ply][1].to == m.to);
    }

    void recordHistory(const MoveBB& m, int depth) {
        history[m.piece][m.to] += depth * depth;
    }

    int historyScore(const MoveBB& m) const {
        return history[m.piece][m.to];
    }
};

// Score used for a checkmate. Large enough to dominate any material
// evaluation, but far below the +/-1000000 search bounds so that
// mate-distance adjustments (MATE_SCORE - ply) can't overflow past them.
static constexpr int MATE_SCORE = 900000;

// True if `score` represents a forced mate rather than a positional
// evaluation -- useful for the GUI ("mate in N") and for avoiding storing
// depth-relative mate scores as if they were absolute evaluations.
inline bool isMateScore(int score) {
    return score > MATE_SCORE - 1000 || score < -MATE_SCORE + 1000;
}

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
    void orderMoves(std::vector<MoveBB>& moves) const;

    // --- Evaluation & search ---
    int evaluateBoardBB() const;
    int negamaxBB(int depth, int alpha, int beta, bool isWhite, TranspositionTable& tt,
                  SearchHeuristics& heur, int ply);
    int quiescenceBB(int alpha, int beta, bool isWhite, int qDepth);
    MoveBB findBestMoveBB(int depth, bool isWhite);
    MoveBB findBestMoveBB_MT(int depth, bool isWhite, unsigned numThreads = 0,
                              const MoveBB* preferredMove = nullptr);
    MoveBB findBestMoveBB_ID(int maxDepth, bool isWhite);
    MoveBB findBestMoveBB_ID_MT(int maxDepth, bool isWhite, unsigned numThreads = 0);

    // Searches for roughly `budgetMs` milliseconds instead of to a fixed
    // depth, and returns the best move found by the deepest iteration that
    // finished. This is what iterative deepening is actually for: a
    // complete, playable move is available after every iteration, so the
    // search can stop on a clock.
    //
    // Why this beats a fixed depth for difficulty levels: the cost of a
    // given depth varies enormously with the position (we measured ~1600x
    // between a bare endgame and a dense middlegame at the same depth), so
    // "depth 5" is instant in one position and seconds in another. A time
    // budget gives a consistent response time and simply reaches a deeper
    // depth when the position is simple.
    MoveBB findBestMoveBB_Timed(int budgetMs, bool isWhite,
                                unsigned numThreads = 0, int maxDepth = 12);

    // Assumed cost multiplier per extra ply, used to decide whether the
    // next iteration fits in the remaining time. Measured at roughly 5-7x
    // for this engine; 5 is slightly optimistic on purpose, so the budget
    // actually gets used rather than being left on the table.
    static constexpr int kPlyGrowthFactor = 5;

    // Depth actually reached by the last findBestMoveBB_Timed call.
    int getLastCompletedDepth() const { return _lastCompletedDepth; }

    // --- Game state ---
    // Distinguishes the three ways a position can be terminal. The GUI
    // needs this to show "checkmate"/"stalemate" instead of silently
    // refusing input when a side has no legal moves.
    enum class GameState { Ongoing, Checkmate, Stalemate };
    GameState getGameState(bool sideToMoveIsWhite) {
        if (!getLegalMovesBB(sideToMoveIsWhite).empty())
            return GameState::Ongoing;
        return isKingInCheckBB(sideToMoveIsWhite)
             ? GameState::Checkmate
             : GameState::Stalemate;
    }

    // --- Draw rules ---
    // Fifty-move rule: 100 halfmoves (50 full moves by each side) with no
    // capture and no pawn move. The counter is maintained by
    // makeMoveBB/undoMoveBB and reset by any irreversible move.
    bool isFiftyMoveDraw() const { return _halfmoveClock >= 100; }

    // Repetition: has the CURRENT position occurred earlier in the game?
    // Only positions with the same side to move can repeat, so we step
    // back through the history two plies at a time. History is cleared on
    // every irreversible move (capture or pawn move), because a position
    // before such a move can never recur -- which also keeps this scan
    // very short in practice.
    //
    // Inside the search a single repetition is treated as a draw (rather
    // than waiting for the third occurrence). That's the standard choice:
    // it detects the drawing line one repetition earlier, which is what
    // you want when deciding whether to ALLOW a repetition.
    bool isRepetition() const {
        // The last entry is the current position itself, so start two
        // before it and step by two to stay on the same side to move.
        for (int i = _posHistoryCount - 3; i >= 0; i -= 2)
            if (_posHistory[i] == zobristHash)
                return true;
        return false;
    }

    // Any automatic draw, from the search's point of view.
    bool isDrawByRule() const {
        return isFiftyMoveDraw() || isRepetition() || isInsufficientMaterial();
    }

    int getHalfmoveClock() const { return _halfmoveClock; }

    // Seeds the repetition history with positions that happened before the
    // current one. The GUI should call this for each earlier position when
    // restoring/replaying a game, so the search can see repetitions that
    // began before the current search root.
    void pushHistoryPosition(uint64_t hash) {
        if (_posHistoryCount < MAX_HISTORY)
            _posHistory[_posHistoryCount++] = hash;
    }
    void clearHistory() { _posHistoryCount = 0; _undoTop = 0; }

    // Draw by insufficient material: neither side can possibly deliver
    // mate (K vs K, K+minor vs K, K+minor vs K+minor).
    bool isInsufficientMaterial() const {
        if (whitePawns || blackPawns) return false;
        if (whiteRooks || blackRooks) return false;
        if (whiteQueen || blackQueen) return false;
        int wMinors = popcount64(whiteKnights) + popcount64(whiteBishops);
        int bMinors = popcount64(blackKnights) + popcount64(blackBishops);
        return wMinors <= 1 && bMinors <= 1;
    }

    // --- Position setup ---
    // Loads a position from FEN notation (e.g. the standard start position
    // is "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1").
    // Returns false and leaves the board untouched if the FEN is malformed.
    // Uses the placement, side to move, castling rights, en passant
    // target and halfmove clock. The fullmove counter is parsed but
    // ignored (the engine has no use for it).
    bool loadFEN(const std::string& fen);

    // --- Transposition table support ---
    uint64_t computeZobristHash() const;
    uint64_t getZobristHash() const { return zobristHash; } // for validation/testing

    // --- Benchmark instrumentation ---
    uint64_t nodeCount = 0;
    void resetNodeCount() { nodeCount = 0; }
    uint64_t getNodeCount() const { return nodeCount; }


    bool getSideToMove() const { return sideToMove; };

    // Read-only bitboard accessors. Useful for tooling (self-play
    // harnesses, alternative evaluations, position dumps) that needs to
    // inspect material without being a friend of this class.
    uint64_t getWhitePawnsBB()   const { return whitePawns; }
    uint64_t getWhiteKnightsBB() const { return whiteKnights; }
    uint64_t getWhiteBishopsBB() const { return whiteBishops; }
    uint64_t getWhiteRooksBB()   const { return whiteRooks; }
    uint64_t getWhiteQueenBB()   const { return whiteQueen; }
    uint64_t getWhiteKingBB()    const { return whiteKing; }
    uint64_t getBlackPawnsBB()   const { return blackPawns; }
    uint64_t getBlackKnightsBB() const { return blackKnights; }
    uint64_t getBlackBishopsBB() const { return blackBishops; }
    uint64_t getBlackRooksBB()   const { return blackRooks; }
    uint64_t getBlackQueenBB()   const { return blackQueen; }
    uint64_t getBlackKingBB()    const { return blackKing; }

    Piece pieceAt(int sq) const;

	// --- Visualization ---
    //std::vector<VisualPiece> getVisualPieces() const;
    //std::vector<VisualPiece> makeVisualPieces(gui::Image* images[12]) const;

    static int popLSB(uint64_t& bb);

    int getEnPassantSq() const { return _enPassantSq; }

    void setEnPassantSq(int sq) { _enPassantSq = sq; }

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

    int _enPassantSq = -1;

    // --- Draw-rule bookkeeping ---
    // Positions (Zobrist hashes) seen since the last irreversible move,
    // used for repetition detection. A fixed array rather than a vector
    // because ChessBoard is copied once per root move by the
    // multithreaded search, and a heap allocation there would show up.
    static constexpr int MAX_HISTORY = 256;
    uint64_t _posHistory[MAX_HISTORY] = {};
    int _posHistoryCount = 0;

    // Halfmoves since the last capture or pawn move (fifty-move rule).
    int _halfmoveClock = 0;

    // makeMoveBB pushes the previous values here so undoMoveBB can restore
    // them. Kept in ChessBoard rather than in MoveBB deliberately: MoveBB
    // is copied for every generated move at every node, and growing it
    // measurably slowed the search when tried.
    struct DrawUndo {
        int16_t prevHalfmoveClock;
        int16_t prevHistoryCount;
    };
    DrawUndo _undoStack[MAX_HISTORY] = {};
    int _undoTop = 0;

    // Incrementally maintained Zobrist hash of the current position.
    // Kept in sync by makeMoveBB/undoMoveBB. computeZobristHash() (public,
    // above) recomputes it from scratch and is used only to initialize
    // this value -- not called per-node during search anymore.
    uint64_t zobristHash = 0;

    int _lastCompletedDepth = 0;
};