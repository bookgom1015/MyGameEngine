#pragma once

#include <d3dx12.h>
#include <wrl.h>

class ShaderManager;
class GpuResource;

struct RenderItem;

namespace DebugCollision {
	namespace RootSignature {
		enum {
			ECB_Pass = 0,
			ECB_Obj,
			Count
		};
	}

	class DebugCollisionClass {
	public:
		DebugCollisionClass() = default;
		virtual ~DebugCollisionClass() = default;

	public:
		BOOL Initialize(ID3D12Device* device, ShaderManager* const manager);
		BOOL CompileShaders(const std::wstring& filePath);
		BOOL BuildRootSignature();
		BOOL BuildPSO();
		void Run(
			ID3D12GraphicsCommandList* const cmdList,
			D3D12_VIEWPORT viewport,
			D3D12_RECT scissorRect,
			GpuResource* const backBuffer,
			D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
			D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
			D3D12_GPU_VIRTUAL_ADDRESS cb_object,
			const std::vector<RenderItem*>& ritems);

	private:
		void DebugCollisionClass::DrawRenderItems(
			ID3D12GraphicsCommandList* cmdList,
			const std::vector<RenderItem*>& ritems,
			D3D12_GPU_VIRTUAL_ADDRESS objCBAddress);

	private:
		ID3D12Device* md3dDevice;
		ShaderManager* mShaderManager;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;
	};
}