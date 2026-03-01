//#pragma once
//#include <gui/View.h>
//#include <gui/Button.h>
//#include <gui/Label.h>
//#include <gui/GridLayout.h>
//#include <gui/GridComposer.h>
//
//class MainView : public gui::View
//{
//private:
//    gui::Label _lblTitle;
//    gui::Button _btnSinglePlayer;
//    gui::Button _btnMultiplayer;
//    gui::Button _btnPlayBot;
//    gui::GridLayout _gl;
//
//public:
//    MainView()
//        : _lblTitle(tr("CHESS"))
//        , _btnSinglePlayer(tr("Single Player"))
//        , _btnMultiplayer(tr("Multiplayer"))
//        , _btnPlayBot(tr("Play Against Bot"))
//        , _gl(4, 1)
//    {
//        _lblTitle.setBold(true);
//
//        _btnSinglePlayer.setAsDefault();
//        _btnPlayBot.setType(gui::Button::Type::Constructive);
//        _btnMultiplayer.setType(gui::Button::Type::Critical);
//
//        gui::GridComposer gc(_gl);
//        gc.appendRow(_lblTitle);
//        gc.appendRow(_btnSinglePlayer);
//        gc.appendRow(_btnMultiplayer);
//        gc.appendRow(_btnPlayBot);
//
//        setLayout(&_gl);
//    }
//
//protected:
//    bool onClick(gui::Button* pBtn) override
//    {
//        if (pBtn == &_btnSinglePlayer)
//        {
//            showAlert(tr("Chess"), tr("Single Player selected"));
//            // TODO: open chess board (local two players)
//            return true;
//        }
//        else if (pBtn == &_btnMultiplayer)
//        {
//            showAlert(tr("Chess"), tr("Multiplayer selected"));
//            // TODO: open network multiplayer
//            return true;
//        }
//        else if (pBtn == &_btnPlayBot)
//        {
//            showAlert(tr("Chess"), tr("Play Against Bot selected"));
//            // TODO: start chess vs AI
//            return true;
//        }
//        return false;
//    }
//};
