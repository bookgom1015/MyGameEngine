#include "DebugCollision.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "D3D12Util.h"
#include "RenderItem.h"
#include "HlslCompaction.h"

using namespace DebugCollision;

namespace {
	const std::string DebugCollisionVS = "DebugCollisionVS";
	const std::string DebugCollisionGS = "DebugCollisionGS";
	const std::string DebugCollisionPS = "DebugCollisionPS";
}

bool DebugCollisionClass::Initialize(ID3D12Device* device, ShaderManager* const manager) {
	md3dDevice = device;
	mShaderManager = manager;

	return true;
}

bool DebugCollisionClass::CompileShaders(const std::wstring& filePath) {
	const std::wstring fullPath = filePath + L"DebugCollision.hlsl";
	auto vsInfo = D3D12ShaderInfo(fullPath.c_str(), L"VS", L"vs_6_3");
	auto gsInfo = D3D12ShaderInfo(fullPath.c_str(), L"GS", L"gs_6_3");
	auto psInfo = D3D12ShaderInfo(fullPath.c_str(), L"PS", L"ps_6_3");
	CheckReturn(mShaderManager->CompileShader(vsInfo, DebugCollisionVS));
	CheckReturn(mShaderManager->CompileShader(gsInfo, DebugCollisionGS));
	CheckReturn(mShaderManager->CompileShader(psInfo, DebugCollisionPS));

	return true;
}

bool DebugCollisionClass::BuildRootSignature() {
	CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignatureLayout::Count];

	slotRootParameter[RootSignatureLayout::ECB_Pass].InitAsConstantBufferView(0);
	slotRootParameter[RootSignatureLayout::ECB_Obj].InitAsConstantBufferView(1);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		_countof(slotRootParameter), slotRootParameter,
		0, nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignature));

	return true;
}

bool DebugCollisionClass::BuildPso() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC debugPsoDesc = D3D12Util::DefaultPsoDesc({ nullptr, 0 }, DXGI_FORMAT_UNKNOWN);
	debugPsoDesc.pRootSignature = mRootSignature.Get();
	{
		auto vs = mShaderManager->GetDxcShader(DebugCollisionVS);
		auto gs = mShaderManager->GetDxcShader(DebugCollisionGS);
		auto ps = mShaderManager->GetDxcShader(DebugCollisionPS);
		debugPsoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
		debugPsoDesc.GS = { reinterpret_cast<BYTE*>(gs->GetBufferPointer()), gs->GetBufferSize() };
		debugPsoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	debugPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	debugPsoDesc.NumRenderTargets = 1;
	debugPsoDesc.RTVFormats[0] = SDR_FORMAT;
	debugPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	debugPsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	debugPsoDesc.DepthStencilState.DepthEnable = false;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&debugPsoDesc, IID_PPV_ARGS(&mPSO)));

	return true;
}

void DebugCollisionClass::Run(
		ID3D12GraphicsCommandList* const cmdList,
		D3D12_VIEWPORT viewport,
		D3D12_RECT scissorRect,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
		D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
		D3D12_GPU_VIRTUAL_ADDRESS cb_object,
		const std::vector<RenderItem*>& ritems) {
	cmdList->SetPipelineState(mPSO.Get());
	cmdList->SetGraphicsRootSignature(mRootSignature.Get());

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	cmdList->OMSetRenderTargets(1, &ro_backBuffer, true, nullptr);

	cmdList->SetGraphicsRootConstantBufferView(RootSignatureLayout::ECB_Pass, cb_pass);

	DrawRenderItems(cmdList, ritems, cb_object);
}

void DebugCollisionClass::DrawRenderItems(
		ID3D12GraphicsCommandList* cmdList, 
		const std::vector<RenderItem*>& ritems,
		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress) {
	UINT objCBByteSize = D3D12Util::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	for (size_t i = 0; i < ritems.size(); ++i) {
		auto& ri = ritems[i];

		D3D12_GPU_VIRTUAL_ADDRESS currRitemObjCBAddress = objCBAddress + ri->ObjCBIndex * objCBByteSize;
		cmdList->SetGraphicsRootConstantBufferView(RootSignatureLayout::ECB_Obj, currRitemObjCBAddress);

		cmdList->IASetVertexBuffers(0, 0, nullptr);
		cmdList->IASetIndexBuffer(nullptr);
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
		cmdList->DrawInstanced(1, 1, 0, 0);
	}
}