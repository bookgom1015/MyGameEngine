#ifndef __DEPTHOFFIELD_INL__
#define __DEPTHOFFIELD_INL__

GpuResource* DepthOfField::DepthOfFieldClass::CoCMapResource() {
	return mCoCMap.get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE DepthOfField::DepthOfFieldClass::CoCMapSrv() const {
	return mhCoCMapGpuSrv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE DepthOfField::DepthOfFieldClass::CoCMapRtv() const {
	return mhCoCMapCpuRtv;
}

GpuResource* DepthOfField::DepthOfFieldClass::FocalDistanceBufferResource() {
	return mFocalDistanceBuffer.get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE DepthOfField::DepthOfFieldClass::FocalDistanceBufferUav() const {
	return mhFocalDistanceGpuUav;
}

#endif // __DEPTHOFFIELD_INL__