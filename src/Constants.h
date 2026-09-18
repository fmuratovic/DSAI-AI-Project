//
//  Created by Izudin Dzafic on 18/10/2025.
//  Copyright © 2025 IDz. All rights reserved.
//

#pragma once
#include <td/Types.h>

constexpr td::BYTE cMenuApp = 10;
constexpr td::BYTE cMenuGame = 30;
constexpr td::BYTE cResetActionItem = 10;
constexpr td::BYTE cStartStopActionItem = 20;
constexpr td::BYTE cUndoActionItem = 30;
constexpr td::BYTE cRedoActionItem = 40;
enum class Message : td::BYTE { Stop = 0, StopOnGoal, AIMove, BotMove };