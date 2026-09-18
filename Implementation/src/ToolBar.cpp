//
//  Created by Izudin Dzafic on 18/10/2025.
//  Copyright © 2025 IDz. All rights reserved.
//
#include "ToolBar.h"

ToolBar::ToolBar(ViewChessBoard* pViewChessBoard, gui::Image* imgRun)
    : gui::ToolBar("mainTB", 2)
    , _viewSettings(pViewChessBoard, this)
    , _imgUndo(":undo")
    , _imgRedo(":redo")
{
    addItem(&_viewSettings, 1000, tr("settings"), tr("settingsTT"));
    addItem(tr("undo"), &_imgUndo, tr("undoTT"), cMenuGame, 0, 0, cUndoActionItem);
    addItem(tr("redo"), &_imgRedo, tr("redoTT"), cMenuGame, 0, 0, cRedoActionItem);
}
