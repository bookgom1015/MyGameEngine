#pragma once

#include <d3dx12.h>
#include <wrl.h>

#include "Samplers.h"
#include "GpuResource.h"

class ShaderManager;

namespace MotionBlur {
	namespace RootSignatureLayout {
		enum {
			ESI_Input = 0,
			ESI_Depth,
			ESI_Velocity,
			EC_Consts,
			Count
		};
	}

	namespace RootConstantsLayout {
		enum {
			EIntensity = 0,
			ELimit,
			EDepthBias,
			ESampleCount,
			Count
		};
	}

	const UINT NumRenderTargets = 1;

	const FLOAT ClearValues[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	class MotionBlurClass {
	public:
		MotionBlurClass();
		virtual ~MotionBlurClass() = default;

	public:
		__forceinline GpuResource* MotionVectorMapResource();

		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE MotionVectorMapRtv() const;

	public:
		BOOL Initialize(ID3D12Device* device, ShaderManager*const manager, UINT width, UINT height);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignature(const StaticSamplers& samplers);
		BOOL BuildPso();

		void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv, UINT rtvDescSize);
		BOOL OnResize(UINT width, UINT height);

		void Run(
			ID3D12GraphicsCommandList*const cmdList,
			const D3D12_VIEWPORT& viewport,
			const D3D12_RECT& scissorRect,
			D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer,
			D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
			D3D12_GPU_DESCRIPTOR_HANDLE si_velocity,
			FLOAT intensity,
			FLOAT limit,
			FLOAT depthBias,
			INT sampleCount);

	private:
		void BuildDescriptors();
		BOOL BuildResources(UINT width, UINT height);

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;

		std::unique_ptr<GpuResource> mMotionVectorMap;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhMotionVectorMapCpuRtv;
	};
}

GpuResource* MotionBlur::MotionBlurClass::MotionVectorMapResource() {
	return mMotionVectorMap.get();
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE MotionBlur::MotionBlurClass::MotionVectorMapRtv() const {
	return mhMotionVectorMapCpuRtv;
}