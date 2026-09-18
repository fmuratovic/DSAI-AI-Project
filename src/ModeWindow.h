#pragma once
#include <gui/Window.h>
#include <functional>

class ModeWindow : public gui::Window
{
public:
    using ChoiceFn = std::function<void(bool vsBot, bool botIsWhite, int depth)>;

private:
    ChoiceFn _onChoice;

public:
    ModeWindow(ChoiceFn cb)
        : gui::Window(gui::Size(360, 180))
        , _onChoice(std::move(cb))
    {
        setTitle("Choose mode");

        // NOTE: You need to replace these with the actual natID controls you have:
        // - two buttons: "Human vs Human", "Play vs Bot"
        // - optionally: "Bot plays White/Black", "Depth"
        //
        // The important part is: when user clicks, call _onChoice(...) and close.
    }

    // Example handlers (wire to your buttons)
    void chooseHuman()
    {
        if (_onChoice) _onChoice(false, false, 4);  // vsBot=false
        close();
    }

    void chooseBot()
    {
        if (_onChoice) _onChoice(true, false, 4);   // vsBot=true, bot black, depth 4
        close();
    }
};
