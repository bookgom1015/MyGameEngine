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
			ESI_Color,
			ESI_Albedo,
			ESI_Normal,
			ESI_Depth,
			ESI_Specular,
			ESI_Shadow,
			ESI_AOCoefficient,
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

	namespace Range {
		enum Type {
			E_SDR = 0,
			E_HDR,
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
		BRDFClass() = default;
		virtual ~BRDFClass() = default;

	public:
		bool Initialize(ID3D12Device* device, ShaderManager* const manager, DXGI_FORMAT sdrFormat, DXGI_FORMAT hdrFormat);
		bool CompileShaders(const std::wstring& filePath);
		bool BuildRootSignature(const StaticSamplers& samplers);
		bool BuildPso();

		void BlinnPhong(
			ID3D12GraphicsCommandList*const cmdList,
			D3D12_VIEWPORT viewport,
			D3D12_RECT scissorRect,
			GpuResource* backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE ri_backBuffer,
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
			D3D12_GPU_DESCRIPTOR_HANDLE si_color,
			D3D12_GPU_DESCRIPTOR_HANDLE si_albedo, 
			D3D12_GPU_DESCRIPTOR_HANDLE si_normal, 
			D3D12_GPU_DESCRIPTOR_HANDLE si_depth, 
			D3D12_GPU_DESCRIPTOR_HANDLE si_specular, 
			D3D12_GPU_DESCRIPTOR_HANDLE si_shadow,
			D3D12_GPU_DESCRIPTOR_HANDLE si_aoCoefficient,
			Render::Type renderType = Render::E_Raster,
			Range::Type rangeType = Range::E_SDR);

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		std::unordered_map<Render::Type, std::unordered_map<Range::Type, std::unordered_map<Model::Type, Microsoft::WRL::ComPtr<ID3D12PipelineState>>>> mPSOs;

		DXGI_FORMAT mSDRFormat;
		DXGI_FORMAT mHDRFormat;
	};
}