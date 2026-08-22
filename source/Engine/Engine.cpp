#include "Engine.h"
#include <imgui.h>
#include <stdexcept>
#include <algorithm>
#include <cstdio>
#include <cfloat>

void Engine::Initialize(HWND window, std::uint32_t width, std::uint32_t height)
{
    if (_initialized)
    {
        throw std::logic_error("Engine is already initialized.");
    }

    try
    {
        // init renderer
        const RendererConfig rdrCfg = GetRendererConfig();
        _renderer.Initialize(window, width, height, rdrCfg);

        // init imgui layer
        {
            ImGuiLayer::InitInfo initInfo{};
            initInfo._hwnd = window;
            initInfo._device = _renderer.GetDevice();
            initInfo._commandQueue = _renderer.GetCommandQueue();
            initInfo._rtvFormat = rdrCfg._rtvFormat;
            initInfo._dsvFormat = rdrCfg._dsvFormat;
            initInfo._frameCount = rdrCfg._frameCount;
            _imguiLayer.Initialize(initInfo);

        }

        OnInitialize();

        _initialized = true;
    }
    catch (...)
    {
        _imguiLayer.Finalize();
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
        _imguiLayer.Finalize();
        _renderer.Finalize();
        _initialized = false;
        throw;
    }

    _imguiLayer.Finalize();
    _renderer.Finalize();
    _initialized = false;
}

void Engine::Tick(float deltaTime)
{
    if (!_initialized)
    {
        throw std::logic_error("Engine is not initialized.");
    }

    _renderer.UpdateMemoryInfo();

    _imguiLayer.BeginFrame();

    Update(deltaTime);

    BuildImGui();

    RenderContext& context = _renderer.BeginFrame();

    Render(context);

    _imguiLayer.EndFrame(context.command);

    _renderer.EndFrame();

}

void Engine::BuildImGui()
{
    const auto& memoryInfo = _renderer.GetMemoryInfo();

    if (ImGui::Begin("GPU Memory"))
    {
        constexpr double MiB = 1024.0 * 1024.0;

        const double usage = static_cast<double>(memoryInfo.CurrentUsage) / MiB;
        const double budget = static_cast<double>(memoryInfo.Budget) / MiB;
        const double available = static_cast<double>(memoryInfo.AvailableForReservation) / MiB;
        const double reservation = static_cast<double>(memoryInfo.CurrentReservation) / MiB;

        ImGui::Text("             Current Usage: %.2f MiB", usage);
        ImGui::Text("                    Budget: %.2f MiB", budget);
        ImGui::Text(" Available for reservation: %.2f MiB", available);
        ImGui::Text("               Reservation: %.2f MiB", reservation);

        if (memoryInfo.Budget > 0)
        {
            const float ratio = static_cast<float>(
                static_cast<double>(memoryInfo.CurrentUsage) /
                static_cast<double>(memoryInfo.Budget)
                );

            char overlay[64];
            std::snprintf(
                overlay,
                sizeof(overlay),
                "%.0f / %.0f MiB (%.1f%%)",
                usage,
                budget,
                ratio * 100.0f
            );

            ImGui::ProgressBar(
                std::clamp(ratio, 0.0f, 1.0f),
                ImVec2(-FLT_MIN, 0.0f),
                overlay
            );
        }
    }

    ImGui::End();
}

void Engine::Render(RenderContext&)
{
}
