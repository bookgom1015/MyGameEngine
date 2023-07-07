#include "BackBuffer.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "D3D12Util.h"

using namespace BackBuffer;

bool BackBufferClass::Initialize(ID3D12Device* device, ShaderManager*const manager, UINT width, UINT height,
		DXGI_FORMAT backBufferFormat, UINT bufferCount) {
	md3dDevice = device;
	mShaderManager = manager;

	mWidth = width;
	mHeight = height;

	mBackBufferFormat = backBufferFormat;
	mBackBufferCount = bufferCount;

	mhBackBufferCpuSrvs.resize(bufferCount);
	mhBackBufferGpuSrvs.resize(bufferCount);

	return true;
}

bool BackBufferClass::CompileShaders(const std::wstring& filePath) {
	const std::wstring actualPath = filePath + L"DrawBackBuffer.hlsl";
	auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
	auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
	CheckReturn(mShaderManager->CompileShader(vsInfo, "DrawBackBufferVS"));
	CheckReturn(mShaderManager->CompileShader(psInfo, "DrawBackBufferPS"));

	return true;
}

bool BackBufferClass::BuildRootSignature(const StaticSamplers& samplers) {
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
	slotRootParameter[RootSignatureLayout::ESI_AOCoefficient].InitAsDescriptorTable(1, &texTables[6]);

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignature));

	return true;
}

bool BackBufferClass::BuildPso() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::QuadPsoDesc();
	psoDesc.pRootSignature = mRootSignature.Get();
	{
		auto vs = mShaderManager->GetDxcShader("DrawBackBufferVS");
		auto ps = mShaderManager->GetDxcShader("DrawBackBufferPS");
		psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
		psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	psoDesc.RTVFormats[0] = mBackBufferFormat;

	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));

	return true;
}

void BackBufferClass::Run(
		ID3D12GraphicsCommandList*const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
		D3D12_GPU_DESCRIPTOR_HANDLE si_color,
		D3D12_GPU_DESCRIPTOR_HANDLE si_albedo,
		D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
		D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
		D3D12_GPU_DESCRIPTOR_HANDLE si_specular,
		D3D12_GPU_DESCRIPTOR_HANDLE si_shadow,
		D3D12_GPU_DESCRIPTOR_HANDLE si_aoCoefficient) {
	cmdList->SetPipelineState(mPSO.Get());
	cmdList->SetGraphicsRootSignature(mRootSignature.Get());

	cmdList->SetGraphicsRootConstantBufferView(RootSignatureLayout::ECB_Pass, cbAddress);

	cmdList->SetGraphicsRootDescriptorTable(RootSignatureLayout::ESI_Color, si_color);
	cmdList->SetGraphicsRootDescriptorTable(RootSignatureLayout::ESI_Albedo, si_albedo);
	cmdList->SetGraphicsRootDescriptorTable(RootSignatureLayout::ESI_Normal, si_normal);
	cmdList->SetGraphicsRootDescriptorTable(RootSignatureLayout::ESI_Depth, si_depth);
	cmdList->SetGraphicsRootDescriptorTable(RootSignatureLayout::ESI_Specular, si_specular);
	cmdList->SetGraphicsRootDescriptorTable(RootSignatureLayout::ESI_Shadow, si_shadow);
	cmdList->SetGraphicsRootDescriptorTable(RootSignatureLayout::ESI_AOCoefficient, si_aoCoefficient);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);
}

void BackBufferClass::BuildDescriptors(
		ID3D12Resource*const buffers[],
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpuSrv,
		UINT descSize) {
	for (UINT i = 0; i < mBackBufferCount; ++i) {
		mhBackBufferCpuSrvs[i] = hCpuSrv;
		mhBackBufferGpuSrvs[i] = hGpuSrv;

		hCpuSrv.Offset(1, descSize);
		hGpuSrv.Offset(1, descSize);
	}

	BuildDescriptors(buffers);
}

bool BackBufferClass::OnResize(ID3D12Resource*const buffers[], UINT width, UINT height) {
	if ((mWidth != width) || (mHeight != height)) {
		mWidth = width;
		mHeight = height;

		BuildDescriptors(buffers);
	}

	return true;
}

void BackBufferClass::BuildDescriptors(ID3D12Resource*const buffers[]) {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = mBackBufferFormat;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.MipLevels = 1;

	for (UINT i = 0; i < mBackBufferCount; ++i) {
		md3dDevice->CreateShaderResourceView(buffers[i], &srvDesc, mhBackBufferCpuSrvs[i]);
	}
}