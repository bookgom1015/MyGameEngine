#include "TemporalAA.h"
#include "Logger.h"

const float TemporalAA::ClearValues[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

TemporalAA::TemporalAA() {}

TemporalAA::~TemporalAA() {}

bool TemporalAA::Initialize(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format) {
	md3dDevice = device;

	mWidth = width;
	mHeight = height;

	mFormat = format;

	CheckReturn(BuildResource());

	return true;
}

void TemporalAA::BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
		UINT descSize, UINT rtvDescSize) {
	mhResolveMapCpuSrv = hCpuSrv;
	mhResolveMapGpuSrv = hGpuSrv;
	mhResolveMapCpuRtv = hCpuRtv;

	mhHistoryMapCpuSrv = hCpuSrv.Offset(1, descSize);
	mhHistoryMapGpuSrv = hGpuSrv.Offset(1, descSize);

	BuildDescriptors();
}

bool TemporalAA::OnResize(UINT width, UINT height) {
	if ((mWidth != width) || (mHeight != height)) {
		mWidth = width;
		mHeight = height;

		CheckReturn(BuildResource());
		BuildDescriptors();
	}

	return true;
}

void TemporalAA::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = mFormat;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = mFormat;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;

	md3dDevice->CreateShaderResourceView(mResolveMap.Get(), &srvDesc, mhResolveMapCpuSrv);
	md3dDevice->CreateRenderTargetView(mResolveMap.Get(), &rtvDesc, mhResolveMapCpuRtv);

	md3dDevice->CreateShaderResourceView(mHistoryMap.Get(), &srvDesc, mhHistoryMapCpuSrv);
}

bool TemporalAA::BuildResource() {
	D3D12_RESOURCE_DESC rscDesc = {};
	rscDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rscDesc.Format = mFormat;
	rscDesc.Alignment = 0;
	rscDesc.Width = mWidth;
	rscDesc.Height = mHeight;
	rscDesc.DepthOrArraySize = 1;
	rscDesc.MipLevels = 1;
	rscDesc.SampleDesc.Count = 1;
	rscDesc.SampleDesc.Quality = 0;
	rscDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	CD3DX12_CLEAR_VALUE optClear(mFormat, ClearValues);

	rscDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	CheckHRESULT(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&rscDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&optClear,
		IID_PPV_ARGS(mResolveMap.GetAddressOf())
	));

	rscDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	CheckHRESULT(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&rscDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		NULL,
		IID_PPV_ARGS(mHistoryMap.GetAddressOf())
	));

	return true;
}