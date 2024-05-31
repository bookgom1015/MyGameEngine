#pragma once

#include <d3dx12.h>
#include <unordered_map>
#include <wrl.h>

#include "Samplers.h"
#include "GpuResource.h"

class ShaderManager;

namespace DXR_Shadow {
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

	class DXR_ShadowClass {
	public:
		DXR_ShadowClass();
		virtual ~DXR_ShadowClass() = default;

	public:
		__forceinline GpuResource* Resource();
		__forceinline D3D12_GPU_DESCRIPTOR_HANDLE Descriptor() const;

	public:
		BOOL Initialize(ID3D12Device5*const device, ID3D12GraphicsCommandList*const cmdList, ShaderManager*const manager, UINT width, UINT height);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignatures(const StaticSamplers& samplers, UINT geometryBufferCount);
		BOOL BuildPSO();
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

		std::unique_ptr<GpuResource> mResource;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuDesc;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhGpuDesc;
	};
}

#include "DXR_Shadow.inl"