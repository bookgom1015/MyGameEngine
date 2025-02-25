#ifndef __BLOOM_INL__
#define __BLOOM_INL__

GpuResource* Bloom::BloomClass::BloomMapResource(UINT index) {
	return mBloomMaps[index].get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE Bloom::BloomClass::BloomMapSrv(UINT index) const {
	return mhBloomMapGpuSrvs[index];
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE Bloom::BloomClass::BloomMapRtv(UINT index) const {
	return mhBloomMapCpuRtvs[index];
}

#endif // __BLOOM_INL__