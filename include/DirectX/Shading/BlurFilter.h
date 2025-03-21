#pragma once

#include <DirectX/d3dx12.h>
#include <unordered_map>
#include <array>
#include <wrl.h>

#include "Common/Util/Locker.h"
#include "DirectX/Infrastructure/GpuResource.h"
#include "DirectX/Shading/Samplers.h"

#include "HlslCompaction.h"

class ShaderManager;
struct IDxcBlob;

namespace BlurFilter {
	__forceinline INT CalcSize(FLOAT sigma);
	__forceinline BOOL CalcGaussWeights(FLOAT sigma, FLOAT weights[]);

	namespace RootSignature {
		namespace Default {
			enum {
				ECB_BlurPass = 0,
				EC_Consts,
				ESI_Normal,
				ESI_Depth,
				ESI_Input_F4,
				ESI_Input_F3,
				ESI_Input_F2,
				ESI_Input_F1,
				Count
			};
		}
	}

	class BlurFilterClass {
	public:
		BlurFilterClass() = default;
		virtual ~BlurFilterClass() = default;

	public:
		BOOL Initialize(Locker<ID3D12Device5>* const device, ShaderManager* const manager);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignature(const StaticSamplers& samplers);
		BOOL BuildPSO();
		void Run(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS cb_blur,
			GpuResource* const primary,
			GpuResource* const secondary,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_primary,
			D3D12_GPU_DESCRIPTOR_HANDLE si_primary,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_secondary,
			D3D12_GPU_DESCRIPTOR_HANDLE si_secondary,
			FilterType type,
			UINT blurCount = 3);
		void Run(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_GPU_VIRTUAL_ADDRESS cb_blur,
			D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
			D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
			GpuResource* const primary,
			GpuResource* const secondary,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_primary,
			D3D12_GPU_DESCRIPTOR_HANDLE si_primary,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_secondary,
			D3D12_GPU_DESCRIPTOR_HANDLE si_secondary,
			FilterType type,
			UINT blurCount = 3);

	private:
		void Blur(
			ID3D12GraphicsCommandList* const cmdList,
			GpuResource* const output,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_output,
			D3D12_GPU_DESCRIPTOR_HANDLE si_input,
			FilterType type,
			BOOL horzBlur);

	private:
		Locker<ID3D12Device5>* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		std::unordered_map<FilterType, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;
	};

	namespace CS {
		namespace RootSignature {
			enum {
				ECB_BlurPass = 0,
				EC_Consts,
				ESI_Normal,
				ESI_Depth,
				ESI_Input,
				EUO_Output,
				Count
			};
		}

		namespace Filter {
			enum Type {
				R8G8B8A8 = 0,
				R16,
				Count
			};
		}

		namespace Direction {
			enum Type {
				Horizontal = 0,
				Vertical,
				Count
			};
		}

		class BlurFilterCSClass {
		public:
			BlurFilterCSClass() = default;
			virtual ~BlurFilterCSClass() = default;

		public:
			BOOL Initialize(Locker<ID3D12Device5>* const device, ShaderManager* const manager);
			BOOL CompileShaders(const std::wstring& filePath);
			BOOL BuildRootSignature(const StaticSamplers& samplers);
			BOOL BuildPSO();
			void Run(
				ID3D12GraphicsCommandList* const cmdList,
				D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
				D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
				D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
				GpuResource* const primary,
				GpuResource* const secondary,
				D3D12_GPU_DESCRIPTOR_HANDLE si_primary,
				D3D12_GPU_DESCRIPTOR_HANDLE uo_primary,
				D3D12_GPU_DESCRIPTOR_HANDLE si_secondary,
				D3D12_GPU_DESCRIPTOR_HANDLE uo_secondary,
				Filter::Type type,
				UINT width, UINT height,
				size_t blurCount = 3);

		private:
			IDxcBlob* GetShader(Filter::Type type, Direction::Type direction);

		private:
			Locker<ID3D12Device5>* md3dDevice;
			ShaderManager* mShaderManager;

			Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
			std::unordered_map<Filter::Type, std::unordered_map<Direction::Type, Microsoft::WRL::ComPtr<ID3D12PipelineState>>> mPSOs;
		};
	}
}

#include "BlurFilter.inl"