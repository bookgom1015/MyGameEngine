#include "DebugCollision.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "D3D12Util.h"
#include "RenderItem.h"
#include "HlslCompaction.h"
#include "GpuResource.h"

using namespace DebugCollision;

namespace {
	const CHAR* const VS_DebugCollision = "VS_DebugCollision";
	const CHAR* const GS_DebugCollision = "GS_DebugCollision";
	const CHAR* const PS_DebugCollision = "PS_DebugCollision";
}

BOOL DebugCollisionClass::Initialize(ID3D12Device* const device, ShaderManager* const manager) {
	md3dDevice = device;
	mShaderManager = manager;

	return TRUE;
}

BOOL DebugCollisionClass::CompileShaders(const std::wstring& filePath) {
	const std::wstring fullPath = filePath + L"DebugCollision.hlsl";
	const auto vsInfo = D3D12ShaderInfo(fullPath.c_str(), L"VS", L"vs_6_3");
	const auto gsInfo = D3D12ShaderInfo(fullPath.c_str(), L"GS", L"gs_6_3");
	const auto psInfo = D3D12ShaderInfo(fullPath.c_str(), L"PS", L"ps_6_3");
	CheckReturn(mShaderManager->CompileShader(vsInfo, VS_DebugCollision));
	CheckReturn(mShaderManager->CompileShader(gsInfo, GS_DebugCollision));
	CheckReturn(mShaderManager->CompileShader(psInfo, PS_DebugCollision));

	return TRUE;
}

BOOL DebugCollisionClass::BuildRootSignature() {
	CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::Count] = {};

	slotRootParameter[RootSignature::ECB_Pass].InitAsConstantBufferView(0);
	slotRootParameter[RootSignature::ECB_Obj].InitAsConstantBufferView(1);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		_countof(slotRootParameter), slotRootParameter,
		0, nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignature));

	return TRUE;
}

BOOL DebugCollisionClass::BuildPSO() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC debugPsoDesc = D3D12Util::DefaultPsoDesc({ nullptr, 0 }, DXGI_FORMAT_UNKNOWN);
	debugPsoDesc.pRootSignature = mRootSignature.Get();
	{
		const auto vs = mShaderManager->GetDxcShader(VS_DebugCollision);
		const auto gs = mShaderManager->GetDxcShader(GS_DebugCollision);
		const auto ps = mShaderManager->GetDxcShader(PS_DebugCollision);
		debugPsoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
		debugPsoDesc.GS = { reinterpret_cast<BYTE*>(gs->GetBufferPointer()), gs->GetBufferSize() };
		debugPsoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	debugPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	debugPsoDesc.NumRenderTargets = 1;
	debugPsoDesc.RTVFormats[0] = SDR_FORMAT;
	debugPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	debugPsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	debugPsoDesc.DepthStencilState.DepthEnable = FALSE;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&debugPsoDesc, IID_PPV_ARGS(&mPSO)));

	return TRUE;
}

void DebugCollisionClass::Run(
		ID3D12GraphicsCommandList* const cmdList,
		D3D12_VIEWPORT viewport,
		D3D12_RECT scissorRect,
		GpuResource* const backBuffer,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
		D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
		D3D12_GPU_VIRTUAL_ADDRESS cb_obj,
		UINT objCBByteSize,
		const std::vector<RenderItem*>& ritems) {
	cmdList->SetPipelineState(mPSO.Get());
	cmdList->SetGraphicsRootSignature(mRootSignature.Get());

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->OMSetRenderTargets(1, &ro_backBuffer, TRUE, nullptr);

	cmdList->SetGraphicsRootConstantBufferView(RootSignature::ECB_Pass, cb_pass);

	DrawRenderItems(cmdList, ritems, cb_obj, objCBByteSize);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PRESENT);
}

void DebugCollisionClass::DrawRenderItems(
		ID3D12GraphicsCommandList* const cmdList, 
		const std::vector<RenderItem*>& ritems,
		D3D12_GPU_VIRTUAL_ADDRESS cb_obj,
		UINT objCBByteSize) {

	for (UINT i = 0; i < ritems.size(); ++i) {
		const auto& ri = ritems[i];

		D3D12_GPU_VIRTUAL_ADDRESS currRitemObjCBAddress = cb_obj + static_cast<UINT64>(ri->ObjCBIndex) * static_cast<UINT64>(objCBByteSize);
		cmdList->SetGraphicsRootConstantBufferView(RootSignature::ECB_Obj, currRitemObjCBAddress);

		cmdList->IASetVertexBuffers(0, 0, nullptr);
		cmdList->IASetIndexBuffer(nullptr);
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
		cmdList->DrawInstanced(1, 1, 0, 0);
	}
}