#pragma once

#include <wrl.h>
#include <DirectX/d3dx12.h>
#include <dxgi1_6.h>

class GpuResource {
public:
	GpuResource() = default;
	virtual ~GpuResource() = default;

public:
	BOOL Initialize(
		ID3D12Device* const device, 
		const D3D12_HEAP_PROPERTIES* const heapProp, 
		D3D12_HEAP_FLAGS heapFlag,
		const D3D12_RESOURCE_DESC* const rscDesc,
		D3D12_RESOURCE_STATES initialState,
		const D3D12_CLEAR_VALUE* const optClear,
		LPCWSTR name = nullptr);
	BOOL OnResize(IDXGISwapChain* const swapChain, UINT index);
	__forceinline void Reset();
	void Swap(Microsoft::WRL::ComPtr<ID3D12Resource>& srcResource, D3D12_RESOURCE_STATES initialState);

	void Transite(ID3D12GraphicsCommandList* const cmdList, D3D12_RESOURCE_STATES state);

	__forceinline ID3D12Resource* const Resource() const;
	__forceinline D3D12_RESOURCE_DESC GetDesc() const;
	__forceinline D3D12_RESOURCE_STATES State() const;

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> mResource;

	D3D12_RESOURCE_STATES mCurrState;
};

void GpuResource::Reset() {
	mResource.Reset();
}

#include "GpuResource.inl"