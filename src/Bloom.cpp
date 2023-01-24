#include "Bloom.h"
#include "Logger.h"

const float Bloom::ClearValues[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

Bloom::Bloom() {}

Bloom::~Bloom() {}

bool Bloom::Initialize(ID3D12Device* device, UINT width, UINT height, UINT divider, DXGI_FORMAT backBufferFormat) {
	md3dDevice = device;

	mBloomMapWidth = width / divider;
	mBloomMapHeight = height / divider;

	mBloomTempMapWidth = width;
	mBloomTempMapHeight = height;

	mDivider = divider;

	mBackBufferFormat = backBufferFormat;

	mViewport = { 0.0f, 0.0f, static_cast<float>(mBloomMapWidth), static_cast<float>(mBloomMapHeight), 0.0f, 1.0f };
	mScissorRect = { 0, 0, static_cast<int>(mBloomMapWidth), static_cast<int>(mBloomMapHeight) };

	CheckReturn(BuildResource());

	return true;
}

void Bloom::BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
		UINT descSize, UINT rtvDescSize) {
	mhBloomMap0CpuSrv = hCpuSrv;
	mhBloomMap0GpuSrv = hGpuSrv;
	mhBloomMap0CpuRtv = hCpuRtv;

	mhBloomMap1CpuSrv = hCpuSrv.Offset(1, descSize);
	mhBloomMap1GpuSrv = hGpuSrv.Offset(1, descSize);
	mhBloomMap1CpuRtv = hCpuRtv.Offset(1, rtvDescSize);

	mhBloomTempMapCpuRtv = hCpuRtv.Offset(1, rtvDescSize);

	BuildDescriptors();
}

bool Bloom::OnResize(UINT width, UINT height) {
	if ((mBloomTempMapWidth != width) || (mBloomTempMapHeight != height)) {
		mBloomMapWidth = width / mDivider;
		mBloomMapHeight = height / mDivider;

		mBloomTempMapWidth = width;
		mBloomTempMapHeight = height;

		mViewport = { 0.0f, 0.0f, static_cast<float>(mBloomMapWidth), static_cast<float>(mBloomMapHeight), 0.0f, 1.0f };
		mScissorRect = { 0, 0, static_cast<int>(mBloomMapWidth), static_cast<int>(mBloomMapHeight) };

		CheckReturn(BuildResource());
		BuildDescriptors();
	}

	return true;
}

void Bloom::BuildDescriptors() {
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

	md3dDevice->CreateShaderResourceView(mBloomMap0.Get(), &srvDesc, mhBloomMap0CpuSrv);
	md3dDevice->CreateRenderTargetView(mBloomMap0.Get(), &rtvDesc, mhBloomMap0CpuRtv);

	md3dDevice->CreateShaderResourceView(mBloomMap1.Get(), &srvDesc, mhBloomMap1CpuSrv);
	md3dDevice->CreateRenderTargetView(mBloomMap1.Get(), &rtvDesc, mhBloomMap1CpuRtv);

	md3dDevice->CreateRenderTargetView(mBloomTempMap.Get(), &rtvDesc, mhBloomTempMapCpuRtv);
}

bool Bloom::BuildResource() {
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
	CheckHRESULT(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&rscDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&optClear,
		IID_PPV_ARGS(mBloomMap0.GetAddressOf())
	));
	CheckHRESULT(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&rscDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&optClear,
		IID_PPV_ARGS(mBloomMap1.GetAddressOf())
	));

	rscDesc.Width = mBloomTempMapWidth;
	rscDesc.Height = mBloomTempMapHeight;
	CheckHRESULT(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&rscDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&optClear,
		IID_PPV_ARGS(mBloomTempMap.GetAddressOf())
	));

	return true;
}