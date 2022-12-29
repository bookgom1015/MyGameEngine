#ifndef __TEMPORALAA_INL__
#define __TEMPORALAA_INL__

constexpr UINT TemporalAA::Width() const {
	return mWidth;
}

constexpr UINT TemporalAA::Height() const {
	return mHeight;
}

ID3D12Resource* TemporalAA::ResolveMapResource() {
	return mResolveMap.Get();
}

ID3D12Resource* TemporalAA::HistoryMapResource() {
	return mHistoryMap.Get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE TemporalAA::ResolveMapSrv() const {
	return mhResolveMapGpuSrv;
}
constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE TemporalAA::ResolveMapRtv() const {
	return mhResolveMapCpuRtv;
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE TemporalAA::HistoryMapSrv() const {
	return mhHistoryMapGpuSrv;
}

#endif // __TEMPORALAA_INL__