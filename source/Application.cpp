#include "Application.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include "Engine/EnginesInclude.h"

namespace
{
    constexpr std::uint32_t InitialWidth = 1280;
    constexpr std::uint32_t InitialHeight = 720;
    constexpr wchar_t WindowTitle[] = L"D3D12 Application";

    constexpr double MaxDeltaTime = 0.1;
}

Application::Application(HINSTANCE instance, int showCommand) noexcept
    : _instance(instance)
    , _showCommand(showCommand)
    , _engine(std::make_unique<UseEngine>())
{
}

Application::~Application()
{
}

int Application::Run()
{
    _window.Initialize(
        _instance,
        _showCommand,
        InitialWidth,
        InitialHeight,
        WindowTitle);

    _engine->Initialize(
        _window.GetHandle(),
        _window.GetClientWidth(),
        _window.GetClientHeight());

    using Clock = std::chrono::steady_clock;

    auto previousTime = Clock::now();

    while (_window.ProcessMessages())
    {
        std::uint32_t width = 0;
        std::uint32_t height = 0;

        if (_window.ConsumeResize(width, height))
        {
            // _engine.Resize(width, height);
        }

        if (_window.IsMinimized())
        {
            WaitMessage();

            // 最小化していた時間を次フレームの deltaTime に含めない
            previousTime = Clock::now();
            continue;
        }

        const auto currentTime = Clock::now();

        double deltaTime =
            std::chrono::duration<double>(
                currentTime - previousTime
            ).count();

        previousTime = currentTime;

        deltaTime = std::min(deltaTime, MaxDeltaTime);

        _engine->Tick(deltaTime);
    }

    _engine->Finalize();

    return 0;
}
