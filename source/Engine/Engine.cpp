#include "Engine.h"

#include <stdexcept>

void Engine::Initialize(HWND window, std::uint32_t width, std::uint32_t height)
{
    if (_initialized)
    {
        throw std::logic_error("Engine is already initialized.");
    }

    try
    {
        const RendererConfig rdrCfg = GetRendererConfig();
        _renderer.Initialize(window, width, height, rdrCfg);
        OnInitialize();

        _initialized = true;
    }
    catch (...)
    {
        _renderer.Finalize();
        _initialized = false;
        throw;
    }
}

void Engine::Finalize()
{
    if (!_initialized)
    {
        return;
    }

    // Derived GPU resources must not be released while the queue still uses them.
    _renderer.WaitForIdle();

    try
    {
        OnFinalize();
    }
    catch (...)
    {
        _renderer.Finalize();
        _initialized = false;
        throw;
    }

    _renderer.Finalize();
    _initialized = false;
}

void Engine::Tick(float deltaTime)
{
    if (!_initialized)
    {
        throw std::logic_error("Engine is not initialized.");
    }

    Update(deltaTime);

    RenderContext& context = _renderer.BeginFrame();
    Render(context);
    _renderer.EndFrame();
}

void Engine::Render(RenderContext&)
{
}
