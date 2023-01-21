#pragma once

#include <d3dx12.h>

class Bloom {
public:
	Bloom();
	virtual ~Bloom();

public:
	bool Initialize(ID3D12Device* device, UINT width, UINT height, UINT divider, DXGI_FORMAT backBufferFormat);

	__forceinline constexpr UINT BloomMapWidth() const;
	__forceinline constexpr UINT BloomMapHeight() const;

	__forceinline constexpr UINT BloomTempMapWidth() const;
	__forceinline constexpr UINT BloomTempMapHeight() const;

	__forceinline constexpr D3D12_VIEWPORT Viewport() const;
	__forceinline constexpr D3D12_RECT ScissorRect() const;

	__forceinline constexpr DXGI_FORMAT Format() const;

	__forceinline ID3D12Resource* BloomMap0Resource();
	__forceinline ID3D12Resource* BloomMap1Resource();
	__forceinline ID3D12Resource* BloomTempMapResource();

	__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE BloomMap0Srv() const;
	__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE BloomMap0Rtv() const;

	__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE BloomMap1Srv() const;
	__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE BloomMap1Rtv() const;

	__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE BloomTempMapRtv() const;

	void BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
		UINT descSize, UINT rtvDescSize);

	bool OnResize(UINT width, UINT height);

public:
	void BuildDescriptors();
	bool BuildResource();

public:
	static const UINT NumRenderTargets = 3;

	static const float ClearValues[4];

private:
	ID3D12Device* md3dDevice;

	UINT mBloomMapWidth;
	UINT mBloomMapHeight;

	UINT mBloomTempMapWidth;
	UINT mBloomTempMapHeight;

	UINT mDivider;

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;

	DXGI_FORMAT mBackBufferFormat;

	Microsoft::WRL::ComPtr<ID3D12Resource> mBloomMap0;
	Microsoft::WRL::ComPtr<ID3D12Resource> mBloomMap1;
	Microsoft::WRL::ComPtr<ID3D12Resource> mBloomTempMap;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhBloomMap0CpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhBloomMap0GpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhBloomMap0CpuRtv;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhBloomMap1CpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhBloomMap1GpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhBloomMap1CpuRtv;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhBloomTempMapCpuRtv;
};

constexpr UINT Bloom::BloomMapWidth() const {
	return mBloomMapWidth;
}

constexpr UINT Bloom::BloomMapHeight() const {
	return mBloomMapHeight;
}

constexpr UINT Bloom::BloomTempMapWidth() const {
	return mBloomTempMapWidth;
}

constexpr UINT Bloom::BloomTempMapHeight() const {
	return mBloomTempMapHeight;
}

constexpr D3D12_VIEWPORT Bloom::Viewport() const {
	return mViewport;
}

constexpr D3D12_RECT Bloom::ScissorRect() const {
	return mScissorRect;
}

constexpr DXGI_FORMAT Bloom::Format() const {
	return mBackBufferFormat;
}

ID3D12Resource* Bloom::BloomMap0Resource() {
	return mBloomMap0.Get();
}

ID3D12Resource* Bloom::BloomMap1Resource() {
	return mBloomMap1.Get();
}

ID3D12Resource* Bloom::BloomTempMapResource() {
	return mBloomTempMap.Get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE Bloom::BloomMap0Srv() const {
	return mhBloomMap0GpuSrv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE Bloom::BloomMap0Rtv() const {
	return mhBloomMap0CpuRtv;
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE Bloom::BloomMap1Srv() const {
	return mhBloomMap1GpuSrv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE Bloom::BloomMap1Rtv() const {
	return mhBloomMap1CpuRtv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE Bloom::BloomTempMapRtv() const {
	return mhBloomTempMapCpuRtv;
}