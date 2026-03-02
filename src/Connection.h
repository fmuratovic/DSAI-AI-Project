#pragma once

#include <iostream>
#include <vector>
#include <limits>
#include <cmath>
#include <algorithm>
#include <thread>
#include <mutex>
#include <chrono>
#include <functional>

#include <td/Types.h>
#include <gui/Canvas.h>
#include <gui/Thread.h>
#include <thread/Thread.h>
#include <gui/Image.h>
#include <gui/Shape.h>
#include <gui/Application.h>

#include "ChessBoard.h" 
#include "Constants.h"

#include <sstream>


constexpr size_t N = 8;

class Chess
{
public:
    // Kept same enum names so your existing external GUI wrapper still calls
    // canDragPiece/testDrag/finishUserMove exactly like professor’s version.
    enum class UserMove : td::BYTE { None = 0, ClickedBK, DragBK };

private:

    int _selectedSq = -1;
    uint64_t _selectedMovesMask = 0ULL; // bitboard of target squares
    bool _whiteToMove = true;           // or read from board.sideToMove

    int _botDepth = 4;   // default AI strength

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
    bool _botThinking = false;  // prevents double moves

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
    // If you switched to file assets, replace with "assets/wk.png" etc.
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

        MoveBB best = _board.findBestMoveBB(_depth, isWhite);
        _board.makeMoveBB(best);

        td::INT4 iMsg = td::INT4(Message::AIMove);
        td::Variant varMsg(iMsg);
        gui::thread::asyncExecInMainThread(callBack, varMsg);
    }

public:
    Chess(int depth)
        : _imgBoard(":board")

        // If you use resources:
        , _blackKing("../assets/bk.png")
        , _blackQueen("../assets/bq.png")
        , _blackRook("../assets/br.png")
        , _blackBishop("../assets/bb.png")
        , _blackKnight("../assets/bn.png")
        , _blackPawn("../assets/bp.png")

        , _whiteKing("../assets/wk.png")
        , _whiteQueen("../assets/wq.png")
        , _whiteRook("../assets/wr.png")
        , _whiteBishop("../assets/wb.png")
        , _whiteKnight("../assets/wn.png")
        , _whitePawn("../assets/wp.png")

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
        _stopPlaying = false;
        _playing = false;
        _userMove = UserMove::None;
        _dragFromSq = -1;
        _calcBoardPlacement = true;

        if (setToInitialState)
            _board = ChessBoard(); // starting position
    }

    // -----------------------------
    // Drawing (called by your wrapper)
    // -----------------------------
    void maybeBotMove()
    {
        if (!_vsBot) return;
        if (_botThinking) return;
        bool sideNow = _board.getSideToMove();
        if (sideNow != _botIsWhite) return;

        _botThinking = true;

        _workingThread = std::thread([this]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            {
                std::lock_guard<std::mutex> lock(_mutex);
                auto legal = _board.getLegalMovesBB(_botIsWhite);
                if (!legal.empty()) {
                    MoveBB best = _board.findBestMoveBB(_botDepth, _botIsWhite);
                    _board.makeMoveBB(best);

                    _moveHistory.push_back({ best, _board.getEnPassantSq() });

                    _redoStack.clear();

                    _lastMoveFrom = best.from;
                    _lastMoveTo = best.to;
                }
            }
            _botThinking = false;
            td::INT4 iMsg = td::INT4(Message::BotMove);
            td::Variant varMsg(iMsg);
            gui::thread::asyncExecInMainThread(_botCallBack, varMsg);
            });
        _workingThread.detach();
    }



    void draw()
    {
        gui::Point pt(0, 0);
        gui::Rect r(pt, _viewSize);

        // 1) Draw board image
        if (_calcBoardPlacement)
            _imgBoard.draw(r, gui::Image::AspectRatio::Keep,
                td::HAlignment::Center, td::VAlignment::Center,
                &_boardPlacement);
        else
            _imgBoard.draw(r);

        // 2) Compute matrix placement (same as you have)
        gui::CoordType origX = _boardPlacement.left + _originXMargin * _boardPlacement.width();
        gui::CoordType origY = _boardPlacement.bottom - _originYMargin * _boardPlacement.height();

        gui::CoordType matrixRight = _boardPlacement.right - _originXMargin * _boardPlacement.width();
        gui::CoordType matrixTop = _boardPlacement.top + _originYMargin * _boardPlacement.height();

        gui::Rect rMatrix(origX, matrixTop, matrixRight, origY);
        gui::Shape::drawRect(rMatrix, td::ColorID::White, 2.5f);

        gui::CoordType cellWidth = (matrixRight - origX) / N;
        gui::CoordType cellHeight = (origY - matrixTop) / N;

        // Keep exactly what you had (no changes)
        _ptOrig = { origX, origY - cellHeight };
        _cellSize = { cellWidth, cellHeight };

        // =========================
        // 3) HIGHLIGHTS (optional)
        // =========================

        // Selected square fill (transparent)
        if (_selectedSq != -1)
        {
            int file = _selectedSq & 7;
            int rank = _selectedSq >> 3;
            int guiRow = rank;                 // ✅ DO NOT CHANGE (as requested)

            gui::Rect rcSel;
            getCellRect(rcSel, file, guiRow);
            gui::Shape::drawRect(rcSel, 0.20f, td::ColorID::Blue);
        }

        // Legal-move circles
        if (_selectedSq != -1 && _selectedMovesMask != 0ULL)
        {
            auto drawMoveCircle = [&](int sqTo)
                {
                    int file = sqTo & 7;
                    int rank = sqTo >> 3;
                    int guiRow = rank;             // ✅ DO NOT CHANGE (as requested)

                    gui::Rect rc;
                    getCellRect(rc, file, guiRow);

                    gui::CoordType cx = (rc.left + rc.right) * 0.5f;
                    gui::CoordType cy = (rc.top + rc.bottom) * 0.5f;
                    gui::CoordType rad = std::min(_cellSize.width, _cellSize.height) * 0.18f;

                    gui::Circle c({ cx, cy }, rad);
                    gui::Shape s;
                    s.createCircle(c, 2.0f);
                    s.drawFillAndWire(td::ColorID::Green, td::ColorID::White);
                };

            uint64_t mask = _selectedMovesMask;
            while (mask)
            {
                int to = ChessBoard::popLSB(mask); // assumes popLSB is accessible like you used before
                drawMoveCircle(to);
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

            // Drag visual (your existing behavior)
            if (_userMove == UserMove::DragBK && sq == _dragFromSq)
            {
                auto off = _currentDragPoint - _dragStartPoint;
                rect += off;
            }

            drawPiece(p, rect);
        }

        if (_lastMoveFrom != -1)
        {
            int file = _lastMoveFrom & 7;
            int rank = _lastMoveFrom >> 3;
            gui::Rect rc;
            getCellRect(rc, file, rank);
            gui::Shape::drawRect(rc, 0.25f, td::ColorID::Yellow);
        }
        if (_lastMoveTo != -1)
        {
            int file = _lastMoveTo & 7;
            int rank = _lastMoveTo >> 3;
            gui::Rect rc;
            getCellRect(rc, file, rank);
            gui::Shape::drawRect(rc, 0.25f, td::ColorID::Yellow);
        }
    }


    void onMouseDown(const gui::Point& p)
    {
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

            for (const auto& mv : moves)
                if (mv.from == _selectedSq && mv.to == sq)
                {
                    chosen = &mv; break;
                }

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
        bool moved = false;                 // ✅ declare

        if (_selectedSq == -1) return false;

        int toSq = pointToSq(cursorPoint);
        if (toSq < 0) return false;

        // If release square isn't legal, clear selection and stop
        if (((_selectedMovesMask >> toSq) & 1ULL) == 0ULL) {
            _selectedSq = -1;
            _selectedMovesMask = 0ULL;
            _userMove = UserMove::None;
            _dragFromSq = -1;
            return false;
        }

        bool sideToMove = _board.getSideToMove();
        const MoveBB* chosen = nullptr;
        auto moves = _board.getLegalMovesBB(sideToMove);

        for (const auto& mv : moves)
            if (mv.from == _selectedSq && mv.to == toSq)
            {
                chosen = &mv; break;
            }

        if (!chosen) {
            _selectedSq = -1;
            _selectedMovesMask = 0ULL;
            _userMove = UserMove::None;
            _dragFromSq = -1;
            return false;
        }

        _board.makeMoveBB(*chosen);
        moved = true;                      // ✅ set

        // reset drag/selection state
        _userMove = UserMove::None;
        _dragFromSq = -1;
        _selectedSq = -1;
        _selectedMovesMask = 0ULL;

        if (moved) maybeBotMove();         // ✅ now valid
        return moved;
    }



    // Called by wrapper on mouse move
    bool testDrag(const gui::Point& cursorPoint)
    {
        /*if (_userMove == UserMove::ClickedBK)
        {
            _currentDragPoint = cursorPoint;
            _userMove = UserMove::DragBK;
            return true;
        }
        else if (_userMove == UserMove::DragBK)
        {
            _currentDragPoint = cursorPoint;
            return true;
        }
        _userMove = UserMove::None;*/
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

    void setOnBotMoveDone(std::function<void()> cb) { _onBotMoveDone = cb; }

    void setBotCallBack(const gui::thread::MainThreadSharedFunction1& cb) { _botCallBack = cb; }

    void undoMove()
    {
        if (_moveHistory.empty()) return;

        if (_vsBot && _moveHistory.size() >= 2)
        {
            _redoStack.push_back(_moveHistory.back());
            _board.undoMoveBB(_moveHistory.back().first);
            _board.setEnPassantSq(_moveHistory.back().second);
            _moveHistory.pop_back();

            _redoStack.push_back(_moveHistory.back());
            _board.undoMoveBB(_moveHistory.back().first);
            _board.setEnPassantSq(_moveHistory.back().second);
            _moveHistory.pop_back();
        }
        else if (!_vsBot && !_moveHistory.empty())
        {
            _redoStack.push_back(_moveHistory.back());
            _board.undoMoveBB(_moveHistory.back().first);
            _board.setEnPassantSq(_moveHistory.back().second);
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
    }

    void redoMove()
    {
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
