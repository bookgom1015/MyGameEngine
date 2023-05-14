#include "Ssr.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "D3D12Util.h"

using namespace Ssr;

bool SsrClass::Initialize(ID3D12Device* device, ShaderManager*const manager, UINT width, UINT height, UINT divider, DXGI_FORMAT backBufferFormat) {
	md3dDevice = device;
	mShaderManager = manager;

	mSsrMapWidth = width / divider;
	mSsrMapHeight = height / divider;

	mResultMapWidth = width;
	mResultMapHeight = height;

	mDivider = divider;

	mBackBufferFormat = backBufferFormat;

	mViewport = { 0.0f, 0.0f, static_cast<float>(mSsrMapWidth), static_cast<float>(mSsrMapHeight), 0.0f, 1.0f };
	mScissorRect = { 0, 0, static_cast<int>(mSsrMapWidth), static_cast<int>(mSsrMapHeight) };

	CheckReturn(BuildResource());

	return true;
}

bool SsrClass::CompileShaders(const std::wstring& filePath) {
	{
		const std::wstring actualPath = filePath + L"BuildingSsr.hlsl";
		auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
		auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, "BuildingSsrVS"));
		CheckReturn(mShaderManager->CompileShader(psInfo, "BuildingSsrPS"));
	}
	{
		const std::wstring actualPath = filePath + L"ApplyingSsr.hlsl";
		auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
		auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, "ApplyingSsrVS"));
		CheckReturn(mShaderManager->CompileShader(psInfo, "ApplyingSsrPS"));
	}

	return true;
}

bool SsrClass::BuildRootSignature(const StaticSamplers& samplers) {
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[Building::RootSignatureLayout::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[4];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
		texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
		texTables[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);
		texTables[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0);

		CD3DX12_DESCRIPTOR_RANGE texTable1;

		slotRootParameter[Building::RootSignatureLayout::ECB_Ssr].InitAsConstantBufferView(0);
		slotRootParameter[Building::RootSignatureLayout::ESI_BackBuffer].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[Building::RootSignatureLayout::ESI_Normal].InitAsDescriptorTable(1, &texTables[1]);
		slotRootParameter[Building::RootSignatureLayout::ESI_Depth].InitAsDescriptorTable(1, &texTables[2]);
		slotRootParameter[Building::RootSignatureLayout::ESI_Spec].InitAsDescriptorTable(1, &texTables[3]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures["buildingSsr"]));
	}
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[Applying::RootSignatureLayout::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[6];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
		texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
		texTables[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);
		texTables[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0);
		texTables[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 0);
		texTables[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5, 0);

		slotRootParameter[Applying::RootSignatureLayout::ECB_Ssr].InitAsConstantBufferView(0);
		slotRootParameter[Applying::RootSignatureLayout::ESI_Cube].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[Applying::RootSignatureLayout::ESI_BackBuffer].InitAsDescriptorTable(1, &texTables[1]);
		slotRootParameter[Applying::RootSignatureLayout::ESI_Normal].InitAsDescriptorTable(1, &texTables[2]);
		slotRootParameter[Applying::RootSignatureLayout::ESI_Depth].InitAsDescriptorTable(1, &texTables[3]);
		slotRootParameter[Applying::RootSignatureLayout::ESI_Spec].InitAsDescriptorTable(1, &texTables[4]);
		slotRootParameter[Applying::RootSignatureLayout::ESI_Ssr].InitAsDescriptorTable(1, &texTables[5]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures["applyingSsr"]));
	}

	return true;
}

bool SsrClass::BuildPso() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC quadPsoDesc = D3D12Util::QuadPsoDesc();

	D3D12_GRAPHICS_PIPELINE_STATE_DESC buildingSsrPsoDesc = quadPsoDesc;
	buildingSsrPsoDesc.pRootSignature = mRootSignatures["buildingSsr"].Get();
	{
		auto vs = mShaderManager->GetDxcShader("BuildingSsrVS");
		auto ps = mShaderManager->GetDxcShader("BuildingSsrPS");
		buildingSsrPsoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
		buildingSsrPsoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	buildingSsrPsoDesc.RTVFormats[0] = mBackBufferFormat;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&buildingSsrPsoDesc, IID_PPV_ARGS(&mPSOs["buildingSsr"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC applyingSsrPsoDesc = quadPsoDesc;
	applyingSsrPsoDesc.pRootSignature = mRootSignatures["applyingSsr"].Get();
	{
		auto vs = mShaderManager->GetDxcShader("ApplyingSsrVS");
		auto ps = mShaderManager->GetDxcShader("ApplyingSsrPS");
		applyingSsrPsoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
		applyingSsrPsoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	applyingSsrPsoDesc.RTVFormats[0] = mBackBufferFormat;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&applyingSsrPsoDesc, IID_PPV_ARGS(&mPSOs["applyingSsr"])));

	return true;
}

void SsrClass::BuildSsr(
		ID3D12GraphicsCommandList*const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
		D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer,
		D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
		D3D12_GPU_DESCRIPTOR_HANDLE si_depth, 
		D3D12_GPU_DESCRIPTOR_HANDLE si_spec) {
	cmdList->SetPipelineState(mPSOs["buildingSsr"].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures["buildingSsr"].Get());
	
	cmdList->RSSetViewports(1, &mViewport);
	cmdList->RSSetScissorRects(1, &mScissorRect);

	const auto rtv = mhSsrMapCpuRtvs[0];
	cmdList->ClearRenderTargetView(rtv, Ssr::ClearValues, 0, nullptr);
	cmdList->OMSetRenderTargets(1, &rtv, true, nullptr);

	cmdList->SetGraphicsRootConstantBufferView(Building::RootSignatureLayout::ECB_Ssr, cbAddress);

	cmdList->SetGraphicsRootDescriptorTable(Building::RootSignatureLayout::ESI_BackBuffer, si_backBuffer);

	cmdList->SetGraphicsRootDescriptorTable(Building::RootSignatureLayout::ESI_Normal, si_normal);
	cmdList->SetGraphicsRootDescriptorTable(Building::RootSignatureLayout::ESI_Depth, si_depth);
	cmdList->SetGraphicsRootDescriptorTable(Building::RootSignatureLayout::ESI_Spec, si_spec);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);
}

void SsrClass::ApplySsr(
		ID3D12GraphicsCommandList*const cmdList,
		D3D12_VIEWPORT viewport,
		D3D12_RECT scissorRect,
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
		D3D12_GPU_DESCRIPTOR_HANDLE si_cube,
		D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer,
		D3D12_GPU_DESCRIPTOR_HANDLE si_normal, 
		D3D12_GPU_DESCRIPTOR_HANDLE si_depth, 
		D3D12_GPU_DESCRIPTOR_HANDLE si_spec) {
	cmdList->SetPipelineState(mPSOs["applyingSsr"].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures["applyingSsr"].Get());

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	cmdList->ClearRenderTargetView(mhResultMapCpuRtv, Ssr::ClearValues, 0, nullptr);
	cmdList->OMSetRenderTargets(1, &mhResultMapCpuRtv, true, nullptr);

	cmdList->SetGraphicsRootConstantBufferView(Applying::RootSignatureLayout::ECB_Ssr, cbAddress);

	cmdList->SetGraphicsRootDescriptorTable(Applying::RootSignatureLayout::ESI_Cube, si_cube);
	cmdList->SetGraphicsRootDescriptorTable(Applying::RootSignatureLayout::ESI_BackBuffer, si_backBuffer);

	cmdList->SetGraphicsRootDescriptorTable(Applying::RootSignatureLayout::ESI_Normal, si_normal);
	cmdList->SetGraphicsRootDescriptorTable(Applying::RootSignatureLayout::ESI_Depth, si_depth);
	cmdList->SetGraphicsRootDescriptorTable(Applying::RootSignatureLayout::ESI_Spec, si_spec);

	cmdList->SetGraphicsRootDescriptorTable(Applying::RootSignatureLayout::ESI_Ssr, mhSsrMapGpuSrvs[0]);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);
}

void SsrClass::BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
		UINT descSize, UINT rtvDescSize) {
	mhSsrMapCpuSrvs[0] = hCpu;
	mhSsrMapGpuSrvs[0] = hGpu;
	mhSsrMapCpuRtvs[0] = hCpuRtv;

	mhSsrMapCpuSrvs[1] = hCpu.Offset(1, descSize);
	mhSsrMapGpuSrvs[1] = hGpu.Offset(1, descSize);
	mhSsrMapCpuRtvs[1] = hCpuRtv.Offset(1, rtvDescSize);

	mhResultMapCpuRtv = hCpuRtv.Offset(1, rtvDescSize);

	hCpu.Offset(1, descSize);
	hGpu.Offset(1, descSize);
	hCpuRtv.Offset(1, rtvDescSize);

	BuildDescriptors();
}

bool SsrClass::OnResize(UINT width, UINT height) {
	if ((mResultMapWidth != width) || (mResultMapHeight != height)) {
		mSsrMapWidth = width / mDivider;
		mSsrMapHeight = height / mDivider;

		mResultMapWidth = width;
		mResultMapHeight = height;

		mViewport = { 0.0f, 0.0f, static_cast<float>(mSsrMapWidth), static_cast<float>(mSsrMapHeight), 0.0f, 1.0f };
		mScissorRect = { 0, 0, static_cast<int>(mSsrMapWidth), static_cast<int>(mSsrMapHeight) };

		CheckReturn(BuildResource());
		BuildDescriptors();
	}

	return true;
}

void SsrClass::BuildDescriptors() {
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
		md3dDevice->CreateShaderResourceView(mSsrMaps[i].Get(), &srvDesc, mhSsrMapCpuSrvs[i]);
		md3dDevice->CreateRenderTargetView(mSsrMaps[i].Get(), &rtvDesc, mhSsrMapCpuRtvs[i]);
	}

	md3dDevice->CreateRenderTargetView(mResultMap.Get(), &rtvDesc, mhResultMapCpuRtv);
}

bool SsrClass::BuildResource() {
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

	rscDesc.Width = mSsrMapWidth;
	rscDesc.Height = mSsrMapHeight;
	for (int i = 0; i < 2; ++i) {
		CheckHRESULT(md3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&rscDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&optClear,
			IID_PPV_ARGS(&mSsrMaps[i])
		));
		std::wstringstream wsstream;
		wsstream << "SsrMap_" << i;
		mSsrMaps[i]->SetName(wsstream.str().c_str());
	}

	rscDesc.Width = mResultMapWidth;
	rscDesc.Height = mResultMapHeight;
	CheckHRESULT(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&rscDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&optClear,
		IID_PPV_ARGS(&mResultMap)
	));
	mResultMap->SetName(L"SsrResultMap");

	return true;
}