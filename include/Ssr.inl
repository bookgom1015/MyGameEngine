#ifndef __SSR_INL__
#define __SSR_INL__

GpuResource* Ssr::SsrClass::SsrMapResource(UINT index) {
	return mSsrMaps[index].get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE Ssr::SsrClass::SsrMapSrv(UINT index) const {
	return mhSsrMapGpuSrvs[index];
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE Ssr::SsrClass::SsrMapRtv(UINT index) const {
	return mhSsrMapCpuRtvs[index];
}

#endif // __SSR_INL__