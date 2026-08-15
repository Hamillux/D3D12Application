#pragma once

#include "Window.h"

#include <Windows.h>
#include <memory>

class Application final
{
public:
    Application(HINSTANCE instance, int showCommand) noexcept;
    ~Application();

    int Run();

private:
    HINSTANCE _instance = nullptr;
    int _showCommand = SW_SHOWNORMAL;
    Window _window;
    std::unique_ptr<class Engine> _engine;
};
