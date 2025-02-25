#include "DirectX/Shading/SSR.h"
#include "Common/Debug/Logger.h"
#include "DirectX/Util/D3D12Util.h"
#include "DirectX/Util/ShaderManager.h"
#include "DirectX/Infrastructure/GpuResource.h"
#include "HlslCompaction.h"

using namespace SSR;

namespace {
	const CHAR* const VS_SSR_Screen = "VS_SSR_Screen";
	const CHAR* const PS_SSR_Screen = "PS_SSR_Screen";

	const CHAR* const VS_SSR_View = "VS_SSR_View";
	const CHAR* const PS_SSR_View = "PS_SSR_View";
}

SSRClass::SSRClass() {
	mSSRMaps[0] = std::make_unique<GpuResource>();
	mSSRMaps[1] = std::make_unique<GpuResource>();	
}

BOOL SSRClass::Initialize(
		Locker<ID3D12Device5>* const device, ShaderManager* const manager,
		UINT width, UINT height, Resolution::Type type) {
	md3dDevice = device;
	mShaderManager = manager;
	mResolutionType = type;

	CheckReturn(BuildResources(width, height));

	return TRUE;
}

BOOL SSRClass::CompileShaders(const std::wstring& filePath) {
	{
		const std::wstring actualPath = filePath + L"SSR_Screen.hlsl";
		const auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
		const auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, VS_SSR_Screen));
		CheckReturn(mShaderManager->CompileShader(psInfo, PS_SSR_Screen));
	}
	{
		const std::wstring actualPath = filePath + L"SSR_View.hlsl";
		const auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
		const auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, VS_SSR_View));
		CheckReturn(mShaderManager->CompileShader(psInfo, PS_SSR_View));
	}

	return TRUE;
}

BOOL SSRClass::BuildRootSignature(const StaticSamplers& samplers) {
	CD3DX12_DESCRIPTOR_RANGE texTables[5] = {}; UINT index = 0;
	texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
	texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
	texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);
	texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0);
	texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 0);

	index = 0;

	CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::Default::Count] = {};
	slotRootParameter[RootSignature::Default::ECB_SSR].InitAsConstantBufferView(0);
	slotRootParameter[RootSignature::Default::ESI_BackBuffer].InitAsDescriptorTable(1, &texTables[index++]);
	slotRootParameter[RootSignature::Default::ESI_Position].InitAsDescriptorTable(	1, &texTables[index++]);
	slotRootParameter[RootSignature::Default::ESI_Normal].InitAsDescriptorTable(	1, &texTables[index++]);
	slotRootParameter[RootSignature::Default::ESI_Depth].InitAsDescriptorTable(		1, &texTables[index++]);
	slotRootParameter[RootSignature::Default::ESI_RMS].InitAsDescriptorTable(		1, &texTables[index++]);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		CheckReturn(D3D12Util::CreateRootSignature(device, rootSigDesc, &mRootSignature));
	}

	return TRUE;
}

BOOL SSRClass::BuildPSO() {
	D3D12Util::Descriptor::PipelineState::Builder builder;
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::QuadPsoDesc();
		psoDesc.pRootSignature = mRootSignature.Get();
		psoDesc.RTVFormats[0] = SSRMapFormat;

		// ScreenSpace
		{
			{
				const auto vs = mShaderManager->GetDxcShader(VS_SSR_Screen);
				const auto ps = mShaderManager->GetDxcShader(PS_SSR_Screen);
				psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
				psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
			}
			builder.Enqueue(psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::EG_ScreenSpace]));
		}
		// ViewSpace
		{
			{
				const auto vs = mShaderManager->GetDxcShader(VS_SSR_View);
				const auto ps = mShaderManager->GetDxcShader(PS_SSR_View);
				psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
				psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
			}
			builder.Enqueue(psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::EG_ViewSpace]));
		}
	}

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		CheckReturn(builder.Build(device));
	}

	return TRUE;
}

void SSRClass::AllocateDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
		UINT descSize, UINT rtvDescSize) {
	for (UINT i = 0; i < 2; ++i) {
		mhSSRMapCpuSrvs[i] = hCpu.Offset(1, descSize);
		mhSSRMapGpuSrvs[i] = hGpu.Offset(1, descSize);
		mhSSRMapCpuRtvs[i] = hCpuRtv.Offset(1, rtvDescSize);
	}
}

BOOL SSRClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = SSRMapFormat;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.f;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = SSRMapFormat;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;

	D3D12Util::Descriptor::View::Builder builder;
	for (UINT i = 0; i < 2; ++i) {
		builder.Enqueue(mSSRMaps[i]->Resource(), srvDesc, mhSSRMapCpuSrvs[i]);
		builder.Enqueue(mSSRMaps[i]->Resource(), rtvDesc, mhSSRMapCpuRtvs[i]);
	}

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		builder.Build(device);
	}

	return TRUE;
}

BOOL SSRClass::OnResize(UINT width, UINT height) {
	CheckReturn(BuildResources(width, height));
	BuildDescriptors();

	return TRUE;
}

void SSRClass::Run(
		ID3D12GraphicsCommandList* const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
		GpuResource* const backBuffer,
		D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer,
		D3D12_GPU_DESCRIPTOR_HANDLE si_position,
		D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
		D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
		D3D12_GPU_DESCRIPTOR_HANDLE si_rms) {
	cmdList->SetPipelineState(mPSOs[StateType].Get());
	cmdList->SetGraphicsRootSignature(mRootSignature.Get());
	
	cmdList->RSSetViewports(1, &mViewport);
	cmdList->RSSetScissorRects(1, &mScissorRect);

	const auto ssrMap = mSSRMaps[0].get();

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	ssrMap->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	const auto rtv = mhSSRMapCpuRtvs[0];
	cmdList->ClearRenderTargetView(rtv, SSRMapClearValues, 0, nullptr);
	cmdList->OMSetRenderTargets(1, &rtv, TRUE, nullptr);

	cmdList->SetGraphicsRootConstantBufferView(RootSignature::Default::ECB_SSR, cbAddress);

	cmdList->SetGraphicsRootDescriptorTable(RootSignature::Default::ESI_BackBuffer, si_backBuffer);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::Default::ESI_Position, si_position);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::Default::ESI_Normal, si_normal);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::Default::ESI_Depth, si_depth);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::Default::ESI_RMS, si_rms);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PRESENT);
	ssrMap->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

BOOL SSRClass::BuildResources(UINT width, UINT height) {
	UINT actWidth;
	UINT actHeight;
	if (mResolutionType == Resolution::E_Fullscreen) {
		actWidth = width;
		actHeight = height;
	}
	else {
		actWidth = static_cast<UINT>(width * 0.5f);
		actHeight = static_cast<UINT>(height * 0.5f);
	}

	mViewport = { 0.f, 0.f, static_cast<FLOAT>(actWidth), static_cast<FLOAT>(actHeight), 0.f, 1.f };
	mScissorRect = { 0, 0, static_cast<INT>(actWidth), static_cast<INT>(actHeight) };

	D3D12_RESOURCE_DESC rscDesc = {};
	rscDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rscDesc.Alignment = 0;
	rscDesc.Width = actWidth;
	rscDesc.Height = actHeight;
	rscDesc.DepthOrArraySize = 1;
	rscDesc.MipLevels = 1;
	rscDesc.SampleDesc.Count = 1;
	rscDesc.SampleDesc.Quality = 0;
	rscDesc.Format = SSRMapFormat;
	rscDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	rscDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	CD3DX12_CLEAR_VALUE optClear(SSRMapFormat, SSRMapClearValues);

	D3D12Util::Descriptor::Resource::Builder builder;
	for (INT i = 0; i < 2; ++i) {
		std::wstringstream wsstream;
		wsstream << L"SSR_SSRMap_" << i;

		builder.Enqueue(
			mSSRMaps[i].get(),
			CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			rscDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&optClear,
			wsstream.str().c_str()
		);
	}

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		CheckReturn(builder.Build(device));
	}

	return TRUE;
}