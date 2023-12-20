#ifndef __GBUFFER_INL__
#define __GBUFFER_INL__

GpuResource* GBuffer::GBufferClass::AlbedoMapResource() {
	return mAlbedoMap.get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::AlbedoMapSrv() const {
	return mhAlbedoMapGpuSrv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::AlbedoMapRtv() const {
	return mhAlbedoMapCpuRtv;
}

GpuResource* GBuffer::GBufferClass::NormalMapResource() {
	return mNormalMap.get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::NormalMapSrv() const {
	return mhNormalMapGpuSrv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::NormalMapRtv() const {
	return mhNormalMapCpuRtv;
}

GpuResource* GBuffer::GBufferClass::NormalDepthMapResource() {
	return mNormalDepthMap.get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::NormalDepthMapSrv() const {
	return mhNormalDepthMapGpuSrv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::NormalDepthMapRtv() const {
	return mhNormalDepthMapCpuRtv;
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::DepthMapSrv() const {
	return mhDepthMapGpuSrv;
}

GpuResource* GBuffer::GBufferClass::RMSMapResource() {
	return mRMSMap.get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::RMSMapSrv() const {
	return mhRMSMapGpuSrv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::RMSMapRtv() const {
	return mhRMSMapCpuRtv;
}

GpuResource* GBuffer::GBufferClass::VelocityMapResource() {
	return mVelocityMap.get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::VelocityMapSrv() const {
	return mhVelocityMapGpuSrv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::VelocityMapRtv() const {
	return mhVelocityMapCpuRtv;
}

GpuResource* GBuffer::GBufferClass::ReprojNormalDepthMapResource() {
	return mReprojNormalDepthMap.get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::ReprojNormalDepthMapSrv() const {
	return mhReprojNormalDepthMapGpuSrv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::ReprojNormalDepthMapRtv() const {
	return mhReprojNormalDepthMapCpuRtv;
}

GpuResource* GBuffer::GBufferClass::PositionMapResource() {
	return mPositionMap.get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::PositionMapSrv() const {
	return mhPositionMapGpuSrv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::PositionMapRtv() const {
	return mhPositionMapCpuRtv;
}

#endif // __GBUFFER_INL__