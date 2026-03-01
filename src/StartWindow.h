#pragma once
#include <gui/Window.h>

#include "MainView.h"
#include "MainWindow.h"

class StartWindow : public gui::Window
{
private:
    MainView _view;

public:
    StartWindow()
        : gui::Window(gui::Size(400, 300))
        , _view([this](bool vsBot)
            {
                // IMPORTANT: Make MainWindow a CHILD of StartWindow,
                // so StartWindow stays the "main window" and app doesn't exit.
                auto* gameWin = new MainWindow(this);
                gameWin->setStartMode(vsBot);
                gameWin->open();

                // No setVisible(false) in this header, so we just "park" the start window:
                freeze();                          // disables interactions
                setResizable(false);
                setGeometry(gui::Geometry(0, 0, 1, 1), true); // make it tiny (acts like hidden)
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
