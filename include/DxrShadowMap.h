#pragma once

#include <d3dx12.h>
#include <unordered_map>
#include <wrl.h>

#include "Samplers.h"
#include "GpuResource.h"

class ShaderManager;

namespace DxrShadowMap {
	namespace RootSignature {
		enum Type {
			E_Global = 0
		};

		namespace Global {
			enum {
				ECB_Pass = 0,
				ESI_AccelerationStructure,
				ESI_Position,
				ESI_Normal,
				ESI_Depth,
				EUO_Shadow,
				Count
			};
		}
	}

	namespace Resources {
		enum Type {
			EShadow0 = 0,
			EShadow1,
			Count
		};
	}

	namespace Descriptors {
		enum Type {
			ES_Shadow0 = 0,
			EU_Shadow0,
			ES_Shadow1,
			EU_Shadow1,
			Count
		};
	}

	using ResourcesType = std::array<std::unique_ptr<GpuResource>, Resources::Count>;
	using ResourcesCpuDescriptors = std::array<CD3DX12_CPU_DESCRIPTOR_HANDLE, Descriptors::Count>;
	using ResourcesGpuDescriptors = std::array<CD3DX12_GPU_DESCRIPTOR_HANDLE, Descriptors::Count>;

	class DxrShadowMapClass {
	public:
		DxrShadowMapClass();
		virtual ~DxrShadowMapClass() = default;

	public:
		__forceinline GpuResource* Resource(Resources::Type type);
		__forceinline D3D12_GPU_DESCRIPTOR_HANDLE Descriptor(Descriptors::Type type) const;

	public:
		BOOL Initialize(ID3D12Device5*const device, ID3D12GraphicsCommandList*const cmdList, ShaderManager*const manager, UINT width, UINT height);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignatures(const StaticSamplers& samplers, UINT geometryBufferCount);
		BOOL BuildPso();
		BOOL BuildShaderTables(UINT numRitems);
		void Run(
			ID3D12GraphicsCommandList4*const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS as_bvh,
			D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
			D3D12_GPU_DESCRIPTOR_HANDLE si_pos,
			D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
			D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
			UINT width, UINT height);

		void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu, CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu, UINT descSize);

		BOOL OnResize(ID3D12GraphicsCommandList* const cmdList, UINT width, UINT height);

	private:
		void BuildDescriptors();
		BOOL BuildResource(ID3D12GraphicsCommandList*const cmdList, UINT width, UINT height);

	private:
		ID3D12Device5* md3dDevice;
		ShaderManager* mShaderManager;

		std::unordered_map<RootSignature::Type, Microsoft::WRL::ComPtr<ID3D12RootSignature>> mRootSignatures;
		Microsoft::WRL::ComPtr<ID3D12StateObject> mPSO;
		Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> mPSOProp;

		std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12Resource>> mShaderTables;
		UINT mHitGroupShaderTableStrideInBytes;

		DxrShadowMap::ResourcesType mResources;
		DxrShadowMap::ResourcesCpuDescriptors mhCpuDescs;
		DxrShadowMap::ResourcesGpuDescriptors mhGpuDescs;
	};
}

D3D12_GPU_DESCRIPTOR_HANDLE DxrShadowMap::DxrShadowMapClass::Descriptor(Descriptors::Type type) const {
	return mhGpuDescs[type];
}

GpuResource* DxrShadowMap::DxrShadowMapClass::Resource(Resources::Type type) {
	return mResources[type].get();
}