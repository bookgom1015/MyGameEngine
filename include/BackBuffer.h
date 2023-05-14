#pragma once

#include <d3dx12.h>
#include <vector>

#include "Samplers.h"

class ShaderManager;

namespace BackBuffer {
	namespace RootSignatureLayout {
		enum {
			ECB_Pass = 0,
			ESI_Color,
			ESI_Albedo,
			ESI_Normal,
			ESI_Depth,
			ESI_Specular,
			ESI_AOCoefficient,
			Count
		};
	}

	class BackBufferClass {
	public:
		BackBufferClass() = default;
		virtual ~BackBufferClass() = default;

	public:
		ID3D12Resource* BackBuffer(size_t index);
		CD3DX12_GPU_DESCRIPTOR_HANDLE BackBufferSrv(size_t index);

	public:
		bool Initialize(ID3D12Device*const device, UINT width, UINT height, ShaderManager*const manager,
			DXGI_FORMAT backBufferFormat, ID3D12Resource*const buffers[], UINT bufferCount);
		bool CompileShaders(const std::wstring& filePath);
		bool BuildRootSignature(const StaticSamplers& samplers);
		bool BuildPso();
		void Run(
			ID3D12GraphicsCommandList*const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
			D3D12_GPU_DESCRIPTOR_HANDLE si_color,
			D3D12_GPU_DESCRIPTOR_HANDLE si_albedo, 
			D3D12_GPU_DESCRIPTOR_HANDLE si_normal, 
			D3D12_GPU_DESCRIPTOR_HANDLE si_depth, 
			D3D12_GPU_DESCRIPTOR_HANDLE si_specular, 
			D3D12_GPU_DESCRIPTOR_HANDLE si_aoCoefficient);

		void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuSrv, CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpuSrv, UINT descSize);
		bool OnResize(UINT width, UINT height);

	private:
		void BuildDescriptors();
		
	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;

		UINT mWidth;
		UINT mHeight;

		DXGI_FORMAT mBackBufferFormat;
		ID3D12Resource*const* mBackBuffers;
		UINT mBackBufferCount;

		std::vector<CD3DX12_CPU_DESCRIPTOR_HANDLE> mhBackBufferCpuSrvs;
		std::vector<CD3DX12_GPU_DESCRIPTOR_HANDLE> mhBackBufferGpuSrvs;
	};
}

ID3D12Resource* BackBuffer::BackBufferClass::BackBuffer(size_t index) {
	return mBackBuffers[index];
}

CD3DX12_GPU_DESCRIPTOR_HANDLE BackBuffer::BackBufferClass::BackBufferSrv(size_t index) {
	return mhBackBufferGpuSrvs[index];
}