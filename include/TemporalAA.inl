#ifndef __TEMPORALAA_INL__
#define __TEMPORALAA_INL__

constexpr UINT TemporalAA::TemporalAAClass::Width() const {
	return mWidth;
}

constexpr UINT TemporalAA::TemporalAAClass::Height() const {
	return mHeight;
}

ID3D12Resource* TemporalAA::TemporalAAClass::ResolveMapResource() {
	return mResolveMap.Get();
}

ID3D12Resource* TemporalAA::TemporalAAClass::HistoryMapResource() {
	return mHistoryMap.Get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE TemporalAA::TemporalAAClass::ResolveMapSrv() const {
	return mhResolveMapGpuSrv;
}
constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE TemporalAA::TemporalAAClass::ResolveMapRtv() const {
	return mhResolveMapCpuRtv;
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE TemporalAA::TemporalAAClass::HistoryMapSrv() const {
	return mhHistoryMapGpuSrv;
}

#endif // __TEMPORALAA_INL__