#include "Bloom.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "D3D12Util.h"

using namespace Bloom;

bool BloomClass::Initialize(ID3D12Device* device, ShaderManager*const manager, UINT width, UINT height, UINT divider, DXGI_FORMAT backBufferFormat) {
	md3dDevice = device;
	mShaderManager = manager;

	mBloomMapWidth = width / divider;
	mBloomMapHeight = height / divider;

	mResultMapWidth = width;
	mResultMapHeight = height;

	mDivider = divider;

	mBackBufferFormat = backBufferFormat;

	mViewport = { 0.0f, 0.0f, static_cast<float>(mBloomMapWidth), static_cast<float>(mBloomMapHeight), 0.0f, 1.0f };
	mScissorRect = { 0, 0, static_cast<int>(mBloomMapWidth), static_cast<int>(mBloomMapHeight) };

	CheckReturn(BuildResource());

	return true;
}

bool BloomClass::CompileShaders(const std::wstring& filePath) {
	{
		const std::wstring actualPath = filePath + L"Bloom.hlsl";
		auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
		auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, "BloomVS"));
		CheckReturn(mShaderManager->CompileShader(psInfo, "BloomPS"));
	}
	{
		const std::wstring actualPath = filePath + L"ExtractHighlights.hlsl";
		auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
		auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, "ExtHlightsVS"));
		CheckReturn(mShaderManager->CompileShader(psInfo, "ExtHlightsPS"));
	}

	return true;
}

bool BloomClass::BuildRootSignature(const StaticSamplers& samplers) {
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[ExtractHighlights::RootSignatureLayout::Count];

		CD3DX12_DESCRIPTOR_RANGE texTable0;
		texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

		slotRootParameter[ExtractHighlights::RootSignatureLayout::ESI_BackBuffer].InitAsDescriptorTable(1, &texTable0);
		slotRootParameter[ExtractHighlights::RootSignatureLayout::EC_Consts].InitAsConstants(ExtractHighlights::RootConstatLayout::Count, 0);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures["extHighlights"]));
	}
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[ApplyBloom::RootSignatureLayout::Count];

		CD3DX12_DESCRIPTOR_RANGE texTable0;
		texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

		CD3DX12_DESCRIPTOR_RANGE texTable1;
		texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);

		slotRootParameter[ApplyBloom::RootSignatureLayout::ESI_BackBuffer].InitAsDescriptorTable(1, &texTable0);
		slotRootParameter[ApplyBloom::RootSignatureLayout::ESI_Bloom].InitAsDescriptorTable(1, &texTable1);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures["bloom"]));
	}

	return true;
}

bool BloomClass::BuildPso(D3D12_INPUT_LAYOUT_DESC inputLayout) {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC quadPsoDesc = D3D12Util::QuadPsoDesc();

	D3D12_GRAPHICS_PIPELINE_STATE_DESC extHlightsPsoDesc = quadPsoDesc;
	extHlightsPsoDesc.pRootSignature = mRootSignatures["extHlights"].Get();
	{
		auto vs = mShaderManager->GetDxcShader("ExtHlightsVS");
		auto ps = mShaderManager->GetDxcShader("ExtHlightsPS");
		extHlightsPsoDesc.VS = {
			reinterpret_cast<BYTE*>(vs->GetBufferPointer()),
			vs->GetBufferSize()
		};
		extHlightsPsoDesc.PS = {
			reinterpret_cast<BYTE*>(ps->GetBufferPointer()),
			ps->GetBufferSize()
		};
	}
	extHlightsPsoDesc.RTVFormats[0] = mBackBufferFormat;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&extHlightsPsoDesc, IID_PPV_ARGS(&mPSOs["extHighlights"])));
	
	D3D12_GRAPHICS_PIPELINE_STATE_DESC bloomPsoDesc = quadPsoDesc;
	bloomPsoDesc.pRootSignature = mRootSignatures["bloom"].Get();
	{
		auto vs = mShaderManager->GetDxcShader("BloomVS");
		auto ps = mShaderManager->GetDxcShader("BloomPS");
		bloomPsoDesc.VS = {
			reinterpret_cast<BYTE*>(vs->GetBufferPointer()),
			vs->GetBufferSize()
		};
		bloomPsoDesc.PS = {
			reinterpret_cast<BYTE*>(ps->GetBufferPointer()),
			ps->GetBufferSize()
		};
	}
	bloomPsoDesc.RTVFormats[0] = mBackBufferFormat;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&bloomPsoDesc, IID_PPV_ARGS(&mPSOs["bloom"])));

	return true;
}

void BloomClass::ExtractHighlights(
		ID3D12GraphicsCommandList*const cmdList,
		D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer,
		float threshold) {
	cmdList->SetPipelineState(mPSOs["extHighlights"].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures["extHighlights"].Get());

	cmdList->RSSetViewports(1, &mViewport);
	cmdList->RSSetScissorRects(1, &mScissorRect);

	auto bloomMapRtv = mhBloomMapCpuRtvs[0];
	cmdList->ClearRenderTargetView(bloomMapRtv, ClearValues, 0, nullptr);
	cmdList->OMSetRenderTargets(1, &bloomMapRtv, true, nullptr);

	cmdList->SetGraphicsRootDescriptorTable(ExtractHighlights::RootSignatureLayout::ESI_BackBuffer, si_backBuffer);

	float values[ExtractHighlights::RootConstatLayout::Count] = { threshold };
	cmdList->SetGraphicsRoot32BitConstants(ExtractHighlights::RootSignatureLayout::EC_Consts, _countof(values), values, 0);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);
}

void BloomClass::Bloom(
		ID3D12GraphicsCommandList*const cmdList,
		D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer) {
	cmdList->SetPipelineState(mPSOs["bloom"].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures["bloom"].Get());

	cmdList->RSSetViewports(1, &mViewport);
	cmdList->RSSetScissorRects(1, &mScissorRect);

	cmdList->ClearRenderTargetView(mhResultMapCpuRtv, Bloom::ClearValues, 0, nullptr);
	cmdList->OMSetRenderTargets(1, &mhResultMapCpuRtv, true, nullptr);

	cmdList->SetGraphicsRootDescriptorTable(ApplyBloom::RootSignatureLayout::ESI_BackBuffer, si_backBuffer);
	cmdList->SetGraphicsRootDescriptorTable(ApplyBloom::RootSignatureLayout::ESI_Bloom, mhBloomMapGpuSrvs[0]);
	 
	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);
}

void BloomClass::BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
		UINT descSize, UINT rtvDescSize) {
	mhBloomMapCpuSrvs[0] = hCpu;
	mhBloomMapGpuSrvs[0] = hGpu;
	mhBloomMapCpuRtvs[0] = hCpuRtv;

	mhBloomMapCpuSrvs[1] = hCpu.Offset(1, descSize);
	mhBloomMapGpuSrvs[1] = hGpu.Offset(1, descSize);
	mhBloomMapCpuRtvs[1] = hCpuRtv.Offset(1, rtvDescSize);

	mhResultMapCpuRtv = hCpuRtv.Offset(1, rtvDescSize);

	hCpu.Offset(1, descSize);
	hGpu.Offset(1, descSize);
	hCpuRtv.Offset(1, rtvDescSize);

	BuildDescriptors();
}

bool BloomClass::OnResize(UINT width, UINT height) {
	if ((mResultMapWidth != width) || (mResultMapHeight != height)) {
		mBloomMapWidth = width / mDivider;
		mBloomMapHeight = height / mDivider;

		mResultMapWidth = width;
		mResultMapHeight = height;

		mViewport = { 0.0f, 0.0f, static_cast<float>(mBloomMapWidth), static_cast<float>(mBloomMapHeight), 0.0f, 1.0f };
		mScissorRect = { 0, 0, static_cast<int>(mBloomMapWidth), static_cast<int>(mBloomMapHeight) };

		CheckReturn(BuildResource());
		BuildDescriptors();
	}

	return true;
}

void BloomClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = mBackBufferFormat;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = mBackBufferFormat;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;

	for (int i = 0; i < 2; ++i) {
		auto bloomMap = mBloomMaps[i].Get();
		md3dDevice->CreateShaderResourceView(bloomMap, &srvDesc, mhBloomMapCpuSrvs[i]);
		md3dDevice->CreateRenderTargetView(bloomMap, &rtvDesc, mhBloomMapCpuRtvs[i]);
	}

	md3dDevice->CreateRenderTargetView(mResultMap.Get(), &rtvDesc, mhResultMapCpuRtv);
}

bool BloomClass::BuildResource() {
	D3D12_RESOURCE_DESC rscDesc = {};
	rscDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rscDesc.Alignment = 0;
	rscDesc.DepthOrArraySize = 1;
	rscDesc.MipLevels = 1;
	rscDesc.SampleDesc.Count = 1;
	rscDesc.SampleDesc.Quality = 0;
	rscDesc.Format = mBackBufferFormat;
	rscDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	rscDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	CD3DX12_CLEAR_VALUE optClear(mBackBufferFormat, ClearValues);
	
	rscDesc.Width = mBloomMapWidth;
	rscDesc.Height = mBloomMapHeight;
	for (int i = 0; i < 2; ++i) {
		CheckHRESULT(md3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&rscDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&optClear,
			IID_PPV_ARGS(&mBloomMaps[i])
		));
		std::wstringstream wsstream;
		wsstream << "BloomMap_" << i;
		mBloomMaps[i]->SetName(wsstream.str().c_str());
	}
	

	rscDesc.Width = mResultMapWidth;
	rscDesc.Height = mResultMapHeight;
	CheckHRESULT(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&rscDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&optClear,
		IID_PPV_ARGS(mResultMap.GetAddressOf())
	));
	mResultMap->SetName(L"BloomResultMap");

	return true;
}