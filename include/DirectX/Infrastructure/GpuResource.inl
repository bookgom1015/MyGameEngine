#ifndef __GPURESOURCE_INL__
#define __GPURESOURCE_INL__

ID3D12Resource* const GpuResource::Resource() const {
	return mResource.Get();
}

D3D12_RESOURCE_DESC GpuResource::GetDesc() const {
	return mResource->GetDesc();
}

D3D12_RESOURCE_STATES GpuResource::State() const {
	return mCurrState;
}

#endif // __GPURESOURCE_INL__