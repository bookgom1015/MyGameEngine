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

	const float ClearValues[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	class MotionBlurClass {
	public:
		MotionBlurClass();
		virtual ~MotionBlurClass() = default;

	public:
		__forceinline constexpr UINT Width() const;
		__forceinline constexpr UINT Height() const;

		__forceinline GpuResource* MotionVectorMapResource();

		__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE MotionVectorMapRtv() const;

	public:
		bool Initialize(ID3D12Device* device, ShaderManager*const manager, UINT width, UINT height, DXGI_FORMAT format);
		bool CompileShaders(const std::wstring& filePath);
		bool BuildRootSignature(const StaticSamplers& samplers);
		bool BuildPso();

		void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv, UINT rtvDescSize);
		bool OnResize(UINT width, UINT height);

		void Run(
			ID3D12GraphicsCommandList*const cmdList,
			D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer,
			D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
			D3D12_GPU_DESCRIPTOR_HANDLE si_velocity,
			float intensity,
			float limit,
			float depthBias,
			int sampleCount);

	private:
		void BuildDescriptors();
		bool BuildResources();

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;

		UINT mWidth;
		UINT mHeight;

		DXGI_FORMAT mBackBufferFormat;

		std::unique_ptr<GpuResource> mMotionVectorMap;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhMotionVectorMapCpuRtv;
	};
}

constexpr UINT MotionBlur::MotionBlurClass::Width() const {
	return mWidth;
}

constexpr UINT MotionBlur::MotionBlurClass::Height() const {
	return mHeight;
}

GpuResource* MotionBlur::MotionBlurClass::MotionVectorMapResource() {
	return mMotionVectorMap.get();
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE MotionBlur::MotionBlurClass::MotionVectorMapRtv() const {
	return mhMotionVectorMapCpuRtv;
}