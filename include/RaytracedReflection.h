#pragma once

#include <d3dx12.h>
#include <unordered_map>
#include <wrl.h>

#include "Samplers.h"

class ShaderManager;
class GpuResource;

struct RenderItem;

namespace RaytracedReflection {
	namespace RootSignature {
		enum Type {
			E_Global,
			E_Local
		};

		namespace Global {
			enum {
				ECB_Rr = 0,
				EAS_BVH,
				ESI_BackBuffer,
				ESI_NormalDepth,
				EUO_Reflection,
				Count
			};
		}

		namespace Local {
			enum {
				ECB_Obj = 0,
				ECB_Mat,
				ESB_Vertices,
				ESB_Indices,
				Count
			};

			struct RootArguments {
				D3D12_GPU_VIRTUAL_ADDRESS	CB_Object;
				D3D12_GPU_VIRTUAL_ADDRESS	CB_Material;
				D3D12_GPU_VIRTUAL_ADDRESS	SB_Vertices;
				D3D12_GPU_VIRTUAL_ADDRESS	AB_Indices;
			};
		}
	}

	class RaytracedReflectionClass {
	public:
		RaytracedReflectionClass();
		virtual ~RaytracedReflectionClass() = default;

	public:
		__forceinline constexpr UINT Width() const;
		__forceinline constexpr UINT Height() const;

		__forceinline D3D12_GPU_DESCRIPTOR_HANDLE ReflectionMapSrv() const;

	public:
		bool Initialize(ID3D12Device5* const device, ID3D12GraphicsCommandList* const cmdList, ShaderManager* const manager, UINT width, UINT height);
		bool CompileShaders(const std::wstring& filePath);
		bool BuildRootSignatures(const StaticSamplers& samplers);
		bool BuildPSO();
		bool BuildShaderTables(
			const std::vector<RenderItem*>& ritems,
			D3D12_GPU_VIRTUAL_ADDRESS cb_mat);
		void BuildDesscriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu, CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu, UINT descSize);
		bool OnResize(ID3D12GraphicsCommandList*const cmdList, UINT width, UINT height);

		void Run(
			ID3D12GraphicsCommandList4* const  cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS cb_rr,
			D3D12_GPU_VIRTUAL_ADDRESS as_bvh,
			D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer,
			D3D12_GPU_DESCRIPTOR_HANDLE si_normalDepth);

	private:
		void BuildDescriptors();
		bool BuildResources(ID3D12GraphicsCommandList* cmdList);

	private:
		ID3D12Device5* md3dDevice;
		ShaderManager* mShaderManager;

		std::unordered_map<RootSignature::Type, Microsoft::WRL::ComPtr<ID3D12RootSignature>> mRootSignatures;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPso;
		Microsoft::WRL::ComPtr<ID3D12StateObject> mDxrPso;
		Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> mDxrPsoProp;

		std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12Resource>> mShaderTables;
		UINT mMissShaderTableStrideInBytes;
		UINT mHitGroupShaderTableStrideInBytes;

		UINT mWidth;
		UINT mHeight;

		std::unique_ptr<GpuResource> mReflectionMap;
		std::unique_ptr<GpuResource> mReflectionUploadBuffer;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhReflectionMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhReflectionMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhReflectionMapCpuUav;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhReflectionMapGpuUav;
	};
}

constexpr UINT RaytracedReflection::RaytracedReflectionClass::Width() const {
	return mWidth;
}

constexpr UINT RaytracedReflection::RaytracedReflectionClass::Height() const {
	return mHeight;
}

D3D12_GPU_DESCRIPTOR_HANDLE RaytracedReflection::RaytracedReflectionClass::ReflectionMapSrv() const {
	return mhReflectionMapGpuSrv;
}