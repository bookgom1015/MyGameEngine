#include "Ssr.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "D3D12Util.h"
#include "GpuResource.h"

using namespace Ssr;

namespace {
	const std::string BuildingSsrVS = "BuildingSsrVS";
	const std::string BuildingSsrPS = "BuildingSsrPS";

	std::string ApplyingSsrVS = "ApplyingSsrVS";
	std::string ApplyingSsrPS = "ApplyingSsrPS";
}

SsrClass::SsrClass() {
	mSsrMaps[0] = std::make_unique<GpuResource>();
	mSsrMaps[1] = std::make_unique<GpuResource>();
	mCopiedBackBuffer = std::make_unique<GpuResource>();
}

bool SsrClass::Initialize(
		ID3D12Device* device, ShaderManager*const manager, 
		UINT width, UINT height, UINT divider) {
	md3dDevice = device;
	mShaderManager = manager;

	mWidth = width;
	mHeight = height;

	mReducedWidth = width / divider;
	mReducedHeight = height / divider;

	mDivider = divider;

	mViewport = { 0.0f, 0.0f, static_cast<float>(mReducedWidth), static_cast<float>(mReducedHeight), 0.0f, 1.0f };
	mScissorRect = { 0, 0, static_cast<int>(mReducedWidth), static_cast<int>(mReducedHeight) };

	CheckReturn(BuildResources());

	return true;
}

bool SsrClass::CompileShaders(const std::wstring& filePath) {
	{
		const std::wstring actualPath = filePath + L"BuildingSsr.hlsl";
		auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
		auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, BuildingSsrVS));
		CheckReturn(mShaderManager->CompileShader(psInfo, BuildingSsrPS));
	}
	{
		const std::wstring actualPath = filePath + L"ApplyingSsr.hlsl";
		auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
		auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, ApplyingSsrVS));
		CheckReturn(mShaderManager->CompileShader(psInfo, ApplyingSsrPS));
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

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures[PipelineState::E_Building]));
	}
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[Applying::RootSignatureLayout::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[7];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
		texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
		texTables[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);
		texTables[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0);
		texTables[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 0);
		texTables[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5, 0);
		texTables[6].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 6, 0);

		slotRootParameter[Applying::RootSignatureLayout::ECB_Pass].InitAsConstantBufferView(0);
		slotRootParameter[Applying::RootSignatureLayout::ESI_BackBuffer].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[Applying::RootSignatureLayout::ESI_Albedo].InitAsDescriptorTable(1, &texTables[1]);
		slotRootParameter[Applying::RootSignatureLayout::ESI_Normal].InitAsDescriptorTable(1, &texTables[2]);
		slotRootParameter[Applying::RootSignatureLayout::ESI_Depth].InitAsDescriptorTable(1, &texTables[3]);
		slotRootParameter[Applying::RootSignatureLayout::ESI_RMS].InitAsDescriptorTable(1, &texTables[4]);
		slotRootParameter[Applying::RootSignatureLayout::ESI_Ssr].InitAsDescriptorTable(1, &texTables[5]);
		slotRootParameter[Applying::RootSignatureLayout::ESI_Environment].InitAsDescriptorTable(1, &texTables[6]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures[PipelineState::E_Applying]));

	}

	return true;
}

bool SsrClass::BuildPso() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC quadPsoDesc = D3D12Util::QuadPsoDesc();

	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = quadPsoDesc;
		psoDesc.pRootSignature = mRootSignatures[PipelineState::E_Building].Get();
		{
			auto vs = mShaderManager->GetDxcShader(BuildingSsrVS);
			auto ps = mShaderManager->GetDxcShader(BuildingSsrPS);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
		psoDesc.RTVFormats[0] = D3D12Util::HDRMapFormat;
		CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::E_Building])));
	}
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::QuadPsoDesc();
		psoDesc.pRootSignature = mRootSignatures[PipelineState::E_Applying].Get();
		{
			auto vs = mShaderManager->GetDxcShader(ApplyingSsrVS);
			auto ps = mShaderManager->GetDxcShader(ApplyingSsrPS);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
		psoDesc.RTVFormats[0] = D3D12Util::HDRMapFormat;
		CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::E_Applying])));
	}

	return true;
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

	mhCopiedBackBufferSrvCpu = hCpu.Offset(1, descSize);
	mhCopiedBackBufferSrvGpu = hGpu.Offset(1, descSize);

	hCpu.Offset(1, descSize);
	hGpu.Offset(1, descSize);
	hCpuRtv.Offset(1, rtvDescSize);

	BuildDescriptors();
}

bool SsrClass::OnResize(UINT width, UINT height) {
	if ((mWidth != width) || (mHeight != height)) {
		mWidth = width;
		mHeight = height;

		mReducedWidth = width / mDivider;
		mReducedHeight = height / mDivider;

		mViewport = { 0.0f, 0.0f, static_cast<float>(mReducedWidth), static_cast<float>(mReducedHeight), 0.0f, 1.0f };
		mScissorRect = { 0, 0, static_cast<int>(mReducedWidth), static_cast<int>(mReducedHeight) };

		CheckReturn(BuildResources());
		BuildDescriptors();
	}

	return true;
}

void SsrClass::Build(
		ID3D12GraphicsCommandList*const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
		D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer,
		D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
		D3D12_GPU_DESCRIPTOR_HANDLE si_depth, 
		D3D12_GPU_DESCRIPTOR_HANDLE si_spec) {
	cmdList->SetPipelineState(mPSOs[PipelineState::E_Building].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[PipelineState::E_Building].Get());
	
	cmdList->RSSetViewports(1, &mViewport);
	cmdList->RSSetScissorRects(1, &mScissorRect);

	const auto ssrMap = mSsrMaps[0].get();
	ssrMap->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

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

	ssrMap->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void SsrClass::Apply(
		ID3D12GraphicsCommandList* const cmdList,
		D3D12_VIEWPORT viewport,
		D3D12_RECT scissorRect,
		GpuResource* backBuffer,
		D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
		D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer,
		D3D12_GPU_DESCRIPTOR_HANDLE si_albedo,
		D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
		D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
		D3D12_GPU_DESCRIPTOR_HANDLE si_rms,
		D3D12_GPU_DESCRIPTOR_HANDLE si_environment) {
	cmdList->SetPipelineState(mPSOs[PipelineState::E_Applying].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[PipelineState::E_Applying].Get());

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_SOURCE);

	cmdList->CopyResource(mCopiedBackBuffer->Resource(), backBuffer->Resource());

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	mCopiedBackBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	cmdList->OMSetRenderTargets(1, &ro_backBuffer, true, nullptr);

	cmdList->SetGraphicsRootConstantBufferView(Applying::RootSignatureLayout::ECB_Pass, cb_pass);
	cmdList->SetGraphicsRootDescriptorTable(Applying::RootSignatureLayout::ESI_BackBuffer, si_backBuffer);
	cmdList->SetGraphicsRootDescriptorTable(Applying::RootSignatureLayout::ESI_Albedo, si_albedo);
	cmdList->SetGraphicsRootDescriptorTable(Applying::RootSignatureLayout::ESI_Normal, si_normal);
	cmdList->SetGraphicsRootDescriptorTable(Applying::RootSignatureLayout::ESI_Depth, si_depth);
	cmdList->SetGraphicsRootDescriptorTable(Applying::RootSignatureLayout::ESI_RMS, si_rms);
	cmdList->SetGraphicsRootDescriptorTable(Applying::RootSignatureLayout::ESI_Ssr, mhSsrMapGpuSrvs[0]);
	cmdList->SetGraphicsRootDescriptorTable(Applying::RootSignatureLayout::ESI_Environment, si_environment);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PRESENT);
	mCopiedBackBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);
}

void SsrClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = D3D12Util::HDRMapFormat;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = D3D12Util::HDRMapFormat;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;

	for (int i = 0; i < 2; ++i) {
		md3dDevice->CreateShaderResourceView(mSsrMaps[i]->Resource(), &srvDesc, mhSsrMapCpuSrvs[i]);
		md3dDevice->CreateRenderTargetView(mSsrMaps[i]->Resource(), &rtvDesc, mhSsrMapCpuRtvs[i]);
	}

	md3dDevice->CreateShaderResourceView(mCopiedBackBuffer->Resource(), &srvDesc, mhCopiedBackBufferSrvCpu);
}

bool SsrClass::BuildResources() {
	D3D12_RESOURCE_DESC rscDesc = {};
	rscDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rscDesc.Alignment = 0;
	rscDesc.DepthOrArraySize = 1;
	rscDesc.MipLevels = 1;
	rscDesc.SampleDesc.Count = 1;
	rscDesc.SampleDesc.Quality = 0;
	rscDesc.Format = D3D12Util::HDRMapFormat;
	rscDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	{
		rscDesc.Width = mReducedWidth;
		rscDesc.Height = mReducedHeight;
		rscDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		CD3DX12_CLEAR_VALUE optClear(D3D12Util::HDRMapFormat, ClearValues);
		for (int i = 0; i < 2; ++i) {
			std::wstringstream wsstream;
			wsstream << "SsrMap_" << i;

			CheckReturn(mSsrMaps[i]->Initialize(
				md3dDevice,
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&rscDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				&optClear,
				wsstream.str().c_str()
			));
		}
	}
	{
		rscDesc.Width = mWidth;
		rscDesc.Height = mHeight;
		rscDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		CheckReturn(mCopiedBackBuffer->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&rscDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			L"CopiedBackBufferMap"
		));
	}

	return true;
}