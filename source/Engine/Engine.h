#pragma once

#include "Renderer.h"
#include "ImGuiLayer.h"

#include <cstdint>
#include <wrl/client.h>

class Engine
{
public:
    Engine() = default;
    virtual ~Engine() = default;

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    void Initialize(HWND window, std::uint32_t width, std::uint32_t height);
    void Finalize();
    void Tick(float deltaTime);

    std::optional<LRESULT> HandleWindowMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        return _imguiLayer.HandleWindowMessage(hwnd, msg, wParam, lParam);
    }

protected:
    template<typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    virtual RendererConfig GetRendererConfig() const
    {
        RendererConfig cfg{};
        cfg._rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        cfg._dsvFormat = DXGI_FORMAT_D32_FLOAT;
        cfg._frameCount = 2;
        return cfg;
    }

    virtual void OnInitialize()
    {}

    virtual void Update(float)
    {}

    virtual void BuildImGui();

    virtual void Render(RenderContext& context);

    virtual void OnFinalize()
    {}

    ID3D12Device* GetDevice() const noexcept
    {
        return _renderer.GetDevice();
    }

    ID3D12GraphicsCommandList* BeginImmediateCommands()
    {
        return _renderer.BeginImmediateCommands();
    }

    void EndImmediateCommands()
    {
        _renderer.EndImmediateCommands();
    }

    std::uint64_t ExecuteImmediateCommands()
    {
        return _renderer.ExecuteImmediateCommands();
    }

private:
    bool _initialized = false;
    Renderer _renderer;
    ImGuiLayer _imguiLayer;
};
