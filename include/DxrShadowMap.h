#pragma once

#include <d3dx12.h>
#include <unordered_map>
#include <wrl.h>

#include "Samplers.h"
#include "GpuResource.h"

class ShaderManager;

namespace DxrShadowMap {
	namespace RootSignatureLayout {
		enum {
			ECB_Pass = 0,
			ESI_AccelerationStructure,
			ESB_Vertices,
			EAB_Indices,
			ESI_Depth,
			EUO_Shadow,
			Count
		};
	}

	namespace Resources {
		enum {
			EShadow0 = 0,
			EShadow1,
			Count
		};

		namespace Descriptors {
			enum {
				ES_Shadow0 = 0,
				EU_Shadow0,
				ES_Shadow1,
				EU_Shadow1,
				Count
			};
		}
	}

	using ResourcesType = std::array<std::unique_ptr<GpuResource>, Resources::Count>;
	using ResourcesCpuDescriptors = std::array<CD3DX12_CPU_DESCRIPTOR_HANDLE, Resources::Descriptors::Count>;
	using ResourcesGpuDescriptors = std::array<CD3DX12_GPU_DESCRIPTOR_HANDLE, Resources::Descriptors::Count>;

	const DXGI_FORMAT ShadowFormat = DXGI_FORMAT_R16_UNORM;

	class DxrShadowMapClass {
	public:
		DxrShadowMapClass();
		virtual ~DxrShadowMapClass() = default;

	public:
		bool Initialize(ID3D12Device5*const device, ID3D12GraphicsCommandList*const cmdList, ShaderManager*const manager, UINT width, UINT height);
		bool CompileShaders(const std::wstring& filePath);
		bool BuildRootSignatures(const StaticSamplers& samplers, UINT geometryBufferCount);
		bool BuildPso();
		bool BuildShaderTables();
		void Run(
			ID3D12GraphicsCommandList4*const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS accelStruct,
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
			D3D12_GPU_VIRTUAL_ADDRESS objSBAddress,
			D3D12_GPU_VIRTUAL_ADDRESS matSBAddress,
			D3D12_GPU_DESCRIPTOR_HANDLE i_vertices,
			D3D12_GPU_DESCRIPTOR_HANDLE i_indices,
			D3D12_GPU_DESCRIPTOR_HANDLE i_depth,
			D3D12_GPU_DESCRIPTOR_HANDLE o_shadow,
			UINT width, UINT height);

		void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu, CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu, UINT descSize);

		bool OnResize(ID3D12GraphicsCommandList* const cmdList, UINT width, UINT height);

	private:
		void BuildDescriptors();
		bool BuildResource(ID3D12GraphicsCommandList*const cmdList);

	private:
		ID3D12Device5* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		Microsoft::WRL::ComPtr<ID3D12StateObject> mPSO;
		Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> mPSOProp;

		std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12Resource>> mShaderTables;

		UINT mWidth;
		UINT mHeight;

		DxrShadowMap::ResourcesType mResources;
		DxrShadowMap::ResourcesCpuDescriptors mhResourcesCpuDescriptors;
		DxrShadowMap::ResourcesGpuDescriptors mhResourcesGpuDescriptors;
	};
}