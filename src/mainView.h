#pragma once
#include <gui/View.h>
#include <gui/Canvas.h>
#include <gui/Button.h>
#include <gui/Label.h>
#include <gui/GridLayout.h>
#include <gui/GridComposer.h>
#include <gui/Image.h>
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
    using StartChoiceFn = std::function<void(bool vsBot)>;

private:
    StartChoiceFn   _onChoice;
    BgView          _bgView;
    gui::Label      _lblSpacer1;
    gui::Label      _lblSpacer2;
    gui::Label      _lblSpacer3;
    gui::Button     _btnSinglePlayer;
    gui::Button     _btnMultiplayer;
    gui::GridLayout _gl;

public:
    MainView(StartChoiceFn cb)
        : _onChoice(std::move(cb))
        , _bgView("../assets/chess_bg.jpg")
        , _lblSpacer1(tr(""))
        , _lblSpacer2(tr(""))
        , _lblSpacer3(tr(""))
        , _btnSinglePlayer(tr("New Game"))
        , _btnMultiplayer(tr("Multiplayer"))
        , _gl(6, 1)
    {
        _btnSinglePlayer.setType(gui::Button::Type::Constructive);
        _btnSinglePlayer.setCircular();
        _btnSinglePlayer.setSizeLimits(340, gui::Control::Limit::Fixed, 60, gui::Control::Limit::Fixed);

        _btnMultiplayer.setType(gui::Button::Type::Constructive);
        _btnMultiplayer.setCircular();
        _btnMultiplayer.setSizeLimits(340, gui::Control::Limit::Fixed, 60, gui::Control::Limit::Fixed);

        gui::GridComposer gc(_gl);
        gc.appendRow(_bgView, -1);                      // banner image full width
        gc.appendRow(_lblSpacer1, td::HAlignment::Center);  // space
        gc.appendRow(_btnSinglePlayer, td::HAlignment::Center);
        gc.appendRow(_lblSpacer2, td::HAlignment::Center);  // space between buttons
        gc.appendRow(_btnMultiplayer, td::HAlignment::Center);
        gc.appendRow(_lblSpacer3, td::HAlignment::Center);  // bottom space

        setLayout(&_gl);
    }

protected:
    bool onClick(gui::Button* pBtn) override
    {
        if (pBtn == &_btnSinglePlayer)
        {
            if (_onChoice) _onChoice(true);
            return true;
        }
        else if (pBtn == &_btnMultiplayer)
        {
            if (_onChoice) _onChoice(false);
            return true;
        }
        return false;
    }
};