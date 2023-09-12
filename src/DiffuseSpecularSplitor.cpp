#include "DiffuseSpecularSplitor.h"
#include "Logger.h"
#include "GpuResource.h"
#include "ShaderManager.h"
#include "D3D12Util.h"

namespace {
	const std::string ReflectanceCompositionVS = "ReflectanceCompositionVS ";
	const std::string ReflectanceCompositionPS = "ReflectanceCompositionPS ";
}

using namespace DiffuseSpecularSplitor;

DiffuseSpecularSplitorClass::DiffuseSpecularSplitorClass() {
	mDiffuseMap = std::make_unique<GpuResource>();
	mSpecularMap = std::make_unique <GpuResource>();
	mReflectivityMap = std::make_unique<GpuResource>();
}

bool DiffuseSpecularSplitorClass::Initialize(ID3D12Device* device, ShaderManager* const manager, UINT width, UINT height) {
	md3dDevice = device;
	mShaderManager = manager;

	mWidth = width;
	mHeight = height;

	CheckReturn(BuildResources());

	return true;
}

bool DiffuseSpecularSplitorClass::CompileShaders(const std::wstring& filePath) {
	const std::wstring fullPath = filePath + L"ReflectanceComposition.hlsl";
	auto vsInfo = D3D12ShaderInfo(fullPath.c_str(), L"VS", L"vs_6_3");
	auto psInfo = D3D12ShaderInfo(fullPath.c_str(), L"PS", L"ps_6_3");
	CheckReturn(mShaderManager->CompileShader(vsInfo, ReflectanceCompositionVS));
	CheckReturn(mShaderManager->CompileShader(psInfo, ReflectanceCompositionPS));

	return true;
}

bool DiffuseSpecularSplitorClass::BuildRootSignature(const StaticSamplers& samplers) {
	CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignatureLayout::Count];

	CD3DX12_DESCRIPTOR_RANGE texTables[2];
	texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
	texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);

	slotRootParameter[RootSignatureLayout::ESI_Diffuse].InitAsDescriptorTable(1, &texTables[0]);
	slotRootParameter[RootSignatureLayout::ESI_Specular].InitAsDescriptorTable(1, &texTables[1]);

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignature));

	return true;
}

bool DiffuseSpecularSplitorClass::BuildPso() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::QuadPsoDesc();
	psoDesc.pRootSignature = mRootSignature.Get();
	{
		auto vs = mShaderManager->GetDxcShader(ReflectanceCompositionVS);
		auto ps = mShaderManager->GetDxcShader(ReflectanceCompositionPS);
		psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
		psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	psoDesc.RTVFormats[0] = D3D12Util::HDRMapFormat;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));

	return true;
}

void DiffuseSpecularSplitorClass::Composite(
		ID3D12GraphicsCommandList* const cmdList,
		D3D12_VIEWPORT viewport,
		D3D12_RECT scissorRect,
		GpuResource* backBuffer,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer) {
	cmdList->SetPipelineState(mPSO.Get());
	cmdList->SetGraphicsRootSignature(mRootSignature.Get());

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->OMSetRenderTargets(1, &ro_backBuffer, true, nullptr);

	cmdList->SetGraphicsRootDescriptorTable(RootSignatureLayout::ESI_Diffuse, mhDiffuseMapGpuSrv);
	cmdList->SetGraphicsRootDescriptorTable(RootSignatureLayout::ESI_Specular, mhSpecularMapGpuSrv);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PRESENT);
}

void DiffuseSpecularSplitorClass::BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
		UINT descSize, UINT rtvDescSize) {
	mhDiffuseMapCpuSrv = hCpuSrv;
	mhDiffuseMapGpuSrv = hGpuSrv;
	mhDiffuseMapCpuRtv = hCpuRtv;

	mhSpecularMapCpuSrv = hCpuSrv.Offset(1, descSize);
	mhSpecularMapGpuSrv = hGpuSrv.Offset(1, descSize);
	mhSpecularMapCpuRtv = hCpuRtv.Offset(1, rtvDescSize);

	mhReflectivityMapCpuSrv = hCpuSrv.Offset(1, descSize);
	mhReflectivityMapGpuSrv = hGpuSrv.Offset(1, descSize);
	mhReflectivityMapCpuRtv = hCpuRtv.Offset(1, rtvDescSize);

	BuildDescriptors();

	hCpuSrv.Offset(1, descSize);
	hGpuSrv.Offset(1, descSize);
	hCpuRtv.Offset(1, rtvDescSize);
}

bool DiffuseSpecularSplitorClass::OnResize(UINT width, UINT height) {
	if ((mWidth != width) || (mHeight != height)) {
		mWidth = width;
		mHeight = height;

		CheckReturn(BuildResources());
		BuildDescriptors();
	}

	return true;
}

void DiffuseSpecularSplitorClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;

	{
		srvDesc.Format = D3D12Util::HDRMapFormat;
		rtvDesc.Format = D3D12Util::HDRMapFormat;

		md3dDevice->CreateShaderResourceView(mDiffuseMap->Resource(), &srvDesc, mhDiffuseMapCpuSrv);
		md3dDevice->CreateRenderTargetView(mDiffuseMap->Resource(), &rtvDesc, mhDiffuseMapCpuRtv);

		md3dDevice->CreateShaderResourceView(mSpecularMap->Resource(), &srvDesc, mhSpecularMapCpuSrv);
		md3dDevice->CreateRenderTargetView(mSpecularMap->Resource(), &rtvDesc, mhSpecularMapCpuRtv);
	}
	{
		srvDesc.Format = D3D12Util::SDRMapFormat;
		rtvDesc.Format = D3D12Util::SDRMapFormat;

		md3dDevice->CreateShaderResourceView(mReflectivityMap->Resource(), &srvDesc, mhReflectivityMapCpuSrv);
		md3dDevice->CreateRenderTargetView(mReflectivityMap->Resource(), &rtvDesc, mhReflectivityMapCpuRtv);
	}
}

bool DiffuseSpecularSplitorClass::BuildResources() {
	D3D12_RESOURCE_DESC rscDesc = {};
	rscDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rscDesc.Alignment = 0;
	rscDesc.Width = mWidth;
	rscDesc.Height = mHeight;
	rscDesc.DepthOrArraySize = 1;
	rscDesc.MipLevels = 1;
	rscDesc.SampleDesc.Count = 1;
	rscDesc.SampleDesc.Quality = 0;
	rscDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	rscDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	{
		rscDesc.Format = D3D12Util::HDRMapFormat;
		CheckReturn(mDiffuseMap->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&rscDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"DiffuseReflectanceMap"
		));
		CheckReturn(mSpecularMap->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&rscDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"SpecularReflectanceMap"
		));
	}
	{
		rscDesc.Format = D3D12Util::SDRMapFormat;
		CheckReturn(mReflectivityMap->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&rscDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"ReflectivityMap"
		));
	}

	return true;
}