#pragma once
#include <gui/Window.h>
#include <functional>
#include <memory>

#include "MenuBar.h"
#include "ToolBar.h"
#include "ViewChessBoard.h"
#include "ModeWindow.h"

class MainWindow : public gui::Window
{
private:
    bool _startVsBot = false;

protected:
    std::unique_ptr<ModeWindow> _modeWindow;

    gui::Image _imgStart;
    gui::Image _imgStop;

    MenuBar _mainMenuBar;
    ToolBar _toolBar;
    ViewChessBoard _mainView;

public:
    // parent defaults to nullptr, but StartWindow will pass itself
    explicit MainWindow(gui::Window* pParent = nullptr)
        : gui::Window(gui::Size(800, 800), pParent)
        , _imgStart(":start")
        , _imgStop(":stop")
        , _mainView(std::bind(&MainWindow::updateMenuAndTB, this))
        , _toolBar(&_mainView, &_imgStart)
    {
        setTitle(tr("appTitle"));
        _mainMenuBar.setAsMain(this);
        setToolBar(_toolBar);
        setCentralView(&_mainView);
    }

    void setStartMode(bool vsBot) { _startVsBot = vsBot; }

protected:
    void onInitialAppearance() override
    {
        _mainView.setFocus();

        // Multiplayer: configure immediately (no dialog)
        if (!_startVsBot)
        {
            _mainView.setVsBot(false, false, 0);
            return;
        }

        _mainView.setVsBot(true, false, 4);

        // Singleplayer: show ModeWindow once
        /*if (!_modeWindow)
        {
            _modeWindow = std::make_unique<ModeWindow>(
                [this](bool vsBot, bool botIsWhite, int depth)
                {
                    _mainView.setVsBot(vsBot, botIsWhite, depth);
                    _modeWindow.reset();
                });

            _modeWindow->open();
        }*/
    }

    void onClose() override
    {
        // When game closes, decide what you want to do with StartWindow:
        // Option A: exit app completely by closing parent (StartWindow is main).
        if (auto* p = getParentWindow())
            p->close();

        // Option B (alternative): show start again instead of exiting.
        // If you want that, don't close parent; instead un-freeze and resize it.
    }

    void updateMenuAndTB()
    {
        bool isGamePlaying = _mainView.isPlaying();

        gui::MenuItem* pMenuItem = _mainMenuBar.getItem(cMenuGame, 0, 0, cStartStopActionItem);
        if (pMenuItem)
            pMenuItem->setChecked(isGamePlaying);

        gui::ToolBarItem* pTBItem = _toolBar.getItem(cMenuGame, 0, 0, cStartStopActionItem);
        if (pTBItem)
        {
            if (isGamePlaying)
            {
                pTBItem->setImage(&_imgStop);
                pTBItem->setLabel(tr("stop"));
                pTBItem->setTooltip(tr("stopTT"));
            }
            else
            {
                pTBItem->setImage(&_imgStart);
                pTBItem->setLabel(tr("start"));
                pTBItem->setTooltip(tr("startTT"));
            }
        }
    }

    bool onActionItem(gui::ActionItemDescriptor& aiDesc) override
    {
        auto [menuID, firstSubMenuID, lastSubMenuID, actionID] = aiDesc.getIDs();

        switch (menuID)
        {
        case cMenuGame:
            switch (actionID)
            {
            case cStartStopActionItem:
                _mainView.startStop();   // make sure this does NOT start old AI thread in vsBot mode
                return true;
            case cResetActionItem:
                _mainView.reset();
                return true;
            default:
                break;
            }
            break;
        default:
            break;
        }
        return false;
    }
};
