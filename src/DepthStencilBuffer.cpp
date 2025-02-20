#include "DepthStencilBuffer.h"
#include "Logger.h"
#include "HlslCompaction.h"

using namespace DepthStencilBuffer;

DepthStencilBufferClass::DepthStencilBufferClass() {
	mDepthStencilBuffer = std::make_unique<GpuResource>();
}

BOOL DepthStencilBufferClass::Initialize(ID3D12Device* const device) {
	md3dDevice = device;

	mWidth = 0;
	mHeight = 0;

	return TRUE;
}

void DepthStencilBufferClass::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuDsv, UINT dsvDescSize) {
	mhCpuDsv = hCpuDsv;

	hCpuDsv.Offset(1, dsvDescSize);
}

BOOL DepthStencilBufferClass::LowOnResize(UINT width, UINT height) {
	if ((mWidth != width) || (mHeight != height)) {
		mWidth = width;
		mHeight = height;

		CheckReturn(BuildResources());
		BuildDescriptors();
	}

	return TRUE;
}

void DepthStencilBufferClass::BuildDescriptors() {
	// Create descriptor to mip level 0 of entire resource using the format of the resource.
	md3dDevice->CreateDepthStencilView(mDepthStencilBuffer->Resource(), nullptr, mhCpuDsv);
}

BOOL DepthStencilBufferClass::BuildResources() {
	// Create the depth/stencil buffer and view.
	D3D12_RESOURCE_DESC depthStencilDesc;
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = mWidth;
	depthStencilDesc.Height = mHeight;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.Format = BufferFormat;
	depthStencilDesc.SampleDesc.Count = 1;
	depthStencilDesc.SampleDesc.Quality = 0;
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	
	D3D12_CLEAR_VALUE optClear;
	optClear.Format = BufferFormat;
	optClear.DepthStencil.Depth = 1.f;
	optClear.DepthStencil.Stencil = 0;
	
	CheckReturn(mDepthStencilBuffer->Initialize(
		md3dDevice,
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&depthStencilDesc,
		D3D12_RESOURCE_STATE_DEPTH_READ,
		&optClear,
		L"DepthStencilBuffer"
	));

	return TRUE;
}