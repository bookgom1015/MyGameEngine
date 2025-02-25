#ifndef __TEMPORALAA_INL__
#define __TEMPORALAA_INL__


GpuResource* TemporalAA::TemporalAAClass::HistoryMapResource() {
	return mHistoryMap.get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE TemporalAA::TemporalAAClass::HistoryMapSrv() const {
	return mhHistoryMapGpuSrv;
}

#endif // __TEMPORALAA_INL__