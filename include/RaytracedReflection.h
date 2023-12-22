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
				EC_Rr = 0,
				ECB_Pass,
				ECB_Rr,
				EAS_BVH,
				ESI_BackBuffer,
				ESI_Normal,
				ESI_Depth,
				ESI_RMS,
				ESI_Position,
				ESI_DiffIrrad,
				ESI_AOCoeiff,
				ESI_Prefiltered,
				ESI_BrdfLUT,
				ESI_TexMaps,
				EUO_Reflection,
				Count
			};
			
			namespace RootConstantsLayout {
				enum {
					E_FrameCount = 0,
					E_ShadowRayOffset,
					E_ReflectionRadius,
					Count
				};
			}
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

	namespace Resource {
		
	}

	namespace Descriptor {
		namespace Reflection {
			enum {
				E_Srv = 0,
				E_Uav,
				Count
			};
		}

		namespace TemporalReflection {
			enum {
				E_Srv = 0,
				E_Uav,
				Count
			};
		}
	}

	using ReflectionCpuDescriptors = std::array<CD3DX12_CPU_DESCRIPTOR_HANDLE, Descriptor::Reflection::Count>;
	using ReflectionGpuDescriptors = std::array<CD3DX12_GPU_DESCRIPTOR_HANDLE, Descriptor::Reflection::Count>;

	using TemporalReflectionType = std::array<std::unique_ptr<GpuResource>, 2>;
	using TemporalReflectionCpuDescriptors = std::array<std::array<CD3DX12_CPU_DESCRIPTOR_HANDLE, Descriptor::TemporalReflection::Count>, 2>;
	using TemporalReflectionGpuDescriptors = std::array<std::array<CD3DX12_GPU_DESCRIPTOR_HANDLE, Descriptor::TemporalReflection::Count>, 2>;

	class RaytracedReflectionClass {
	public:
		RaytracedReflectionClass();
		virtual ~RaytracedReflectionClass() = default;

	public:
		__forceinline D3D12_GPU_DESCRIPTOR_HANDLE ReflectionMapSrv() const;

		__forceinline constexpr UINT TemporalCurrentFrameResourceIndex() const;
		__forceinline constexpr UINT TemporalCurrentFrameTemporalReflectionResourceIndex() const;

	public:
		BOOL Initialize(ID3D12Device5* const device, ID3D12GraphicsCommandList* const cmdList, ShaderManager* const manager, UINT width, UINT height);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignatures(const StaticSamplers& samplers);
		BOOL BuildPSO();
		BOOL BuildShaderTables(
			const std::vector<RenderItem*>& ritems,
			D3D12_GPU_VIRTUAL_ADDRESS cb_mat);
		void BuildDesscriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu, CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu, UINT descSize);
		BOOL OnResize(ID3D12GraphicsCommandList*const cmdList, UINT width, UINT height);

		void CalcReflection(
			ID3D12GraphicsCommandList4* const  cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
			D3D12_GPU_VIRTUAL_ADDRESS cb_rr,
			D3D12_GPU_VIRTUAL_ADDRESS as_bvh,
			D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer,
			D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
			D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
			D3D12_GPU_DESCRIPTOR_HANDLE si_rms,
			D3D12_GPU_DESCRIPTOR_HANDLE si_pos,
			D3D12_GPU_DESCRIPTOR_HANDLE si_diffIrrad,
			D3D12_GPU_DESCRIPTOR_HANDLE si_aocoeiff,
			D3D12_GPU_DESCRIPTOR_HANDLE si_prefiltered,
			D3D12_GPU_DESCRIPTOR_HANDLE si_brdf,
			D3D12_GPU_DESCRIPTOR_HANDLE si_texMaps,
			UINT width, UINT height,
			FLOAT radius);

		UINT MoveToNextFrame();
		UINT MoveToNextFrameTemporalReflection();

	private:
		void BuildDescriptors();
		BOOL BuildResources(ID3D12GraphicsCommandList* cmdList, UINT width, UINT height);

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
		UINT mShadowRayOffset;

		std::unique_ptr<GpuResource> mReflectionMap;
		ReflectionCpuDescriptors mhReflectionMapCpus;
		ReflectionGpuDescriptors mhReflectionMapGpus;

		std::unique_ptr<GpuResource> mReflectionUploadBuffer;

		TemporalReflectionType mTemporalReflectionMaps;
		TemporalReflectionCpuDescriptors mhTemporalReflectionMapCpus;
		TemporalReflectionGpuDescriptors mhTemporalReflectionMapGpus;

		UINT mTemporalCurrentFrameResourceIndex = 0;
		UINT mTemporalCurrentFrameTemporalReflectionResourceIndex = 0;
	};
}

D3D12_GPU_DESCRIPTOR_HANDLE RaytracedReflection::RaytracedReflectionClass::ReflectionMapSrv() const {
	return mhReflectionMapGpus[RaytracedReflection::Descriptor::Reflection::E_Srv];
}

constexpr UINT RaytracedReflection::RaytracedReflectionClass::TemporalCurrentFrameResourceIndex() const {
	return mTemporalCurrentFrameResourceIndex;
}

constexpr UINT RaytracedReflection::RaytracedReflectionClass::TemporalCurrentFrameTemporalReflectionResourceIndex() const {
	return mTemporalCurrentFrameTemporalReflectionResourceIndex;
}