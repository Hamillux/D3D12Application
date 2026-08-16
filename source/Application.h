#pragma once

#include "Window.h"

#include <Windows.h>
#include <memory>

class Application final
{
public:
    Application(HINSTANCE instance, int showCommand);
    ~Application();

    int Run();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

private:
    bool Startup();
    void Shutdown();

    HINSTANCE _instance = nullptr;
    int _showCommand = SW_SHOWNORMAL;
    Window _window;
    std::unique_ptr<class Engine> _engine;
};
