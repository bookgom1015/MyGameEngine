#include "BlurFilter.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "D3D12Util.h"

using namespace BlurFilter;

namespace {
	const CHAR* GaussianBlurVS_F4 = "GaussianBlurVS_F4";
	const CHAR* GaussianBlurPS_F4 = "GaussianBlurPS_F4";

	const CHAR* GaussianBlurVS_F3 = "GaussianBlurVS_F3";
	const CHAR* GaussianBlurPS_F3 = "GaussianBlurPS_F3";

	const CHAR* GaussianBlurVS_F2 = "GaussianBlurVS_F2";
	const CHAR* GaussianBlurPS_F2 = "GaussianBlurPS_F2";

	const CHAR* GaussianBlurVS_F1 = "GaussianBlurVS_F1";
	const CHAR* GaussianBlurPS_F1 = "GaussianBlurPS_F1";
}

BOOL BlurFilterClass::Initialize(ID3D12Device*const device, ShaderManager*const manager) {
	md3dDevice = device;
	mShaderManager = manager;

	return true;
}

BOOL BlurFilterClass::CompileShaders(const std::wstring& filePath) {
	const auto path = filePath + L"GaussianBlur.hlsl";
	
	{
		DxcDefine defines[] = {
			{ L"FT_F4", L"1" }
		};
		auto vsInfo = D3D12ShaderInfo(path.c_str(), L"VS", L"vs_6_3", defines, _countof(defines));
		auto psInfo = D3D12ShaderInfo(path.c_str(), L"PS", L"ps_6_3", defines, _countof(defines));
		CheckReturn(mShaderManager->CompileShader(vsInfo, GaussianBlurVS_F4));
		CheckReturn(mShaderManager->CompileShader(psInfo, GaussianBlurPS_F4));
	}
	{
		DxcDefine defines[] = {
			{ L"FT_F3", L"1" }
		};
		auto vsInfo = D3D12ShaderInfo(path.c_str(), L"VS", L"vs_6_3", defines, _countof(defines));
		auto psInfo = D3D12ShaderInfo(path.c_str(), L"PS", L"ps_6_3", defines, _countof(defines));
		CheckReturn(mShaderManager->CompileShader(vsInfo, GaussianBlurVS_F3));
		CheckReturn(mShaderManager->CompileShader(psInfo, GaussianBlurPS_F3));
	}
	{
		DxcDefine defines[] = {
			{ L"FT_F2", L"1" }
		};
		auto vsInfo = D3D12ShaderInfo(path.c_str(), L"VS", L"vs_6_3", defines, _countof(defines));
		auto psInfo = D3D12ShaderInfo(path.c_str(), L"PS", L"ps_6_3", defines, _countof(defines));
		CheckReturn(mShaderManager->CompileShader(vsInfo, GaussianBlurVS_F2));
		CheckReturn(mShaderManager->CompileShader(psInfo, GaussianBlurPS_F2));
	}
	{
		DxcDefine defines[] = {
			{ L"FT_F1", L"1" }
		};
		auto vsInfo = D3D12ShaderInfo(path.c_str(), L"VS", L"vs_6_3", defines, _countof(defines));
		auto psInfo = D3D12ShaderInfo(path.c_str(), L"PS", L"ps_6_3", defines, _countof(defines));
		CheckReturn(mShaderManager->CompileShader(vsInfo, GaussianBlurVS_F1));
		CheckReturn(mShaderManager->CompileShader(psInfo, GaussianBlurPS_F1));
	}
	
	return true;
}

BOOL BlurFilterClass::BuildRootSignature(const StaticSamplers& samplers) {
	// For blur
	CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignatureLayout::Count];

	CD3DX12_DESCRIPTOR_RANGE texTables[6];
	texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
	texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
	texTables[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);
	texTables[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0);
	texTables[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 0);
	texTables[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5, 0);

	slotRootParameter[RootSignatureLayout::ECB_BlurPass].InitAsConstantBufferView(0);
	slotRootParameter[RootSignatureLayout::EC_Consts].InitAsConstants(3, 1);
	slotRootParameter[RootSignatureLayout::ESI_Normal].InitAsDescriptorTable(1, &texTables[0]);
	slotRootParameter[RootSignatureLayout::ESI_Depth].InitAsDescriptorTable(1, &texTables[1]);
	slotRootParameter[RootSignatureLayout::ESI_Input_F4].InitAsDescriptorTable(1, &texTables[2]);
	slotRootParameter[RootSignatureLayout::ESI_Input_F3].InitAsDescriptorTable(1, &texTables[3]);
	slotRootParameter[RootSignatureLayout::ESI_Input_F2].InitAsDescriptorTable(1, &texTables[4]);
	slotRootParameter[RootSignatureLayout::ESI_Input_F1].InitAsDescriptorTable(1, &texTables[5]);

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignature));

	return true;
}

BOOL BlurFilterClass::BuildPso() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC quadPsoDesc;
	ZeroMemory(&quadPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	quadPsoDesc.InputLayout = { nullptr, 0 };
	quadPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	quadPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	quadPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	quadPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	quadPsoDesc.NumRenderTargets = 1;
	quadPsoDesc.SampleMask = UINT_MAX;
	quadPsoDesc.SampleDesc.Count = 1;
	quadPsoDesc.SampleDesc.Quality = 0;
	quadPsoDesc.DepthStencilState.DepthEnable = FALSE;
	quadPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	quadPsoDesc.pRootSignature = mRootSignature.Get();

	for (UINT i = 0; i < FilterType::Count; ++i) {
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = quadPsoDesc;
		switch (i) {
		case FilterType::R8G8B8A8:
		{
			auto vs = mShaderManager->GetDxcShader(GaussianBlurVS_F4);
			auto ps = mShaderManager->GetDxcShader(GaussianBlurPS_F4);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
			psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
			break;
		case FilterType::R16:
		{
			auto vs = mShaderManager->GetDxcShader(GaussianBlurVS_F1);
			auto ps = mShaderManager->GetDxcShader(GaussianBlurPS_F1);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
			psoDesc.RTVFormats[0] = DXGI_FORMAT_R16_UNORM;
			break;
		case FilterType::R16G16B16A16:
		{
			auto vs = mShaderManager->GetDxcShader(GaussianBlurVS_F4);
			auto ps = mShaderManager->GetDxcShader(GaussianBlurPS_F4);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
			psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
			break;
		case FilterType::R32G32B32A32:
		{
			auto vs = mShaderManager->GetDxcShader(GaussianBlurVS_F4);
			auto ps = mShaderManager->GetDxcShader(GaussianBlurPS_F4);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
			psoDesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
			break;
		default:
			ReturnFalse(L"Unknown FilterType");
		}
		CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[(FilterType)i])));
	}

	return true;
}

void BlurFilterClass::Run(
		ID3D12GraphicsCommandList*const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
		GpuResource* const primary,
		GpuResource* const secondary,
		D3D12_CPU_DESCRIPTOR_HANDLE primaryRtv,
		D3D12_GPU_DESCRIPTOR_HANDLE primarySrv,
		D3D12_CPU_DESCRIPTOR_HANDLE secondaryRtv,
		D3D12_GPU_DESCRIPTOR_HANDLE secondarySrv,
		FilterType type,
		size_t blurCount) {
	cmdList->SetPipelineState(mPSOs[type].Get());
	cmdList->SetGraphicsRootSignature(mRootSignature.Get());

	cmdList->SetGraphicsRootConstantBufferView(RootSignatureLayout::ECB_BlurPass, cbAddress);
	cmdList->SetGraphicsRoot32BitConstant(RootSignatureLayout::EC_Consts, 0, RootConstantsLayout::EBilateral);

	for (INT i = 0; i < blurCount; ++i) {
		cmdList->SetGraphicsRoot32BitConstant(RootSignatureLayout::EC_Consts, 1, RootConstantsLayout::EHorizontal);
		Blur(cmdList, secondary, secondaryRtv, primarySrv, type, true);

		cmdList->SetGraphicsRoot32BitConstant(RootSignatureLayout::EC_Consts, 0, RootConstantsLayout::EHorizontal);
		Blur(cmdList, primary, primaryRtv, secondarySrv, type, false);
	}
}

void BlurFilterClass::Run(
		ID3D12GraphicsCommandList*const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
		D3D12_GPU_DESCRIPTOR_HANDLE normalSrv,
		D3D12_GPU_DESCRIPTOR_HANDLE depthSrv,
		GpuResource* const primary,
		GpuResource* const secondary,
		D3D12_CPU_DESCRIPTOR_HANDLE primaryRtv,
		D3D12_GPU_DESCRIPTOR_HANDLE primarySrv,
		D3D12_CPU_DESCRIPTOR_HANDLE secondaryRtv,
		D3D12_GPU_DESCRIPTOR_HANDLE secondarySrv,
		FilterType type,
		size_t blurCount) {
	cmdList->SetPipelineState(mPSOs[type].Get());
	cmdList->SetGraphicsRootSignature(mRootSignature.Get());

	cmdList->SetGraphicsRootConstantBufferView(RootSignatureLayout::ECB_BlurPass, cbAddress);
	cmdList->SetGraphicsRoot32BitConstant(RootSignatureLayout::EC_Consts, 1, RootConstantsLayout::EBilateral);

	cmdList->SetGraphicsRootDescriptorTable(RootSignatureLayout::ESI_Normal, normalSrv);
	cmdList->SetGraphicsRootDescriptorTable(RootSignatureLayout::ESI_Depth, depthSrv);
	
	for (INT i = 0; i < blurCount; ++i) {
		cmdList->SetGraphicsRoot32BitConstant(RootSignatureLayout::EC_Consts, 1, RootConstantsLayout::EHorizontal);
		Blur(cmdList, secondary, secondaryRtv, primarySrv, type, true);

		cmdList->SetGraphicsRoot32BitConstant(RootSignatureLayout::EC_Consts, 0, RootConstantsLayout::EHorizontal);
		Blur(cmdList, primary, primaryRtv, secondarySrv, type, false);
	}
}

void BlurFilterClass::Blur(
		ID3D12GraphicsCommandList* cmdList,
		GpuResource* const output,
		D3D12_CPU_DESCRIPTOR_HANDLE outputRtv,
		D3D12_GPU_DESCRIPTOR_HANDLE inputSrv,
		FilterType type,
		BOOL horzBlur) {
	output->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->OMSetRenderTargets(1, &outputRtv, true, nullptr);

	switch (type)
	{
	case BlurFilter::R8G8B8A8:
		cmdList->SetGraphicsRootDescriptorTable(RootSignatureLayout::ESI_Input_F4, inputSrv);
		break;
	case BlurFilter::R16:
		cmdList->SetGraphicsRootDescriptorTable(RootSignatureLayout::ESI_Input_F1, inputSrv);
		break;
	case BlurFilter::R16G16B16A16:
		cmdList->SetGraphicsRootDescriptorTable(RootSignatureLayout::ESI_Input_F4, inputSrv);
		break;
	case BlurFilter::R32G32B32A32:
		cmdList->SetGraphicsRootDescriptorTable(RootSignatureLayout::ESI_Input_F4, inputSrv);
		break;
	case BlurFilter::Count:
		break;
	default:
		break;
	}

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	output->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}