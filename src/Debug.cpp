#include "Debug.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "D3D12Util.h"
#include "GpuResource.h"

using namespace Debug;

namespace {
	const CHAR* const VS_DebugTexMap = "VS_DebugMap";
	const CHAR* const PS_DebugTexMap = "PS_DebugMap";

	const CHAR* const VS_DrawCube = "VS_DrawCube";
	const CHAR* const PS_DrawCube = "PS_DrawCube";

	const CHAR* const VS_DrawEquirectangular = "VS_DrawEquirectangular";
	const CHAR* const PS_DrawEquirectangular = "PS_DrawEquirectangular";
}

BOOL DebugClass::Initialize(ID3D12Device* const device, ShaderManager* const manager) {
	md3dDevice = device;
	mShaderManager = manager;

	mNumEnabledMaps = 0;

	return TRUE;
}

BOOL DebugClass::CompileShaders(const std::wstring& filePath) {
	{
		const std::wstring fullPath = filePath + L"DebugTexMap.hlsl";
		const auto vsInfo = D3D12ShaderInfo(fullPath.c_str(), L"VS", L"vs_6_3");
		const auto psInfo = D3D12ShaderInfo(fullPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, VS_DebugTexMap));
		CheckReturn(mShaderManager->CompileShader(psInfo, PS_DebugTexMap));
	}
	{
		const std::wstring actualPath = filePath + L"DebugCubeMap.hlsl";
		{
			DxcDefine defines[] = {
				{ L"SPHERICAL", L"1" }
			};

			const auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3", defines, _countof(defines));
			const auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3", defines, _countof(defines));
			CheckReturn(mShaderManager->CompileShader(vsInfo, VS_DrawEquirectangular));
			CheckReturn(mShaderManager->CompileShader(psInfo, PS_DrawEquirectangular));
		}
		{
			const auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
			const auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
			CheckReturn(mShaderManager->CompileShader(vsInfo, VS_DrawCube));
			CheckReturn(mShaderManager->CompileShader(psInfo, PS_DrawCube));
		}
	}

	return TRUE;
}

BOOL DebugClass::BuildRootSignature(const StaticSamplers& samplers) {
	// DebugTexMap
	{
		CD3DX12_DESCRIPTOR_RANGE texTables[10] = {}; UINT index = 0;
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 0);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 1);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 1);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 1);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 1);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 1);

		index = 0;

		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::DebugTexMap::Count] = {};
		slotRootParameter[RootSignature::DebugTexMap::ECB_Debug].InitAsConstantBufferView(0);
		slotRootParameter[RootSignature::DebugTexMap::EC_Consts].InitAsConstants(RootConstant::DebugTexMap::Count, 1);
		slotRootParameter[RootSignature::DebugTexMap::ESI_Debug0].InitAsDescriptorTable(1, &texTables[index++]);
		slotRootParameter[RootSignature::DebugTexMap::ESI_Debug1].InitAsDescriptorTable(1, &texTables[index++]);
		slotRootParameter[RootSignature::DebugTexMap::ESI_Debug2].InitAsDescriptorTable(1, &texTables[index++]);
		slotRootParameter[RootSignature::DebugTexMap::ESI_Debug3].InitAsDescriptorTable(1, &texTables[index++]);
		slotRootParameter[RootSignature::DebugTexMap::ESI_Debug4].InitAsDescriptorTable(1, &texTables[index++]);
		slotRootParameter[RootSignature::DebugTexMap::ESI_Debug0_uint].InitAsDescriptorTable(1, &texTables[index++]);
		slotRootParameter[RootSignature::DebugTexMap::ESI_Debug1_uint].InitAsDescriptorTable(1, &texTables[index++]);
		slotRootParameter[RootSignature::DebugTexMap::ESI_Debug2_uint].InitAsDescriptorTable(1, &texTables[index++]);
		slotRootParameter[RootSignature::DebugTexMap::ESI_Debug3_uint].InitAsDescriptorTable(1, &texTables[index++]);
		slotRootParameter[RootSignature::DebugTexMap::ESI_Debug4_uint].InitAsDescriptorTable(1, &texTables[index++]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures[RootSignature::E_DebugTexMap]));
	}
	// DebugCubeMap
	{
		CD3DX12_DESCRIPTOR_RANGE texTables[2] = {}; UINT index = 0;
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);

		index = 0;

		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::DebugCubeMap::Count] = {};
		slotRootParameter[RootSignature::DebugCubeMap::ECB_Pass].InitAsConstantBufferView(0);
		slotRootParameter[RootSignature::DebugCubeMap::EC_Consts].InitAsConstants(RootConstant::DebugCubeMap::Count, 1);
		slotRootParameter[RootSignature::DebugCubeMap::ESI_Cube].InitAsDescriptorTable(1, &texTables[index++]);
		slotRootParameter[RootSignature::DebugCubeMap::ESI_Equirectangular].InitAsDescriptorTable(1, &texTables[index++]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures[RootSignature::E_DebugCubeMap]));
	}
	
	return TRUE;
}

BOOL DebugClass::BuildPSO() {
	const D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = { nullptr, 0 };

	// DebugTexMap
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC debugPsoDesc = D3D12Util::QuadPsoDesc();
		debugPsoDesc.pRootSignature = mRootSignatures[RootSignature::E_DebugTexMap].Get();
		{
			const auto vs = mShaderManager->GetDxcShader(VS_DebugTexMap);
			const auto ps = mShaderManager->GetDxcShader(PS_DebugTexMap);
			debugPsoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			debugPsoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
		debugPsoDesc.RTVFormats[0] = SDR_FORMAT;
		CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&debugPsoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::EG_DebugTexMap])));
	}
	// DebugCubeMap
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::DefaultPsoDesc(inputLayoutDesc, DXGI_FORMAT_UNKNOWN);
		psoDesc.pRootSignature = mRootSignatures[RootSignature::E_DebugCubeMap].Get();
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = SDR_FORMAT;
		psoDesc.DepthStencilState.DepthEnable = FALSE;

		{
			const auto vs = mShaderManager->GetDxcShader(VS_DrawCube);
			const auto ps = mShaderManager->GetDxcShader(PS_DrawCube);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
		CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::EG_DebugCubeMap])));

		{
			const auto vs = mShaderManager->GetDxcShader(VS_DrawEquirectangular);
			const auto ps = mShaderManager->GetDxcShader(PS_DrawEquirectangular);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
		CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::EG_DebugEquirectangularMap])));
	}

	return TRUE;
}

BOOL DebugClass::AddDebugMap(D3D12_GPU_DESCRIPTOR_HANDLE hGpuSrv, SampleMask::Type mask) {
	if (mNumEnabledMaps >= 5) return FALSE;

	mhDebugGpuSrvs[mNumEnabledMaps] = hGpuSrv;
	mDebugMasks[mNumEnabledMaps] = mask;

	++mNumEnabledMaps;
	
	return TRUE;
}

BOOL DebugClass::AddDebugMap(D3D12_GPU_DESCRIPTOR_HANDLE hGpuSrv, SampleMask::Type mask, DebugSampleDesc desc) {
	if (mNumEnabledMaps >= 5) return FALSE;

	mhDebugGpuSrvs[mNumEnabledMaps] = hGpuSrv;
	mDebugMasks[mNumEnabledMaps] = mask;
	mSampleDescs[mNumEnabledMaps] = desc;

	++mNumEnabledMaps;

	return TRUE;
}

void DebugClass::RemoveDebugMap(D3D12_GPU_DESCRIPTOR_HANDLE hGpuSrv) {
	const auto num = static_cast<INT>(mNumEnabledMaps);
	for (INT i = 0; i < num; ++i) {
		if (mhDebugGpuSrvs[i].ptr == hGpuSrv.ptr) {
			for (INT curr = i, end = num - 2; curr <= end; ++curr) {
				const INT next = curr + 1;
				mhDebugGpuSrvs[curr] = mhDebugGpuSrvs[next];
				mDebugMasks[curr] = mDebugMasks[next];
				mSampleDescs[curr] = mSampleDescs[next];
			}			

			--mNumEnabledMaps;
			return;
		}
	}
}

void DebugClass::DebugTexMap(
		ID3D12GraphicsCommandList* const cmdList,
		D3D12_VIEWPORT viewport,
		D3D12_RECT scissorRect,
		GpuResource* const backBuffer,
		D3D12_GPU_VIRTUAL_ADDRESS cb_debug,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
		D3D12_CPU_DESCRIPTOR_HANDLE dio_dsv) {
	cmdList->SetPipelineState(mPSOs[PipelineState::EG_DebugTexMap].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[RootSignature::E_DebugTexMap].Get());

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->OMSetRenderTargets(1, &ro_backBuffer, TRUE, &dio_dsv);

	cmdList->SetGraphicsRootConstantBufferView(RootSignature::DebugTexMap::ECB_Debug, cb_debug);

	cmdList->SetGraphicsRoot32BitConstants(
		RootSignature::DebugTexMap::EC_Consts,
		static_cast<UINT>(mDebugMasks.size()),
		mDebugMasks.data(),
		0
	);

	for (UINT i = 0; i < mNumEnabledMaps; ++i) {
		if (mDebugMasks[i] == SampleMask::UINT)
			cmdList->SetGraphicsRootDescriptorTable(static_cast<UINT>(RootSignature::DebugTexMap::ESI_Debug0_uint + i), mhDebugGpuSrvs[i]);
		else
			cmdList->SetGraphicsRootDescriptorTable(static_cast<UINT>(RootSignature::DebugTexMap::ESI_Debug0 + i), mhDebugGpuSrvs[i]);
	}

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, static_cast<UINT>(mNumEnabledMaps), 0, 0);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PRESENT);
}

BOOL DebugClass::DebugCubeMap(
		ID3D12GraphicsCommandList* const cmdList,
		D3D12_VIEWPORT viewport,
		D3D12_RECT scissorRect,
		GpuResource* const backBuffer,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
		D3D12_GPU_VIRTUAL_ADDRESS cbPassAddress,
		D3D12_GPU_DESCRIPTOR_HANDLE si_input,
		FLOAT mipLevel) {
	if (DebugCubeMapType == IrradianceMap::CubeMap::E_Equirectangular) cmdList->SetPipelineState(mPSOs[PipelineState::EG_DebugEquirectangularMap].Get());
	else cmdList->SetPipelineState(mPSOs[PipelineState::EG_DebugCubeMap].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[RootSignature::E_DebugCubeMap].Get());

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->OMSetRenderTargets(1, &ro_backBuffer, TRUE, nullptr);

	cmdList->SetGraphicsRootConstantBufferView(RootSignature::DebugCubeMap::ECB_Pass, cbPassAddress);

	RootConstant::DebugCubeMap::Struct rc;
	rc.gMipLevel = mipLevel;

	std::array<std::uint32_t, RootConstant::DebugCubeMap::Count> consts;
	std::memcpy(consts.data(), &rc, sizeof(RootConstant::DebugCubeMap::Struct));

	cmdList->SetGraphicsRoot32BitConstants(RootSignature::DebugCubeMap::EC_Consts, RootConstant::DebugCubeMap::Count, consts.data(), 0);

	switch (DebugCubeMapType) {
	case IrradianceMap::CubeMap::E_EnvironmentCube:
	case IrradianceMap::CubeMap::E_DiffuseIrradianceCube:
	case IrradianceMap::CubeMap::E_PrefilteredIrradianceCube:
		cmdList->SetGraphicsRootDescriptorTable(RootSignature::DebugCubeMap::ESI_Cube, si_input);
		break;
	case IrradianceMap::CubeMap::E_Equirectangular:
		cmdList->SetGraphicsRootDescriptorTable(RootSignature::DebugCubeMap::ESI_Equirectangular, si_input);
		break;
	}

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(36, 1, 0, 0);

	return TRUE;
}