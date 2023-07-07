#ifndef __DEPTHOFFIELD_INL__
#define __DEPTHOFFIELD_INL__

constexpr UINT DepthOfField::DepthOfFieldClass::CocMapWidth() const {
	return mWidth;
}

constexpr UINT DepthOfField::DepthOfFieldClass::CocMapHeight() const {
	return mHeight;
}

constexpr UINT DepthOfField::DepthOfFieldClass::DofMapWidth() const {
	return mWidth;
}

constexpr UINT DepthOfField::DepthOfFieldClass::DofMapHeight() const {
	return mHeight;
}

GpuResource* DepthOfField::DepthOfFieldClass::CocMapResource() {
	return mCocMap.get();
}

GpuResource* DepthOfField::DepthOfFieldClass::DofMapResource() {
	return mDofMaps[0].get();
}

GpuResource* DepthOfField::DepthOfFieldClass::FocalDistanceBufferResource() {
	return mFocalDistanceBuffer.get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE DepthOfField::DepthOfFieldClass::CocMapSrv() const {
	return mhCocMapGpuSrv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE DepthOfField::DepthOfFieldClass::CocMapRtv() const {
	return mhCocMapCpuRtv;
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE DepthOfField::DepthOfFieldClass::DofMapSrv() const {
	return mhDofMapGpuSrvs[0];
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE DepthOfField::DepthOfFieldClass::DofMapRtv() const {
	return mhDofMapCpuRtvs[0];
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE DepthOfField::DepthOfFieldClass::FocalDistanceBufferUav() const {
	return mhFocalDistanceGpuUav;
}

#endif // __DEPTHOFFIELD_INL__