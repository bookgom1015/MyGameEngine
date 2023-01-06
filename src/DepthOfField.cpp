#include "DepthOfField.h"
#include "Logger.h"

const float DepthOfField::CocMapClearValues[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
const float DepthOfField::BokehMapClearValues[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
const float DepthOfField::DofMapClearValues[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

DepthOfField::DepthOfField() {}

DepthOfField::~DepthOfField() {}

bool DepthOfField::Initialize(ID3D12Device* device, UINT width, UINT height, UINT divider, DXGI_FORMAT backBufferFormat) {
	md3dDevice = device;

	mWidth = width;
	mHeight = height;

	mDivider = divider;
	mReducedWidth = width / divider;
	mReducedHeight = height / divider;

	mBackBufferFormat = backBufferFormat;

	mViewport = { 0.0f, 0.0f, static_cast<float>(mReducedWidth), static_cast<float>(mReducedHeight), 0.0f, 1.0f };
	mScissorRect = { 0, 0, static_cast<int>(mReducedWidth), static_cast<int>(mReducedHeight) };

	CheckReturn(BuildResource());

	return true;
}

void DepthOfField::BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
		UINT descSize, UINT rtvDescSize) {
	mhCocMapCpuSrv = hCpuSrv;
	mhCocMapGpuSrv = hGpuSrv;
	mhCocMapCpuRtv = hCpuRtv;

	mhBokehMapCpuSrv = hCpuSrv.Offset(1, descSize);
	mhBokehMapGpuSrv = hGpuSrv.Offset(1, descSize);
	mhBokehMapCpuRtv = hCpuRtv.Offset(1, rtvDescSize);

	mhDofMapCpuRtv = hCpuRtv.Offset(1, rtvDescSize);

	BuildDescriptors();
}

bool DepthOfField::OnResize(UINT width, UINT height) {
	if ((mWidth != width) || (mHeight != height)) {
		mWidth = width;
		mHeight = height;

		mReducedWidth = width / mDivider;
		mReducedHeight = height / mDivider;

		mViewport = { 0.0f, 0.0f, static_cast<float>(mReducedWidth), static_cast<float>(mReducedHeight), 0.0f, 1.0f };
		mScissorRect = { 0, 0, static_cast<int>(mReducedWidth), static_cast<int>(mReducedHeight) };

		CheckReturn(BuildResource());
		BuildDescriptors();
	}

	return true;
}

void DepthOfField::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = CocMapFormat;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = CocMapFormat;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;

	md3dDevice->CreateShaderResourceView(mCocMap.Get(), &srvDesc, mhCocMapCpuSrv);
	md3dDevice->CreateRenderTargetView(mCocMap.Get(), &rtvDesc, mhCocMapCpuRtv);

	srvDesc.Format = mBackBufferFormat;
	rtvDesc.Format = mBackBufferFormat;
	md3dDevice->CreateShaderResourceView(mBokehMap.Get(), &srvDesc, mhBokehMapCpuSrv);
	md3dDevice->CreateRenderTargetView(mBokehMap.Get(), &rtvDesc, mhBokehMapCpuRtv);

	md3dDevice->CreateRenderTargetView(mDofMap.Get(), &rtvDesc, mhDofMapCpuRtv);
}

bool DepthOfField::BuildResource() {
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
			IID_PPV_ARGS(mCocMap.GetAddressOf())
		));
	}
	{
		CD3DX12_CLEAR_VALUE optClear(mBackBufferFormat, BokehMapClearValues);

		rscDesc.Width = mReducedWidth;
		rscDesc.Height = mReducedHeight;
		rscDesc.Format = mBackBufferFormat;
		CheckHRESULT(md3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&rscDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&optClear,
			IID_PPV_ARGS(mBokehMap.GetAddressOf())
		));
	}
	{
		CD3DX12_CLEAR_VALUE optClear(mBackBufferFormat, DofMapClearValues);

		rscDesc.Width = mWidth;
		rscDesc.Height = mHeight;
		rscDesc.Format = mBackBufferFormat;
		CheckHRESULT(md3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&rscDesc,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			&optClear,
			IID_PPV_ARGS(mDofMap.GetAddressOf())
		));
	}

	return true;
}