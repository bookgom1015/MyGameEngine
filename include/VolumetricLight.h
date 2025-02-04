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
			E_AccumulateScattering,
			E_ApplyFog,
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
					E_UniformDensity,
					E_AnisotropicCoefficient,
					Count
				};
			}
		}

		namespace AccumulateScattering {
			enum {
				EC_Consts = 0,
				EUIO_FrustumVolume,
				Count
			};

			namespace RootConstant {
				enum {
					E_NearPlane = 0,
					E_FarPlane,
					E_DepthExponent,
					E_DensityScale,
					Count
				};
			}
		}

		namespace ApplyFog {
			enum {
				ECB_Pass = 0,
				ESI_Position,
				ESI_FrustumVolume,
				Count
			};
		}
	}

	namespace PipelineState {
		enum {
			EC_CalcScatteringAndDensity = 0,
			EC_AccumulateScattering,
			EG_ApplyFog,
			Count
		};
	}

	static const UINT NumRenderTargets = 1;

	class VolumetricLightClass {
	public:
		VolumetricLightClass();
		virtual ~VolumetricLightClass() = default;

	public:
		BOOL Initialize(
			ID3D12Device* device, ShaderManager* const manager,
			UINT clientW, UINT clientH,
			UINT texW, UINT texH, UINT texD);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignature(const StaticSamplers& samplers);
		BOOL BuildPSO();
		void Run(
			ID3D12GraphicsCommandList* const cmdList,
			const D3D12_VIEWPORT& viewport,
			const D3D12_RECT& scissorRect,
			GpuResource* backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
			D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
			D3D12_GPU_DESCRIPTOR_HANDLE si_zdepth,
			D3D12_GPU_DESCRIPTOR_HANDLE si_zdepthCube,
			D3D12_GPU_DESCRIPTOR_HANDLE si_faceIDCube,
			D3D12_GPU_DESCRIPTOR_HANDLE si_position,
			FLOAT nearZ, FLOAT farZ,
			FLOAT depth_exp, 
			FLOAT uniformDensity, 
			FLOAT anisotropicCoeiff,
			FLOAT densityScale);

		void BuildDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
			CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
			CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
			UINT descSize, UINT rtvDescSize);

	private:
		void BuildDescriptors();
		BOOL BuildResources();

		void CalculateScatteringAndDensity(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
			D3D12_GPU_DESCRIPTOR_HANDLE si_zdepth,
			D3D12_GPU_DESCRIPTOR_HANDLE si_zdepthCube,
			D3D12_GPU_DESCRIPTOR_HANDLE si_faceIDCube,
			FLOAT nearZ, FLOAT farZ,
			FLOAT depth_exp, FLOAT uniformDensity, FLOAT anisotropicCoeiff);
		void AccumulateScattering(
			ID3D12GraphicsCommandList* const cmdList,
			FLOAT nearZ, FLOAT farZ,
			FLOAT densityScale);
		void ApplyFog(
			ID3D12GraphicsCommandList* const cmdList, 
			const D3D12_VIEWPORT& viewport,
			const D3D12_RECT& scissorRect,
			GpuResource* backBuffer, 
			D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
			D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
			D3D12_GPU_DESCRIPTOR_HANDLE si_position);

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

		std::unique_ptr<GpuResource> mDebugMap;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhDebugMapCpuRtv;

		UINT mClientWidth;
		UINT mClientHeight;

		UINT mTexWidth;
		UINT mTexHeight;
		UINT mTexDepth;
	};
}