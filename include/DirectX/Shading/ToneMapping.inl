#ifndef _TONEMAPPING_INL__
#define _TONEMAPPING_INL__

GpuResource* ToneMapping::ToneMappingClass::InterMediateMapResource() {
	return mIntermediateMap.get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE ToneMapping::ToneMappingClass::InterMediateMapSrv() const {
	return mhIntermediateMapGpuSrv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE ToneMapping::ToneMappingClass::InterMediateMapRtv() const {
	return mhIntermediateMapCpuRtv;
}

#endif // _TONEMAPPING_INL__