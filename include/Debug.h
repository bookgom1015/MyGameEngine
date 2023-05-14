#pragma once

#include <d3dx12.h>

#include "MathHelper.h"
#include "HlslCompaction.h"

#include "Samplers.h"

class ShaderManager;

namespace Debug {
	namespace RootSignatureLayout {
		enum {
			EC_Consts = 0,
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

	class DebugClass {
	public:
		DebugClass() = default;
		virtual ~DebugClass() = default;

	public:
		bool Initialize(ID3D12Device* device, ShaderManager*const manager, UINT width, UINT height, DXGI_FORMAT backBufferFormat);
		bool CompileShaders(const std::wstring& filePath);
		bool BuildRootSignature(const StaticSamplers& samplers);
		bool BuildPso();
		void Run(
			ID3D12GraphicsCommandList*const cmdList,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE dio_dsv,
			DebugShaderParams::SampleMask::Type mask0 = DebugShaderParams::SampleMask::RGB,
			DebugShaderParams::SampleMask::Type mask1 = DebugShaderParams::SampleMask::RGB,
			DebugShaderParams::SampleMask::Type mask2 = DebugShaderParams::SampleMask::RGB,
			DebugShaderParams::SampleMask::Type mask3 = DebugShaderParams::SampleMask::RGB,
			DebugShaderParams::SampleMask::Type mask4 = DebugShaderParams::SampleMask::RGB);

		bool OnResize(UINT width, UINT height);

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;

		UINT mWidth;
		UINT mHeight;

		D3D12_VIEWPORT mViewport;
		D3D12_RECT mScissorRect;

		DXGI_FORMAT mBackBufferFormat;
	};
};