#pragma once

#include <Windows.h>
#include <cstdint>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <vector>

struct RenderContext
{
    ID3D12GraphicsCommandList* command = nullptr;
    const D3D12_RECT* scissorRect = nullptr;
    const D3D12_VIEWPORT* viewport = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView{};
    D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView{};
};

class Renderer final
{
public:
    Renderer() = default;
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    void Initialize(
        HWND window,
        std::uint32_t width,
        std::uint32_t height,
        std::uint32_t frameCount);
    void WaitForIdle();
    void Finalize();

    RenderContext& BeginFrame();
    void EndFrame();

    ID3D12GraphicsCommandList* BeginUploadCommands();
    void EndUploadCommands();
    std::uint64_t ExecuteUploadCommands();

    ID3D12Device* GetDevice() const noexcept
    {
        return _device.Get();
    }

private:
    template<typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    struct FrameContext
    {
        std::uint64_t fenceValue = 0;
        ComPtr<ID3D12CommandAllocator> commandAllocator;
        ComPtr<ID3D12Resource> renderTarget;
        D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView{};
        ComPtr<ID3D12Resource> depthStencilBuffer;
        D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView{};
    };

    static void ThrowIfFailed(HRESULT result);

    void CreateCommandLists();
    void CreateFrameContexts(
        std::uint32_t width,
        std::uint32_t height,
        std::uint32_t frameCount);
    void CreateFence();
    void WaitForFence(std::uint64_t fenceValue);
    void SignalFence();

    bool _initialized = false;
    bool _frameActive = false;
    bool _recordingUploadCommands = false;

    ComPtr<ID3D12Device> _device;
    ComPtr<IDXGISwapChain3> _swapChain;
    ComPtr<ID3D12CommandQueue> _commandQueue;
    ComPtr<ID3D12DescriptorHeap> _renderTargetViewHeap;
    ComPtr<ID3D12DescriptorHeap> _depthStencilViewHeap;
    ComPtr<ID3D12Fence> _fence;
    ComPtr<ID3D12GraphicsCommandList> _commandList;
    ComPtr<ID3D12CommandAllocator> _uploadCommandAllocator;
    ComPtr<ID3D12GraphicsCommandList> _uploadCommandList;

    std::vector<FrameContext> _frameContexts;
    RenderContext _renderContext{};

    HANDLE _fenceEvent = nullptr;
    std::uint64_t _nextFenceValue = 1;
    std::uint32_t _frameIndex = 0;

    D3D12_RECT _scissorRect{};
    D3D12_VIEWPORT _viewport{};
};
