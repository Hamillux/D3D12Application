#pragma once

#include "d3d12.h"
#include <wrl/client.h>
#include <optional>
#include <vector>

class ImGuiLayer
{
public:
	struct InitInfo
	{
		HWND _hwnd = nullptr;
		ID3D12Device* _device{ nullptr };
		ID3D12CommandQueue* _commandQueue{ nullptr };
		uint32_t _frameCount{ 1 };
		DXGI_FORMAT _rtvFormat{ DXGI_FORMAT_UNKNOWN };
		DXGI_FORMAT _dsvFormat{ DXGI_FORMAT_UNKNOWN };

		bool IsValid() const
		{
			return _hwnd && _device && _commandQueue;
		}
	};
	void Initialize(const InitInfo& initInfo);
	void Finalize();

	void BeginFrame();
	void EndFrame(ID3D12GraphicsCommandList* commandList);

	std::optional<LRESULT> HandleWindowMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);


private:
	static constexpr uint32_t SrvDescriptorCapacity = 64;
	static void AllocateSrvDescriptor(
		struct ImGui_ImplDX12_InitInfo* info,
		D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle,
		D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle);
	static void FreeSrvDescriptor(
		struct ImGui_ImplDX12_InitInfo* info,
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle);
	UINT _srvDescriptorIncrement = 0;
	std::vector<uint32_t> _freeSrvDescriptorIndices;

	bool _initialized = false;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _srvDescHeap;
};
