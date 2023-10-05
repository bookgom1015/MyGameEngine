#pragma once

#include <d3dx12.h>
#include <vector>
#include <unordered_map>
#include <wrl.h>

#include "Samplers.h"

class ShaderManager;
class GpuResource;

namespace DxrBackBuffer { class DxrBackBufferClass; }

namespace BRDF {
	namespace RootSignatureLayout {
		enum {
			ECB_Pass = 0,
			ESI_Albedo,
			ESI_Normal,
			ESI_Depth,
			ESI_RMS,
			ESI_Shadow,
			ESI_AOCoeiff,
			ESI_Diffuse,
			ESI_Prefiltered,
			ESI_Brdf,
			Count
		};
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
		bool Initialize(ID3D12Device* device, ShaderManager* const manager);
		bool CompileShaders(const std::wstring& filePath);
		bool BuildRootSignature(const StaticSamplers& samplers);
		bool BuildPso();

		void Run(
			ID3D12GraphicsCommandList*const cmdList,
			D3D12_VIEWPORT viewport,
			D3D12_RECT scissorRect,
			GpuResource* backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE ri_backBuffer,
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
			D3D12_GPU_DESCRIPTOR_HANDLE si_albedo,
			D3D12_GPU_DESCRIPTOR_HANDLE si_normal, 
			D3D12_GPU_DESCRIPTOR_HANDLE si_depth, 
			D3D12_GPU_DESCRIPTOR_HANDLE si_rms, 
			D3D12_GPU_DESCRIPTOR_HANDLE si_shadow,
			D3D12_GPU_DESCRIPTOR_HANDLE si_aocoeiff,
			D3D12_GPU_DESCRIPTOR_HANDLE si_diffuse,
			D3D12_GPU_DESCRIPTOR_HANDLE si_specular,
			D3D12_GPU_DESCRIPTOR_HANDLE si_brdf,
			Render::Type renderType = Render::E_Raster);

	public:
		Model::Type ModelType;

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		std::unordered_map<Render::Type, std::unordered_map<Model::Type, Microsoft::WRL::ComPtr<ID3D12PipelineState>>> mPSOs;
	};
}