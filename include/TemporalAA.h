#pragma once

#include <d3dx12.h>
#include <array>

class TemporalAA {
public:
	TemporalAA();
	virtual ~TemporalAA();

public:
	bool Initialize(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format);

	__forceinline constexpr UINT Width() const;
	__forceinline constexpr UINT Height() const;

	__forceinline ID3D12Resource* ResolveMapResource();
	__forceinline ID3D12Resource* HistoryMapResource();

	__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE ResolveMapSrv() const;
	__forceinline constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE ResolveMapRtv() const;

	__forceinline constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE HistoryMapSrv() const;

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
	static const UINT NumRenderTargets = 1;

	static const float ClearValues[4];

private:
	ID3D12Device* md3dDevice;

	UINT mWidth;
	UINT mHeight;

	DXGI_FORMAT mFormat;

	Microsoft::WRL::ComPtr<ID3D12Resource> mResolveMap;
	Microsoft::WRL::ComPtr<ID3D12Resource> mHistoryMap;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhResolveMapCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhResolveMapGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhResolveMapCpuRtv;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhHistoryMapCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhHistoryMapGpuSrv;
};

#include "TemporalAA.inl"