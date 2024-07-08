#include "DepthOfField.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "D3D12Util.h"
#include "Vertex.h"
#include "HlslCompaction.h"

using namespace DepthOfField;

namespace {
	const CHAR* VS_CoC = "VS_CoC";
	const CHAR* PS_CoC = "PS_CoC";

	const CHAR* VS_DoF = "VS_DoF";
	const CHAR* PS_DoF = "PS_DoF";

	const CHAR* VS_DoFBlur = "VS_DoFBlur";
	const CHAR* PS_DoFBlur = "PS_DoFBlur";

	const CHAR* VS_FD = "VS_FD";
	const CHAR* PS_FD = "PS_FD";
}

DepthOfFieldClass::DepthOfFieldClass() {
	mCoCMap = std::make_unique<GpuResource>();
	mCopiedBackBuffer = std::make_unique<GpuResource>();
	mFocalDistanceBuffer = std::make_unique<GpuResource>();
}

BOOL DepthOfFieldClass::Initialize(
		ID3D12Device* device, 
		ShaderManager*const manager, 
		ID3D12GraphicsCommandList* cmdList, 
		UINT width, UINT height) {
	md3dDevice = device;
	mShaderManager = manager;

	CheckReturn(BuildResources(cmdList, width, height));

	return TRUE;
}

BOOL DepthOfFieldClass::CompileShaders(const std::wstring& filePath) {
	{
		const std::wstring actualPath = filePath + L"CoC.hlsl";
		auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
		auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, VS_CoC));
		CheckReturn(mShaderManager->CompileShader(psInfo, PS_CoC));
	}
	{
		const std::wstring actualPath = filePath + L"DepthOfField.hlsl";
		auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
		auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, VS_DoF));
		CheckReturn(mShaderManager->CompileShader(psInfo, PS_DoF));
	}
	{
		const std::wstring actualPath = filePath + L"DoFBlur.hlsl";
		auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
		auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, VS_DoFBlur));
		CheckReturn(mShaderManager->CompileShader(psInfo, PS_DoFBlur));
	}
	{
		const std::wstring actualPath = filePath + L"FocalDistance.hlsl";
		auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
		auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, VS_FD));
		CheckReturn(mShaderManager->CompileShader(psInfo, PS_FD));
	}

	return TRUE;
}

BOOL DepthOfFieldClass::BuildRootSignature(const StaticSamplers& samplers) {
	// Circle of confusion
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::CircleOfConfusion::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[2];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
		texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

		slotRootParameter[RootSignature::CircleOfConfusion::ECB_DoF].InitAsConstantBufferView(0);
		slotRootParameter[RootSignature::CircleOfConfusion::ESI_Depth].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[RootSignature::CircleOfConfusion::EUI_FocalDist].InitAsDescriptorTable(1, &texTables[1]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures[RootSignature::E_CoC]));
	}
	// Depth of field
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::ApplyingDoF::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[2];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
		texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);

		slotRootParameter[RootSignature::ApplyingDoF::EC_Consts].InitAsConstants(RootSignature::ApplyingDoF::RootConstant::Count, 0);
		slotRootParameter[RootSignature::ApplyingDoF::ESI_BackBuffer].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[RootSignature::ApplyingDoF::ESI_CoC].InitAsDescriptorTable(1, &texTables[1]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures[RootSignature::E_DoF]));
	}
	// DoF blur
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::BlurringDoF::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[2];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
		texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);

		slotRootParameter[RootSignature::BlurringDoF::ECB_Blur].InitAsConstantBufferView(0);
		slotRootParameter[RootSignature::BlurringDoF::EC_Consts].InitAsConstants(RootSignature::BlurringDoF::RootConstant::Count, 1);
		slotRootParameter[RootSignature::BlurringDoF::ESI_Input].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[RootSignature::BlurringDoF::ESI_CoC].InitAsDescriptorTable(1, &texTables[1]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures[RootSignature::E_DoFBlur]));
	}
	// Focal distance
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::FocalDistance::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[2];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
		texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

		slotRootParameter[RootSignature::FocalDistance::ECB_DoF].InitAsConstantBufferView(0);
		slotRootParameter[RootSignature::FocalDistance::ESI_Depth].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[RootSignature::FocalDistance::EUO_FocalDist].InitAsDescriptorTable(1, &texTables[1]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures[RootSignature::E_FD]));
	}

	return TRUE;
}

BOOL DepthOfFieldClass::BuildPSO() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC defaultPsoDesc = D3D12Util::DefaultPsoDesc(Vertex::InputLayoutDesc(), DepthStencilBuffer::BufferFormat);
	D3D12_GRAPHICS_PIPELINE_STATE_DESC quadPsoDesc = D3D12Util::QuadPsoDesc();

	D3D12_GRAPHICS_PIPELINE_STATE_DESC cocPsoDesc = quadPsoDesc;
	cocPsoDesc.pRootSignature = mRootSignatures[RootSignature::E_CoC].Get();
	{
		auto vs = mShaderManager->GetDxcShader(VS_CoC);
		auto ps = mShaderManager->GetDxcShader(PS_CoC);
		cocPsoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
		cocPsoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	cocPsoDesc.RTVFormats[0] = CoCMapFormat;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&cocPsoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::E_CoC])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC dofPsoDesc = quadPsoDesc;
	dofPsoDesc.pRootSignature = mRootSignatures[RootSignature::E_DoF].Get();
	{
		auto vs = mShaderManager->GetDxcShader(VS_DoF);
		auto ps = mShaderManager->GetDxcShader(PS_DoF);
		dofPsoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
		dofPsoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	dofPsoDesc.RTVFormats[0] = HDR_FORMAT;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&dofPsoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::E_DoF])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC dofBlurPsoDesc = dofPsoDesc;
	dofBlurPsoDesc.pRootSignature = mRootSignatures[RootSignature::E_DoFBlur].Get();
	{
		auto vs = mShaderManager->GetDxcShader(VS_DoFBlur);
		auto ps = mShaderManager->GetDxcShader(PS_DoFBlur);
		dofBlurPsoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
		dofBlurPsoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	dofPsoDesc.RTVFormats[0] = HDR_FORMAT;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&dofBlurPsoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::E_DoFBlur])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC fdPsoDesc = defaultPsoDesc;
	fdPsoDesc.InputLayout = { nullptr, 0 };
	fdPsoDesc.pRootSignature = mRootSignatures[RootSignature::E_FD].Get();
	{
		auto vs = mShaderManager->GetDxcShader(VS_FD);
		auto ps = mShaderManager->GetDxcShader(PS_FD);
		fdPsoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
		fdPsoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	fdPsoDesc.NumRenderTargets = 0;
	fdPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	fdPsoDesc.DepthStencilState.DepthEnable = false;
	fdPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	fdPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&fdPsoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::E_FD])));

	return TRUE;
}

void DepthOfFieldClass::CalcFocalDist(
		ID3D12GraphicsCommandList*const cmdList,
		const D3D12_VIEWPORT& viewport,
		const D3D12_RECT& scissorRect,
		D3D12_GPU_VIRTUAL_ADDRESS cb_dof,
		D3D12_GPU_DESCRIPTOR_HANDLE si_depth) {
	cmdList->SetPipelineState(mPSOs[PipelineState::E_FD].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[RootSignature::E_FD].Get());
	
	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	cmdList->SetGraphicsRootConstantBufferView(RootSignature::FocalDistance::ECB_DoF, cb_dof);

	cmdList->SetGraphicsRootDescriptorTable(RootSignature::FocalDistance::ESI_Depth, si_depth);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::FocalDistance::EUO_FocalDist, mhFocalDistanceGpuUav);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
	cmdList->DrawInstanced(1, 1, 0, 0);
	D3D12Util::UavBarrier(cmdList, mFocalDistanceBuffer->Resource());
}

void DepthOfFieldClass::CalcCoC(
		ID3D12GraphicsCommandList*const cmdList,
		const D3D12_VIEWPORT& viewport,
		const D3D12_RECT& scissorRect,
		D3D12_GPU_VIRTUAL_ADDRESS cb_dof,
		D3D12_GPU_DESCRIPTOR_HANDLE si_depth) {
	cmdList->SetPipelineState(mPSOs[PipelineState::E_CoC].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[RootSignature::E_CoC].Get());

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	mCoCMap->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->OMSetRenderTargets(1, &mhCoCMapCpuRtv, true, nullptr);

	cmdList->SetGraphicsRootConstantBufferView(RootSignature::CircleOfConfusion::ECB_DoF, cb_dof);

	cmdList->SetGraphicsRootDescriptorTable(RootSignature::CircleOfConfusion::ESI_Depth, si_depth);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::CircleOfConfusion::EUI_FocalDist, mhFocalDistanceGpuUav);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	mCoCMap->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void DepthOfFieldClass::ApplyDoF(
		ID3D12GraphicsCommandList*const cmdList,
		const D3D12_VIEWPORT& viewport,
		const D3D12_RECT& scissorRect,
		GpuResource* backBuffer,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
		FLOAT bokehRadius,
		FLOAT cocMaxDevTolerance,
		FLOAT highlightPower,
		INT numSamples) {
	cmdList->SetPipelineState(mPSOs[PipelineState::E_DoF].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[RootSignature::E_DoF].Get());
	
	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);
	
	const auto copiedBackBuffer = mCopiedBackBuffer.get();

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_SOURCE);
	copiedBackBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);

	cmdList->CopyResource(copiedBackBuffer->Resource(), backBuffer->Resource());

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	copiedBackBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	
	cmdList->OMSetRenderTargets(1, &ro_backBuffer, true, nullptr);
	
	FLOAT dofConstValues[RootSignature::ApplyingDoF::RootConstant::Count] = { bokehRadius, cocMaxDevTolerance, highlightPower, static_cast<FLOAT>(numSamples) };
	cmdList->SetGraphicsRoot32BitConstants(RootSignature::ApplyingDoF::EC_Consts, _countof(dofConstValues), dofConstValues, 0);
	
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::ApplyingDoF::ESI_BackBuffer, mhCopiedBackBufferGpuSrv);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::ApplyingDoF::ESI_CoC, mhCoCMapGpuSrv);
	
	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);
	
	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PRESENT);
}

void DepthOfFieldClass::BlurDoF(
		ID3D12GraphicsCommandList*const cmdList,
		const D3D12_VIEWPORT& viewport,
		const D3D12_RECT& scissorRect,
		D3D12_GPU_VIRTUAL_ADDRESS cb_blur,
		GpuResource* backBuffer,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
		D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer,
		UINT blurCount) {
	cmdList->SetPipelineState(mPSOs[PipelineState::E_DoFBlur].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[RootSignature::E_DoFBlur].Get());

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	const auto copiedBackBuffer = mCopiedBackBuffer.get();

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_SOURCE);
	copiedBackBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);

	cmdList->CopyResource(copiedBackBuffer->Resource(), backBuffer->Resource());

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	copiedBackBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	cmdList->OMSetRenderTargets(1, &ro_backBuffer, true, nullptr);

	cmdList->SetGraphicsRootConstantBufferView(RootSignature::BlurringDoF::ECB_Blur, cb_blur);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::BlurringDoF::ESI_CoC, mhCoCMapGpuSrv);

	cmdList->SetGraphicsRootDescriptorTable(RootSignature::BlurringDoF::ESI_Input, mhCopiedBackBufferGpuSrv);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PRESENT);
}

void DepthOfFieldClass::BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
		UINT descSize, UINT rtvDescSize) {
	mhCoCMapCpuSrv = hCpu.Offset(1, descSize);
	mhCoCMapGpuSrv = hGpu.Offset(1, descSize);
	mhCoCMapCpuRtv = hCpuRtv.Offset(1, rtvDescSize);

	mhCopiedBackBufferCpuSrv = hCpu.Offset(1, descSize);
	mhCopiedBackBufferGpuSrv = hGpu.Offset(1, descSize);

	mhFocalDistanceCpuUav = hCpu.Offset(1, descSize);
	mhFocalDistanceGpuUav = hGpu.Offset(1, descSize);

	BuildDescriptors();	
}

BOOL DepthOfFieldClass::OnResize(ID3D12GraphicsCommandList* cmdList, UINT width, UINT height) {
	CheckReturn(BuildResources(cmdList, width, height));
	BuildDescriptors();

	return TRUE;
}

void DepthOfFieldClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = CoCMapFormat;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = 1;
	uavDesc.Buffer.StructureByteStride = sizeof(FLOAT);
	uavDesc.Buffer.CounterOffsetInBytes = 0;

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = CoCMapFormat;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;

	md3dDevice->CreateShaderResourceView(mCoCMap->Resource(), &srvDesc, mhCoCMapCpuSrv);
	md3dDevice->CreateRenderTargetView(mCoCMap->Resource(), &rtvDesc, mhCoCMapCpuRtv);

	srvDesc.Format = HDR_FORMAT;
	md3dDevice->CreateShaderResourceView(mCopiedBackBuffer->Resource(), &srvDesc, mhCopiedBackBufferCpuSrv);

	md3dDevice->CreateUnorderedAccessView(mFocalDistanceBuffer->Resource(), nullptr, &uavDesc, mhFocalDistanceCpuUav);
}

BOOL DepthOfFieldClass::BuildResources(ID3D12GraphicsCommandList* cmdList, UINT width, UINT height) {
	D3D12_RESOURCE_DESC rscDesc = {};
	rscDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rscDesc.Width = width;
	rscDesc.Height = height;
	rscDesc.Alignment = 0;
	rscDesc.DepthOrArraySize = 1;
	rscDesc.MipLevels = 1;
	rscDesc.SampleDesc.Count = 1;
	rscDesc.SampleDesc.Quality = 0;
	rscDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	auto defaultHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	// Circle of confusion
	{
		rscDesc.Format = CoCMapFormat;
		rscDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		CheckReturn(mCoCMap->Initialize(
			md3dDevice,
			&defaultHeapProp,
			D3D12_HEAP_FLAG_NONE,
			&rscDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"DoF_CoCMap"
		));
	}
	// Copied back buffer
	{
		rscDesc.Format = HDR_FORMAT;
		rscDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		
		CheckReturn(mCopiedBackBuffer->Initialize(
			md3dDevice,
			&defaultHeapProp,
			D3D12_HEAP_FLAG_NONE,
			&rscDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"DoF_CopiedBackBuffer"
		));
	}
	// Focal distance
	{
		auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(FLOAT), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

		CheckReturn(mFocalDistanceBuffer->Initialize(
			md3dDevice,
			&defaultHeapProp,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			L"DoF_FocalDistanceMap"
		));
	}

	mFocalDistanceBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	D3D12Util::UavBarrier(cmdList, mFocalDistanceBuffer->Resource());

	return TRUE;
}