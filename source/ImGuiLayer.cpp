#include "ImGuiLayer.h"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#include <stdexcept>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

void ImGuiLayer::Initialize(const InitInfo& initInfo)
{
	if (_initialized)
	{
		throw std::logic_error("[ImGuiLayer::Initialize] Multiple initialization of ImGuiLayer");
	}

	if (!initInfo.IsValid())
	{
		throw std::invalid_argument("[ImGuiLayer::Initialize] initInfo is invalid");
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	ImGui::StyleColorsDark();

	if (!ImGui_ImplWin32_Init(initInfo._hwnd))
	{
		ImGui::DestroyContext();
		throw std::runtime_error("Failed to initialize ImGui Win32 backend.");
	}

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
	heapDesc.NodeMask = 0;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDesc.NumDescriptors = SrvDescriptorCapacity;
	HRESULT hr = initInfo._device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&_srvDescHeap));
	if (FAILED(hr))
	{
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		throw std::runtime_error("failed to create SRV descriptor heap");
	}

	_srvDescriptorIncrement = initInfo._device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	_freeSrvDescriptorIndices.clear();
	_freeSrvDescriptorIndices.reserve(SrvDescriptorCapacity);
	for (int32_t index = SrvDescriptorCapacity - 1; index >= 0; --index)
	{
		_freeSrvDescriptorIndices.push_back(index);
	}

	ImGui_ImplDX12_InitInfo imguiInitInfo{};
	imguiInitInfo.Device = initInfo._device;
	imguiInitInfo.CommandQueue = initInfo._commandQueue;
	imguiInitInfo.NumFramesInFlight = initInfo._frameCount;
	imguiInitInfo.RTVFormat = initInfo._rtvFormat;
	imguiInitInfo.DSVFormat = initInfo._dsvFormat;
	imguiInitInfo.SrvDescriptorAllocFn = &ImGuiLayer::AllocateSrvDescriptor;
	imguiInitInfo.SrvDescriptorFreeFn = &ImGuiLayer::FreeSrvDescriptor;
	imguiInitInfo.SrvDescriptorHeap = _srvDescHeap.Get();
	imguiInitInfo.UserData = this;
	if (!ImGui_ImplDX12_Init(&imguiInitInfo))
	{
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		throw std::runtime_error("Failed to initialize ImGui DX12 backend.");
	}

	_initialized = true;
}

void ImGuiLayer::Finalize()
{
	if (!_initialized)
	{
		return;
	}

	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	_freeSrvDescriptorIndices.clear();
	_srvDescHeap.Reset();

	_initialized = false;
}

void ImGuiLayer::BeginFrame()
{
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void ImGuiLayer::EndFrame(ID3D12GraphicsCommandList* commandList)
{
	ImGui::Render();

	ID3D12DescriptorHeap* heaps[] = { _srvDescHeap.Get() };
	commandList->SetDescriptorHeaps(1, heaps);
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
}

std::optional<LRESULT> ImGuiLayer::HandleWindowMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (!_initialized)
	{
		return std::nullopt;
	}

	LRESULT result = ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
	if (result == 0)
	{
		return std::nullopt;
	}

	return result;
}

void ImGuiLayer::AllocateSrvDescriptor(
	ImGui_ImplDX12_InitInfo* info,
	D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle,
	D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle)
{
	auto* self = static_cast<ImGuiLayer*>(info->UserData);

	if (self == nullptr ||
		self->_freeSrvDescriptorIndices.empty())
	{
		throw std::runtime_error(
			"ImGui SRV descriptor heap is full.");
	}

	const std::uint32_t index =
		self->_freeSrvDescriptorIndices.back();

	self->_freeSrvDescriptorIndices.pop_back();

	*outCpuHandle =
		self->_srvDescHeap->GetCPUDescriptorHandleForHeapStart();

	*outGpuHandle =
		self->_srvDescHeap->GetGPUDescriptorHandleForHeapStart();

	outCpuHandle->ptr +=
		static_cast<SIZE_T>(index) *
		self->_srvDescriptorIncrement;

	outGpuHandle->ptr +=
		static_cast<UINT64>(index) *
		self->_srvDescriptorIncrement;
}

void ImGuiLayer::FreeSrvDescriptor(
	ImGui_ImplDX12_InitInfo* info,
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
	D3D12_GPU_DESCRIPTOR_HANDLE)
{
	auto* self = static_cast<ImGuiLayer*>(info->UserData);

	if (self == nullptr ||
		self->_srvDescriptorIncrement == 0)
	{
		return;
	}

	const D3D12_CPU_DESCRIPTOR_HANDLE base =
		self->_srvDescHeap->
		GetCPUDescriptorHandleForHeapStart();

	const SIZE_T offset = cpuHandle.ptr - base.ptr;

	if (offset % self->_srvDescriptorIncrement != 0)
	{
		throw std::logic_error(
			"Invalid ImGui SRV descriptor handle.");
	}

	const auto index = static_cast<std::uint32_t>(
		offset / self->_srvDescriptorIncrement);

	if (index >= SrvDescriptorCapacity)
	{
		throw std::logic_error(
			"ImGui SRV descriptor is out of range.");
	}

	self->_freeSrvDescriptorIndices.push_back(index);
}
