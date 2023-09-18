#pragma once

#include <d3dx12.h>
#include <wrl.h>

#include "Samplers.h"

class ShaderManager;
class GpuResource;

struct RenderItem;

namespace CubeMapConverter {
	namespace RootSignatureLayout {
		enum {
			ECB_Pass = 0,
			ECB_Obj,
			ESI_Equirectangular,
			Count
		};
	}

	class CubeMapConverterClass {
	public:
		CubeMapConverterClass();
		virtual ~CubeMapConverterClass() = default;

	public:
		bool Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* const cmdList,ShaderManager* const manager);
		bool CompileShaders(const std::wstring& filePath);
		bool BuildRootSignature(const StaticSamplers& samplers);
		bool BuildPso(D3D12_INPUT_LAYOUT_DESC inputLayout);
		void Run(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_VIEWPORT viewport,
			D3D12_RECT scissorRect,
			GpuResource* backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
			D3D12_GPU_VIRTUAL_ADDRESS cbPassAddress,
			D3D12_GPU_VIRTUAL_ADDRESS cbObjAddress,
			UINT objCBByteSize,
			const std::vector<RenderItem*>& ritems);

		void BuildDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
			CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
			UINT descSize);

		bool SetEquirectangularMap(ID3D12CommandQueue* const queue, const std::string& file);

	private:
		void BuildDescriptors();
		bool BuildResources(ID3D12GraphicsCommandList* const cmdList);

		void DrawRenderItems(
			ID3D12GraphicsCommandList* const cmdList,
			const std::vector<RenderItem*>& ritems,
			D3D12_GPU_VIRTUAL_ADDRESS cbObjAddress,
			UINT objCBByteSize);

	public:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;

		std::unique_ptr<GpuResource> mEquirectangularMap;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhEquirectangularMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhEquirectangularMapGpuSrv;

		std::unique_ptr<GpuResource> mCubeMap;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhCubeMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhCubeMapGpuSrv;

	};
}