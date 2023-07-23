#pragma once

#include <d3dx12.h>
#include <vector>
#include <wrl.h>

#include "Samplers.h"

class ShaderManager;
namespace DxrBackBuffer { class DxrBackBufferClass; }

namespace BackBuffer {
	namespace RootSignatureLayout {
		enum {
			ECB_Pass = 0,
			ESI_Color,
			ESI_Albedo,
			ESI_Normal,
			ESI_Depth,
			ESI_Specular,
			ESI_Shadow,
			ESI_AOCoefficient,
			Count
		};
	}

	class BackBufferClass {
	private:
		friend class DxrBackBuffer::DxrBackBufferClass;

	public:
		BackBufferClass() = default;
		virtual ~BackBufferClass() = default;

	public:
		__forceinline CD3DX12_GPU_DESCRIPTOR_HANDLE BackBufferSrv(size_t index) const;

	public:
		bool Initialize(ID3D12Device*const device, ShaderManager*const manager, UINT width, UINT height,
			DXGI_FORMAT backBufferFormat, UINT bufferCount);
		bool CompileShaders(const std::wstring& filePath);
		bool BuildRootSignature(const StaticSamplers& samplers);
		bool BuildPso();
		void Run(
			ID3D12GraphicsCommandList*const cmdList,
			ID3D12Resource* backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE ri_backBuffer,
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
			D3D12_GPU_DESCRIPTOR_HANDLE si_color,
			D3D12_GPU_DESCRIPTOR_HANDLE si_albedo, 
			D3D12_GPU_DESCRIPTOR_HANDLE si_normal, 
			D3D12_GPU_DESCRIPTOR_HANDLE si_depth, 
			D3D12_GPU_DESCRIPTOR_HANDLE si_specular, 
			D3D12_GPU_DESCRIPTOR_HANDLE si_shadow,
			D3D12_GPU_DESCRIPTOR_HANDLE si_aoCoefficient);

		void BuildDescriptors(
			ID3D12Resource*const buffers[],
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuSrv, 
			CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpuSrv, 
			UINT descSize);
		bool OnResize(ID3D12Resource*const buffers[], UINT width, UINT height);

	private:
		void BuildDescriptors(ID3D12Resource*const buffers[]);

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;

		UINT mWidth;
		UINT mHeight;

		D3D12_VIEWPORT mViewport;
		D3D12_RECT mScissorRect;

		DXGI_FORMAT mBackBufferFormat;
		UINT mBackBufferCount;

		std::vector<CD3DX12_CPU_DESCRIPTOR_HANDLE> mhBackBufferCpuSrvs;
		std::vector<CD3DX12_GPU_DESCRIPTOR_HANDLE> mhBackBufferGpuSrvs;
	};
}

CD3DX12_GPU_DESCRIPTOR_HANDLE BackBuffer::BackBufferClass::BackBufferSrv(size_t index) const {
	return mhBackBufferGpuSrvs[index];
}