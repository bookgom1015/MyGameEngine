#pragma once

#include <d3dx12.h>
#include <dxgi1_6.h>
#include <wrl.h>

class GpuResource {
public:
	GpuResource() = default;
	virtual ~GpuResource() = default;

public:
	bool Initialize(
		ID3D12Device*const device, 
		const D3D12_HEAP_PROPERTIES* heapProp, 
		D3D12_HEAP_FLAGS heapFlag,
		const D3D12_RESOURCE_DESC* rscDesc,
		D3D12_RESOURCE_STATES initialState,
		const D3D12_CLEAR_VALUE* optClear,
		LPCWSTR name = nullptr);
	bool OnResize(IDXGISwapChain*const swapChain, UINT index);
	__forceinline void Reset();
	void Swap(Microsoft::WRL::ComPtr<ID3D12Resource>& srcResource, D3D12_RESOURCE_STATES initialState);

	void Transite(ID3D12GraphicsCommandList*const cmdList, D3D12_RESOURCE_STATES state);

	__forceinline ID3D12Resource*const Resource() const;
	__forceinline D3D12_RESOURCE_DESC GetDesc() const;
	__forceinline D3D12_RESOURCE_STATES State() const;

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> mResource;

	D3D12_RESOURCE_STATES mCurrState;
};

void GpuResource::Reset() {
	mResource.Reset();
}

ID3D12Resource* const GpuResource::Resource() const {
	return mResource.Get();
}

D3D12_RESOURCE_DESC GpuResource::GetDesc() const {
	return mResource->GetDesc();
}

D3D12_RESOURCE_STATES GpuResource::State() const {
	return mCurrState;
}