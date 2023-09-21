#pragma once

#include <d3dx12.h>
#include <vector>
#include <unordered_map>
#include <wrl.h>

#include "Samplers.h"

class ShaderManager;
class GpuResource;

struct RenderItem;

namespace IrradianceMap {
	namespace ConvEquirectToCube {
		namespace RootSignatureLayout {
			enum {
				ECB_ConvEquirectToCube = 0,
				EC_Consts,
				ESI_Equirectangular,
				Count
			};
		}

		namespace RootConstantsLayout {
			enum {
				E_FaceID = 0,
				Count
			};
		}
	}

	namespace DrawCube {
		namespace RootSignatureLayout {
			enum {
				ECB_Pass = 0,
				ECB_Obj,
				ESI_Cube,
				ESI_Equirectangular,
				Count
			};
		}
	}

	namespace PipelineState {
		enum Type {
			E_ConvEquirectToConv = 0,
			E_DrawEquirectangularCube,
			E_DrawConvertedCube,
			Count
		};
	}

	namespace RootSignatureLayout {
		enum Type {
			E_ConvEquirectToConv = 0,
			E_DrawCube,
			Count
		};
	}

	namespace CubeMapFace {
		enum Type {
			E_PositiveX = 0,	// Right
			E_NegativeX,		// Left
			E_PositiveY,		// Upper
			E_NegativeY,		// Bottom
			E_PositiveZ,		// Frontward
			E_NegativeZ,		// Backward
			Count
		};
	}

	static const UINT NumRenderTargets = 6;

	class IrradianceMapClass {
	public:
		IrradianceMapClass();
		virtual ~IrradianceMapClass() = default;

	public:
		bool Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* const cmdList,ShaderManager* const manager);
		bool CompileShaders(const std::wstring& filePath);
		bool BuildRootSignature(const StaticSamplers& samplers);
		bool BuildPso(D3D12_INPUT_LAYOUT_DESC inputLayout);

		void BuildDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuSrv,
			CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpuSrv,
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
			UINT descSize, UINT rtvDescSize);

		bool SetEquirectangularMap(ID3D12CommandQueue* const queue,  const std::string& file);

		bool Update(
			ID3D12DescriptorHeap* descHeap,
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS cbConvEquirectToCube,
			RenderItem* box);
		bool DrawCubeMap(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_VIEWPORT viewport,
			D3D12_RECT scissorRect,
			GpuResource* backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
			D3D12_GPU_VIRTUAL_ADDRESS cbPassAddress,
			D3D12_GPU_VIRTUAL_ADDRESS cbObjAddress,
			UINT objCBByteSize,
			RenderItem* cube);

	private:
		void BuildDescriptors();
		bool BuildResources(ID3D12GraphicsCommandList* const cmdList);

		void Convert(
			ID3D12DescriptorHeap* descHeap,
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS cbConvEquirectToCube,
			RenderItem* box);

	public:
		PipelineState::Type PipelineStateType;

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		std::unordered_map<RootSignatureLayout::Type, Microsoft::WRL::ComPtr<ID3D12RootSignature>> mRootSignatures;
		std::unordered_map<PipelineState::Type, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;

		std::unique_ptr<GpuResource> mEquirectangularMap;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhEquirectangularMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhEquirectangularMapGpuSrv;

		std::unique_ptr<GpuResource> mConvertedCubeMap;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhConvertedCubeMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhConvertedCubeMapGpuSrv;
		std::vector<CD3DX12_CPU_DESCRIPTOR_HANDLE> mhConvertedCubeMapCpuRtvs;

		D3D12_VIEWPORT mViewport;
		D3D12_RECT mScissorRect;

		bool bNeedToUpdate;
	};
}