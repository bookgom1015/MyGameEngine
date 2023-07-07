#ifndef __TEMPORALAA_INL__
#define __TEMPORALAA_INL__

constexpr UINT TemporalAA::TemporalAAClass::Width() const {
	return mWidth;
}

constexpr UINT TemporalAA::TemporalAAClass::Height() const {
	return mHeight;
}

GpuResource* TemporalAA::TemporalAAClass::ResolveMapResource() {
	return mResolveMap.get();
}

GpuResource* TemporalAA::TemporalAAClass::HistoryMapResource() {
	return mHistoryMap.get();
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