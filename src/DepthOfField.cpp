#include "DepthOfField.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "D3D12Util.h"

using namespace DepthOfField;

bool DepthOfFieldClass::Initialize(
		ID3D12Device* device, 
		ShaderManager*const manager, 
		ID3D12GraphicsCommandList* cmdList, 
		UINT width, UINT height, UINT divider, 
		DXGI_FORMAT backBufferFormat) {
	md3dDevice = device;
	mShaderManager = manager;

	mWidth = width;
	mHeight = height;

	mDivider = divider;
	mReducedWidth = width / divider;
	mReducedHeight = height / divider;

	mBackBufferFormat = backBufferFormat;

	mViewport = { 0.0f, 0.0f, static_cast<float>(mReducedWidth), static_cast<float>(mReducedHeight), 0.0f, 1.0f };
	mScissorRect = { 0, 0, static_cast<int>(mReducedWidth), static_cast<int>(mReducedHeight) };

	CheckReturn(BuildResource(cmdList));

	return true;
}

bool DepthOfFieldClass::CompileShaders(const std::wstring& filePath) {
	{
		CheckReturn(mShaderManager->CompileShader(filePath, nullptr, "VS", "vs_5_1", "cocVS"));
		CheckReturn(mShaderManager->CompileShader(filePath, nullptr, "PS", "ps_5_1", "cocPS"));

		const std::wstring actualPath = filePath + L"Coc.hlsl";
		auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
		auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, "CocVS"));
		CheckReturn(mShaderManager->CompileShader(psInfo, "CocPS"));
	}
	{
		const std::wstring actualPath = filePath + L"DepthOfField.hlsl";
		auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
		auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, "DofVS"));
		CheckReturn(mShaderManager->CompileShader(psInfo, "DofPS"));
	}
	{
		const std::wstring actualPath = filePath + L"DofBlur.hlsl";
		auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
		auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, "DofBlurVS"));
		CheckReturn(mShaderManager->CompileShader(psInfo, "DofBlurPS"));
	}
	{
		const std::wstring actualPath = filePath + L"FocalDistance.hlsl";
		auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
		auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, "FdVS"));
		CheckReturn(mShaderManager->CompileShader(psInfo, "FdPS"));
	}

	return true;
}

bool DepthOfFieldClass::BuildRootSignature(const StaticSamplers& samplers) {
	// Circle of confusion
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[CircleOfConfusion::RootSignatureLayout::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[2];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
		texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

		slotRootParameter[CircleOfConfusion::RootSignatureLayout::ECB_Dof].InitAsConstantBufferView(0);
		slotRootParameter[CircleOfConfusion::RootSignatureLayout::ESI_Depth].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[CircleOfConfusion::RootSignatureLayout::EUI_FocalDist].InitAsDescriptorTable(1, &texTables[1]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures["coc"]));
	}
	// Bokeh
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[Bokeh::RootSignatureLayout::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[1];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

		slotRootParameter[Bokeh::RootSignatureLayout::ESI_Input].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[Bokeh::RootSignatureLayout::EC_Consts].InitAsConstants(Bokeh::RootConstantsLayout::Count, 0);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures["bokeh"]));
	}
	// Depth of field
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[ApplyingDof::RootSignatureLayout::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[2];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
		texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);

		slotRootParameter[ApplyingDof::RootSignatureLayout::EC_Consts].InitAsConstants(ApplyingDof::RootConstantLayout::Count, 0);
		slotRootParameter[ApplyingDof::RootSignatureLayout::ESI_BackBuffer].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[ApplyingDof::RootSignatureLayout::ESI_Coc].InitAsDescriptorTable(1, &texTables[1]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures["dof"]));
	}
	// DOF blur
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[DofBlur::RootSignatureLayout::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[2];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
		texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);

		slotRootParameter[DofBlur::RootSignatureLayout::ECB_Blur].InitAsConstantBufferView(0);
		slotRootParameter[DofBlur::RootSignatureLayout::EC_Consts].InitAsConstants(DofBlur::RootConstantLayout::Count, 1);
		slotRootParameter[DofBlur::RootSignatureLayout::ESI_Input].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[DofBlur::RootSignatureLayout::ESI_Coc].InitAsDescriptorTable(1, &texTables[1]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures["dofBlur"]));
	}
	// Focal distance
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[FocalDistance::RootSignatureLayout::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[2];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
		texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

		slotRootParameter[FocalDistance::RootSignatureLayout::ECB_Dof].InitAsConstantBufferView(0);
		slotRootParameter[FocalDistance::RootSignatureLayout::ESI_Depth].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[FocalDistance::RootSignatureLayout::EUO_FocalDist].InitAsDescriptorTable(1, &texTables[1]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures["fd"]));
	}

	return true;
}

bool DepthOfFieldClass::BuildPso(D3D12_INPUT_LAYOUT_DESC inputLayout, DXGI_FORMAT dsvFormat) {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC defaultPsoDesc = D3D12Util::DefaultPsoDesc(inputLayout, dsvFormat);
	D3D12_GRAPHICS_PIPELINE_STATE_DESC quadPsoDesc = D3D12Util::QuadPsoDesc();

	D3D12_GRAPHICS_PIPELINE_STATE_DESC cocPsoDesc = quadPsoDesc;
	cocPsoDesc.pRootSignature = mRootSignatures["coc"].Get();
	{
		auto vs = mShaderManager->GetDxcShader("CocVS");
		auto ps = mShaderManager->GetDxcShader("CocPS");
		cocPsoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
		cocPsoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	cocPsoDesc.RTVFormats[0] = CocMapFormat;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&cocPsoDesc, IID_PPV_ARGS(&mPSOs["coc"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC dofPsoDesc = quadPsoDesc;
	dofPsoDesc.pRootSignature = mRootSignatures["dof"].Get();
	{
		auto vs = mShaderManager->GetDxcShader("DofVS");
		auto ps = mShaderManager->GetDxcShader("DofPS");
		dofPsoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
		dofPsoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	dofPsoDesc.RTVFormats[0] = mBackBufferFormat;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&dofPsoDesc, IID_PPV_ARGS(&mPSOs["dof"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC dofBlurPsoDesc = dofPsoDesc;
	dofBlurPsoDesc.pRootSignature = mRootSignatures["dofBlur"].Get();
	{
		auto vs = mShaderManager->GetDxcShader("DofBlurVS");
		auto ps = mShaderManager->GetDxcShader("DofBlurPS");
		dofBlurPsoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
		dofBlurPsoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&dofBlurPsoDesc, IID_PPV_ARGS(&mPSOs["dofBlur"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC fdPsoDesc = defaultPsoDesc;
	fdPsoDesc.InputLayout = { nullptr, 0 };
	fdPsoDesc.pRootSignature = mRootSignatures["fd"].Get();
	{
		auto vs = mShaderManager->GetDxcShader("FdVS");
		auto ps = mShaderManager->GetDxcShader("FdPS");
		fdPsoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
		fdPsoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	fdPsoDesc.NumRenderTargets = 0;
	fdPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	fdPsoDesc.DepthStencilState.DepthEnable = false;
	fdPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	fdPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&fdPsoDesc, IID_PPV_ARGS(&mPSOs["fd"])));

	return true;
}

void DepthOfFieldClass::CalcFocalDist(
		ID3D12GraphicsCommandList*const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
		D3D12_GPU_DESCRIPTOR_HANDLE si_depth) {
	cmdList->SetPipelineState(mPSOs["fd"].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures["fd"].Get());

	cmdList->RSSetViewports(1, &mViewport);
	cmdList->RSSetScissorRects(1, &mScissorRect);

	cmdList->SetGraphicsRootConstantBufferView(FocalDistance::RootSignatureLayout::ECB_Dof, cbAddress);

	cmdList->SetGraphicsRootDescriptorTable(FocalDistance::RootSignatureLayout::ESI_Depth, si_depth);
	cmdList->SetGraphicsRootDescriptorTable(FocalDistance::RootSignatureLayout::EUO_FocalDist, mhFocalDistanceGpuUav);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
	cmdList->DrawInstanced(1, 1, 0, 0);
	D3D12Util::UavBarrier(cmdList, mFocalDistanceBuffer.Get());
}

void DepthOfFieldClass::CalcCoc(
		ID3D12GraphicsCommandList*const cmdList,
		D3D12_VIEWPORT viewport,
		D3D12_RECT scissorRect,
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
		D3D12_GPU_DESCRIPTOR_HANDLE si_depth) {
	cmdList->SetPipelineState(mPSOs["coc"].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures["coc"].Get());

	cmdList->ClearRenderTargetView(mhCocMapCpuRtv, CocMapClearValues, 0, nullptr);
	cmdList->OMSetRenderTargets(1, &mhCocMapCpuRtv, true, nullptr);

	cmdList->SetGraphicsRootConstantBufferView(CircleOfConfusion::RootSignatureLayout::ECB_Dof, cbAddress);

	cmdList->SetGraphicsRootDescriptorTable(CircleOfConfusion::RootSignatureLayout::ESI_Depth, si_depth);
	cmdList->SetGraphicsRootDescriptorTable(CircleOfConfusion::RootSignatureLayout::EUI_FocalDist, mhFocalDistanceGpuUav);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);
}

void DepthOfFieldClass::ApplyDof(
		ID3D12GraphicsCommandList*const cmdList,
		D3D12_VIEWPORT viewport,
		D3D12_RECT scissorRect,
		D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer,
		float bokehRadius,
		float cocThreshold,
		float cocDiffThreshold,
		float highlightPower,
		int numSamples) {
	cmdList->SetPipelineState(mPSOs["dof"].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures["dof"].Get());

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	cmdList->OMSetRenderTargets(1, &mhDofMapCpuRtvs[0], true, nullptr);

	float dofConstValues[ApplyingDof::RootConstantLayout::Count] = { bokehRadius, cocThreshold, cocDiffThreshold, highlightPower, static_cast<float>(numSamples) };
	cmdList->SetGraphicsRoot32BitConstants(ApplyingDof::RootSignatureLayout::EC_Consts, _countof(dofConstValues), dofConstValues, 0);

	cmdList->SetGraphicsRootDescriptorTable(ApplyingDof::RootSignatureLayout::ESI_BackBuffer, si_backBuffer);
	cmdList->SetGraphicsRootDescriptorTable(ApplyingDof::RootSignatureLayout::ESI_Coc, mhCocMapGpuSrv);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);
}

void DepthOfFieldClass::BlurDof(
		ID3D12GraphicsCommandList*const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
		UINT blurCount) {

	cmdList->SetPipelineState(mPSOs["dofBlur"].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures["dofBlur"].Get());

	cmdList->SetGraphicsRootConstantBufferView(DofBlur::RootSignatureLayout::ECB_Blur, cbAddress);
	cmdList->SetGraphicsRootDescriptorTable(DofBlur::RootSignatureLayout::ESI_Coc, mhCocMapGpuSrv);

	for (UINT i = 0; i < blurCount; ++i) {
		Blur(cmdList, true);
		Blur(cmdList, false);
	}
}

void DepthOfFieldClass::BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
		UINT descSize, UINT rtvDescSize) {
	mhCocMapCpuSrv = hCpu;
	mhCocMapGpuSrv = hGpu;
	mhCocMapCpuRtv = hCpuRtv;

	mhDofMapCpuSrvs[0] = hCpu.Offset(1, descSize);
	mhDofMapGpuSrvs[0] = hGpu.Offset(1, descSize);
	mhDofMapCpuRtvs[0] = hCpuRtv.Offset(1, rtvDescSize);

	mhDofMapCpuSrvs[1] = hCpu.Offset(1, descSize);
	mhDofMapGpuSrvs[1] = hGpu.Offset(1, descSize);
	mhDofMapCpuRtvs[1] = hCpuRtv.Offset(1, rtvDescSize);

	mhFocalDistanceCpuUav = hCpu.Offset(1, descSize);
	mhFocalDistanceGpuUav = hGpu.Offset(1, descSize);

	hCpu.Offset(1, descSize);
	hGpu.Offset(1, descSize);
	hCpuRtv.Offset(1, rtvDescSize);

	BuildDescriptors();
}

bool DepthOfFieldClass::OnResize(ID3D12GraphicsCommandList* cmdList, UINT width, UINT height) {
	if ((mWidth != width) || (mHeight != height)) {
		mWidth = width;
		mHeight = height;

		mReducedWidth = width / mDivider;
		mReducedHeight = height / mDivider;

		mViewport = { 0.0f, 0.0f, static_cast<float>(mReducedWidth), static_cast<float>(mReducedHeight), 0.0f, 1.0f };
		mScissorRect = { 0, 0, static_cast<int>(mReducedWidth), static_cast<int>(mReducedHeight) };

		CheckReturn(BuildResource(cmdList));
		BuildDescriptors();
	}

	return true;
}

void DepthOfFieldClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = CocMapFormat;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = 1;
	uavDesc.Buffer.StructureByteStride = sizeof(float);
	uavDesc.Buffer.CounterOffsetInBytes = 0;

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = CocMapFormat;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;

	md3dDevice->CreateShaderResourceView(mCocMap.Get(), &srvDesc, mhCocMapCpuSrv);
	md3dDevice->CreateRenderTargetView(mCocMap.Get(), &rtvDesc, mhCocMapCpuRtv);

	srvDesc.Format = mBackBufferFormat;
	rtvDesc.Format = mBackBufferFormat;	
	for (int i = 0; i < 2; ++i) {
		md3dDevice->CreateShaderResourceView(mDofMaps[i].Get(), &srvDesc, mhDofMapCpuSrvs[i]);
		md3dDevice->CreateRenderTargetView(mDofMaps[i].Get(), &rtvDesc, mhDofMapCpuRtvs[i]);
	}

	md3dDevice->CreateUnorderedAccessView(mFocalDistanceBuffer.Get(), nullptr, &uavDesc, mhFocalDistanceCpuUav);
}

bool DepthOfFieldClass::BuildResource(ID3D12GraphicsCommandList* cmdList) {
	D3D12_RESOURCE_DESC rscDesc = {};
	rscDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rscDesc.Alignment = 0;
	rscDesc.DepthOrArraySize = 1;
	rscDesc.MipLevels = 1;
	rscDesc.SampleDesc.Count = 1;
	rscDesc.SampleDesc.Quality = 0;
	rscDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	rscDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	{
		CD3DX12_CLEAR_VALUE optClear(CocMapFormat, CocMapClearValues);

		rscDesc.Width = mWidth;
		rscDesc.Height = mHeight;
		rscDesc.Format = CocMapFormat;
		CheckHRESULT(md3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&rscDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&optClear,
			IID_PPV_ARGS(&mCocMap)
		));
		mCocMap->SetName(L"CircleOfConfusionMap");
	}
	{
		CD3DX12_CLEAR_VALUE optClear(mBackBufferFormat, DofMapClearValues);

		rscDesc.Width = mWidth;
		rscDesc.Height = mHeight;
		rscDesc.Format = mBackBufferFormat;
		for (int i = 0; i < 2; ++i) {
			CheckHRESULT(md3dDevice->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&rscDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				&optClear,
				IID_PPV_ARGS(&mDofMaps[0])
			));
			std::wstringstream wsstream;
			wsstream << L"DepthOfFieldMap_" << i;
			mDofMaps[0]->SetName(wsstream.str().c_str());
		}
	}
	{
		CheckHRESULT(md3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(
				sizeof(float),
				D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
			),
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&mFocalDistanceBuffer)
		));
		mFocalDistanceBuffer->SetName(L"FocalDistanceMap");
	}

	const auto focalDistBuffer = mFocalDistanceBuffer.Get();
	cmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			focalDistBuffer,
			D3D12_RESOURCE_STATE_COMMON,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS
		)
	);
	D3D12Util::UavBarrier(cmdList, focalDistBuffer);

	return true;
}

void DepthOfFieldClass::Blur(ID3D12GraphicsCommandList*const cmdList, bool horzBlur) {
	ID3D12Resource* output = nullptr;
	CD3DX12_GPU_DESCRIPTOR_HANDLE inputSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE outputRtv;

	if (horzBlur == true) {
		output = mDofMaps[1].Get();
		outputRtv = mhDofMapCpuRtvs[1];
		inputSrv = mhDofMapGpuSrvs[0];
		cmdList->SetGraphicsRoot32BitConstant(DofBlur::RootSignatureLayout::EC_Consts, 1, DofBlur::RootConstantLayout::EHorizontalBlur);
	}
	else {
		output = mDofMaps[0].Get();
		outputRtv = mhDofMapCpuRtvs[0];
		inputSrv = mhDofMapGpuSrvs[1];
		cmdList->SetGraphicsRoot32BitConstant(DofBlur::RootSignatureLayout::EC_Consts, 0, DofBlur::RootConstantLayout::EHorizontalBlur);
	}

	cmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			output,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		)
	);

	cmdList->ClearRenderTargetView(outputRtv, DepthOfField::DofMapClearValues, 0, nullptr);

	cmdList->OMSetRenderTargets(1, &outputRtv, true, nullptr);

	cmdList->SetGraphicsRootDescriptorTable(DofBlur::RootSignatureLayout::ESI_Input, inputSrv);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	cmdList->ResourceBarrier(
		1, 
		&CD3DX12_RESOURCE_BARRIER::Transition(
			output, 
			D3D12_RESOURCE_STATE_RENDER_TARGET, 
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		)
	);
}