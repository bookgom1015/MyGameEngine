#pragma once

#include <d3dx12.h>

class DepthOfField {
public:
	DepthOfField();
	virtual ~DepthOfField();

public:
	bool Initialize(ID3D12Device* device, UINT width, UINT height, UINT divider, DXGI_FORMAT backBufferFormat);
	
	__forceinline constexpr UINT CocMapWidth() const;
	__forceinline constexpr UINT CocMapHeight() const;

	__forceinline constexpr UINT BokehMapWidth() const;
	__forceinline constexpr UINT BokehMapHeight() const;

	__forceinline constexpr UINT DofMapWidth() const;
	__forceinline constexpr UINT DofMapHeight() const;

	__forceinline constexpr D3D12_VIEWPORT Viewport() const;
	__forceinline constexpr D3D12_RECT ScissorRect() const;

	__forceinline constexpr DXGI_FORMAT BokehMapFormat() const;

	__forceinline ID3D12Resource* CocMapResource();
	__forceinline ID3D12Resource* BokehMapResource();
	__forceinline ID3D12Resource* BokehBlurMapResource();
	__forceinline ID3D12Resource* DofMapResource();
	__forceinline ID3D12Resource* FocusDistanceBufferResource();

	__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE CocMapSrv() const;
	__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE CocMapRtv() const;

	__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE BokehMapSrv() const;
	__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE BokehMapRtv() const;

	__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE BokehBlurMapSrv() const;
	__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE BokehBlurMapRtv() const;

	__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE DofMapRtv() const;

	__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE FocusDistanceBufferUav() const;

	void BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuUav,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuUav,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
		UINT descSize, UINT rtvDescSize);

	bool OnResize(UINT width, UINT height);

public:
	void BuildDescriptors();
	bool BuildResource();

public:
	static const UINT NumRenderTargets = 4;

	static const DXGI_FORMAT CocMapFormat = DXGI_FORMAT_R8G8B8A8_SNORM;

	static const float CocMapClearValues[4];
	static const float BokehMapClearValues[4];
	static const float DofMapClearValues[4];

private:
	ID3D12Device* md3dDevice;

	UINT mWidth;
	UINT mHeight;

	UINT mDivider;
	UINT mReducedWidth;
	UINT mReducedHeight;

	DXGI_FORMAT mBackBufferFormat;

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;

	Microsoft::WRL::ComPtr<ID3D12Resource> mCocMap;
	Microsoft::WRL::ComPtr<ID3D12Resource> mBokehMap;
	Microsoft::WRL::ComPtr<ID3D12Resource> mBokehBlurMap;
	Microsoft::WRL::ComPtr<ID3D12Resource> mDofMap;
	Microsoft::WRL::ComPtr<ID3D12Resource> mFocusDistanceBuffer;

	BYTE* mMappedBuffer;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCocMapCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhCocMapGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCocMapCpuRtv;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhBokehMapCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhBokehMapGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhBokehMapCpuRtv;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhBokehBlurMapCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhBokehBlurMapGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhBokehBlurMapCpuRtv;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhDofMapCpuRtv;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhFocusDistanceCpuUav;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhFocusDistanceGpuUav;
};

ID3D12Resource* DepthOfField::BokehBlurMapResource() {
	return mBokehBlurMap.Get();
}

ID3D12Resource* DepthOfField::FocusDistanceBufferResource() {
	return mFocusDistanceBuffer.Get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE DepthOfField::BokehBlurMapSrv() const {
	return mhBokehBlurMapGpuSrv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE DepthOfField::BokehBlurMapRtv() const {
	return mhBokehBlurMapCpuRtv;
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE DepthOfField::FocusDistanceBufferUav() const {
	return mhFocusDistanceGpuUav;
}

#include "DepthOfField.inl"