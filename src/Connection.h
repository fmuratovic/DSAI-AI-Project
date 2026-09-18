#pragma once

#include <iostream>
#include <vector>
#include <limits>
#include <cmath>
#include <algorithm>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <random>
#include <functional>

#include <td/Types.h>
#include <gui/Canvas.h>
#include <gui/Thread.h>
#include <thread/Thread.h>
#include <gui/Image.h>
#include <gui/Shape.h>
#include <gui/Sound.h>
#include <gui/Application.h>

#include "ChessBoard.h" 
#include "Constants.h"

#include <sstream>


constexpr size_t N = 8;

// ---------------------------------------------------------------------
// TEST HOOK -- uncomment to start from a promotion test position instead
// of the normal opening setup. Remember to comment it out again before
// handing the project in.
//
// The position below gives, in one board:
//   * white pawn b7 -> b8      promotion by push        (picker draws DOWN)
//   * white pawn b7 x a8       promotion by capture
//   * black pawn g2 -> g1      promotion by push        (picker draws UP)
// so all three cases can be checked in a few clicks.
//
// Pick "Multiplayer" on the start screen when testing, so both sides are
// human and the bot doesn't move for you.
// ---------------------------------------------------------------------
//#define CHESS_TEST_PROMOTION

#ifdef CHESS_TEST_PROMOTION
static const char* kTestPromotionFEN = "n3k3/1P6/8/8/8/8/6p1/4K3 w - - 0 1";
#endif

class Chess
{
public:
    // Kept same enum names so your existing external GUI wrapper still calls
    // canDragPiece/testDrag/finishUserMove exactly like professor’s version.
    enum class UserMove : td::BYTE { None = 0, ClickedBK, DragBK };

    // How the game has ended, if it has. Declared up here because member
    // function RETURN TYPES are resolved at their declaration point (unlike
    // function bodies, which are compiled as though the class were already
    // complete) -- so this must appear before computeStatusUnlocked().
    enum class Status {
        Ongoing,
        WhiteWinsCheckmate,
        BlackWinsCheckmate,
        DrawStalemate,
        DrawFiftyMove,
        DrawRepetition,
        DrawInsufficientMaterial
    };

private:

    int _selectedSq = -1;
    uint64_t _selectedMovesMask = 0ULL; // bitboard of target squares
    bool _whiteToMove = true;           // or read from board.sideToMove

    int _botDepth = 4;      // fallback when no time budget is set
    int _botBudgetMs = 0;   // >0 => search on a clock instead of to a depth

    // --- Bot pacing ---
    // A reply that lands the instant you release the mouse reads as a bug
    // rather than as a move. A FLAT floor isn't enough though: in sparse
    // endgames the search finishes in well under 100ms, so the bot would
    // answer near-instantly on Expert while taking a second in the
    // middlegame -- the pace would lurch about as the game simplified.
    //
    // So the floor is a fraction of the difficulty's own time budget. The
    // bot then feels roughly as deliberate as its level implies, whatever
    // the position, and Easy still stays snappy.
    static constexpr double kMinThinkFraction = 0.65;
    static constexpr int kMinThinkFloorMs = 160;   // used when no budget is set

    // Small random variation on top, so the pace isn't metronomic. A bot
    // that answers after exactly the same interval every single move feels
    // more mechanical than one that varies a little.
    static constexpr double kThinkJitter = 0.18;   // +/- 18%

    // Used if the GUI somehow never set a budget, so the bot can't end up
    // quietly running on the old fixed-depth path with no pacing.
    static constexpr int kDefaultBudgetMs = 350;

    int minBotThinkMs() const
    {
        const int base = (_botBudgetMs > 0)
            ? (int)(_botBudgetMs * kMinThinkFraction)
            : kMinThinkFloorMs;
        return (base < kMinThinkFloorMs) ? kMinThinkFloorMs : base;
    }

    // Which piece a promoting pawn becomes. The engine generates all four
    // options, but a click only carries a from/to square -- there's no way
    // to express "and promote to a rook" in a single click. Rather than
    // silently always queening (the previous behaviour, since queen moves
    // are generated first), the choice is a setting the player controls.
    Piece _promotionChoiceWhite = WQUEEN;
    Piece _promotionChoiceBlack = BQUEEN;

    // --- In-game promotion picker ---
    // When a human move would promote, the move is NOT played immediately.
    // Instead the four candidate moves are held here and a chooser is drawn
    // over the board; the next click selects one. A click carries only a
    // destination square, so there is no other way for the player to say
    // "promote to a rook" -- previously this silently used a preset.
    bool _promotionPending = false;
    int  _promoTo = -1;
    std::vector<MoveBB> _promoOptions;

    bool _mouseIsDown = false;

    bool _pressActive = false;
    bool _didDrag = false;
    gui::Point _pressPt;
    int _pressSq = -1;

    static constexpr float DRAG_THRESH = 3.0f; // pixels

    int _lastMoveFrom = -1;
    int _lastMoveTo = -1;

    std::vector<std::pair<MoveBB, int>> _moveHistory; // move + enPassantSq before move

    std::vector<std::pair<MoveBB, int>> _redoStack;




    bool _vsBot = false;
    bool _botIsWhite = false;   // common: bot plays black
    // Written by the bot worker thread, read by the UI thread every frame
    // (isBotThinking / maybeBotMove / onMouseDown). A plain bool here is a
    // data race: the compiler may cache it in a register and the UI thread
    // can miss the update, leaving the board permanently "thinking".
    std::atomic<bool> _botThinking{ false };  // prevents double moves

    std::function<void()> _onBotMoveDone;
    gui::thread::MainThreadSharedFunction1 _botCallBack;


    // -----------------------------
    // Engine (bitboard)
    // -----------------------------
    ChessBoard _board;

    // -----------------------------
    // GUI resources/images
    // -----------------------------
    gui::Image _imgBoard;

    // If you still use resource IDs like ":whiteKing" keep these.
    // Loaded by resource id (see res/main.xml) so the app stays
    // relocatable on Windows, Linux and macOS.
    gui::Image _blackKing;
    gui::Image _blackQueen;
    gui::Image _blackRook;
    gui::Image _blackBishop;
    gui::Image _blackKnight;
    gui::Image _blackPawn;

    gui::Image _whiteKing;
    gui::Image _whiteQueen;
    gui::Image _whiteRook;
    gui::Image _whiteBishop;
    gui::Image _whiteKnight;
    gui::Image _whitePawn;

    // -----------------------------
    // Threading
    // -----------------------------
    std::thread _workingThread;
    std::mutex _mutex;

    // Drives the think-time jitter. Touched only from the bot worker, and
    // only one of those runs at a time (_botThinking guards that), so no
    // locking is needed.
    std::mt19937 _pacingRng{ std::random_device{}() };

    // -----------------------------
    // Board placement / sizing
    // -----------------------------
    gui::Rect _boardPlacement;
    gui::Size _boardImgSize;
    gui::Point _ptOrig;
    gui::Point _dragStartPoint;
    gui::Point _currentDragPoint;
    gui::Size _cellSize;

    const float _originXMargin = 0.045f;
    const float _originYMargin = 0.045f;

    // --- Board appearance ---
    // Set to true to use the bitmap board image again instead of drawing
    // the squares. The drawn version is sharper at every window size.
    bool _useImageBoard = false;

    // Gap between the window edge and the board.
    static constexpr gui::CoordType kBoardMargin = 12.0;

    // --- Captured-piece side panels ---
    // These are drawn in whatever horizontal space is left over once the
    // board has been sized (the board is limited by window HEIGHT, so a
    // wide or maximised window leaves a column free on each side). They
    // appear only when that leftover is genuinely wide enough, so a
    // narrow/windowed layout is unaffected and nothing ever overlaps the
    // board.
    // Panel width as a fraction of the board's side. 0.15 makes each icon
    // roughly 55% of a square, which reads clearly without stealing much
    // board. Raise for bigger captured pieces and a smaller board.
    // Margin around each piece inside its square, as a fraction of the
    // square. Purely visual breathing room -- hit-testing still uses the
    // full square, so clicking near a square's edge still selects it.
    static constexpr double kPieceInset = 0.045;

    static constexpr double kPanelFraction = 0.15;
    static constexpr gui::CoordType kMinPanelWidth = 30.0;  // below this, no panels
    static constexpr gui::CoordType kPanelGap = 10.0;       // board <-> panel

    // Dark squares. Only these ColorIDs are known to exist in this
    // framework build (White/Blue/Green/Yellow are used elsewhere in the
    // project), so the dark squares are a translucent green over the white
    // base -- the familiar green-and-cream look. Lower the alpha for a
    // paler board, raise it for more contrast.
    static constexpr float kDarkSquareAlpha = 0.55f;
    static constexpr td::ColorID kDarkSquareColor = td::ColorID::Green;

    // Colour used to flag the king when it is in check. This is the only
    // ColorID here that isn't already used somewhere else in the project,
    // so if td::ColorID::Red doesn't exist in this framework build, change
    // just this line (Blue and Yellow are both known to work).
    static constexpr td::ColorID kCheckColor = td::ColorID::Red;

    UserMove _userMove = UserMove::None;

    bool _playing = false;
    bool _stopPlaying = false;
    bool _calcBoardPlacement = true;

    gui::Size _viewSize;
    int _depth;

    // -----------------------------
    // Dragging state (bitboard square index)
    // -----------------------------
    int _dragFromSq = -1;
    
private:
    // -----------------------------
    // Geometry helpers
    // -----------------------------
    void getCellRect(gui::Rect& rect, td::INT2 x, td::INT2 y) const
    {
        gui::Point tl(_ptOrig);
        tl.x += x * _cellSize.width;
        tl.y -= y *_cellSize.height;

        rect.left = tl.x;
        rect.top = tl.y;
        rect.right = tl.x + _cellSize.width;
        rect.bottom = tl.y + _cellSize.height;
    }

    td::Point<td::INT2> getCellCoordinate(const gui::Point& cursorPoint) const
    {
        float fx = (cursorPoint.x - _ptOrig.x) / _cellSize.width;
        float fy = (_ptOrig.y - cursorPoint.y) / _cellSize.height;

        td::INT2 x;
        td::INT2 y;

        if (fx < 0.)
            x = 0;
        else
            x = static_cast<td::INT2>(fx);

        if (fy < 0.)
            y = 0;
        else
            y = static_cast<td::INT2>(fy) + 1;

        return { x, y };
    }

    // Convert mouse point -> bitboard square (A1=0 .. H8=63)
    int pointToSq(const gui::Point& cursorPoint) const
    {
        // 1. Calculate how many cells we are from the origin (A1 area)
        float fx = (cursorPoint.x - _ptOrig.x) / _cellSize.width;

        // 2. Since Y decreases as we go up in GUI terms, 
        // we use (OriginY - CursorY) to get a positive value going up.
        float fy = (_ptOrig.y - cursorPoint.y) / _cellSize.height;

        int file = (int)std::floor(fx);
        int rank = (int)std::floor(fy + 1.0f); // This aligns with your draw() logic offset

        // 3. Boundary check
        if (file < 0 || file > 7 || rank < 0 || rank > 7) return -1;

        // 4. Standard Index: Rank * 8 + File
        // If Rank 1 is the bottom, it must result in 0-7.
        return rank * 8 + file;
    }


    static inline bool isWhitePiece(Piece p) { return p >= WPAWN && p <= WKING; }
    static inline bool isBlackPiece(Piece p) { return p >= BPAWN && p <= BKING; }

    void drawPiece(Piece p, const gui::Rect& rect)
    {
        switch (p)
        {
        case WKING:   _whiteKing.draw(rect); break;
        case WQUEEN:  _whiteQueen.draw(rect); break;
        case WROOK:   _whiteRook.draw(rect); break;
        case WBISHOP: _whiteBishop.draw(rect); break;
        case WKNIGHT: _whiteKnight.draw(rect); break;
        case WPAWN:   _whitePawn.draw(rect); break;

        case BKING:   _blackKing.draw(rect); break;
        case BQUEEN:  _blackQueen.draw(rect); break;
        case BROOK:   _blackRook.draw(rect); break;
        case BBISHOP: _blackBishop.draw(rect); break;
        case BKNIGHT: _blackKnight.draw(rect); break;
        case BPAWN:   _blackPawn.draw(rect); break;

        default: break;
        }
    }

    // Worker thread: find best move and notify UI via callback
    void findBestMove(const gui::thread::MainThreadSharedFunction1& callBack)
    {
        // Optional “stop requested” handling
        if (_stopPlaying)
        {
            td::INT4 iMsg = td::INT4(Message::Stop);
            td::Variant varMsg(iMsg);
            gui::thread::asyncExecInMainThread(callBack, varMsg);
            _playing = false;
            _stopPlaying = false;
            return;
        }
        bool sideToMove = _board.getSideToMove();
        bool isWhite = sideToMove;

        auto legal = _board.getLegalMovesBB(isWhite);
        if (legal.empty())
        {
            // No legal moves => mate/stalemate. Stop gracefully.
            td::INT4 iMsg = td::INT4(Message::Stop);
            td::Variant varMsg(iMsg);
            gui::thread::asyncExecInMainThread(callBack, varMsg);
            _playing = false;
            return;
        }

        MoveBB best = _board.findBestMoveBB_ID_MT(_depth, isWhite);
        _board.makeMoveBB(best);

        td::INT4 iMsg = td::INT4(Message::AIMove);
        td::Variant varMsg(iMsg);
        gui::thread::asyncExecInMainThread(callBack, varMsg);
    }

public:
    Chess(int depth)
        : _imgBoard(":board")

        // If you use resources:
        , _blackKing(":blackKing")
        , _blackQueen(":blackQueen")
        , _blackRook(":blackRook")
        , _blackBishop(":blackBishop")
        , _blackKnight(":blackKnight")
        , _blackPawn(":blackPawn")

        , _whiteKing(":whiteKing")
        , _whiteQueen(":whiteQueen")
        , _whiteRook(":whiteRook")
        , _whiteBishop(":whiteBishop")
        , _whiteKnight(":whiteKnight")
        , _whitePawn(":whitePawn")

        , _depth(depth)
    {
        reset(true);
        _imgBoard.getSize(_boardImgSize);
    }

    // -----------------------------
    // Locking
    // -----------------------------
    void lock() { _mutex.lock(); }
    void unlock() { _mutex.unlock(); }

    // -----------------------------
    // Reset
    // -----------------------------
    void reset(bool setToInitialState = false)
    {
        // Same race as undo/redo: never rebuild the board under the bot.
        if (_botThinking) { gui::Sound::play(gui::Sound::Type::Beep); return; }
        std::lock_guard<std::mutex> lock(_mutex);

        cancelPromotion();
        _stopPlaying = false;
        _playing = false;
        _userMove = UserMove::None;
        _dragFromSq = -1;
        _calcBoardPlacement = true;

        if (setToInitialState)
        {
            _board = ChessBoard(); // starting position
#ifdef CHESS_TEST_PROMOTION
            _board.loadFEN(kTestPromotionFEN);
#endif
            _moveHistory.clear();
            _redoStack.clear();
            _lastMoveFrom = -1;
            _lastMoveTo = -1;
        }
    }

    // -----------------------------
    // Drawing (called by your wrapper)
    // -----------------------------
    // Caller must already hold _mutex (or be on a path where the board
    // cannot be mutated concurrently). Kept separate from getStatus() so
    // internal callers that already hold the lock don't deadlock on it.
    Status computeStatusUnlocked()
    {
        const bool sideToMove = _board.getSideToMove();

        if (_board.getLegalMovesBB(sideToMove).empty())
        {
            if (_board.isKingInCheckBB(sideToMove))
                return sideToMove ? Status::BlackWinsCheckmate   // white is mated
                                  : Status::WhiteWinsCheckmate;  // black is mated
            return Status::DrawStalemate;
        }

        // Order matters only for which message the player sees; all three
        // are equally drawn.
        if (_board.isInsufficientMaterial()) return Status::DrawInsufficientMaterial;
        if (_board.isFiftyMoveDraw())        return Status::DrawFiftyMove;
        if (_board.isRepetition())           return Status::DrawRepetition;

        return Status::Ongoing;
    }

    // Finds the legal move matching from/to. When several match they are
    // promotion alternatives (same squares, different promotion piece), so
    // the player's configured preference decides which one is played.
    const MoveBB* pickMove(const std::vector<MoveBB>& moves, int from, int to) const
    {
        const MoveBB* first = nullptr;
        const Piece want = _board.getSideToMove() ? _promotionChoiceWhite
                                                  : _promotionChoiceBlack;
        for (const auto& mv : moves)
        {
            if (mv.from != from || mv.to != to) continue;
            if (!first) first = &mv;                  // fallback: first match
            if (mv.promotion && mv.promotionPiece == want) return &mv;
            if (!mv.promotion) return &mv;            // ordinary move
        }
        return first;
    }

    void maybeBotMove()
    {
        if (!_vsBot) return;
        if (_botThinking) return;
        bool sideNow = _board.getSideToMove();
        if (sideNow != _botIsWhite) return;

        _botThinking = true;

        _workingThread = std::thread([this]() {
            // Was an unconditional 500ms sleep, which added half a second
            // to EVERY bot move on top of the search itself -- a large
            // part of why the harder levels felt unplayable. Now the
            // search runs first and we only pad out to a small minimum,
            // so a fast reply still doesn't appear instantaneously but a
            // slow one isn't punished twice.
            const auto botStart = std::chrono::steady_clock::now();

            // ---- Phase 1: take a COPY of the position. ----
            // The lock is held only for the copy, never for the search.
            // Two reasons this matters:
            //
            //  * The UI thread needs the lock too (the window title asks
            //    for the game status). Holding it across the whole search
            //    froze the UI for the bot's entire think time, so the
            //    human's own move never got painted -- the screen stalled
            //    and then both moves appeared at once.
            //  * The search transiently make/unmakes moves on the board it
            //    is given. draw() reads the board without locking, so
            //    searching on the live board risks painting a half-made
            //    position.
            //
            // Searching a copy removes both problems: _board is untouched
            // from the moment the human moves until the bot's move is
            // applied.
            ChessBoard searchBoard;
            bool shouldSearch = false;
            {
                std::lock_guard<std::mutex> lock(_mutex);
                auto legal = _board.getLegalMovesBB(_botIsWhite);
                // Also stop on draws: without this the bot keeps "playing"
                // a game that is already drawn by repetition or the
                // fifty-move rule.
                if (!legal.empty() && computeStatusUnlocked() == Status::Ongoing) {
                    searchBoard = _board;
                    shouldSearch = true;
                }
            }

            // ---- Phase 2: SEARCH, unlocked, on the copy. ----
            MoveBB best;
            bool haveMove = false;
            if (shouldSearch)
            {
                // A time budget gives a predictable response time; a fixed
                // depth does not, because the same depth costs wildly
                // different amounts in different positions.
                const int budget = (_botBudgetMs > 0) ? _botBudgetMs : kDefaultBudgetMs;
                best = searchBoard.findBestMoveBB_Timed(budget, _botIsWhite);
                haveMove = true;
            }

            // ---- Phase 3: PACING. Board still untouched, so this delay
            // is what the player actually experiences. ----
            if (haveMove)
            {
                int target = minBotThinkMs();

                // Jitter the target so successive moves don't all land on
                // the same interval.
                {
                    std::uniform_real_distribution<double> dist(-kThinkJitter, kThinkJitter);
                    target = (int)(target * (1.0 + dist(_pacingRng)));
                    if (target < 0) target = 0;
                }

                const int spent = (int)std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - botStart).count();
                if (spent < target)
                    std::this_thread::sleep_for(std::chrono::milliseconds(target - spent));
            }

            // ---- Phase 4: APPLY the move. ----
            // Safe to re-lock: the human can't have moved in between,
            // because onMouseDown bails out while _botThinking is set.
            if (haveMove)
            {
                std::lock_guard<std::mutex> lock(_mutex);
                _board.makeMoveBB(best);
                _moveHistory.push_back({ best, _board.getEnPassantSq() });
                _redoStack.clear();
                _lastMoveFrom = best.from;
                _lastMoveTo = best.to;
            }

            _botThinking = false;
            td::INT4 iMsg = td::INT4(Message::BotMove);
            td::Variant varMsg(iMsg);
            gui::thread::asyncExecInMainThread(_botCallBack, varMsg);
            });
        _workingThread.detach();
    }



    // Captured pieces, derived from _moveHistory rather than kept in a
    // parallel list. That way undo/redo need no extra bookkeeping: the
    // history IS the source of truth, so shrinking it automatically
    // un-captures the piece.
    void collectCaptured(std::vector<Piece>& takenByWhite,
                         std::vector<Piece>& takenByBlack) const
    {
        takenByWhite.clear();
        takenByBlack.clear();

        for (const auto& entry : _moveHistory)
        {
            const MoveBB& m = entry.first;

            Piece victim = m.captured;
            if (m.flags & EN_PASSANT)      // en passant victim isn't on m.to
                victim = (m.piece <= WKING) ? BPAWN : WPAWN;

            if (victim == EMPTY) continue;

            if (m.piece <= WKING) takenByWhite.push_back(victim);
            else                  takenByBlack.push_back(victim);
        }

        // Heaviest first, so the panel reads like a normal material list.
        auto weight = [](Piece p) {
            switch (p) {
            case WQUEEN: case BQUEEN:   return 5;
            case WROOK: case BROOK:     return 4;
            case WBISHOP: case BBISHOP: return 3;
            case WKNIGHT: case BKNIGHT: return 2;
            default:                    return 1;
            }
        };
        auto cmp = [&](Piece a, Piece b) { return weight(a) > weight(b); };
        std::sort(takenByWhite.begin(), takenByWhite.end(), cmp);
        std::sort(takenByBlack.begin(), takenByBlack.end(), cmp);
    }

    // Draws one column of captured pieces inside `panel`, top-down.
    // `anchorBottom` grows the list upward from the bottom of the panel
    // instead of downward from the top. Each player's captures are anchored
    // to their OWN end of the board -- white's beside the white army at the
    // bottom, black's beside the black army at the top. With both lists
    // starting at the top, a single captured piece floated in the corner
    // looking unrelated to anything; anchored, it reads as belonging to a
    // player and the two lists grow towards each other.
    void drawCapturedPanel(const gui::Rect& panel, const std::vector<Piece>& pieces,
                           gui::CoordType icon, bool anchorBottom)
    {
        if (pieces.empty()) return;

        const gui::CoordType panelW = panel.right - panel.left;
        const gui::CoordType panelH = panel.bottom - panel.top;
        if (panelW <= 0 || panelH <= 0) return;

        // Two per row when the panel allows it, otherwise one.
        const int perRow = (panelW >= icon * 2.0) ? 2 : 1;

        // A full side can lose 15 pieces; make sure they all fit the
        // column height even if that means shrinking below the requested
        // icon size.
        const int rowsNeeded = (15 + perRow - 1) / perRow;
        icon = std::min(icon, panelH / (rowsNeeded * 1.05));
        icon = std::min(icon, panelW / perRow);
        if (icon < 6.0) return;             // too small to be legible

        const gui::CoordType rowH = icon * 1.05;
        const int maxRows = (int)(panelH / rowH);
        if (maxRows <= 0) return;

        const gui::CoordType blockW = icon * perRow;
        const gui::CoordType x0 = panel.left + (panelW - blockW) * 0.5;

        for (size_t i = 0; i < pieces.size(); i++)
        {
            const int row = (int)(i / perRow);
            if (row >= maxRows) break;      // silently stop rather than overflow
            const int col = (int)(i % perRow);

            gui::Rect rc;
            rc.left = x0 + col * icon;
            rc.right = rc.left + icon;
            if (anchorBottom)
            {
                rc.bottom = panel.bottom - row * rowH;
                rc.top = rc.bottom - icon;
            }
            else
            {
                rc.top = panel.top + row * rowH;
                rc.bottom = rc.top + icon;
            }
            drawPiece(pieces[i], rc);
        }
    }

    // The four choices, strongest first.
    void getPromotionOrder(Piece out[4], bool isWhite) const
    {
        if (isWhite) { out[0]=WQUEEN; out[1]=WROOK; out[2]=WBISHOP; out[3]=WKNIGHT; }
        else         { out[0]=BQUEEN; out[1]=BROOK; out[2]=BBISHOP; out[3]=BKNIGHT; }
    }

    // Lays the four choices out along the file of the promotion square,
    // running back onto the board so they're always visible: downward from
    // rank 8 for white, upward from rank 1 for black.
    void getPromotionOptionRects(gui::Rect out[4]) const
    {
        const int file = _promoTo & 7;
        const int rank = _promoTo >> 3;
        const bool fromTop = (rank == 7); // white promotes on the top rank

        for (int i = 0; i < 4; i++)
        {
            int r = fromTop ? (rank - i) : (rank + i);
            if (r < 0) r = 0;
            if (r > 7) r = 7;
            getCellRect(out[i], (td::INT2)file, (td::INT2)r);
        }
    }

    // If this move is a promotion there will be four legal moves sharing
    // the same from/to. Stash them and let the player choose.
    bool beginPromotionIfNeeded(const std::vector<MoveBB>& moves, int from, int to)
    {
        std::vector<MoveBB> promos;
        for (const auto& m : moves)
            if (m.from == from && m.to == to && m.promotion)
                promos.push_back(m);

        if (promos.size() < 2) return false;   // not a promotion

        _promoOptions = std::move(promos);
        _promoTo = to;
        _promotionPending = true;
        return true;
    }

    void cancelPromotion()
    {
        _promotionPending = false;
        _promoTo = -1;
        _promoOptions.clear();
    }

    // Returns true if the click landed on one of the choices and the move
    // was played.
    bool handlePromotionClick(const gui::Point& p)
    {
        gui::Rect rects[4];
        getPromotionOptionRects(rects);

        Piece order[4];
        getPromotionOrder(order, _board.getSideToMove());

        for (int i = 0; i < 4; i++)
        {
            const gui::Rect& r = rects[i];
            if (p.x < r.left || p.x > r.right || p.y < r.top || p.y > r.bottom)
                continue;

            for (const auto& m : _promoOptions)
            {
                if (m.promotionPiece != order[i]) continue;

                _board.makeMoveBB(m);
                _moveHistory.push_back({ m, _board.getEnPassantSq() });
                _redoStack.clear();
                _lastMoveFrom = m.from;
                _lastMoveTo = m.to;

                cancelPromotion();
                _selectedSq = -1;
                _selectedMovesMask = 0ULL;
                _userMove = UserMove::None;
                _dragFromSq = -1;

                maybeBotMove();
                return true;
            }
        }
        return false;
    }

    // Drawn last, on top of the pieces, so the chooser is never obscured.
    void drawPromotionPicker()
    {
        if (!_promotionPending) return;

        gui::Rect rects[4];
        getPromotionOptionRects(rects);

        Piece order[4];
        getPromotionOrder(order, _board.getSideToMove());

        for (int i = 0; i < 4; i++)
        {
            // Near-opaque panel so the piece underneath doesn't show through
            // and make the choice ambiguous.
            gui::Shape::drawRect(rects[i], 0.92f, td::ColorID::White);
            gui::Shape::drawRect(rects[i], td::ColorID::Black, 1.5f);
            drawPiece(order[i], rects[i]);
        }
    }

    void draw()
    {
        gui::Point pt(0, 0);
        gui::Rect r(pt, _viewSize);

        // =========================
        // 1) BOARD
        // =========================
        // The board is drawn as 64 filled rectangles rather than as a
        // bitmap. A scaled JPEG is resampled every frame, which is what
        // made the board look soft -- especially at window sizes that
        // aren't an exact multiple of the source image. Vector fills stay
        // pixel-crisp at any size and any DPI, and the square grid lines
        // up exactly with the hit-testing maths instead of relying on the
        // image's border happening to sit at a fixed 4.5% margin.
        //
        // Set _useImageBoard = true to go back to the bitmap.
        // Board size, with permanent room reserved for the captured-piece
        // panels. Panel width is a FRACTION of the board, so everything --
        // panels, icons, squares -- scales together: maximise the window
        // and the captured pieces get bigger along with the board.
        //
        // The two depend on each other (board size limits panel width,
        // reserved panels limit board size), so solve it directly rather
        // than iterating:
        //     board + 2*(f*board + gap) <= usableWidth
        //  => board <= (usableWidth - 2*gap) / (1 + 2f)
        // and the board is independently capped by the window height.
        const gui::CoordType usableW = _viewSize.width - 2.0 * kBoardMargin;
        const gui::CoordType usableH = _viewSize.height - 2.0 * kBoardMargin;

        gui::CoordType boardSide = std::min(
            usableH,
            (usableW - 2.0 * kPanelGap) / (1.0 + 2.0 * kPanelFraction));

        // Very small windows: giving up ~24% of the board to panels would
        // leave the squares unusably small, so fall back to a full-width
        // board and no panels.
        gui::CoordType panelW = kPanelFraction * boardSide;
        bool showPanels = (panelW >= kMinPanelWidth);
        if (!showPanels)
        {
            boardSide = std::min(usableW, usableH);
            panelW = 0.0;
        }

        if (boardSide < 8.0) boardSide = 8.0;

        // Snap the board to a whole number of pixels per cell. Without
        // this, cell edges land on fractional pixels and neighbouring
        // squares get a faint seam or a doubled edge line.
        gui::CoordType cell = std::floor(boardSide / (gui::CoordType)N);
        if (cell < 1.0) cell = 1.0;
        boardSide = cell * (gui::CoordType)N;

        gui::CoordType boardLeft = std::floor((_viewSize.width - boardSide) * 0.5);
        gui::CoordType boardTop = std::floor((_viewSize.height - boardSide) * 0.5);
        gui::Rect rMatrix(boardLeft, boardTop, boardLeft + boardSide, boardTop + boardSide);

        _boardPlacement = rMatrix;
        _ptOrig = { boardLeft, boardTop + boardSide - cell };
        _cellSize = { cell, cell };

        if (_useImageBoard)
        {
            _imgBoard.draw(rMatrix);
        }
        else
        {
            // Light base, then only the dark squares on top: half as many
            // fills as colouring every square individually.
            gui::Shape::drawRect(rMatrix, 1.0f, td::ColorID::White);

            for (int sq = 0; sq < 64; sq++)
            {
                int file = sq & 7;
                int rank = sq >> 3;
                if (((file + rank) & 1) != 0) continue;   // light square, already painted

                gui::Rect rc;
                getCellRect(rc, (td::INT2)file, (td::INT2)rank);
                gui::Shape::drawRect(rc, kDarkSquareAlpha, kDarkSquareColor);
            }
        }

        // Frame: a wide, faint "mat" just outside the squares, then a
        // single hairline on the edge itself. A single hard 2px white line
        // against a dark window reads as a harsh cut-out; a graded border
        // lets the board sit on the background instead of being stamped
        // onto it.
        {
            const gui::CoordType b1 = std::max<gui::CoordType>(3.0, cell * 0.09);
            gui::Rect mat(rMatrix.left - b1, rMatrix.top - b1,
                          rMatrix.right + b1, rMatrix.bottom + b1);
            gui::Shape::drawRect(mat, 0.22f, td::ColorID::White);
            gui::Shape::drawRect(rMatrix, td::ColorID::White, 1.0f);
        }

        // =========================
        // 2) HIGHLIGHTS
        // =========================

        auto fillSquare = [&](int sq, float alpha, td::ColorID color)
        {
            gui::Rect rc;
            getCellRect(rc, (td::INT2)(sq & 7), (td::INT2)(sq >> 3));
            gui::Shape::drawRect(rc, alpha, color);
        };

        // Last move: a soft wash plus a thin outline, rather than the flat
        // saturated yellow block this used to paint. The outline is what
        // makes it read as "these two squares" instead of looking like a
        // misplaced sticker over the board.
        // Fills only -- no outline. The hard yellow border this used to add
        // fought with the piece artwork and with the board's own grid, and
        // on a light square it looked like a sticker laid over the board.
        // A slightly stronger wash on the destination is enough to show
        // the direction of the move.
        if (_lastMoveFrom != -1) fillSquare(_lastMoveFrom, 0.17f, td::ColorID::Yellow);
        if (_lastMoveTo != -1)   fillSquare(_lastMoveTo, 0.30f, td::ColorID::Yellow);

        // King in check: unmistakable, and it's information the player
        // currently has to work out for themselves.
        {
            const bool stm = _board.getSideToMove();
            if (_board.isKingInCheckBB(stm))
            {
                uint64_t kingBB = stm ? _board.getWhiteKingBB() : _board.getBlackKingBB();
                if (kingBB)
                {
                    int kSq = ChessBoard::popLSB(kingBB);
                    if (kSq >= 0) fillSquare(kSq, 0.45f, kCheckColor);
                }
            }
        }

        // Selected square
        if (_selectedSq != -1)
            fillSquare(_selectedSq, 0.32f, td::ColorID::Blue);

        // Legal moves: a small dot for a quiet move, a ring for a capture.
        // Previously every target got the same dot, so you couldn't tell
        // at a glance which moves win material -- and a dot centred on an
        // enemy piece partly hides the piece you're about to take.
        if (_selectedSq != -1 && _selectedMovesMask != 0ULL)
        {
            uint64_t mask = _selectedMovesMask;
            while (mask)
            {
                int to = ChessBoard::popLSB(mask);
                if (to < 0) break;

                gui::Rect rc;
                getCellRect(rc, (td::INT2)(to & 7), (td::INT2)(to >> 3));

                gui::CoordType cx = (rc.left + rc.right) * 0.5;
                gui::CoordType cy = (rc.top + rc.bottom) * 0.5;
                gui::CoordType minSide = std::min(_cellSize.width, _cellSize.height);

                const bool isCapture = (_board.pieceAt(to) != EMPTY);

                if (isCapture)
                {
                    // Outline the square rather than drawing a filled dot
                    // on top of it: the dot used to sit over the enemy
                    // piece and hide what you were about to capture.
                    // Inset slightly so the marker reads as its own ring
                    // instead of merging with the square boundary.
                    const gui::CoordType in = minSide * 0.06;
                    gui::Rect ring(rc.left + in, rc.top + in,
                                   rc.right - in, rc.bottom - in);
                    gui::Shape::drawRect(ring, td::ColorID::Green, 2.5f);
                }
                else
                {
                    gui::Circle c({ cx, cy }, minSide * 0.16);
                    gui::Shape sh;
                    sh.createCircle(c, 1.0f);
                    sh.drawFillAndWire(td::ColorID::Green, td::ColorID::Green);
                }
            }
        }

        // =========================
        // 4) DRAW PIECES
        // =========================
        for (int sq = 0; sq < 64; sq++)
        {
            Piece p = _board.pieceAt(sq);
            if (p == EMPTY) continue;

            int file = sq & 7;
            int rank = sq >> 3;
            int guiRow = rank;                 // ✅ DO NOT CHANGE (as requested)

            gui::Rect rect;
            getCellRect(rect, file, guiRow);

            // Inset the piece inside its square. Drawn edge-to-edge the
            // pieces touch the square boundaries and each other, which
            // makes a full board look cramped and muddies the checker
            // pattern. A small margin lets each piece sit ON its square.
            {
                const gui::CoordType pad = cell * kPieceInset;
                rect.left += pad; rect.top += pad;
                rect.right -= pad; rect.bottom -= pad;
            }

            // Drag visual: the piece follows the cursor.
            if (_userMove == UserMove::DragBK && sq == _dragFromSq)
            {
                auto off = _currentDragPoint - _dragStartPoint;
                rect += off;
            }

            drawPiece(p, rect);
        }

        // =========================
        // Captured pieces (side panels)
        // =========================
        // Only drawn when there is real space beside the board. The board
        // is sized by the window's HEIGHT, so in a maximised (wide) window
        // a column is free on each side, while in a narrow window the
        // leftover is ~0 and the panels simply don't appear. Nothing is
        // ever squeezed and the board never shrinks to make room.
        if (showPanels)
        {
            std::vector<Piece> takenByWhite, takenByBlack;
            collectCaptured(takenByWhite, takenByBlack);

            // Icon size is derived from the board, so it grows with the
            // window instead of staying a fixed pixel size.
            const gui::CoordType icon = panelW / 2.2;

            const gui::CoordType top = rMatrix.top;
            const gui::CoordType bot = rMatrix.bottom;

            // Left panel: what WHITE has captured (black pieces), on the
            // same side as the white army at the bottom. Right panel: what
            // black has captured.
            gui::Rect leftPanel(rMatrix.left - kPanelGap - panelW, top,
                                rMatrix.left - kPanelGap, bot);
            gui::Rect rightPanel(rMatrix.right + kPanelGap, top,
                                 rMatrix.right + kPanelGap + panelW, bot);

            // White plays from the bottom, so its captures stack upward
            // from the bottom-left; black's stack downward from the
            // top-right.
            drawCapturedPanel(leftPanel, takenByWhite, icon, true);
            drawCapturedPanel(rightPanel, takenByBlack, icon, false);
        }

        // While the bot is searching, the board simply ignores clicks with
        // no explanation. A thin border in the bot's colour makes it clear
        // the game is waiting on the engine, not on the player. Kept
        // deliberately subtle -- it repaints every frame at 60fps.
        if (_botThinking)
            gui::Shape::drawRect(rMatrix, td::ColorID::Blue, 4.0f);

        drawPromotionPicker();   // always last: must sit above the pieces
    }


    void onMouseDown(const gui::Point& p)
    {
        // A pending promotion swallows all input until it's resolved: the
        // player must pick a piece (or click away to cancel the move).
        if (_promotionPending)
        {
            if (handlePromotionClick(p)) return;

            // Clicked outside the chooser -> abandon the move entirely and
            // put the pawn back. Safer than silently defaulting to a queen,
            // which is the behaviour this whole picker exists to remove.
            cancelPromotion();
            _selectedSq = -1;
            _selectedMovesMask = 0ULL;
            _userMove = UserMove::None;
            _dragFromSq = -1;
            return;
        }

        // Once the game has ended, ignore board input entirely -- otherwise
        // the player can keep moving pieces around after checkmate.
        if (computeStatusUnlocked() != Status::Ongoing) return;

        if (_botThinking) return;

        bool sideToMove = _board.getSideToMove();
        if (_vsBot && !isHumanTurn()) return;

        int sq = pointToSq(p);
        if (sq < 0) return;

        // Case 1: Nothing selected yet — try to select a piece
        if (_selectedSq == -1)
        {
            Piece pc = _board.pieceAt(sq);
            if (pc == EMPTY) return;

            bool pcIsWhite = (pc >= WPAWN && pc <= WKING);
            if (pcIsWhite != sideToMove) return;

            if (_vsBot && (pcIsWhite != humanIsWhite())) return;

            _selectedSq = sq;
            _selectedMovesMask = 0ULL;

            auto moves = _board.getLegalMovesBB(sideToMove);
            for (const auto& m : moves)
                if (m.from == sq)
                    _selectedMovesMask |= (1ULL << m.to);

            // Arm the drag pipeline. Nothing visual happens yet -- the
            // piece only starts following the cursor once testDrag() sees
            // real movement -- so click-to-move still behaves as before.
            _dragFromSq = sq;
            _pressPt = p;
            _dragStartPoint = p;
            _currentDragPoint = p;
            _userMove = UserMove::ClickedBK;
            _didDrag = false;

            return;
        }

        // Case 2: Same square clicked — deselect
        if (sq == _selectedSq)
        {
            _selectedSq = -1;
            _selectedMovesMask = 0ULL;
            return;
        }

        // Case 3: A legal target square — execute the move
        if ((_selectedMovesMask >> sq) & 1ULL)
        {
            bool sideToMove = _board.getSideToMove();
            const MoveBB* chosen = nullptr;
            auto moves = _board.getLegalMovesBB(sideToMove);

            // Promotion? Show the chooser instead of playing immediately.
            if (beginPromotionIfNeeded(moves, _selectedSq, sq))
                return;

            chosen = pickMove(moves, _selectedSq, sq);

            if (chosen)
            {
                _board.makeMoveBB(*chosen);

                _moveHistory.push_back({ *chosen, _board.getEnPassantSq() });

                _redoStack.clear();

                _lastMoveFrom = _selectedSq;
                _lastMoveTo = sq;

                _selectedSq = -1;
                _selectedMovesMask = 0ULL;
                maybeBotMove();
            }
            return;
        }

        // Case 4: Clicked a different piece of same side — re-select it
        Piece pc = _board.pieceAt(sq);
        if (pc != EMPTY)
        {
            bool pcIsWhite = (pc >= WPAWN && pc <= WKING);
            if (pcIsWhite == sideToMove)
            {
                if (_vsBot && pcIsWhite != humanIsWhite()) {
                    _selectedSq = -1;
                    _selectedMovesMask = 0ULL;
                    return;
                }

                _selectedSq = sq;
                _selectedMovesMask = 0ULL;

                auto moves = _board.getLegalMovesBB(sideToMove);
                for (const auto& m : moves)
                    if (m.from == sq)
                        _selectedMovesMask |= (1ULL << m.to);

                return;
            }
        }

        // Case 5: Clicked empty or enemy square that isn't legal — deselect
        _selectedSq = -1;
        _selectedMovesMask = 0ULL;
    }

    // -----------------------------
    // Thread access
    // -----------------------------
    std::thread& getThread() { return _workingThread; }

    // -----------------------------
    // Playing control
    // -----------------------------
    void stopPlaying()
    {
        _stopPlaying = true;
        _playing = false;
    }

    bool isPlaying() const { return _playing; }

    void updateModelSize(const gui::Size& newSize)
    {
        _calcBoardPlacement = true;
        _viewSize = newSize;
    }

    // -----------------------------
    // Dragging: called by wrapper on mouse down
    // -----------------------------
    bool canDragPiece(const gui::Point& cursorPoint)
    {
        int sq = pointToSq(cursorPoint);
        if (sq < 0) return false;

        // bot mode: if it’s bot’s turn, human can’t drag
        if (_vsBot && (_board.getSideToMove() == _botIsWhite))
            return false;

        Piece pc = _board.pieceAt(sq);
        if (pc == EMPTY) return false;

        bool sideToMove = _board.getSideToMove();   // true=white
        bool pcIsWhite = isWhitePiece(pc);

        // must match side-to-move always
        if (pcIsWhite != sideToMove) return false;

        // ✅ bot mode: must be human’s color
        if (_vsBot)
        {
            bool humanIsWhite = !_botIsWhite;
            if (pcIsWhite != humanIsWhite) return false;
        }

        // keep your existing highlight/drag setup after this...
        _selectedSq = sq;
        _selectedMovesMask = 0ULL;

        auto moves = _board.getLegalMovesBB(sideToMove);
        for (const auto& m : moves)
            if (m.from == sq)
                _selectedMovesMask |= (1ULL << m.to);

        _dragFromSq = sq;
        _dragStartPoint = cursorPoint;
        _currentDragPoint = cursorPoint;
        _userMove = UserMove::ClickedBK;

        return true;
    }





    // Called by wrapper on mouse up
    bool finishUserMove(const gui::Point& cursorPoint)
    {
        bool moved = false;

        // A press-and-release without movement is a CLICK, not a drag:
        // leave the selection in place so the second click can choose the
        // destination. Without this check, releasing the button after
        // clicking a piece would immediately clear the selection and
        // click-to-move would stop working entirely.
        if (_userMove != UserMove::DragBK)
        {
            _userMove = (_selectedSq == -1) ? UserMove::None : UserMove::ClickedBK;
            return false;
        }

        if (_selectedSq == -1) return false;

        int toSq = pointToSq(cursorPoint);
        if (toSq < 0) return false;

        // Dropped on a square that isn't a legal target: snap the piece
        // back but KEEP it selected, so the player can just click the
        // square they meant instead of starting over.
        if (((_selectedMovesMask >> toSq) & 1ULL) == 0ULL) {
            _userMove = UserMove::None;
            _dragFromSq = -1;
            _didDrag = false;
            return false;
        }

        bool sideToMove = _board.getSideToMove();
        const MoveBB* chosen = nullptr;
        auto moves = _board.getLegalMovesBB(sideToMove);

        // Promotion? Leave the move unplayed and let the chooser resolve it.
        // Returning false keeps the caller from treating this as a
        // completed move; the picker will finish it on the next click.
        if (beginPromotionIfNeeded(moves, _selectedSq, toSq))
        {
            _userMove = UserMove::None;
            _dragFromSq = -1;
            return false;
        }

        chosen = pickMove(moves, _selectedSq, toSq);

        if (!chosen) {
            _selectedSq = -1;
            _selectedMovesMask = 0ULL;
            _userMove = UserMove::None;
            _dragFromSq = -1;
            return false;
        }

        _board.makeMoveBB(*chosen);
        moved = true;

        // Two things were missing here, so a drag-and-drop move behaved
        // differently from a click move:
        //
        //  1. The move was never pushed onto _moveHistory, so undo/redo
        //     silently ignored every dragged move -- pressing undo would
        //     revert some EARLIER move instead, corrupting the board.
        //  2. _lastMoveFrom/_lastMoveTo were never updated, so the yellow
        //     last-move highlight kept pointing at whatever move was made
        //     before, which is why it looked wrong after dragging.
        _moveHistory.push_back({ *chosen, _board.getEnPassantSq() });
        _redoStack.clear();
        _lastMoveFrom = chosen->from;
        _lastMoveTo = chosen->to;

        // reset drag/selection state
        _userMove = UserMove::None;
        _dragFromSq = -1;
        _selectedSq = -1;
        _selectedMovesMask = 0ULL;

        if (moved) maybeBotMove();
        return moved;
    }



    // Called by wrapper on mouse move
    // Called on cursor movement while the button is held. Promotes a
    // press into a drag once the cursor has actually moved a few pixels,
    // so that an ordinary click (which always jitters by a pixel or two)
    // isn't mistaken for a drag and doesn't cancel click-to-move.
    bool testDrag(const gui::Point& cursorPoint)
    {
        if (_promotionPending) return false;
        if (_dragFromSq < 0) return false;

        if (_userMove == UserMove::ClickedBK)
        {
            const gui::CoordType dx = cursorPoint.x - _pressPt.x;
            const gui::CoordType dy = cursorPoint.y - _pressPt.y;
            if ((dx * dx + dy * dy) < (DRAG_THRESH * DRAG_THRESH))
                return false;               // still just a click

            _currentDragPoint = cursorPoint;
            _userMove = UserMove::DragBK;
            _didDrag = true;
            return true;
        }

        if (_userMove == UserMove::DragBK)
        {
            _currentDragPoint = cursorPoint;
            return true;
        }
        return false;
    }

    void start()
    {
        if (!_playing)
            _playing = true;
    }

    bool canAIMakeMove() const
    {
        // If AI should be one side only, restrict by _board.sideToMove
        return _playing;
    }

    void moveAI(gui::thread::MainThreadSharedFunction1 callBack)
    {
        _workingThread = std::thread(&Chess::findBestMove, this, callBack);
    }

    void onPokreniClicked()
    {
        _vsBot = true;
        _botIsWhite = false; // bot plays black
        _selectedSq = -1;
        _selectedMovesMask = 0ULL;

        // Only move if bot is actually to move
        maybeBotMove();
    }

    // Difficulty as a time budget (milliseconds per move). Pass 0 to go
    // back to fixed-depth searching.
    void setBotTimeBudget(int ms) { _botBudgetMs = (ms > 0) ? ms : 0; }

    void setVsBot(bool vsBot, bool botIsWhite, int depth)
    {
        _vsBot = vsBot;
        _botIsWhite = botIsWhite;
        _botDepth = depth;

        // start fresh when selecting mode
        reset(true);

        _selectedSq = -1;
        _selectedMovesMask = 0ULL;

        // If bot plays white, bot must move first
        if (_vsBot && _botIsWhite && _board.getSideToMove() == true)
            maybeBotMove();
    }

    bool getSideToMove() const { return _board.getSideToMove(); } // true=white, false=black

    bool humanIsWhite() const { return !_botIsWhite; }
    bool isHumanTurn() const
    {
        if (!_vsBot) return true;
        return _board.getSideToMove() == humanIsWhite(); // true=white
    }

    bool isBotThinking() const { return _botThinking; }
    bool isPromotionPending() const { return _promotionPending; }

    // 0 = Queen, 1 = Rook, 2 = Bishop, 3 = Knight
    void setPromotionChoice(int idx)
    {
        switch (idx)
        {
        case 1: _promotionChoiceWhite = WROOK;   _promotionChoiceBlack = BROOK;   break;
        case 2: _promotionChoiceWhite = WBISHOP; _promotionChoiceBlack = BBISHOP; break;
        case 3: _promotionChoiceWhite = WKNIGHT; _promotionChoiceBlack = BKNIGHT; break;
        default:_promotionChoiceWhite = WQUEEN;  _promotionChoiceBlack = BQUEEN;  break;
        }
    }

    // -----------------------------
    // Game status (checkmate / stalemate / draws)
    // -----------------------------
    // The engine can detect all of these, but nothing was querying it: when
    // a side had no legal moves the bot simply stopped moving and clicks
    // were silently ignored, with no indication the game had ended. The GUI
    // should call getStatus()/getStatusText() after each move and show the
    // result.
    Status getStatus()
    {
        // Called from the UI thread every time the title refreshes. While
        // the bot is working, report Ongoing without touching the lock:
        // the game cannot have ended on the bot's own turn, and waiting
        // for the mutex here is what used to stall the UI mid-turn.
        if (_botThinking) return Status::Ongoing;

        std::lock_guard<std::mutex> lock(_mutex);
        return computeStatusUnlocked();
    }

    bool isGameOver()
    {
        return getStatus() != Status::Ongoing;
    }

    // Human-readable, ready to drop into a label or message box.
    const char* getStatusText()
    {
        switch (getStatus())
        {
        case Status::WhiteWinsCheckmate:       return "Checkmate - White wins";
        case Status::BlackWinsCheckmate:       return "Checkmate - Black wins";
        case Status::DrawStalemate:            return "Draw - stalemate";
        case Status::DrawFiftyMove:            return "Draw - fifty-move rule";
        case Status::DrawRepetition:           return "Draw - repetition";
        case Status::DrawInsufficientMaterial: return "Draw - insufficient material";
        case Status::Ongoing:                  break;
        }
        return "";
    }

    void setOnBotMoveDone(std::function<void()> cb) { _onBotMoveDone = cb; }

    void setBotCallBack(const gui::thread::MainThreadSharedFunction1& cb) { _botCallBack = cb; }

    void undoMove()
    {
        // The bot worker thread mutates _board under _mutex. Undo/redo ran
        // without taking that lock, so undoing mid-search raced the search
        // and could corrupt the position. Refuse while the bot is busy --
        // simpler and more predictable than cancelling a search in flight.
        if (_botThinking) { gui::Sound::play(gui::Sound::Type::Beep); return; }
        std::lock_guard<std::mutex> lock(_mutex);

        if (_moveHistory.empty()) return;

        // Against the bot, undo BOTH the bot's reply and the human move
        // before it, so the human gets their own turn back. With only one
        // move on the stack (e.g. the bot opened as white, or its reply
        // hasn't landed yet) fall through to the single-move branch --
        // previously this did nothing at all and the button looked broken.
        if (_vsBot && _moveHistory.size() >= 2)
        {
            // NOTE: no setEnPassantSq() here. undoMoveBB restores the
            // pre-move en passant square itself (from the move's own
            // prevEnPassantSq). The value cached in _moveHistory was read
            // AFTER makeMoveBB, so it is the post-move square -- writing
            // it back would overwrite the correct restore with the wrong
            // value and break en passant for the rest of the game.
            _redoStack.push_back(_moveHistory.back());
            _board.undoMoveBB(_moveHistory.back().first);
            _moveHistory.pop_back();

            _redoStack.push_back(_moveHistory.back());
            _board.undoMoveBB(_moveHistory.back().first);
            _moveHistory.pop_back();
        }
        else if (!_moveHistory.empty())
        {
            _redoStack.push_back(_moveHistory.back());
            _board.undoMoveBB(_moveHistory.back().first);
            _moveHistory.pop_back();
        }

        if (!_moveHistory.empty())
        {
            _lastMoveFrom = _moveHistory.back().first.from;
            _lastMoveTo = _moveHistory.back().first.to;
        }
        else
        {
            _lastMoveFrom = -1;
            _lastMoveTo = -1;
        }

        _selectedSq = -1;
        _selectedMovesMask = 0ULL;
        cancelPromotion();
    }

    void redoMove()
    {
        if (_botThinking) { gui::Sound::play(gui::Sound::Type::Beep); return; }
        std::lock_guard<std::mutex> lock(_mutex);

        if (_redoStack.empty()) return;

        if (_vsBot && _redoStack.size() >= 2)
        {
            auto [move, epSq] = _redoStack.back();
            _board.makeMoveBB(move);
            _moveHistory.push_back({ move, epSq });
            _redoStack.pop_back();

            auto [move2, epSq2] = _redoStack.back();
            _board.makeMoveBB(move2);
            _moveHistory.push_back({ move2, epSq2 });
            _redoStack.pop_back();
        }
        else if (!_redoStack.empty())
        {
            auto [move, epSq] = _redoStack.back();
            _board.makeMoveBB(move);
            _moveHistory.push_back({ move, epSq });
            _redoStack.pop_back();
        }

        if (!_moveHistory.empty())
        {
            _lastMoveFrom = _moveHistory.back().first.from;
            _lastMoveTo = _moveHistory.back().first.to;
        }
        else
        {
            _lastMoveFrom = -1;
            _lastMoveTo = -1;
        }

        _selectedSq = -1;
        _selectedMovesMask = 0ULL;
    }


};
