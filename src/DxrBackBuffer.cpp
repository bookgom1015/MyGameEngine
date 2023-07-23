#include "DxrBackBuffer.h"
#include "Logger.h"
#include "BackBuffer.h"
#include "ShaderManager.h"
#include "D3D12Util.h"

#include <DirectXColors.h>

using namespace DxrBackBuffer;

namespace {
	std::string VS = "DxrBackBufferVS";
	std::string PS = "DxrBackBufferPS";
}

bool DxrBackBufferClass::Initialize(BackBuffer::BackBufferClass* const backBuffer) {
	mBackBuffer = backBuffer;

	return true;
}

bool DxrBackBufferClass::CompileShaders(const std::wstring& filePath) {
	const auto fullPath = filePath + L"DxrBackBuffer.hlsl";
	auto vsInfo = D3D12ShaderInfo(fullPath.c_str(), L"VS", L"vs_6_3");
	auto psInfo = D3D12ShaderInfo(fullPath.c_str(), L"PS", L"ps_6_3");
	const auto shaderManager = mBackBuffer->mShaderManager;
	CheckReturn(shaderManager->CompileShader(vsInfo, VS));
	CheckReturn(shaderManager->CompileShader(psInfo, PS));

	return true;
}

bool DxrBackBufferClass::BuildRootSignature(const StaticSamplers& samplers) {
	CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignatureLayout::Count];
	
	CD3DX12_DESCRIPTOR_RANGE texTables[7];
	texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
	texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
	texTables[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);
	texTables[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0);
	texTables[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 0);
	texTables[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5, 0);
	texTables[6].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 6, 0);
	
	slotRootParameter[RootSignatureLayout::ECB_Pass].InitAsConstantBufferView(0);
	slotRootParameter[RootSignatureLayout::ESI_Color].InitAsDescriptorTable(1, &texTables[0]);
	slotRootParameter[RootSignatureLayout::ESI_Albedo].InitAsDescriptorTable(1, &texTables[1]);
	slotRootParameter[RootSignatureLayout::ESI_Normal].InitAsDescriptorTable(1, &texTables[2]);
	slotRootParameter[RootSignatureLayout::ESI_Depth].InitAsDescriptorTable(1, &texTables[3]);
	slotRootParameter[RootSignatureLayout::ESI_Specular].InitAsDescriptorTable(1, &texTables[4]);
	slotRootParameter[RootSignatureLayout::ESI_Shadow].InitAsDescriptorTable(1, &texTables[5]);
	slotRootParameter[RootSignatureLayout::ESI_AOCoeff].InitAsDescriptorTable(1, &texTables[6]);
	
	CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_NONE
	);
	CheckReturn(D3D12Util::CreateRootSignature(mBackBuffer->md3dDevice, globalRootSignatureDesc, &mRootSignature));

	return true;
}

bool DxrBackBufferClass::BuildPso() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::QuadPsoDesc();
	psoDesc.pRootSignature = mRootSignature.Get();
	{
		const auto shaderManager = mBackBuffer->mShaderManager;
		auto vs = shaderManager->GetDxcShader(VS);
		auto ps = shaderManager->GetDxcShader(PS);
		psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
		psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	psoDesc.RTVFormats[0] = mBackBuffer->mBackBufferFormat;
	CheckHRESULT(mBackBuffer->md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));

	return true;
}

void DxrBackBufferClass::Run(
		ID3D12GraphicsCommandList* const cmdList,
		ID3D12Resource* backBuffer,
		D3D12_CPU_DESCRIPTOR_HANDLE ri_backBuffer,
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
		D3D12_GPU_DESCRIPTOR_HANDLE si_color,
		D3D12_GPU_DESCRIPTOR_HANDLE si_albedo,
		D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
		D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
		D3D12_GPU_DESCRIPTOR_HANDLE si_specular,
		D3D12_GPU_DESCRIPTOR_HANDLE si_shadow,
		D3D12_GPU_DESCRIPTOR_HANDLE si_aoceoff) {
	cmdList->SetPipelineState(mPSO.Get());
	cmdList->SetGraphicsRootSignature(mRootSignature.Get());

	cmdList->RSSetViewports(1, &mBackBuffer->mViewport);
	cmdList->RSSetScissorRects(1, &mBackBuffer->mScissorRect);

	cmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			backBuffer,
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		)
	);

	cmdList->ClearRenderTargetView(ri_backBuffer, DirectX::Colors::AliceBlue, 0, nullptr);
	cmdList->OMSetRenderTargets(1, &ri_backBuffer, true, nullptr);

	cmdList->SetGraphicsRootConstantBufferView(DxrBackBuffer::RootSignatureLayout::ECB_Pass, cbAddress);

	cmdList->SetGraphicsRootDescriptorTable(DxrBackBuffer::RootSignatureLayout::ESI_Color, si_color);
	cmdList->SetGraphicsRootDescriptorTable(DxrBackBuffer::RootSignatureLayout::ESI_Albedo, si_albedo);
	cmdList->SetGraphicsRootDescriptorTable(DxrBackBuffer::RootSignatureLayout::ESI_Normal, si_normal);
	cmdList->SetGraphicsRootDescriptorTable(DxrBackBuffer::RootSignatureLayout::ESI_Depth, si_depth);
	cmdList->SetGraphicsRootDescriptorTable(DxrBackBuffer::RootSignatureLayout::ESI_Specular, si_specular);
	cmdList->SetGraphicsRootDescriptorTable(DxrBackBuffer::RootSignatureLayout::ESI_Shadow, si_shadow);
	cmdList->SetGraphicsRootDescriptorTable(DxrBackBuffer::RootSignatureLayout::ESI_AOCoeff, si_aoceoff);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	cmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			backBuffer,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT
		)
	);
}