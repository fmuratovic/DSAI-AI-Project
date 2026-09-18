#pragma once
#include <gui/View.h>
#include <gui/Canvas.h>
#include <gui/Button.h>
#include <gui/Label.h>
#include <gui/GridLayout.h>
#include <gui/GridComposer.h>
#include <gui/Image.h>
#include <gui/ComboBox.h>
#include <functional>

class BgView : public gui::Canvas
{
private:
    gui::Image _img;
public:
    BgView(const char* imgPath) : _img(imgPath)
    {
        // Fix the height of the banner to 180px
        setSizeLimits(0, gui::Control::Limit::None, 180, gui::Control::Limit::Fixed);
    }
protected:
    void onDraw(const gui::Rect& rect) override
    {
        if (_img.isOK())
            _img.draw(rect, gui::Image::AspectRatio::No);
    }
};

class MainView : public gui::View
{
public:
    // Now also carries the chosen search depth, so the start screen can set
    // the bot's difficulty before the game window opens.
    // Carries every start-screen setting: mode, difficulty, which colour
    // the player takes, and the default promotion piece.
    using StartChoiceFn = std::function<void(bool vsBot, int budgetMs,
                                             bool botIsWhite)>;

    // Difficulty is a TIME BUDGET per move, in milliseconds -- not a fixed
    // search depth.
    //
    // Fixed depth was the obvious choice but behaves badly in practice:
    // the cost of a given depth swings enormously with the position, so
    // "depth 5" answered instantly in an endgame and took seconds in a
    // dense middlegame. A budget gives the player a consistent response
    // time and lets the engine go deeper for free when the position is
    // simple (measured: depth 8 in ~210ms in a simple endgame, versus
    // depth 4 in the same time in a crowded middlegame).
    //
    // `labelId` is a TRANSLATION ID, not display text -- it must go
    // through tr() before reaching the combo box.
    struct Level { const char* labelId; int budgetMs; };
    static const Level* levels() {
        // Budgets chosen by MEASURING what each extra ply costs rather
        // than by picking round numbers. Search cost grows ~5-7x per ply,
        // so the levels have to be spaced by roughly that factor or two
        // adjacent levels reach the same depth and play identically --
        // which is exactly what happened with an earlier 700/1200 pair:
        // both stopped at depth 4 because the next ply needed ~1900ms.
        //
        // Reference timings (single core, dense middlegame):
        //   depth 3 ~ 70ms | depth 4 ~ 280ms | depth 5 ~ 1900ms
        // A multicore machine reaches one or two plies deeper for the same
        // budget, since the deepest iteration is the parallelised one.
        static const Level lv[] = {
            { "Easy",    150 },
            { "Normal",  400 },
            { "Hard",   1000 },
            { "Expert", 2200 },
        };
        return lv;
    }
    static int numLevels() { return 4; }
    static int defaultLevelIndex() { return 1; } // Normal

private:
    StartChoiceFn   _onChoice;
    BgView          _bgView;
    gui::Label      _lblSpacer1;
    gui::Label      _lblSpacer2;
    gui::Label      _lblSpacer3;
    gui::Label      _lblDifficulty;
    gui::ComboBox   _cmbDifficulty;
    gui::Label      _lblSide;
    gui::ComboBox   _cmbSide;
    gui::Button     _btnSinglePlayer;
    gui::Button     _btnMultiplayer;
    gui::GridLayout _gl;

public:
    MainView(StartChoiceFn cb)
        : _onChoice(std::move(cb))
        , _bgView(":chessBg")
        , _lblSpacer1(tr(""))
        , _lblSpacer2(tr(""))
        , _lblSpacer3(tr(""))
        , _lblDifficulty(tr("Difficulty"))
        , _lblSide(tr("Play as"))
        , _btnSinglePlayer(tr("New Game"))
        , _btnMultiplayer(tr("Multiplayer"))
        , _gl(10, 1)   // MUST equal the number of gc.appendRow calls below
    {
        for (int i = 0; i < numLevels(); i++)
            _cmbDifficulty.addItem(tr(levels()[i].labelId));
        _cmbDifficulty.selectIndex(defaultLevelIndex());
        _cmbDifficulty.setSizeLimits(340, gui::Control::Limit::Fixed,
                                     30, gui::Control::Limit::Fixed);

        // Index 0 = White (bot takes black, player moves first).
        _cmbSide.addItem(tr("White"));
        _cmbSide.addItem(tr("Black"));
        _cmbSide.selectIndex(0);
        _cmbSide.setSizeLimits(340, gui::Control::Limit::Fixed,
                               30, gui::Control::Limit::Fixed);

        _btnSinglePlayer.setType(gui::Button::Type::Constructive);
        _btnSinglePlayer.setCircular();
        _btnSinglePlayer.setSizeLimits(340, gui::Control::Limit::Fixed, 60, gui::Control::Limit::Fixed);

        _btnMultiplayer.setType(gui::Button::Type::Constructive);
        _btnMultiplayer.setCircular();
        _btnMultiplayer.setSizeLimits(340, gui::Control::Limit::Fixed, 60, gui::Control::Limit::Fixed);

        // NOTE: gui::GridLayout is constructed with a FIXED row count
        // (see _gl above). Appending more rows than declared overruns it
        // and the framework fires a debug break. If you add or remove an
        // appendRow call here, update the _gl(...) row count to match --
        // there are 10 rows below.
        gui::GridComposer gc(_gl);
        gc.appendRow(_bgView, -1);                      // 1  banner image full width
        gc.appendRow(_lblSpacer1, td::HAlignment::Center);   // 2  space
        gc.appendRow(_lblDifficulty, td::HAlignment::Center); // 3
        gc.appendRow(_cmbDifficulty, td::HAlignment::Center); // 4
        gc.appendRow(_lblSide, td::HAlignment::Center);       // 5
        gc.appendRow(_cmbSide, td::HAlignment::Center);       // 6
        gc.appendRow(_lblSpacer2, td::HAlignment::Center);   // 7
        gc.appendRow(_btnSinglePlayer, td::HAlignment::Center); // 8
        gc.appendRow(_btnMultiplayer, td::HAlignment::Center);  // 9
        gc.appendRow(_lblSpacer3, td::HAlignment::Center);   // 10 bottom space

        setLayout(&_gl);
    }

    int selectedBudgetMs() const
    {
        int i = _cmbDifficulty.getSelectedIndex();
        if (i < 0 || i >= numLevels()) i = defaultLevelIndex();
        return levels()[i].budgetMs;
    }

    // The player picks their own colour, so the bot takes the other one.
    bool botIsWhite() const { return _cmbSide.getSelectedIndex() == 1; }



protected:
    bool onClick(gui::Button* pBtn) override
    {
        if (pBtn == &_btnSinglePlayer)
        {
            if (_onChoice) _onChoice(true, selectedBudgetMs(), botIsWhite());
            return true;
        }
        else if (pBtn == &_btnMultiplayer)
        {
            // Depth is irrelevant for human-vs-human, but pass it anyway so
            // the callback signature stays uniform.
            if (_onChoice) _onChoice(false, selectedBudgetMs(), false);
            return true;
        }
        return false;
    }
};