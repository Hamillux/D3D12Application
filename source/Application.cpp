#include "Application.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include "Engine/EnginesInclude.h"
#include "InputSystem.h"
namespace
{
    constexpr std::uint32_t InitialWidth = 1280;
    constexpr std::uint32_t InitialHeight = 720;
    constexpr wchar_t WindowTitle[] = L"D3D12 Application";

    constexpr double MaxDeltaTime = 0.1;
}

Application::Application(HINSTANCE instance, int showCommand)
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
    if (!Startup())
    {
        Shutdown();
        return 1;
    }

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

        InputSystem::GetInstance().Update();
        _engine->Tick(static_cast<float>(deltaTime));
    }

    Shutdown();

    return 0;
}

bool Application::Startup()
{
    try
    {
        if (FAILED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED)))
        {
            return false;
        }

        if(!InputSystem::GetInstance().Initialize())
        {
            return false;
        }

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

        _engineMsgHandlerId = _window.RegisterHandler<Engine, &Engine::HandleWindowMessage>(_engine.get());

    }
    catch (...)
    {
        return false;
    }

    return true;
}

void Application::Shutdown()
{
    if(_engineMsgHandlerId != Window::NULL_MESSAGE_HANDLER_ID)
    {
        _window.UnregisterMessageHandler(_engineMsgHandlerId);
    }
    _engine->Finalize();
    InputSystem::GetInstance().Shutdown();
    CoUninitialize();
}
