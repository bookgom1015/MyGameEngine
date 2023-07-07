#include "TemporalAA.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "D3D12Util.h"

using namespace TemporalAA;

TemporalAAClass::TemporalAAClass() {
	mResolveMap	= std::make_unique<GpuResource>();
	mHistoryMap = std::make_unique<GpuResource>();
}

bool TemporalAAClass::Initialize(ID3D12Device* device, ShaderManager*const manager, UINT width, UINT height, DXGI_FORMAT format) {
	md3dDevice = device;
	mShaderManager = manager;

	mWidth = width;
	mHeight = height;

	mViewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
	mScissorRect = { 0, 0, static_cast<int>(width), static_cast<int>(height) };

	mBackBufferFormat = format;

	bInitiatingTaa = true;

	CheckReturn(BuildResources());

	return true;
}

bool TemporalAAClass::CompileShaders(const std::wstring& filePath) {
	const std::wstring actualPath = filePath + L"TemporalAA.hlsl";
	auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
	auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
	CheckReturn(mShaderManager->CompileShader(vsInfo, "TaaVS"));
	CheckReturn(mShaderManager->CompileShader(psInfo, "TaaPS"));

	return true;
}

bool TemporalAAClass::BuildRootSignature(const StaticSamplers& samplers) {
	CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignatureLayout::Count];

	CD3DX12_DESCRIPTOR_RANGE texTables[3];
	texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
	texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
	texTables[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);

	slotRootParameter[RootSignatureLayout::ESI_Input].InitAsDescriptorTable(1, &texTables[0]);
	slotRootParameter[RootSignatureLayout::ESI_History].InitAsDescriptorTable(1, &texTables[1]);
	slotRootParameter[RootSignatureLayout::ESI_Velocity].InitAsDescriptorTable(1, &texTables[2]);
	slotRootParameter[RootSignatureLayout::ESI_Factor].InitAsConstants(1, 0);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignature));

	return true;
}

bool TemporalAAClass::BuildPso() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC taaPsoDesc = D3D12Util::QuadPsoDesc();
	taaPsoDesc.pRootSignature = mRootSignature.Get();
	{
		auto vs = mShaderManager->GetDxcShader("TaaVS");
		auto ps = mShaderManager->GetDxcShader("TaaPS");
		taaPsoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
		taaPsoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	taaPsoDesc.RTVFormats[0] = mBackBufferFormat;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&taaPsoDesc, IID_PPV_ARGS(&mPSO)));

	return true;
}

void TemporalAAClass::Run(
		ID3D12GraphicsCommandList*const cmdList,
		ID3D12Resource* backBuffer,
		D3D12_GPU_DESCRIPTOR_HANDLE si_backBuffer,
		D3D12_GPU_DESCRIPTOR_HANDLE si_velocity, 
		float factor) {
	cmdList->SetPipelineState(mPSO.Get());
	cmdList->SetGraphicsRootSignature(mRootSignature.Get());
	
	cmdList->RSSetViewports(1, &mViewport);
	cmdList->RSSetScissorRects(1, &mScissorRect);

	const auto resolveMap = mResolveMap->Resource();
	if (bInitiatingTaa) {
		{
			D3D12_RESOURCE_BARRIER barriers[] = {
				CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_SOURCE),
				CD3DX12_RESOURCE_BARRIER::Transition(resolveMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST)
			};	
			cmdList->ResourceBarrier(_countof(barriers), barriers);
		}
		cmdList->CopyResource(resolveMap, backBuffer);
		{
			D3D12_RESOURCE_BARRIER barriers[] = {
				CD3DX12_RESOURCE_BARRIER::Transition(resolveMap, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
				CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT)			
			};
			cmdList->ResourceBarrier(_countof(barriers), barriers);
		}

		bInitiatingTaa = false;
	}

	{
		D3D12_RESOURCE_BARRIER barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(resolveMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
		};	
		cmdList->ResourceBarrier(_countof(barriers), barriers);
	}

	cmdList->ClearRenderTargetView(mhResolveMapCpuRtv, TemporalAA::ClearValues, 0, nullptr);
	cmdList->OMSetRenderTargets(1, &mhResolveMapCpuRtv, true, nullptr);

	cmdList->SetGraphicsRootDescriptorTable(RootSignatureLayout::ESI_Input, si_backBuffer);
	cmdList->SetGraphicsRootDescriptorTable(RootSignatureLayout::ESI_History, mhHistoryMapGpuSrv);
	cmdList->SetGraphicsRootDescriptorTable(RootSignatureLayout::ESI_Velocity, si_velocity);

	cmdList->SetGraphicsRoot32BitConstants(RootSignatureLayout::ESI_Factor, 1, &factor, 0);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	const auto historyMap = mHistoryMap->Resource();
	{
		D3D12_RESOURCE_BARRIER barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(historyMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST),
			CD3DX12_RESOURCE_BARRIER::Transition(resolveMap, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST)
		};
		cmdList->ResourceBarrier(_countof(barriers), barriers);
	}
	cmdList->CopyResource(historyMap, resolveMap);
	cmdList->CopyResource(backBuffer, resolveMap);
	{
		D3D12_RESOURCE_BARRIER barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT),
			CD3DX12_RESOURCE_BARRIER::Transition(resolveMap, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(historyMap, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		};
		cmdList->ResourceBarrier(_countof(barriers), barriers);
	}
}

void TemporalAAClass::BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
		UINT descSize, UINT rtvDescSize) {
	mhResolveMapCpuSrv = hCpu;
	mhResolveMapGpuSrv = hGpu;
	mhResolveMapCpuRtv = hCpuRtv;

	mhHistoryMapCpuSrv = hCpu.Offset(1, descSize);
	mhHistoryMapGpuSrv = hGpu.Offset(1, descSize);

	hCpu.Offset(1, descSize);
	hGpu.Offset(1, descSize);
	hCpuRtv.Offset(1, rtvDescSize);

	BuildDescriptors();
}

bool TemporalAAClass::OnResize(UINT width, UINT height) {
	if ((mWidth != width) || (mHeight != height)) {
		mWidth = width;
		mHeight = height;

		mViewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
		mScissorRect = { 0, 0, static_cast<int>(width), static_cast<int>(height) };

		CheckReturn(BuildResources());
		BuildDescriptors();
	}

	return true;
}

void TemporalAAClass::BuildDescriptors() {
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

	md3dDevice->CreateShaderResourceView(mResolveMap->Resource(), &srvDesc, mhResolveMapCpuSrv);
	md3dDevice->CreateRenderTargetView(mResolveMap->Resource(), &rtvDesc, mhResolveMapCpuRtv);

	md3dDevice->CreateShaderResourceView(mHistoryMap->Resource(), &srvDesc, mhHistoryMapCpuSrv);
}

bool TemporalAAClass::BuildResources() {
	D3D12_RESOURCE_DESC rscDesc = {};
	rscDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rscDesc.Format = mBackBufferFormat;
	rscDesc.Alignment = 0;
	rscDesc.Width = mWidth;
	rscDesc.Height = mHeight;
	rscDesc.DepthOrArraySize = 1;
	rscDesc.MipLevels = 1;
	rscDesc.SampleDesc.Count = 1;
	rscDesc.SampleDesc.Quality = 0;
	rscDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	CD3DX12_CLEAR_VALUE optClear(mBackBufferFormat, ClearValues);

	rscDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	CheckReturn(mResolveMap->Initialize(
		md3dDevice,
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&rscDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&optClear,
		L"TAA_ResolveMap"
	));

	rscDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	CheckReturn(mHistoryMap->Initialize(
		md3dDevice,
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&rscDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		NULL,
		L"TAA_HistoryMap"
	));

	return true;
}