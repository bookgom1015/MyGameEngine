#pragma once

#include <d3dx12.h>
#include <unordered_map>
#include <wrl.h>

#include "Samplers.h"

class ShaderManager;
class GpuResource;

namespace RaytracedReflection {
	namespace RootSignature {
		enum {
			ECB_Rr = 0,
			ESI_AS,
			ESI_BackBuffer,
			ESI_NormalDepth,
			EUO_Reflection,
			Count
		};
	}

	class RaytracedReflectionClass {
	public:
		RaytracedReflectionClass();
		virtual ~RaytracedReflectionClass() = default;

	public:
		__forceinline constexpr UINT Width() const;
		__forceinline constexpr UINT Height() const;

	public:
		bool Initialize(ID3D12Device5* const device, ID3D12GraphicsCommandList* const cmdList, ShaderManager* const manager, UINT width, UINT height);
		bool CompileShaders(const std::wstring& filePath);
		bool BuildRootSignatures(const StaticSamplers& samplers);
		bool BuildPSO();
		bool BuildShaderTables();
		void BuildDesscriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu, CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu, UINT descSize);
		bool OnResize(ID3D12GraphicsCommandList* cmdList, UINT width, UINT height);

	private:
		void BuildDescriptors();
		bool BuildResources(ID3D12GraphicsCommandList* cmdList);

	private:
		ID3D12Device5* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPso;
		Microsoft::WRL::ComPtr<ID3D12StateObject> mDxrPso;
		Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> mDxrPsoProp;

		std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12Resource>> mShaderTables;

		UINT mWidth;
		UINT mHeight;

		std::unique_ptr<GpuResource> mReflectionMap;
		std::unique_ptr<GpuResource> mReflectionUploadBuffer;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhReflectionMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhReflectionMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhReflectionMapCpuUav;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhReflectionMapGpuUav;
	};
}

constexpr UINT RaytracedReflection::RaytracedReflectionClass::Width() const {
	return mWidth;
}

constexpr UINT RaytracedReflection::RaytracedReflectionClass::Height() const {
	return mHeight;
}