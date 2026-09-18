//
//  Created by Izudin Dzafic on 10 Nov 2022.
//  Copyright © 2022 IDz. All rights reserved.
//
#pragma once
#include <gui/Application.h>
#include "StartWindow.h"   // ✅ include this

class Application : public gui::Application
{
protected:
    gui::Window* createInitialWindow() override
    {
        std::cerr << "createInitialWindow called\n";
        return new StartWindow();   // ✅ start screen first
    }

public:
    Application(int argc, const char** argv)
        : gui::Application(argc, argv)
    {
    }
};

