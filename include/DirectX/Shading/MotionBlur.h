#pragma once

#include <DirectX/d3dx12.h>
#include <wrl.h>

#include "Common/Util/Locker.h"
#include "DirectX/Infrastructure/GpuResource.h"
#include "DirectX/Shading/Samplers.h"

class ShaderManager;

namespace MotionBlur {
	namespace RootSignature {
		namespace Default {
			enum {
				EC_Consts = 0,
				ESI_BackBuffer,
				ESI_Depth,
				ESI_Velocity,
				Count
			};
		}
	}

	class MotionBlurClass {
	public:
		MotionBlurClass();
		virtual ~MotionBlurClass() = default;

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

		void Run(
			ID3D12GraphicsCommandList* const cmdList,
			const D3D12_VIEWPORT& viewport,
			const D3D12_RECT& scissorRect,
			GpuResource* const backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
			D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer,
			D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
			D3D12_GPU_DESCRIPTOR_HANDLE si_velocity,
			FLOAT intensity,
			FLOAT limit,
			FLOAT depthBias,
			INT sampleCount);

	private:
		BOOL BuildResources(UINT width, UINT height);

	private:
		Locker<ID3D12Device5>* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;

		std::unique_ptr<GpuResource> mCopiedBackBuffer;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhCopiedBackBufferCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhCopiedBackBufferGpuSrv;
	};
}