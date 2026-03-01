//#pragma once
//#pragma once#include <gui/View.h>
//#include <gui/ToolBar.h>
//#include <gui/Button.h>
//#include <gui/Icon.h>
//#include <iostream>
//
//class ToolBarBlock : public gui::ToolBar
//{
//private:
//    gui::Button _btnNew;
//    gui::Button _btnUndo;
//    gui::Button _btnRedo;
//    gui::Button _btnHint;
//
//public:
//    ToolBarBlock()
//        : _btnNew(tr("New"))
//        , _btnUndo(tr("Undo"))
//        , _btnRedo(tr("Redo"))
//        , _btnHint(tr("Hint"))
//    {
//        _btnNew.setType(gui::Button::Type::Constructive);
//        _btnUndo.setType(gui::Button::Type::Default);
//        _btnRedo.setType(gui::Button::Type::Default);
//        _btnHint.setType(gui::Button::Type::Critical);
//
//        addItem(&_btnNew);
//        addItem(&_btnUndo);
//        addItem(&_btnRedo);
//        addItem(&_btnHint);
//    }
//
//protected:
//    bool onClick(gui::Button* pBtn) override
//    {
//        if (pBtn == &_btnNew) { std::cout << "Toolbar: New Game\n"; return true; }
//        if (pBtn == &_btnUndo) { std::cout << "Toolbar: Undo\n"; return true; }
//        if (pBtn == &_btnRedo) { std::cout << "Toolbar: Redo\n"; return true; }
//        if (pBtn == &_btnHint) { std::cout << "Toolbar: Hint\n"; return true; }
//        return false;
//    }
//};
