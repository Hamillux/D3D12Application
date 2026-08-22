#include "Renderer.h"

#include <d3dx12.h>
#include <stdexcept>

Renderer::~Renderer()
{
    if (_initialized)
    {
        // Explicit Finalize() is the error-reporting shutdown path.
        try
        {
            Finalize();
        }
        catch (...)
        {
            if (_fenceEvent != nullptr)
            {
                CloseHandle(_fenceEvent);
                _fenceEvent = nullptr;
            }
        }
    }
}

void Renderer::Initialize(
    HWND window,
    std::uint32_t width,
    std::uint32_t height,
    const RendererConfig& config)
{
    if (_initialized)
    {
        throw std::logic_error("Renderer is already initialized.");
    }
    if (window == nullptr || width == 0 || height == 0 ||
        config._frameCount < 2 || config._rtvFormat == DXGI_FORMAT_UNKNOWN || config._dsvFormat == DXGI_FORMAT_UNKNOWN)
    {
        throw std::invalid_argument("Invalid renderer initialization parameters.");
    }

    _scissorRect = CD3DX12_RECT(0, 0, width, height);
    _viewport = CD3DX12_VIEWPORT(
        0.0f,
        0.0f,
        static_cast<float>(width),
        static_cast<float>(height));

    UINT factoryFlags = 0;
#ifdef _DEBUG
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();
        factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif

    ThrowIfFailed(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&_factory)));
    ThrowIfFailed(D3D12CreateDevice(
        nullptr,
        D3D_FEATURE_LEVEL_12_2,
        IID_PPV_ARGS(&_device)));

    const LUID luid = _device->GetAdapterLuid();
    ThrowIfFailed(
        _factory->EnumAdapterByLuid(luid, IID_PPV_ARGS(&_adapter))
    );

    D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
    commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    commandQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    ThrowIfFailed(_device->CreateCommandQueue(
        &commandQueueDesc,
        IID_PPV_ARGS(&_commandQueue)));

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = config._frameCount;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(_factory->CreateSwapChainForHwnd(
        _commandQueue.Get(),
        window,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain));
    ThrowIfFailed(swapChain.As(&_swapChain));
    ThrowIfFailed(_factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER));

    _frameIndex = _swapChain->GetCurrentBackBufferIndex();
    CreateFrameContexts(width, height, config);

    ThrowIfFailed(_device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&_uploadCommandAllocator)));

    CreateCommandLists();
    CreateFence();
    _initialized = true;
}

void Renderer::Finalize()
{
    if (!_initialized)
    {
        return;
    }
    if (_frameActive || _recordingUploadCommands)
    {
        throw std::logic_error("Cannot finalize the renderer while recording commands.");
    }

    WaitForIdle();

    CloseHandle(_fenceEvent);
    _fenceEvent = nullptr;

    _frameContexts.clear();
    _uploadCommandList.Reset();
    _uploadCommandAllocator.Reset();
    _commandList.Reset();
    _fence.Reset();
    _depthStencilViewHeap.Reset();
    _renderTargetViewHeap.Reset();
    _swapChain.Reset();
    _commandQueue.Reset();
    _device.Reset();
    _adapter.Reset();
    _factory.Reset();
    _memoryInfo = DXGI_QUERY_VIDEO_MEMORY_INFO{};

    _renderContext = {};
    _nextFenceValue = 1;
    _frameIndex = 0;
    _initialized = false;
}

void Renderer::WaitForIdle()
{
    if (!_initialized)
    {
        return;
    }
    if (_frameActive || _recordingUploadCommands)
    {
        throw std::logic_error("Cannot wait for the renderer while recording commands.");
    }

    const std::uint64_t fenceValue = _nextFenceValue;
    SignalFence();
    WaitForFence(fenceValue);
}

RenderContext& Renderer::BeginFrame()
{
    if (!_initialized)
    {
        throw std::logic_error("Renderer is not initialized.");
    }
    if (_frameActive)
    {
        throw std::logic_error("A frame is already active.");
    }

    _frameIndex = _swapChain->GetCurrentBackBufferIndex();
    FrameContext& frame = _frameContexts[_frameIndex];
    WaitForFence(frame.fenceValue);

    ThrowIfFailed(frame.commandAllocator->Reset());
    ThrowIfFailed(_commandList->Reset(frame.commandAllocator.Get(), nullptr));

    _renderContext = {
        _commandList.Get(),
        &_scissorRect,
        &_viewport,
        frame.renderTargetView,
        frame.depthStencilView
    };

    _commandList->RSSetScissorRects(1, _renderContext.scissorRect);
    _commandList->RSSetViewports(1, _renderContext.viewport);

    const D3D12_RESOURCE_BARRIER renderTargetBarrier =
        CD3DX12_RESOURCE_BARRIER::Transition(
            frame.renderTarget.Get(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
    _commandList->ResourceBarrier(1, &renderTargetBarrier);
    _commandList->OMSetRenderTargets(
        1,
        &_renderContext.renderTargetView,
        TRUE,
        &_renderContext.depthStencilView);

    constexpr FLOAT clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    _commandList->ClearRenderTargetView(
        _renderContext.renderTargetView,
        clearColor,
        0,
        nullptr);
    _commandList->ClearDepthStencilView(
        _renderContext.depthStencilView,
        D3D12_CLEAR_FLAG_DEPTH,
        1.0f,
        0,
        0,
        nullptr);

    _frameActive = true;
    return _renderContext;
}

void Renderer::EndFrame()
{
    if (!_frameActive)
    {
        throw std::logic_error("No frame is active.");
    }

    FrameContext& frame = _frameContexts[_frameIndex];
    const D3D12_RESOURCE_BARRIER presentBarrier =
        CD3DX12_RESOURCE_BARRIER::Transition(
            frame.renderTarget.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT);
    _commandList->ResourceBarrier(1, &presentBarrier);
    ThrowIfFailed(_commandList->Close());

    ID3D12CommandList* commandLists[] = { _commandList.Get() };
    _commandQueue->ExecuteCommandLists(1, commandLists);
    const HRESULT presentResult = _swapChain->Present(1, 0);
    frame.fenceValue = _nextFenceValue;
    SignalFence();
    _frameActive = false;
    ThrowIfFailed(presentResult);
}

ID3D12GraphicsCommandList* Renderer::BeginUploadCommands()
{
    if (!_initialized)
    {
        throw std::logic_error("Renderer is not initialized.");
    }
    if (_recordingUploadCommands)
    {
        throw std::logic_error("Upload commands are already being recorded.");
    }

    ThrowIfFailed(_uploadCommandAllocator->Reset());
    ThrowIfFailed(_uploadCommandList->Reset(_uploadCommandAllocator.Get(), nullptr));
    _recordingUploadCommands = true;
    return _uploadCommandList.Get();
}

void Renderer::EndUploadCommands()
{
    if (!_recordingUploadCommands)
    {
        throw std::logic_error("No upload commands are being recorded.");
    }

    ThrowIfFailed(_uploadCommandList->Close());
    _recordingUploadCommands = false;
}

std::uint64_t Renderer::ExecuteUploadCommands()
{
    if (!_initialized)
    {
        throw std::logic_error("Renderer is not initialized.");
    }
    if (_recordingUploadCommands)
    {
        throw std::logic_error("Upload commands must be ended before execution.");
    }

    ID3D12CommandList* commandLists[] = { _uploadCommandList.Get() };
    _commandQueue->ExecuteCommandLists(1, commandLists);

    const std::uint64_t fenceValue = _nextFenceValue;
    SignalFence();
    WaitForFence(fenceValue);
    return fenceValue;
}

const DXGI_QUERY_VIDEO_MEMORY_INFO& Renderer::UpdateMemoryInfo()
{
    if (!_adapter)
    {
        throw std::logic_error("adapter is null");
    }

    ThrowIfFailed(
        _adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &_memoryInfo)
    );

    return _memoryInfo;
}

void Renderer::ThrowIfFailed(HRESULT result)
{
    if (FAILED(result))
    {
        throw std::runtime_error("Direct3D 12 API call failed.");
    }
}

void Renderer::CreateCommandLists()
{
    ThrowIfFailed(_device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        _frameContexts.front().commandAllocator.Get(),
        nullptr,
        IID_PPV_ARGS(&_commandList)));
    ThrowIfFailed(_commandList->Close());

    ThrowIfFailed(_device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        _uploadCommandAllocator.Get(),
        nullptr,
        IID_PPV_ARGS(&_uploadCommandList)));
    ThrowIfFailed(_uploadCommandList->Close());
}

void Renderer::CreateFrameContexts(
    std::uint32_t width,
    std::uint32_t height,
    const RendererConfig& config)
{
    const uint32_t frameCount = config._frameCount;

    _frameContexts.clear();
    _frameContexts.resize(frameCount);

    for (FrameContext& frame : _frameContexts)
    {
        ThrowIfFailed(_device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&frame.commandAllocator)));
    }

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = frameCount;
    ThrowIfFailed(_device->CreateDescriptorHeap(
        &rtvHeapDesc,
        IID_PPV_ARGS(&_renderTargetViewHeap)));

    const UINT rtvDescriptorSize = _device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
        _renderTargetViewHeap->GetCPUDescriptorHandleForHeapStart();

    for (std::uint32_t index = 0; index < frameCount; ++index)
    {
        FrameContext& frame = _frameContexts[index];
        ThrowIfFailed(_swapChain->GetBuffer(
            index,
            IID_PPV_ARGS(&frame.renderTarget)));

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
        rtvDesc.Format = config._rtvFormat;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        _device->CreateRenderTargetView(frame.renderTarget.Get(), &rtvDesc, rtvHandle);
        frame.renderTargetView = rtvHandle;
        rtvHandle.ptr += rtvDescriptorSize;
    }

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.NumDescriptors = frameCount;
    ThrowIfFailed(_device->CreateDescriptorHeap(
        &dsvHeapDesc,
        IID_PPV_ARGS(&_depthStencilViewHeap)));

    const UINT dsvDescriptorSize = _device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle =
        _depthStencilViewHeap->GetCPUDescriptorHandleForHeapStart();

    for (FrameContext& frame : _frameContexts)
    {
        const CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
        const D3D12_RESOURCE_DESC depthStencilDesc = CD3DX12_RESOURCE_DESC::Tex2D(
            config._dsvFormat,
            width,
            height,
            1,
            0,
            1,
            0,
            D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

        D3D12_CLEAR_VALUE clearValue{};
        clearValue.Format = config._dsvFormat;
        clearValue.DepthStencil.Depth = 1.0f;

        ThrowIfFailed(_device->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &depthStencilDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &clearValue,
            IID_PPV_ARGS(&frame.depthStencilBuffer)));

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
        dsvDesc.Format = config._dsvFormat;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        _device->CreateDepthStencilView(
            frame.depthStencilBuffer.Get(),
            &dsvDesc,
            dsvHandle);
        frame.depthStencilView = dsvHandle;
        dsvHandle.ptr += dsvDescriptorSize;
    }
}

void Renderer::CreateFence()
{
    ThrowIfFailed(_device->CreateFence(
        0,
        D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(&_fence)));
    _nextFenceValue = 1;

    _fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (_fenceEvent == nullptr)
    {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }
}

void Renderer::WaitForFence(std::uint64_t fenceValue)
{
    if (_fence->GetCompletedValue() >= fenceValue)
    {
        return;
    }

    ThrowIfFailed(_fence->SetEventOnCompletion(fenceValue, _fenceEvent));
    WaitForSingleObject(_fenceEvent, INFINITE);
}

void Renderer::SignalFence()
{
    ThrowIfFailed(_commandQueue->Signal(_fence.Get(), _nextFenceValue));
    ++_nextFenceValue;
}
