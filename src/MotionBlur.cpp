#include "MotionBlur.h"
#include "Logger.h"

const float MotionBlur::ClearValues[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

MotionBlur::MotionBlur() {}

MotionBlur::~MotionBlur() {}

bool MotionBlur::Initialize(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format) {
	md3dDevice = device;

	mWidth = width;
	mHeight = height;

	mFormat = format;

	CheckReturn(BuildResource());

	return true;
}

void MotionBlur::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv) {
	mhMotionMapCpuRtv = hCpuRtv;

	BuildDescriptors();
}

bool MotionBlur::OnResize(UINT width, UINT height) {
	if ((mWidth != width) || (mHeight != height)) {
		mWidth = width;
		mHeight = height;

		CheckReturn(BuildResource());
		BuildDescriptors();
	}

	return true;
}

void MotionBlur::BuildDescriptors() {
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = mFormat;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;

	md3dDevice->CreateRenderTargetView(mMotionMap.Get(), &rtvDesc, mhMotionMapCpuRtv);
}

bool MotionBlur::BuildResource() {
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
	rscDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	CD3DX12_CLEAR_VALUE optClear(mFormat, ClearValues);

	CheckHRESULT(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&rscDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&optClear,
		IID_PPV_ARGS(mMotionMap.GetAddressOf())
	));

	return true;
}