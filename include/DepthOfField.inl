#ifndef __DEPTHOFFIELD_INL__
#define __DEPTHOFFIELD_INL__

GpuResource* DepthOfField::DepthOfFieldClass::CocMapResource() {
	return mCocMap.get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE DepthOfField::DepthOfFieldClass::CocMapSrv() const {
	return mhCocMapGpuSrv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE DepthOfField::DepthOfFieldClass::CocMapRtv() const {
	return mhCocMapCpuRtv;
}

GpuResource* DepthOfField::DepthOfFieldClass::FocalDistanceBufferResource() {
	return mFocalDistanceBuffer.get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE DepthOfField::DepthOfFieldClass::FocalDistanceBufferUav() const {
	return mhFocalDistanceGpuUav;
}

#endif // __DEPTHOFFIELD_INL__