#ifndef __GBUFFER_INL__
#define __GBUFFER_INL__

constexpr UINT GBuffer::GBufferClass::Width() const {
	return mWidth;
}

constexpr UINT GBuffer::GBufferClass::Height() const {
	return mHeight;
}

ID3D12Resource* GBuffer::GBufferClass::ColorMapResource() {
	return mColorMap.Get();
}

ID3D12Resource* GBuffer::GBufferClass::AlbedoMapResource() {
	return mAlbedoMap.Get();
}

ID3D12Resource* GBuffer::GBufferClass::NormalMapResource() {
	return mNormalMap.Get();
}

ID3D12Resource* GBuffer::GBufferClass::SpecularMapResource() {
	return mSpecularMap.Get();
}

ID3D12Resource* GBuffer::GBufferClass::VelocityMapResource() {
	return mVelocityMap.Get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::ColorMapSrv() const {
	return mhColorMapGpuSrv;
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::AlbedoMapSrv() const {
	return mhAlbedoMapGpuSrv;
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::NormalMapSrv() const {
	return mhNormalMapGpuSrv;
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::DepthMapSrv() const {
	return mhDepthMapGpuSrv;
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::SpecularMapSrv() const {
	return mhSpecularMapGpuSrv;
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::VelocityMapSrv() const {
	return mhVelocityMapGpuSrv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::ColorMapRtv() const {
	return mhColorMapCpuRtv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::AlbedoMapRtv() const {
	return mhAlbedoMapCpuRtv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::NormalMapRtv() const {
	return mhNormalMapCpuRtv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::SpecularMapRtv() const {
	return mhSpecularMapCpuRtv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE GBuffer::GBufferClass::VelocityMapRtv() const {
	return mhVelocityMapCpuRtv;
}

#endif // __GBUFFER_INL__