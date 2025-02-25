#pragma once

#include <DirectX/d3dx12.h>
#include <vector>
#include <unordered_map>
#include <wrl.h>

#include "Common/Util/Locker.h"

#include "Samplers.h"

class ShaderManager;
class GpuResource;

namespace DxrBackBuffer { class DxrBackBufferClass; }

namespace BRDF {
	namespace RootSignature {
		enum Type {
			E_CalcReflectanceEquation,
			E_IntegrateSpecular,
			Count
		};

		namespace CalcReflectanceEquation {
			enum {
				ECB_Pass = 0,
				ESI_Albedo,
				ESI_Normal,
				ESI_Depth,
				ESI_RMS,
				ESI_Position,
				ESI_DiffuseIrrad,
				ESI_AOCoeiff,
				ESI_Shadow,
				Count
			};
		}

		namespace IntegrateSpecular {
			enum {
				ECB_Pass = 0,
				ESI_BackBuffer,
				ESI_Albedo,
				ESI_Normal,
				ESI_Depth,
				ESI_RMS,
				ESI_Position,
				ESI_AOCoeiff,
				ESI_Prefiltered,
				ESI_BrdfLUT,
				ESI_Reflection,
				Count
			};
		}
	}

	namespace Render {
		enum Type {
			E_Raster = 0,
			E_Raytrace,
			Count
		};
	}

	namespace Model {
		enum Type {
			E_BlinnPhong = 0,
			E_CookTorrance,
			Count
		};
	}

	class BRDFClass {
	private:
		friend class DxrBackBuffer::DxrBackBufferClass;

	public:
		BRDFClass();
		virtual ~BRDFClass() = default;

	public:
		BOOL Initialize(Locker<ID3D12Device5>* const device, ShaderManager* const manager, UINT width, UINT height);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignature(const StaticSamplers& samplers);
		BOOL BuildPSO();
		void AllocateDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
			CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
			UINT descSize);
		BOOL BuildDescriptors();
		BOOL OnResize(UINT width, UINT height);

		void CalcReflectanceWithoutSpecIrrad(
			ID3D12GraphicsCommandList* const cmdList,
			const D3D12_VIEWPORT& viewport,
			const D3D12_RECT& scissorRect,
			GpuResource* const backBuffer,
			D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
			D3D12_GPU_DESCRIPTOR_HANDLE si_albedo,
			D3D12_GPU_DESCRIPTOR_HANDLE si_normal, 
			D3D12_GPU_DESCRIPTOR_HANDLE si_depth, 
			D3D12_GPU_DESCRIPTOR_HANDLE si_rms, 
			D3D12_GPU_DESCRIPTOR_HANDLE si_pos,
			D3D12_GPU_DESCRIPTOR_HANDLE si_diffuseIrrad,
			D3D12_GPU_DESCRIPTOR_HANDLE si_aocoeiff,
			D3D12_GPU_DESCRIPTOR_HANDLE si_shadow,
			Render::Type renderType = Render::E_Raster);

		void IntegrateSpecularIrrad(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_VIEWPORT viewport,
			D3D12_RECT scissorRect,
			GpuResource* const backBuffer,
			D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
			D3D12_GPU_DESCRIPTOR_HANDLE si_albedo,
			D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
			D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
			D3D12_GPU_DESCRIPTOR_HANDLE si_rms,
			D3D12_GPU_DESCRIPTOR_HANDLE si_pos,
			D3D12_GPU_DESCRIPTOR_HANDLE si_aocoeiff,
			D3D12_GPU_DESCRIPTOR_HANDLE si_prefiltered,
			D3D12_GPU_DESCRIPTOR_HANDLE si_brdf,
			D3D12_GPU_DESCRIPTOR_HANDLE si_ssr);

	private:
		BOOL BuildResources(UINT width, UINT height);

	public:
		Model::Type ModelType = Model::E_CookTorrance;

	private:
		Locker<ID3D12Device5>* md3dDevice;
		ShaderManager* mShaderManager;

		std::unordered_map<RootSignature::Type, Microsoft::WRL::ComPtr<ID3D12RootSignature>> mRootSignatures;
		std::unordered_map<Render::Type, std::unordered_map<Model::Type, Microsoft::WRL::ComPtr<ID3D12PipelineState>>> mPSOs;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mIntegrateSpecularPSO;

		std::unique_ptr<GpuResource> mCopiedBackBuffer;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhCopiedBackBufferSrvCpu;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhCopiedBackBufferSrvGpu;
	};
}