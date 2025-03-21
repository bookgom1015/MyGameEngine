#pragma once

#include <DirectX/d3dx12.h>
#include <wrl.h>

#include "Common/Util/Locker.h"
#include "DirectX/Infrastructure/GpuResource.h"
#include "DirectX/Shading/Samplers.h"

class ShaderManager;

struct RenderItem;

namespace Shadow {
	namespace RootSignature {
		enum {
			E_ZDepth = 0,
			E_Shadow,
			Count
		};

		namespace ZDepth {
			enum {
				ECB_Pass = 0,
				ECB_Obj,
				ECB_Mat,
				EC_Consts,
				ESI_TexMaps,
				Count
			};
		}

		namespace Shadow {
			enum {
				ECB_Pass = 0,
				EC_Consts,
				ESI_Position,
				ESI_ZDepth,
				ESI_ZDepthCube,
				ESI_FaceIDCube,
				EUO_Shadow,
				EUO_Debug,
				Count
			};
		}
	}

	namespace PipelineState {
		enum {
			EG_ZDepth = 0,
			EG_ZDepthCube,
			EC_Shadow,
			Count
		};
	}

	namespace Resource {
		enum Type {
			E_ZDepth = 0,
			E_ZDepthCube,
			E_FaceIDCube,
			E_Shadow,
			E_Debug,
			Count
		};
	}

	namespace Descriptor {
		enum Type {
			ESI_ZDepth = 0,
			ESI_ZDepthCube,
			ESI_FaceIDCube,
			ESI_Shadow,
			EUO_Shadow,
			EUO_Debug,
			Count
		};

		namespace DSV {
			enum {
				EDS_ZDepth = 0,
				EDS_ZDepthCube,
				Count
			};
		}

		namespace RTV {
			enum {
				ERT_FaceIDCube = 0,
				Count
			};
		}
	}

	static const UINT NumRenderTargets	= 2;
	static const UINT NumDepthStenciles	= 2;

	class ShadowClass {
	public:
		ShadowClass();
		virtual ~ShadowClass() = default;

	public:
		__forceinline constexpr UINT Width() const;
		__forceinline constexpr UINT Height() const;

		__forceinline constexpr D3D12_VIEWPORT Viewport() const;
		__forceinline constexpr D3D12_RECT ScissorRect() const;

		__forceinline GpuResource* Resource(Resource::Type type);
		__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE Srv(Descriptor::Type type) const;
		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE Dsv() const;

		__forceinline BOOL* DebugShadowMap();

	public:
		BOOL Initialize(
			Locker<ID3D12Device5>* const device, ShaderManager* const manager,
			UINT clientW, UINT clientH, UINT texW, UINT texH);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignature(const StaticSamplers& samplers);
		BOOL BuildPSO();
		void Run(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
			D3D12_GPU_VIRTUAL_ADDRESS cb_obj,
			D3D12_GPU_VIRTUAL_ADDRESS cb_mat,
			UINT objCBByteSize,
			UINT matCBByteSize,
			D3D12_GPU_DESCRIPTOR_HANDLE si_pos,
			D3D12_GPU_DESCRIPTOR_HANDLE si_texMaps,
			const std::vector<RenderItem*>& ritems,
			GpuResource* const dst_zdepth,
			GpuResource* const dst_faceIDCube,
			UINT lightType,
			UINT index);

		void AllocateDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
			CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuDsv,
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
			UINT descSize, UINT dsvDescSize, UINT rtvDescSize);
		BOOL BuildDescriptors();

	private:
		BOOL BuildResources();

		void DrawZDepth(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
			D3D12_GPU_VIRTUAL_ADDRESS cb_obj, 
			D3D12_GPU_VIRTUAL_ADDRESS cb_mat,
			UINT objCBByteSize, UINT matCBByteSize,
			D3D12_GPU_DESCRIPTOR_HANDLE si_texMaps,
			BOOL needCubemap, UINT index,
			const std::vector<RenderItem*>& ritems);
		void CopyZDepth(
			ID3D12GraphicsCommandList* const cmdList,
			GpuResource* const dst_zdepth, 
			GpuResource* const dst_faceIDCube,
			BOOL needCubemap);
		void DrawShadow(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
			D3D12_GPU_DESCRIPTOR_HANDLE si_pos,
			UINT index);

	private:
		Locker<ID3D12Device5>* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignatures[RootSignature::Count];
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSOs[PipelineState::Count];

		UINT mTexWidth;
		UINT mTexHeight;

		UINT mClientWidth;
		UINT mClientHeight;

		D3D12_VIEWPORT mViewport;
		D3D12_RECT mScissorRect;

		std::unique_ptr<GpuResource> mShadowMaps[Resource::Count];

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuDescs[Descriptor::Count];
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhGpuDescs[Descriptor::Count];

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuDsvs[Descriptor::DSV::Count];

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuRtvs[Descriptor::RTV::Count];

		BOOL mDebugShadowMap;
	};
};

#include "Shadow.inl"