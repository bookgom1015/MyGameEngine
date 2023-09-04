#include "GpuResource.h"
#include "Logger.h"
#include "D3D12Util.h"

bool GpuResource::Initialize(
		ID3D12Device* const device,
		const D3D12_HEAP_PROPERTIES* heapProp,
		D3D12_HEAP_FLAGS heapFlag,
		const D3D12_RESOURCE_DESC* rscDesc,
		D3D12_RESOURCE_STATES initialState,
		const D3D12_CLEAR_VALUE* optClear,
		LPCWSTR name) {
	CheckHRESULT(device->CreateCommittedResource(
		heapProp,
		heapFlag,
		rscDesc,
		initialState,
		optClear,
		IID_PPV_ARGS(&mResource)
	));
	if (name != nullptr) mResource->SetName(name);

	mCurrState = initialState;

	return true;
}

bool GpuResource::OnResize(IDXGISwapChain* const swapChain, UINT index) {
	CheckHRESULT(swapChain->GetBuffer(index, IID_PPV_ARGS(&mResource)));

	mCurrState = D3D12_RESOURCE_STATE_PRESENT;

	return true;
}

void GpuResource::Reset() {
	mResource.Reset();
}

void GpuResource::Transite(ID3D12GraphicsCommandList* const cmdList, D3D12_RESOURCE_STATES state) {
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mResource.Get(), mCurrState, state));

	if (mCurrState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS || state == D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		D3D12Util::UavBarrier(cmdList, mResource.Get());

	mCurrState = state;
}

ID3D12Resource* const GpuResource::Resource() const {
	return mResource.Get();
}