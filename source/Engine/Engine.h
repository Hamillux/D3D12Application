#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dx12.h>
#include <wrl/client.h>

#include <vector>
#include "Utility/Math.h"
#include <stdexcept>

class Engine
{
    template<typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;
public:
    Engine() = default;
    virtual ~Engine() {}

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    virtual void Initialize(HWND window, uint32_t width, uint32_t height);
    virtual void Finalize();
    void Tick(float deltaTime);

protected:
    virtual size_t GetFrameCount() const
    {
        return 2;
    }

    virtual void Update(float deltaTime)
    {}

    struct RenderContext
    {
        ID3D12GraphicsCommandList* command = nullptr;
        const D3D12_RECT* scissorRect = nullptr;
        const D3D12_VIEWPORT* viewport = nullptr;
    };
    virtual void Render(RenderContext& context);

    static void ThrowIfFailed(HRESULT hr)
    {
        if (FAILED(hr))
        {
            throw std::runtime_error("API call failed.");
        }
    }

    ID3D12GraphicsCommandList* BeginUploadCommands();
    void EndUploadCommands();
    uint64_t ExecuteUploadCommands();

    void GetCurrentBackBufferHandles(D3D12_CPU_DESCRIPTOR_HANDLE& rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE& dsvHandle)
    {
        rtvHandle = _frameContexts[_frameIndex]._rtvHandle;
        dsvHandle = _frameContexts[_frameIndex]._dsvHandle;
    }

private:
    void ExecuteRender();

    void CreateCommandLists();
    void CreateFrameContexts();
    void CreateFence();
    void WaitForFence(std::uint64_t fenceValue);
    void SignalAndIncFenceValue();

    ComPtr<ID3D12Device> _device;
    ComPtr<IDXGISwapChain3> _swapChain;
    ComPtr<ID3D12CommandQueue> _commandQueue;
    ComPtr<ID3D12DescriptorHeap> _renderTargetViewHeap;
    ComPtr<ID3D12DescriptorHeap> _depthStencilViewHeap;
    ComPtr<ID3D12Fence> _fence;
    ComPtr<ID3D12GraphicsCommandList> _commandList;
    ComPtr<ID3D12CommandAllocator> _uploadCommandAllocator;
    ComPtr<ID3D12GraphicsCommandList> _uploadCommandList;

    struct FrameContext
    {
        std::uint64_t _fenceValue = 0;
        ComPtr<ID3D12CommandAllocator> _commandAllocator;
        ComPtr<ID3D12Resource> _renderTarget;
        D3D12_CPU_DESCRIPTOR_HANDLE _rtvHandle{};
        ComPtr<ID3D12Resource> _depthStencilBuffer;
        D3D12_CPU_DESCRIPTOR_HANDLE _dsvHandle{};

        RenderContext* _currRenderContext = nullptr;

        void BeginFrame(RenderContext* renderContext)
        {
            _currRenderContext = renderContext;

            ID3D12GraphicsCommandList* command = _currRenderContext->command;

            _commandAllocator->Reset();
            command->Reset(_commandAllocator.Get(), nullptr);

            command->RSSetScissorRects(1, _currRenderContext->scissorRect);
            command->RSSetViewports(1, _currRenderContext->viewport);

            const D3D12_RESOURCE_BARRIER renderTargetBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
                _renderTarget.Get(),
                D3D12_RESOURCE_STATE_PRESENT,
                D3D12_RESOURCE_STATE_RENDER_TARGET
            );
            command->ResourceBarrier(1, &renderTargetBarrier);

            command->OMSetRenderTargets(1, &_rtvHandle, TRUE, &_dsvHandle);
        }

        void EndFrame()
        {
            ID3D12GraphicsCommandList* command = _currRenderContext->command;

            const D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                _renderTarget.Get(),
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                D3D12_RESOURCE_STATE_PRESENT
            );

            command->ResourceBarrier(1, &barrier);
            command->Close();

            _currRenderContext = nullptr;
        }

    };
    std::vector<FrameContext> _frameContexts;

    HANDLE _fenceEvent = nullptr;
    uint64_t _nextFenceValue = 1;
    uint32_t _frameIndex = 0;
    uint32_t _width = 0;
    uint32_t _height = 0;
    bool _recordingUploadCommands = false;

    D3D12_RECT _scissorRect{};
    D3D12_VIEWPORT _viewport{};
};
