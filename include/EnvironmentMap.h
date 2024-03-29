#pragma once

#include <d3dx12.h>
#include <unordered_map>
#include <wrl.h>

#include "Samplers.h"

class ShaderManager;
class GpuResource;

struct RenderItem;

namespace EnvironmentMap {
	namespace RootSignatureLayout {
		enum {
			ECB_Pass = 0,
			ECB_Obj,
			ESI_Cube,
			Count
		};
	}

	class EnvironmentMapClass {
	public:
		EnvironmentMapClass();
		virtual ~EnvironmentMapClass() = default;

	public:
		__forceinline constexpr D3D12_GPU_DESCRIPTOR_HANDLE CubeMapSrv() const;

	public:
		BOOL Initialize(ID3D12Device* device, ID3D12GraphicsCommandList*const cmdList,
			ShaderManager*const manager, UINT width, UINT height);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignature(const StaticSamplers& samplers);
		BOOL BuildPso(D3D12_INPUT_LAYOUT_DESC inputLayout, DXGI_FORMAT dsvFormat);
		void Run(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_VIEWPORT viewport,
			D3D12_RECT scissorRect,
			GpuResource* backBuffer,
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

		BOOL SetCubeMap(ID3D12CommandQueue*const queue, const std::string& file);

	private:
		void BuildDescriptors();
		BOOL BuildResources(ID3D12GraphicsCommandList*const cmdList);

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

		std::unique_ptr<GpuResource> mCubeMap;
		std::unique_ptr<GpuResource> mCubeMapUploadBuffer;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhGpuSrv;
	};
}

constexpr D3D12_GPU_DESCRIPTOR_HANDLE EnvironmentMap::EnvironmentMapClass::CubeMapSrv() const {
	return mhGpuSrv;
}