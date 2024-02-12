#ifndef __SSR_INL__
#define __SSR_INL__

GpuResource* SSR::SSRClass::SSRMapResource(UINT index) {
	return mSSRMaps[index].get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE SSR::SSRClass::SSRMapSrv(UINT index) const {
	return mhSSRMapGpuSrvs[index];
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE SSR::SSRClass::SSRMapRtv(UINT index) const {
	return mhSSRMapCpuRtvs[index];
}

#endif // __SSR_INL__