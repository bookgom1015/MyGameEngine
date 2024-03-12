#ifndef __SSAO_INL__
#define __SSAO_INL__

constexpr D3D12_VIEWPORT SSAO::SSAOClass::Viewport() const {
	return mViewport;
}

constexpr D3D12_RECT SSAO::SSAOClass::ScissorRect() const {
	return mScissorRect;
}

GpuResource* SSAO::SSAOClass::AOCoefficientMapResource(UINT index) {
	return mAOCoefficientMaps[index].get();
}

GpuResource* SSAO::SSAOClass::RandomVectorMapResource() {
	return mRandomVectorMap.get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE SSAO::SSAOClass::AOCoefficientMapSrv(UINT index) const {
	return mhAOCoefficientMapGpuSrvs[index];
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE SSAO::SSAOClass::AOCoefficientMapRtv(UINT index) const {
	return mhAOCoefficientMapCpuRtvs[index];
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE SSAO::SSAOClass::RandomVectorMapSrv() const {
	return mhRandomVectorMapGpuSrv;
}

#endif // __SSAO_INL__