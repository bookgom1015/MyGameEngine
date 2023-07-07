#pragma once

#include <d3dx12.h>
#include <wrl.h>

#include "Samplers.h"

class ShaderManager;

struct RenderItem;

namespace SkyCube {
	namespace RootSignatureLayout {
		enum {
			ECB_Pass = 0,
			ECB_Obj,
			ESI_Cube,
			Count
		};
	}

	class SkyCubeClass {
	public:
		SkyCubeClass() = default;
		virtual ~SkyCubeClass() = default;

	public:
		__forceinline constexpr D3D12_GPU_DESCRIPTOR_HANDLE CubeMapSrv() const;

	public:
		bool Initialize(ID3D12Device* device, ID3D12GraphicsCommandList*const cmdList, 
			ShaderManager*const manager, UINT width, UINT height, DXGI_FORMAT backBufferFormat);
		bool CompileShaders(const std::wstring& filePath);
		bool BuildRootSignature(const StaticSamplers& samplers);
		bool BuildPso(D3D12_INPUT_LAYOUT_DESC inputLayout, DXGI_FORMAT dsvFormat);
		void Run(
			ID3D12GraphicsCommandList*const cmdList,
			D3D12_VIEWPORT viewport,
			D3D12_RECT scissorRect,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE dio_dsv,
			D3D12_GPU_VIRTUAL_ADDRESS cbPassAddress,
			D3D12_GPU_VIRTUAL_ADDRESS cbObjAddress,
			UINT objCBByteSize,
			const std::vector<RenderItem*>& ritems);

		void BuildDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
			CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
			UINT descSize);

		bool SetCubeMap(ID3D12CommandQueue*const queue, const std::string& file);

	private:
		void BuildDescriptors();
		bool BuildResources(ID3D12GraphicsCommandList*const cmdList);

		void DrawRenderItems(
			ID3D12GraphicsCommandList*const cmdList, 
			const std::vector<RenderItem*>& ritems,
			D3D12_GPU_VIRTUAL_ADDRESS cbObjAddress,
			UINT objCBByteSize);

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;

		UINT mWidth;
		UINT mHeight;

		DXGI_FORMAT mBackBufferFormat;

		Microsoft::WRL::ComPtr<ID3D12Resource> mCubeMap;
		Microsoft::WRL::ComPtr<ID3D12Resource> mCubeMapUploadBuffer;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhGpuSrv;
	};
}

constexpr D3D12_GPU_DESCRIPTOR_HANDLE SkyCube::SkyCubeClass::CubeMapSrv() const {
	return mhGpuSrv;
}