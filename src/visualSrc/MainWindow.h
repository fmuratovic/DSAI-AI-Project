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
//    gui::Button _btnBot;
//    gui::GridLayout _layout;
//
//public:
//    MainView()
//        : _lblTitle(tr("CHESS GAME"))
//        , _btnSinglePlayer(tr("Single Player"))
//        , _btnMultiplayer(tr("Multiplayer"))
//        , _btnBot(tr("Play Against Bot"))
//        , _layout(4, 1)
//    {
//        _btnSinglePlayer.setAsDefault();
//        _btnBot.setType(gui::Button::Type::Constructive);
//        _btnMultiplayer.setType(gui::Button::Type::Critical);
//
//        gui::GridComposer gc(_layout);
//        gc.appendRow(_lblTitle);
//        gc.appendRow(_btnSinglePlayer);
//        gc.appendRow(_btnMultiplayer);
//        gc.appendRow(_btnBot);
//
//        setLayout(&_layout);
//    }
//
//protected:
//    bool onClick(gui::Button* pBtn) override
//    {
//        if (pBtn == &_btnSinglePlayer)
//        {
//            showAlert(tr("Mode"), tr("Single Player selected"));
//            // TODO: open chess board vs human (local)
//            return true;
//        }
//        else if (pBtn == &_btnMultiplayer)
//        {
//            showAlert(tr("Mode"), tr("Multiplayer selected"));
//            // TODO: open multiplayer chess window
//            return true;
//        }
//        else if (pBtn == &_btnBot)
//        {
//            showAlert(tr("Mode"), tr("Play Against Bot selected"));
//            // TODO: start chess AI
//            return true;
//        }
//        return false;
//    }
//};
