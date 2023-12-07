#include "DebugMap.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "D3D12Util.h"
#include "GpuResource.h"

using namespace DebugMap;

namespace {
	const std::string DebugMapVS = "DebugMapVS";
	const std::string DebugMapPS = "DebugMapPS";
}

BOOL DebugMapClass::Initialize(ID3D12Device* device, ShaderManager*const manager) {
	md3dDevice = device;
	mShaderManager = manager;

	mNumEnabledMaps = 0;

	return true;
}

BOOL DebugMapClass::CompileShaders(const std::wstring& filePath) {
	const std::wstring fullPath = filePath + L"DebugMap.hlsl";
	auto vsInfo = D3D12ShaderInfo(fullPath .c_str(), L"VS", L"vs_6_3");
	auto psInfo = D3D12ShaderInfo(fullPath .c_str(), L"PS", L"ps_6_3");
	CheckReturn(mShaderManager->CompileShader(vsInfo, DebugMapVS));
	CheckReturn(mShaderManager->CompileShader(psInfo, DebugMapPS));

	return true;
}

BOOL DebugMapClass::BuildRootSignature(const StaticSamplers& samplers) {
	CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignatureLayout::Count];

	CD3DX12_DESCRIPTOR_RANGE texTables[10];
	texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
	texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
	texTables[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);
	texTables[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0);
	texTables[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 0);
	texTables[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 1);
	texTables[6].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 1);
	texTables[7].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 1);
	texTables[8].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 1);
	texTables[9].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 1);

	slotRootParameter[RootSignatureLayout::ECB_DebugMap].InitAsConstantBufferView(0);
	slotRootParameter[RootSignatureLayout::EC_Consts].InitAsConstants(RootConstantsLayout::Count, 1);
	slotRootParameter[RootSignatureLayout::ESI_Debug0].InitAsDescriptorTable(1, &texTables[0]);
	slotRootParameter[RootSignatureLayout::ESI_Debug1].InitAsDescriptorTable(1, &texTables[1]);
	slotRootParameter[RootSignatureLayout::ESI_Debug2].InitAsDescriptorTable(1, &texTables[2]);
	slotRootParameter[RootSignatureLayout::ESI_Debug3].InitAsDescriptorTable(1, &texTables[3]);
	slotRootParameter[RootSignatureLayout::ESI_Debug4].InitAsDescriptorTable(1, &texTables[4]);
	slotRootParameter[RootSignatureLayout::ESI_Debug0_uint].InitAsDescriptorTable(1, &texTables[5]);
	slotRootParameter[RootSignatureLayout::ESI_Debug1_uint].InitAsDescriptorTable(1, &texTables[6]);
	slotRootParameter[RootSignatureLayout::ESI_Debug2_uint].InitAsDescriptorTable(1, &texTables[7]);
	slotRootParameter[RootSignatureLayout::ESI_Debug3_uint].InitAsDescriptorTable(1, &texTables[8]);
	slotRootParameter[RootSignatureLayout::ESI_Debug4_uint].InitAsDescriptorTable(1, &texTables[9]);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignature));
	
	return true;
}

BOOL DebugMapClass::BuildPso() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC debugPsoDesc = D3D12Util::QuadPsoDesc();
	debugPsoDesc.pRootSignature = mRootSignature.Get();
	{
		auto vs = mShaderManager->GetDxcShader(DebugMapVS);
		auto ps = mShaderManager->GetDxcShader(DebugMapPS);
		debugPsoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
		debugPsoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	debugPsoDesc.RTVFormats[0] = SDR_FORMAT;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&debugPsoDesc, IID_PPV_ARGS(&mPSO)));

	return true;
}

void DebugMapClass::Run(
		ID3D12GraphicsCommandList*const cmdList,
		D3D12_VIEWPORT viewport,
		D3D12_RECT scissorRect,
		GpuResource* backBuffer,
		D3D12_GPU_VIRTUAL_ADDRESS cb_debug,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
		D3D12_CPU_DESCRIPTOR_HANDLE dio_dsv) {
	cmdList->SetPipelineState(mPSO.Get());
	cmdList->SetGraphicsRootSignature(mRootSignature.Get());
	
	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);
	
	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->OMSetRenderTargets(1, &ro_backBuffer, true, &dio_dsv);

	cmdList->SetGraphicsRootConstantBufferView(RootSignatureLayout::ECB_DebugMap, cb_debug);

	cmdList->SetGraphicsRoot32BitConstants(
		RootSignatureLayout::EC_Consts, 
		static_cast<UINT>(mDebugMasks.size()), 
		mDebugMasks.data(), 
		RootConstantsLayout::ESampleMask0
	);

	for (INT i = 0; i < mNumEnabledMaps; ++i) {
		if (mDebugMasks[i] == SampleMask::UINT) 
			cmdList->SetGraphicsRootDescriptorTable(static_cast<UINT>(RootSignatureLayout::ESI_Debug0_uint + i), mhDebugGpuSrvs[i]);
		else
			cmdList->SetGraphicsRootDescriptorTable(static_cast<UINT>(RootSignatureLayout::ESI_Debug0 + i), mhDebugGpuSrvs[i]);
	}

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, static_cast<UINT>(mNumEnabledMaps), 0, 0);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PRESENT);
}

BOOL DebugMapClass::AddDebugMap(D3D12_GPU_DESCRIPTOR_HANDLE hGpuSrv, SampleMask::Type mask) {
	if (mNumEnabledMaps >= 5) return false;

	mhDebugGpuSrvs[mNumEnabledMaps] = hGpuSrv;
	mDebugMasks[mNumEnabledMaps] = mask;

	++mNumEnabledMaps;
	
	return true;
}

BOOL DebugMapClass::AddDebugMap(D3D12_GPU_DESCRIPTOR_HANDLE hGpuSrv, SampleMask::Type mask, DebugMapSampleDesc desc) {
	if (mNumEnabledMaps >= 5) return false;

	mhDebugGpuSrvs[mNumEnabledMaps] = hGpuSrv;
	mDebugMasks[mNumEnabledMaps] = mask;
	mSampleDescs[mNumEnabledMaps] = desc;

	++mNumEnabledMaps;

	return true;
}

void DebugMapClass::RemoveDebugMap(D3D12_GPU_DESCRIPTOR_HANDLE hGpuSrv) {
	for (INT i = 0; i < mNumEnabledMaps; ++i) {
		if (mhDebugGpuSrvs[i].ptr == hGpuSrv.ptr) {
			for (INT curr = i, end = mNumEnabledMaps - 2; curr <= end; ++curr) {
				INT next = curr + 1;
				mhDebugGpuSrvs[curr] = mhDebugGpuSrvs[next];
				mDebugMasks[curr] = mDebugMasks[next];
				mSampleDescs[curr] = mSampleDescs[next];
			}
			

			--mNumEnabledMaps;
			return;
		}
	}
}