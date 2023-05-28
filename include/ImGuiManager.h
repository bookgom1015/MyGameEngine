#pragma once

#include <d3dx12.h>

class ImGuiManager {
public:
	ImGuiManager() = default;
	virtual ~ImGuiManager() = default;

public:
	bool Initialize(HWND hwnd, ID3D12Device*const device, ID3D12DescriptorHeap*const heap, int bufferCount, DXGI_FORMAT format);
	void CleanUp();

	bool BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
		UINT descSize);

private:
	ID3D12Device* md3dDevice;
	ID3D12DescriptorHeap* mHeap;

	int mSwapChainBufferCount;
	DXGI_FORMAT mBackBufferFormat;

	D3D12_CPU_DESCRIPTOR_HANDLE mhCpuSrv;
	D3D12_GPU_DESCRIPTOR_HANDLE mhGpuSrv;
};