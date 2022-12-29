#pragma once

#include <d3dx12.h>

class MotionBlur {
public:
	MotionBlur();
	virtual ~MotionBlur();

public:
	bool Initialize(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format);

	__forceinline constexpr UINT Width() const;
	__forceinline constexpr UINT Height() const;

	__forceinline ID3D12Resource* MotionMapResource();

	__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE MotionMapRtv() const;

	void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv);

	bool OnResize(UINT width, UINT height);

public:
	void BuildDescriptors();
	bool BuildResource();

public:
	static const UINT NumRenderTargets = 1;

	static const float ClearValues[4];

private:
	ID3D12Device* md3dDevice;

	UINT mWidth;
	UINT mHeight;

	DXGI_FORMAT mFormat;

	Microsoft::WRL::ComPtr<ID3D12Resource> mMotionMap;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhMotionMapCpuRtv;
};

constexpr UINT MotionBlur::Width() const {
	return mWidth;
}

constexpr UINT MotionBlur::Height() const {
	return mHeight;
}

ID3D12Resource* MotionBlur::MotionMapResource() {
	return mMotionMap.Get();
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE MotionBlur::MotionMapRtv() const {
	return mhMotionMapCpuRtv;
}