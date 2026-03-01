//
//  Created by Izudin Dzafic on 18/10/2025.
//  Copyright © 2025 IDz. All rights reserved.
//
#pragma once
#include <gui/Canvas.h>
#include <gui/Sound.h>
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

protected:
    void onResize(const gui::Size& newSize) override
    {
        _size = newSize;
        _chess.updateModelSize(newSize);
    }

    void onDraw(const gui::Rect& /*rect*/) override
    {
        _chess.draw();
        if (_chess.isBotThinking())
            reDraw(); // keep polling until bot finishes
    }

    void onPrimaryButtonPressed(const gui::InputDevice& inputDevice) override
    {
        const gui::Point& modelPoint = inputDevice.getModelPoint();
        _chess.onMouseDown(modelPoint);  // ← was canDragPiece
        reDraw();
    }

    void onPrimaryButtonReleased(const gui::InputDevice& inputDevice) override
    {
        

            reDraw();
    }

    void onCursorDragged(const gui::InputDevice& inputDevice) override
    {
      
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
        // ✅ In vsBot mode, this old Start/Stop (thread autoplay) must not run.
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
    }

    void reset()
    {
        if (_chess.isPlaying())
        {
            gui::Sound::play(gui::Sound::Type::Beep);
            return;
        }

        _chess.reset(true);

        // If you reset while vsBot and bot is white, let bot play first.
        if (_vsBot && _botIsWhite)
            _chess.maybeBotMove();

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

        // Ensure old thread autoplay is not “running”
        if (_chess.isPlaying())
            _chess.stopPlaying();

        // If bot is white, it should move first
        if (_vsBot && _botIsWhite)
            _chess.maybeBotMove();

        _fnUpdateMenuAndTB();
        reDraw();
    }
};
