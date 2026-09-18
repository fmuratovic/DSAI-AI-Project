#pragma once
#include <gui/Window.h>

#include "mainView.h"   // exact case matters on Linux/macOS
#include "MainWindow.h"

class StartWindow : public gui::Window
{
private:
    MainView _view;

public:
    StartWindow()
        // Sized for the content: banner + two labelled combo boxes
        // (difficulty, side) + two buttons.
        : gui::Window(gui::Size(420, 470))
        , _view([this](bool vsBot, int budgetMs, bool botIsWhite)
            {
                // IMPORTANT: Make MainWindow a CHILD of StartWindow,
                // so StartWindow stays the "main window" and app doesn't exit.
                auto* gameWin = new MainWindow(this);
                gameWin->setStartMode(vsBot, budgetMs, botIsWhite);
                gameWin->open();

                // Park the start window while the game is open. It can't be
                // closed (it's the app's main window -- closing it would
                // quit), so it gets frozen and pushed out of the way.
                //
                // It used to be resized to 1x1 here. A 1x1 window still
                // gets a title bar, so the OS had to squeeze the
                // minimise/close/fullscreen buttons into a few pixels --
                // which is why those controls looked distorted, and left a
                // stray sliver in the top-left corner of the screen.
                // Moving it off-screen at its natural size avoids that
                // entirely: nothing is rendered at an impossible size.
                freeze();                          // disables interactions
                setResizable(false);
                setGeometry(gui::Geometry(-4000, -4000, 420, 470), true);
            })
    {
        setTitle(tr("CHESS"));
        setCentralView(&_view);
    }

protected:
    // Optional: prevent user from closing the main window while game is open
    bool shouldClose() override
    {
        // allow closing anytime, or block if you want:
        return true;
    }
};
