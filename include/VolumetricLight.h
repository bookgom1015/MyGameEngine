#pragma once

#include <d3dx12.h>
#include <wrl.h>

#include "Samplers.h"
#include "GpuResource.h"

class ShaderManager;
class GpuResource;

namespace VolumetricLight {
	namespace RootSignature {
		enum {
			E_CalcScatteringAndDensity = 0,
			Count
		};

		namespace CalcScatteringAndDensity {
			enum {
				ECB_Pass = 0,
				EC_Consts,
				ESI_ZDepth,
				ESI_ZDepthCube,
				ESI_FaceIDCube,
				EUO_FrustumVolume,
				Count
			};

			namespace RootConstant {
				enum {
					E_NearPlane = 0,
					E_FarPlane,
					E_DepthExponent,
					Count
				};
			}
		}
	}

	namespace PipelineState {
		enum {
			EC_CalcScatteringAndDensity = 0,
			Count
		};
	}

	class VolumetricLightClass {
	public:
		VolumetricLightClass();
		virtual ~VolumetricLightClass() = default;

	public:
		BOOL Initialize(
			ID3D12Device* device, ShaderManager* const manager,
			UINT width, UINT height, UINT depth);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignature(const StaticSamplers& samplers);
		BOOL BuildPSO();
		void Run(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
			D3D12_GPU_DESCRIPTOR_HANDLE si_zdepth,
			D3D12_GPU_DESCRIPTOR_HANDLE si_zdepthCube,
			D3D12_GPU_DESCRIPTOR_HANDLE si_faceIDCube,
			FLOAT nearZ, FLOAT farZ);

		void BuildDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
			CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
			UINT descSize);

	private:
		void BuildDescriptors();
		BOOL BuildResources();

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignatures[RootSignature::Count];
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSOs[PipelineState::Count];

		std::unique_ptr<GpuResource> mFrustumMap;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhFrustumMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhFrustumMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhFrustumMapCpuUav;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhFrustumMapGpuUav;

		UINT mWidth;
		UINT mHeight;
		UINT mDepth;
	};
}