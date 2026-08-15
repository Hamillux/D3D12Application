#include "Engine.h"
#include <SimpleMath.h>
#include <algorithm>

void Engine::Initialize(HWND window, uint32_t width, uint32_t height)
{
	_height = height;
	_width = width;

	_scissorRect = CD3DX12_RECT(0, 0, _width, _height);
	_viewport = CD3DX12_VIEWPORT(0.f, 0.f, static_cast<float>(_width), static_cast<float>(_height));

	UINT factoryFlags = 0;
#ifdef _DEBUG
	ComPtr<ID3D12Debug> debugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		debugController->EnableDebugLayer();
		factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
	}
#endif

	ComPtr<IDXGIFactory2> factory;
	ThrowIfFailed(
		CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&factory))
	);

	// create device
	ThrowIfFailed(
		D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&_device))
	);

	// create command queue
	{
		D3D12_COMMAND_QUEUE_DESC desc{};
		desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		desc.NodeMask = 0;
		ThrowIfFailed(
			_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&_commandQueue))
		);
	}

	// create swapchain
	{
		DXGI_SWAP_CHAIN_DESC1 desc{};
		desc.Width = width;
		desc.Height = height;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.Stereo = FALSE;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.BufferCount = GetFrameCount();
		desc.Scaling = DXGI_SCALING_STRETCH;
		desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
		desc.Flags = 0;

		ComPtr<IDXGISwapChain1> swapChain;
		ThrowIfFailed(
			factory->CreateSwapChainForHwnd(
				_commandQueue.Get(),
				window,
				&desc,
				nullptr,
				nullptr,
				&swapChain
			)
		);

		ThrowIfFailed(
			swapChain.As(&_swapChain)
		);

		ThrowIfFailed(
			factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER)
		);

		_frameIndex = _swapChain->GetCurrentBackBufferIndex();

	}

	CreateFrameContexts();

	ThrowIfFailed(
		_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_uploadCommandAllocator))
	);

	CreateCommandLists();
	CreateFence();
}

void Engine::Finalize()
{
	const uint64_t fenceValue = _nextFenceValue;
	SignalAndIncFenceValue();
	WaitForFence(fenceValue);

	CloseHandle(_fenceEvent);
}

void Engine::Tick(float deltaTime)
{
	Update(deltaTime);
	ExecuteRender();
}

void Engine::Render(RenderContext& context)
{
	using namespace DirectX::SimpleMath;

	// Clear back buffer.
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle{}, dsvHandle{};
	GetCurrentBackBufferHandles(rtvHandle, dsvHandle);
	const FLOAT fillColor[4] = { 0.f, 0.f, 0.f, 0.f };
	context.command->ClearRenderTargetView(rtvHandle, fillColor, 0, nullptr);
	context.command->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);
}

ID3D12GraphicsCommandList* Engine::BeginUploadCommands()
{
	if (_recordingUploadCommands)
	{
		throw std::runtime_error("");
	}

	ThrowIfFailed(_uploadCommandAllocator->Reset());
	ThrowIfFailed(_uploadCommandList->Reset(_uploadCommandAllocator.Get(), nullptr));
	_recordingUploadCommands = true;

	return _uploadCommandList.Get();
}

void Engine::EndUploadCommands()
{
	if (!_recordingUploadCommands)
	{
		throw std::runtime_error("");
	}

	_uploadCommandList->Close();
	_recordingUploadCommands = false;
}

uint64_t Engine::ExecuteUploadCommands()
{
	if (_recordingUploadCommands)
	{
		throw std::runtime_error("");
	}

	ID3D12CommandList* commands[] = { _uploadCommandList.Get() };
	_commandQueue->ExecuteCommandLists(1, commands);
	const uint64_t fenceValue = _nextFenceValue;
	SignalAndIncFenceValue();
	WaitForFence(fenceValue);

	return fenceValue;
}

void Engine::ExecuteRender()
{
	_frameIndex = _swapChain->GetCurrentBackBufferIndex();

	FrameContext& currFrame = _frameContexts[_frameIndex];
	WaitForFence(currFrame._fenceValue);

	RenderContext context{_commandList.Get(), &_scissorRect, &_viewport};
	currFrame.BeginFrame(&context);
	Render(context);
	currFrame.EndFrame();
	
	ID3D12CommandList* cmdList = {_commandList.Get()};
	_commandQueue->ExecuteCommandLists(1, &cmdList);

	_swapChain->Present(1, 0);

	currFrame._fenceValue = _nextFenceValue;
	SignalAndIncFenceValue();
}

void Engine::CreateCommandLists()
{
	ThrowIfFailed(
		_device->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			_frameContexts[0]._commandAllocator.Get(),
			nullptr,
			IID_PPV_ARGS(&_commandList)
		)
	);
	ThrowIfFailed(_commandList->Close());

	ThrowIfFailed(
		_device->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			_uploadCommandAllocator.Get(),
			nullptr,
			IID_PPV_ARGS(&_uploadCommandList)
		)
	);
	ThrowIfFailed(_uploadCommandList->Close());
}

void Engine::CreateFrameContexts()
{
	const UINT frameCount = static_cast<UINT>(GetFrameCount());

	_frameContexts.clear();

	// get back buffers
	for (size_t i = 0; i < frameCount; ++i)
	{
		_frameContexts.emplace_back();
		ThrowIfFailed(
			_device->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_frameContexts.back()._commandAllocator)
			)
		);
	}

	// create RTV heap
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc{};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		desc.NumDescriptors = frameCount;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		desc.NodeMask = 0;

		ThrowIfFailed(
			_device->CreateDescriptorHeap(
				&desc,
				IID_PPV_ARGS(&_renderTargetViewHeap)
			)
		);
	}

	// create RTVs
	{
		const UINT renderTargetViewDescriptorSize =
			_device->GetDescriptorHandleIncrementSize(
				D3D12_DESCRIPTOR_HEAP_TYPE_RTV
			);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
			_renderTargetViewHeap->GetCPUDescriptorHandleForHeapStart();
		for (UINT i = 0; i < frameCount; ++i)
		{
			ThrowIfFailed(
				_swapChain->GetBuffer(
					i,
					IID_PPV_ARGS(&_frameContexts[i]._renderTarget)
				)
			);

			D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
			rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			rtvDesc.Texture2D.MipSlice = 0;
			rtvDesc.Texture2D.PlaneSlice = 0;

			_device->CreateRenderTargetView(
				_frameContexts[i]._renderTarget.Get(),
				&rtvDesc,
				rtvHandle
			);
			_frameContexts[i]._rtvHandle = rtvHandle;

			rtvHandle.ptr += renderTargetViewDescriptorSize;
		}
	}

	// create depth stencil buffer
	for (UINT i = 0; i < frameCount; ++i)
	{
		CD3DX12_HEAP_PROPERTIES heapProp(D3D12_HEAP_TYPE_DEFAULT);

		const D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(
			DXGI_FORMAT_D32_FLOAT,
			_width, _height,
			1, 0, 1, 0,
			D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
		);

		D3D12_CLEAR_VALUE clearValue{};
		clearValue.Format = desc.Format;
		clearValue.DepthStencil.Depth = 1.0f;
		clearValue.DepthStencil.Stencil = 0;

		ThrowIfFailed(
			_device->CreateCommittedResource(
				&heapProp,
				D3D12_HEAP_FLAG_NONE,
				&desc,
				D3D12_RESOURCE_STATE_DEPTH_WRITE,
				&clearValue,
				IID_PPV_ARGS(&_frameContexts[i]._depthStencilBuffer)
			)
		);
	}

	// create DSV heap
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc{};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		desc.NumDescriptors = frameCount;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		desc.NodeMask = 0;

		ThrowIfFailed(
			_device->CreateDescriptorHeap(
				&desc,
				IID_PPV_ARGS(&_depthStencilViewHeap)
			)
		);
	}

	// create DSVs
	{
		const UINT depthStencilViewDescriptorSize =
			_device->GetDescriptorHandleIncrementSize(
				D3D12_DESCRIPTOR_HEAP_TYPE_DSV
			);

		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle =
			_depthStencilViewHeap->GetCPUDescriptorHandleForHeapStart();

		for (UINT i = 0; i < frameCount; ++i)
		{
			D3D12_DEPTH_STENCIL_VIEW_DESC desc{};
			desc.Format = DXGI_FORMAT_D32_FLOAT;
			desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			desc.Flags = D3D12_DSV_FLAG_NONE;
			desc.Texture2D.MipSlice = 0;
			
			_device->CreateDepthStencilView(
				_frameContexts[i]._depthStencilBuffer.Get(),
				&desc,
				dsvHandle
			);
			_frameContexts[i]._dsvHandle = dsvHandle;

			dsvHandle.ptr += depthStencilViewDescriptorSize;
		}
	}

}

void Engine::CreateFence()
{
	ThrowIfFailed(
		_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence))
	);
	_nextFenceValue = 1;

	_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (_fenceEvent == nullptr)
	{
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}
}

void Engine::WaitForFence(std::uint64_t fenceValue)
{
	if (_fence->GetCompletedValue() >= fenceValue)
	{
		return;
	}

	ThrowIfFailed(
		_fence->SetEventOnCompletion(fenceValue, _fenceEvent)
	);
	WaitForSingleObject(_fenceEvent, INFINITE);
}

void Engine::SignalAndIncFenceValue()
{
	_commandQueue->Signal(_fence.Get(), _nextFenceValue);
	++_nextFenceValue;
}
