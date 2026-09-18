//
//  Created by Izudin Dzafic on 18/10/2025.
//  Copyright © 2025 IDz. All rights reserved.
//
#pragma once
#include <gui/Canvas.h>
#include <gui/Sound.h>
#include <gui/Alert.h>
#include <functional>

#include "Connection.h"   // contains class Chess, Message enum etc.

class ViewChessBoard : public gui::Canvas
{
protected:
    Chess _chess;
    gui::Sound _soundMove;
    gui::Sound _soundMissionSuccess;
    gui::Size _size;
    std::function<void()> _fnUpdateMenuAndTB;

    // -------------------------
    // Mode flags (kept in ViewChessBoard)
    // so we can gate old threaded AI logic safely.
    // -------------------------
    bool _vsBot = false;
    bool _botIsWhite = false;
    int  _botDepth = 4;

    // The result dialog must fire exactly once per finished game. onDraw
    // runs at 60fps, so without this latch the alert would be raised
    // continuously and make the app unusable.
    bool _gameOverReported = false;

    // Tracks the last thinking state we told MainWindow about, so the title
    // is refreshed exactly when it flips rather than on every frame.
    bool _lastThinkingState = false;

protected:
    void onResize(const gui::Size& newSize) override
    {
        _size = newSize;
        _chess.updateModelSize(newSize);
    }

    void onDraw(const gui::Rect& /*rect*/) override
    {
        _chess.draw();

        // Push a title update only on the transition, not every frame.
        const bool thinking = _chess.isBotThinking();
        if (thinking != _lastThinkingState)
        {
            _lastThinkingState = thinking;
            _fnUpdateMenuAndTB();
        }

        if (thinking)
            reDraw(); // keep polling until bot finishes
        else
            checkGameOver(); // only once the position has settled
    }

    // Announces the result and offers a rematch. Called from onDraw rather
    // than from each move handler so it catches every path that can end a
    // game -- human move, bot move, and undo/redo -- without having to
    // remember to add a call to each one.
    void checkGameOver()
    {
        if (_gameOverReported) return;
        if (!_chess.isGameOver()) return;

        _gameOverReported = true;

        // Let the menu/toolbar and window title update too.
        _fnUpdateMenuAndTB();

        if (_chess.getStatus() == Chess::Status::WhiteWinsCheckmate ||
            _chess.getStatus() == Chess::Status::BlackWinsCheckmate)
            _soundMissionSuccess.play();

        gui::Alert::showYesNoQuestion(
            tr("gameOver"),
            _chess.getStatusText(),
            tr("newGame"),
            tr("keepBoard"),
            [this](gui::Alert::Answer answer)
            {
                if (answer == gui::Alert::Answer::Yes)
                    reset();
                // "No" leaves the final position on screen for review;
                // input stays blocked because the game really is over.
            });
    }

    void onPrimaryButtonPressed(const gui::InputDevice& inputDevice) override
    {
        const gui::Point& modelPoint = inputDevice.getModelPoint();
        _chess.onMouseDown(modelPoint);  // ← was canDragPiece
        reDraw();
    }

    void onPrimaryButtonReleased(const gui::InputDevice& inputDevice) override
    {
        // Completes a DRAG. For a plain click this is a no-op -- the piece
        // stays selected and the next click picks the destination. Both of
        // these handlers were previously empty, which meant the entire
        // drag-and-drop path (testDrag / finishUserMove and the drag
        // rendering in Chess::draw) was unreachable: pieces could only be
        // moved by clicking twice.
        _chess.finishUserMove(inputDevice.getModelPoint());
        reDraw();
    }

    void onCursorDragged(const gui::InputDevice& inputDevice) override
    {
        if (_chess.testDrag(inputDevice.getModelPoint()))
            reDraw();   // only repaint when the piece actually moved
    }

    void chessEngineCallBack(td::Variant param)
    {
        td::INT4 iVal = param.i4Val();
        Message msg = Message(iVal);

        switch (msg)
        {
        case Message::Stop:
            onWorkerCompleted();
            break;

        case Message::StopOnGoal:
            onWorkerCompleted();
            _soundMissionSuccess.play();
            break;

        case Message::AIMove:
        {
            _soundMove.play();

            // clean the thread state
            auto& th = _chess.getThread();
            if (th.joinable())
                th.join();
        }
        case Message::BotMove:
        {
            _soundMove.play();
            reDraw();
        }
        break;

        default:
            assert(false);
        }

        reDraw();
    }

public:
    ViewChessBoard(const std::function<void()>& fnUpdateMenuAndTB)
        : Canvas({ gui::InputDevice::Event::Keyboard,
                   gui::InputDevice::Event::PrimaryClicks,
                   gui::InputDevice::Event::CursorDrag })
        , _chess(8)
        , _soundMove(":move")
        , _soundMissionSuccess(":success")
        , _fnUpdateMenuAndTB(fnUpdateMenuAndTB)
    {
        setPreferredFrameRateRange(60, 60);
        enableResizeEvent(true);

        // ← add these two lines here
        auto cb = std::make_shared<gui::thread::MainThreadFunction1>(
            std::bind(&ViewChessBoard::chessEngineCallBack, this, std::placeholders::_1));
        _chess.setBotCallBack(cb);
    }

    void onWorkerCompleted()
    {
        auto& th = _chess.getThread();
        if (th.joinable())
            th.join();

        _fnUpdateMenuAndTB();
        reDraw();
    }

    void stop()
    {
        _chess.stopPlaying();
        _fnUpdateMenuAndTB();
    }

    bool isPlaying() const
    {
        return _chess.isPlaying();
    }

    void refresh()
    {
        reDraw();
    }

    void startStop()
    {
        // The toolbar's Start/Stop is a leftover from when this project
        // auto-played AI-vs-AI. It is wrong in BOTH of the modes the game
        // actually offers now:
        //
        //   * vs Bot      -- the bot already moves by itself after each
        //                    human move; starting the old autoplay thread
        //                    on top of that makes two AIs fight over the
        //                    same board.
        //   * Multiplayer -- it was NOT guarded here, so pressing Start in
        //                    a human-vs-human game made the engine play a
        //                    move for whoever's turn it was.
        //
        // Disabled in both. The Start/Stop toolbar and menu items can be
        // removed outright once you're happy with that (see ToolBar.cpp
        // and MenuBar.h); this guard makes them harmless in the meantime.
        gui::Sound::play(gui::Sound::Type::Beep);
        return;

#if 0
        if (_vsBot)
        {
            gui::Sound::play(gui::Sound::Type::Beep);
            return;
        }

        if (_chess.isPlaying())
        {
            stop();
        }
        else
        {
            _chess.start();

            auto mainThreadCallBack =
                std::make_shared<gui::thread::MainThreadFunction1>(
                    std::bind(&ViewChessBoard::chessEngineCallBack, this, std::placeholders::_1));

            _chess.moveAI(mainThreadCallBack);

            _fnUpdateMenuAndTB();
        }
#endif
    }

    void reset()
    {
        if (_chess.isPlaying())
        {
            gui::Sound::play(gui::Sound::Type::Beep);
            return;
        }

        _chess.reset(true);
        _gameOverReported = false;   // new game -> allow reporting again

        // If you reset while vsBot and bot is white, let bot play first.
        if (_vsBot && _botIsWhite)
            _chess.maybeBotMove();

        _fnUpdateMenuAndTB();        // clears the result from the title
        reDraw();
    }

    // Called from MainWindow after ModeWindow selection
    void setVsBot(bool vsBot, bool botIsWhite, int botDepth)
    {
        _vsBot = vsBot;
        _botIsWhite = botIsWhite;
        _botDepth = botDepth;

        // Push mode into Chess
        _chess.setVsBot(vsBot, botIsWhite, botDepth);
        _gameOverReported = false;

        // Ensure old thread autoplay is not “running”
        if (_chess.isPlaying())
            _chess.stopPlaying();

        // If bot is white, it should move first
        if (_vsBot && _botIsWhite)
            _chess.maybeBotMove();

        _fnUpdateMenuAndTB();
        reDraw();
    }

    void undoMove()
    {
        _chess.undoMove();
        // Undoing out of a checkmate makes the game live again.
        _gameOverReported = false;
        _fnUpdateMenuAndTB();
        reDraw();
    }


    void redoMove()
    {
        _chess.redoMove();
        _gameOverReported = false;   // redo may re-enter a finished position
        _fnUpdateMenuAndTB();
        reDraw();
    }

    // Exposed for MainWindow, which shows these in the window title.
    const char* getStatusText() { return _chess.getStatusText(); }
    bool isBotThinking() const { return _chess.isBotThinking(); }

    // Default piece a promoting pawn becomes (0=Q, 1=R, 2=B, 3=N).
    void setPromotionChoice(int idx) { _chess.setPromotionChoice(idx); }

    // Difficulty: milliseconds the bot may spend per move.
    void setBotTimeBudget(int ms) { _chess.setBotTimeBudget(ms); }
};
