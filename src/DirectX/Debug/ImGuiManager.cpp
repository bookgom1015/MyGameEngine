#include "DirectX/Debug/ImGuiManager.h"
#include "Common/Debug/Logger.h"

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_win32.h>
#include <imgui/backends/imgui_impl_dx12.h>

BOOL ImGuiManager::Initialize(HWND hwnd, ID3D12Device* const device, ID3D12DescriptorHeap* const heap, INT bufferCount, DXGI_FORMAT format) {
	md3dDevice = device;
	mHeap = heap;

	mSwapChainBufferCount = bufferCount;
	mBackBufferFormat = format;

	// Setup dear ImGui context.
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	// Setup Dear ImGui style.
	ImGui::StyleColorsDark();
	
	// Setup platform/renderer backends
	CheckReturn(ImGui_ImplWin32_Init(hwnd));

	return TRUE;
}

void ImGuiManager::AllocateDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu, CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu, UINT descSize) {
	mhCpuSrv = hCpu.Offset(1, descSize);
	mhGpuSrv = hGpu.Offset(1, descSize);
}

BOOL ImGuiManager::BuildDescriptors() {
	CheckReturn(ImGui_ImplDX12_Init(
		md3dDevice,
		mSwapChainBufferCount,
		mBackBufferFormat,
		mHeap,
		mhCpuSrv,
		mhGpuSrv
	));

	return TRUE;
}

void ImGuiManager::CleanUp() {
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}