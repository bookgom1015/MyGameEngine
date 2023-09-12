#include "BRDF.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "D3D12Util.h"
#include "GpuResource.h"

#include <DirectXColors.h>

using namespace BRDF;

namespace {
	const std::string BlinnPhongVS = "BlinnPhongVS";
	const std::string BlinnPhongPS = "BlinnPhongPS";

	const std::string DxrBlinnPhongVS = "DxrBlinnPhongVS";
	const std::string DxrBlinnPhongPS = "DxrBlinnPhongPS";

	const std::string CookTorranceVS = "CookTorranceVS";
	const std::string CookTorrancePS = "CookTorrancePS";
}

bool BRDFClass::Initialize(ID3D12Device* device, ShaderManager*const manager) {
	md3dDevice = device;
	mShaderManager = manager;

	return true;
}

bool BRDFClass::CompileShaders(const std::wstring& filePath) {
	{
		const std::wstring fullPath = filePath + L"BlinnPhong.hlsl";
		auto vsInfo = D3D12ShaderInfo(fullPath.c_str(), L"VS", L"vs_6_3");
		auto psInfo = D3D12ShaderInfo(fullPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, BlinnPhongVS));
		CheckReturn(mShaderManager->CompileShader(psInfo, BlinnPhongPS));
	}
	{
		const auto fullPath = filePath + L"DxrBlinnPhong.hlsl";
		auto vsInfo = D3D12ShaderInfo(fullPath.c_str(), L"VS", L"vs_6_3");
		auto psInfo = D3D12ShaderInfo(fullPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, DxrBlinnPhongVS));
		CheckReturn(mShaderManager->CompileShader(psInfo, DxrBlinnPhongPS));
	}
	{
		const std::wstring fullPath = filePath + L"CookTorrance.hlsl";
		auto vsInfo = D3D12ShaderInfo(fullPath.c_str(), L"VS", L"vs_6_3");
		auto psInfo = D3D12ShaderInfo(fullPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, CookTorranceVS));
		CheckReturn(mShaderManager->CompileShader(psInfo, CookTorrancePS));
	}

	return true;
}

bool BRDFClass::BuildRootSignature(const StaticSamplers& samplers) {
	CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignatureLayout::Count];

	CD3DX12_DESCRIPTOR_RANGE texTables[6];
	texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
	texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
	texTables[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);
	texTables[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0);
	texTables[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 0);
	texTables[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5, 0);

	slotRootParameter[RootSignatureLayout::ECB_Pass].InitAsConstantBufferView(0);
	slotRootParameter[RootSignatureLayout::ESI_Albedo].InitAsDescriptorTable(1, &texTables[0]);
	slotRootParameter[RootSignatureLayout::ESI_Normal].InitAsDescriptorTable(1, &texTables[1]);
	slotRootParameter[RootSignatureLayout::ESI_Depth].InitAsDescriptorTable(1, &texTables[2]);
	slotRootParameter[RootSignatureLayout::ESI_RMS].InitAsDescriptorTable(1, &texTables[3]);
	slotRootParameter[RootSignatureLayout::ESI_Shadow].InitAsDescriptorTable(1, &texTables[4]);
	slotRootParameter[RootSignatureLayout::ESI_AOCoefficient].InitAsDescriptorTable(1, &texTables[5]);

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignature));

	return true;
}

bool BRDFClass::BuildPso() {
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::QuadPsoDesc();
		psoDesc.pRootSignature = mRootSignature.Get();
		{
			auto vs = mShaderManager->GetDxcShader(BlinnPhongVS);
			auto ps = mShaderManager->GetDxcShader(BlinnPhongPS);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = D3D12Util::HDRMapFormat;

		CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[Render::E_Raster][Model::Type::E_BlinnPhong])));
	}
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::QuadPsoDesc();
		psoDesc.pRootSignature = mRootSignature.Get();
		{
			auto vs = mShaderManager->GetDxcShader(DxrBlinnPhongVS);
			auto ps = mShaderManager->GetDxcShader(DxrBlinnPhongPS);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = D3D12Util::HDRMapFormat;

		CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[Render::E_Raytrace][Model::Type::E_BlinnPhong])));
	}

	return true;
}

void BRDFClass::BlinnPhong(
		ID3D12GraphicsCommandList*const cmdList,
		D3D12_VIEWPORT viewport,
		D3D12_RECT scissorRect,
		GpuResource* backBuffer,
		D3D12_CPU_DESCRIPTOR_HANDLE ri_backBuffer,
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
		D3D12_GPU_DESCRIPTOR_HANDLE si_albedo,
		D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
		D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
		D3D12_GPU_DESCRIPTOR_HANDLE si_rms,
		D3D12_GPU_DESCRIPTOR_HANDLE si_shadow,
		D3D12_GPU_DESCRIPTOR_HANDLE si_aoCoefficient,
		Render::Type renderType) {
	cmdList->SetPipelineState(mPSOs[renderType][Model::Type::E_BlinnPhong].Get());
	cmdList->SetGraphicsRootSignature(mRootSignature.Get());

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->OMSetRenderTargets(1, &ri_backBuffer, true, nullptr);

	cmdList->SetGraphicsRootConstantBufferView(RootSignatureLayout::ECB_Pass, cbAddress);

	cmdList->SetGraphicsRootDescriptorTable(RootSignatureLayout::ESI_Albedo, si_albedo);
	cmdList->SetGraphicsRootDescriptorTable(RootSignatureLayout::ESI_Normal, si_normal);
	cmdList->SetGraphicsRootDescriptorTable(RootSignatureLayout::ESI_Depth, si_depth);
	cmdList->SetGraphicsRootDescriptorTable(RootSignatureLayout::ESI_RMS, si_rms);
	cmdList->SetGraphicsRootDescriptorTable(RootSignatureLayout::ESI_Shadow, si_shadow);
	cmdList->SetGraphicsRootDescriptorTable(RootSignatureLayout::ESI_AOCoefficient, si_aoCoefficient);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PRESENT);
}