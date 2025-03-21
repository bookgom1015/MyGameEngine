#include "DirectX/Shading/Bloom.h"
#include "Common/Debug/Logger.h"
#include "DirectX/Util/D3D12Util.h"
#include "DirectX/Util/ShaderManager.h"
#include "HlslCompaction.h"

using namespace Bloom;

namespace {
	const CHAR* const VS_Bloom = "VS_Bloom";
	const CHAR* const PS_Bloom = "PS_Bloom";

	const CHAR* const VS_HighlightExtraction = "VS_HighlightExtraction";
	const CHAR* const PS_HighlightExtraction = "PS_HighlightExtraction";
}

BloomClass::BloomClass() {
	mCopiedBackBuffer= std::make_unique<GpuResource>();
	mBloomMaps[0] = std::make_unique<GpuResource>();
	mBloomMaps[1] = std::make_unique<GpuResource>();
}

BOOL BloomClass::Initialize(
		Locker<ID3D12Device5>* const device, ShaderManager* const manager,
		UINT width, UINT height, Resolution::Type type) {
	md3dDevice = device;
	mShaderManager = manager;
	mResolutionType = type;

	CheckReturn(BuildResources(width, height));

	return TRUE;
}

BOOL BloomClass::CompileShaders(const std::wstring& filePath) {
	{
		const std::wstring actualPath = filePath + L"Bloom.hlsl";
		const auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
		const auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, VS_Bloom));
		CheckReturn(mShaderManager->CompileShader(psInfo, PS_Bloom));
	}
	{
		const std::wstring actualPath = filePath + L"ExtractHighlight.hlsl";
		const auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
		const auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, VS_HighlightExtraction));
		CheckReturn(mShaderManager->CompileShader(psInfo, PS_HighlightExtraction));
	}

	return TRUE;
}

BOOL BloomClass::BuildRootSignature(const StaticSamplers& samplers) {
	D3D12Util::Descriptor::RootSignature::Builder builder;
	// ExtractHighlight
	{
		CD3DX12_DESCRIPTOR_RANGE texTable0;
		texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::ExtractHighlight::Count] = {};
		slotRootParameter[RootSignature::ExtractHighlight::ESI_BackBuffer].InitAsDescriptorTable(1, &texTable0);
		slotRootParameter[RootSignature::ExtractHighlight::EC_Consts].InitAsConstants(RootConstant::ExtractHighlight::Count, 0);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		builder.Enqueue(rootSigDesc, IID_PPV_ARGS(&mRootSignatures[PipelineState::EG_ExtractHighlight]));
	}
	// ApplyBloom
	{
		CD3DX12_DESCRIPTOR_RANGE texTable0;
		texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

		CD3DX12_DESCRIPTOR_RANGE texTable1;
		texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);

		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::ApplyBloom::Count] = {};
		slotRootParameter[RootSignature::ApplyBloom::ESI_BackBuffer].InitAsDescriptorTable(1, &texTable0);
		slotRootParameter[RootSignature::ApplyBloom::ESI_Bloom].InitAsDescriptorTable(1, &texTable1);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		builder.Enqueue(rootSigDesc, IID_PPV_ARGS(&mRootSignatures[PipelineState::EG_ApplyBloom]));
	}

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		CheckReturn(builder.Build(device));
	}

	return TRUE;
}

BOOL BloomClass::BuildPSO() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC quadPsoDesc = D3D12Util::QuadPsoDesc();

	D3D12Util::Descriptor::PipelineState::Builder builder;
	// ExtractHighlight
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = quadPsoDesc;
		psoDesc.pRootSignature = mRootSignatures[RootSignature::E_ExtractHighlight].Get();
		{
			const auto vs = mShaderManager->GetDxcShader(VS_HighlightExtraction);
			const auto ps = mShaderManager->GetDxcShader(PS_HighlightExtraction);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
		psoDesc.RTVFormats[0] = HDR_FORMAT;

		builder.Enqueue(psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::EG_ExtractHighlight]));
	}
	// ApplyBloom
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = quadPsoDesc;
		psoDesc.pRootSignature = mRootSignatures[RootSignature::E_ApplyBloom].Get();
		{
			const auto vs = mShaderManager->GetDxcShader(VS_Bloom);
			const auto ps = mShaderManager->GetDxcShader(PS_Bloom);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
		psoDesc.RTVFormats[0] = HDR_FORMAT;

		builder.Enqueue(psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::EG_ApplyBloom]));
	}

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		CheckReturn(builder.Build(device));
	}

	return TRUE;
}

void BloomClass::ExtractHighlight(
		ID3D12GraphicsCommandList* const cmdList,
		D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer,
		FLOAT threshold) {
	cmdList->SetPipelineState(mPSOs[PipelineState::EG_ExtractHighlight].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[RootSignature::E_ExtractHighlight].Get());

	cmdList->RSSetViewports(1, &mViewport);
	cmdList->RSSetScissorRects(1, &mScissorRect);

	const auto bloomMap0 = mBloomMaps[0].get();
	bloomMap0->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	
	auto bloomMapRtv = mhBloomMapCpuRtvs[0];
	cmdList->ClearRenderTargetView(bloomMapRtv, ClearValues, 0, nullptr);
	cmdList->OMSetRenderTargets(1, &bloomMapRtv, TRUE, nullptr);

	cmdList->SetGraphicsRootDescriptorTable(RootSignature::ExtractHighlight::ESI_BackBuffer, si_backBuffer);

	FLOAT values[RootConstant::ExtractHighlight::Count] = { threshold };
	cmdList->SetGraphicsRoot32BitConstants(RootSignature::ExtractHighlight::EC_Consts, _countof(values), values, 0);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	bloomMap0->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void BloomClass::ApplyBloom(
		ID3D12GraphicsCommandList* const cmdList,
		const D3D12_VIEWPORT& viewport,
		const D3D12_RECT& scissorRect,
		GpuResource* const backBuffer,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer) {
	cmdList->SetPipelineState(mPSOs[PipelineState::EG_ApplyBloom].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[RootSignature::E_ApplyBloom].Get());

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_SOURCE);
	mCopiedBackBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);

	cmdList->CopyResource(mCopiedBackBuffer->Resource(), backBuffer->Resource());

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	mCopiedBackBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	cmdList->OMSetRenderTargets(1, &ro_backBuffer, TRUE, nullptr);

	cmdList->SetGraphicsRootDescriptorTable(RootSignature::ApplyBloom::ESI_BackBuffer, mhCopiedBackBufferGpuSrv);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::ApplyBloom::ESI_Bloom, mhBloomMapGpuSrvs[0]);
	 
	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PRESENT);
}

void BloomClass::AllocateDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
		UINT descSize, UINT rtvDescSize) {
	mhCopiedBackBufferCpuSrv = hCpu.Offset(1, descSize);
	mhCopiedBackBufferGpuSrv = hGpu.Offset(1, descSize);

	mhBloomMapCpuSrvs[0] = hCpu.Offset(1, descSize);
	mhBloomMapGpuSrvs[0] = hGpu.Offset(1, descSize);
	mhBloomMapCpuRtvs[0] = hCpuRtv.Offset(1, rtvDescSize);

	mhBloomMapCpuSrvs[1] = hCpu.Offset(1, descSize);
	mhBloomMapGpuSrvs[1] = hGpu.Offset(1, descSize);
	mhBloomMapCpuRtvs[1] = hCpuRtv.Offset(1, rtvDescSize);
}

BOOL BloomClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = HDR_FORMAT;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = HDR_FORMAT;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;

	D3D12Util::Descriptor::View::Builder builder;

	builder.Enqueue(mCopiedBackBuffer->Resource(), srvDesc, mhCopiedBackBufferCpuSrv);

	for (UINT i = 0; i < 2; ++i) {
		const auto bloomMap = mBloomMaps[i]->Resource();
		builder.Enqueue(bloomMap, srvDesc, mhBloomMapCpuSrvs[i]);
		builder.Enqueue(bloomMap, rtvDesc, mhBloomMapCpuRtvs[i]);
	}

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		builder.Build(device);
	}

	return TRUE;
}

BOOL BloomClass::OnResize(UINT width, UINT height) {
	CheckReturn(BuildResources(width, height));
	BuildDescriptors();

	return TRUE;
}

BOOL BloomClass::BuildResources(UINT width, UINT height) {
	UINT actWidth;
	UINT actHeight;
	if (mResolutionType == Resolution::E_Fullscreen) {
		actWidth = width;
		actHeight = height;
	}
	else if (mResolutionType == Resolution::E_Quarter) {
		actWidth = static_cast<UINT>(width * 0.5f);
		actHeight = static_cast<UINT>(height * 0.5f);
	}
	else {
		actWidth = static_cast<UINT>(width * 0.25f);
		actHeight = static_cast<UINT>(height * 0.25f);
	}

	mViewport = { 0.0f, 0.0f, static_cast<FLOAT>(actWidth), static_cast<FLOAT>(actHeight), 0.0f, 1.0f };
	mScissorRect = { 0, 0, static_cast<INT>(actWidth), static_cast<INT>(actHeight) };

	D3D12_RESOURCE_DESC rscDesc = {};
	rscDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rscDesc.Alignment = 0;
	rscDesc.DepthOrArraySize = 1;
	rscDesc.MipLevels = 1;
	rscDesc.SampleDesc.Count = 1;
	rscDesc.SampleDesc.Quality = 0;
	rscDesc.Format = HDR_FORMAT;
	rscDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	CD3DX12_CLEAR_VALUE optClear(HDR_FORMAT, ClearValues);

	D3D12Util::Descriptor::Resource::Builder builder;
	{
		rscDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		rscDesc.Width = width;
		rscDesc.Height = height;
		builder.Enqueue(
			mCopiedBackBuffer.get(),
			CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			rscDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"Bloom_CopiedBackBuffer"
		);
	}
	{
		rscDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		rscDesc.Width = actWidth;
		rscDesc.Height = actHeight;

		for (UINT i = 0; i < 2; ++i) {
			std::wstringstream wsstream;
			wsstream << L"Bloom_BloomMap_" << i;

			builder.Enqueue(
				mBloomMaps[i].get(),
				CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				rscDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				&optClear,
				wsstream.str().c_str()
			);
		}
	}

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		CheckReturn(builder.Build(device));
	}

	return TRUE;
}