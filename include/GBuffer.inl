#ifndef __GBUFFER_INL__
#define __GBUFFER_INL__

constexpr UINT GBuffer::GBufferClass::Width() const {
	return mWidth;
}

constexpr UINT GBuffer::GBufferClass::Height() const {
	return mHeight;
}

GpuResource* GBuffer::GBufferClass::AlbedoMapResource() {
	return mAlbedoMap.get();
}

GpuResource* GBuffer::GBufferClass::NormalDepthMapResource() {
	return mNormalDepthMap.get();
}

GpuResource* GBuffer::GBufferClass::RMSMapResource() {
	return mRMSMap.get();
}

GpuResource* GBuffer::GBufferClass::VelocityMapResource() {
	return mVelocityMap.get();
}

GpuResource* GBuffer::GBufferClass::ReprojNormalDepthMapResource() {
	return mReprojNormalDepthMap.get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::AlbedoMapSrv() const {
	return mhAlbedoMapGpuSrv;
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::NormalDepthMapSrv() const {
	return mhNormalDepthMapGpuSrv;
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::DepthMapSrv() const {
	return mhDepthMapGpuSrv;
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::RMSMapSrv() const {
	return mhRMSMapGpuSrv;
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::VelocityMapSrv() const {
	return mhVelocityMapGpuSrv;
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::ReprojNormalDepthMapSrv() const {
	return mhReprojNormalDepthMapGpuSrv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::AlbedoMapRtv() const {
	return mhAlbedoMapCpuRtv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::NormalDepthMapRtv() const {
	return mhNormalDepthMapCpuRtv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::RMSMapRtv() const {
	return mhRMSMapCpuRtv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::VelocityMapRtv() const {
	return mhVelocityMapCpuRtv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::ReprojNormalDepthMapRtv() const {
	return mhReprojNormalDepthMapCpuRtv;
}

#endif // __GBUFFER_INL__