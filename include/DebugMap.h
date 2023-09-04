#pragma once

#include <d3dx12.h>
#include <wrl.h>

#include "MathHelper.h"
#include "HlslCompaction.h"

#include "Samplers.h"

class ShaderManager;
class GpuResource;

namespace DebugMap {
	namespace RootSignatureLayout {
		enum {
			EC_Consts = 0,
			ESI_Debug0,
			ESI_Debug1,
			ESI_Debug2,
			ESI_Debug3,
			ESI_Debug4,
			Count
		};
	}

	namespace RootConstantsLayout {
		enum {
			ESampleMask0 = 0,
			ESampleMask1,
			ESampleMask2,
			ESampleMask3,
			ESampleMask4,
			Count
		};
	}

	class DebugMapClass {
	public:
		DebugMapClass() = default;
		virtual ~DebugMapClass() = default;

	public:
		bool Initialize(ID3D12Device* device, ShaderManager*const manager, DXGI_FORMAT backBufferFormat);
		bool CompileShaders(const std::wstring& filePath);
		bool BuildRootSignature(const StaticSamplers& samplers);
		bool BuildPso();
		void Run(
			ID3D12GraphicsCommandList*const cmdList,
			D3D12_VIEWPORT viewport,
			D3D12_RECT scissorRect,
			GpuResource* backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE dio_dsv);

		bool AddDebugMap(
			D3D12_GPU_DESCRIPTOR_HANDLE hGpuSrv, 
			Debug::SampleMask::Type mask = Debug::SampleMask::RGB);
		void RemoveDebugMap(D3D12_GPU_DESCRIPTOR_HANDLE hGpuSrv);

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;

		DXGI_FORMAT mBackBufferFormat;

		std::array<D3D12_GPU_DESCRIPTOR_HANDLE, 5> mhDebugGpuSrvs;
		std::array<Debug::SampleMask::Type, 5> mDebugMasks;
		int mNumEnabledMaps;
	};
};