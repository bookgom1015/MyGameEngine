#include "Ssr.h"
#include "Logger.h"

const float Ssr::ClearValues[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

Ssr::Ssr() {}

Ssr::~Ssr() {}

bool Ssr::Initialize(ID3D12Device* device, UINT width, UINT height, UINT divider, DXGI_FORMAT backBufferFormat) {
	md3dDevice = device;

	mSsrMapWidth = width / divider;
	mSsrMapHeight = height / divider;

	mSsrTempMapWidth = width;
	mSsrTempMapHeight = height;

	mDivider = divider;

	mBackBufferFormat = backBufferFormat;

	mViewport = { 0.0f, 0.0f, static_cast<float>(mSsrMapWidth), static_cast<float>(mSsrMapHeight), 0.0f, 1.0f };
	mScissorRect = { 0, 0, static_cast<int>(mSsrMapWidth), static_cast<int>(mSsrMapHeight) };

	CheckReturn(BuildResource());

	return true;
}

void Ssr::BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
		UINT descSize, UINT rtvDescSize) {
	mhSsrMap0CpuSrv = hCpuSrv;
	mhSsrMap0GpuSrv = hGpuSrv;
	mhSsrMap0CpuRtv = hCpuRtv;

	mhSsrMap1CpuSrv = hCpuSrv.Offset(1, descSize);
	mhSsrMap1GpuSrv = hGpuSrv.Offset(1, descSize);
	mhSsrMap1CpuRtv = hCpuRtv.Offset(1, rtvDescSize);

	mhSsrTempMapCpuRtv = hCpuRtv.Offset(1, rtvDescSize);

	BuildDescriptors();
}

bool Ssr::OnResize(UINT width, UINT height) {
	if ((mSsrTempMapWidth != width) || (mSsrTempMapHeight != height)) {
		mSsrMapWidth = width / mDivider;
		mSsrMapHeight = height / mDivider;

		mSsrTempMapWidth = width;
		mSsrTempMapHeight = height;

		mViewport = { 0.0f, 0.0f, static_cast<float>(mSsrMapWidth), static_cast<float>(mSsrMapHeight), 0.0f, 1.0f };
		mScissorRect = { 0, 0, static_cast<int>(mSsrMapWidth), static_cast<int>(mSsrMapHeight) };

		CheckReturn(BuildResource());
		BuildDescriptors();
	}

	return true;
}

void Ssr::BuildDescriptors() {
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

	md3dDevice->CreateShaderResourceView(mSsrMap0.Get(), &srvDesc, mhSsrMap0CpuSrv);
	md3dDevice->CreateRenderTargetView(mSsrMap0.Get(), &rtvDesc, mhSsrMap0CpuRtv);

	md3dDevice->CreateShaderResourceView(mSsrMap1.Get(), &srvDesc, mhSsrMap1CpuSrv);
	md3dDevice->CreateRenderTargetView(mSsrMap1.Get(), &rtvDesc, mhSsrMap1CpuRtv);

	md3dDevice->CreateRenderTargetView(mSsrTempMap.Get(), &rtvDesc, mhSsrTempMapCpuRtv);
}

bool Ssr::BuildResource() {
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
	CheckHRESULT(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&rscDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&optClear,
		IID_PPV_ARGS(mSsrMap0.GetAddressOf())
	));
	CheckHRESULT(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&rscDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&optClear,
		IID_PPV_ARGS(mSsrMap1.GetAddressOf())
	));

	rscDesc.Width = mSsrTempMapWidth;
	rscDesc.Height = mSsrTempMapHeight;
	CheckHRESULT(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&rscDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&optClear,
		IID_PPV_ARGS(mSsrTempMap.GetAddressOf())
	));

	return true;
}