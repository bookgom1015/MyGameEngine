#ifndef __GBUFFER_INL__
#define __GBUFFER_INL__

constexpr UINT GBuffer::Width() const {
	return mWidth;
}

constexpr UINT GBuffer::Height() const {
	return mHeight;
}

ID3D12Resource* GBuffer::ColorMapResource() {
	return mColorMap.Get();
}

ID3D12Resource* GBuffer::AlbedoMapResource() {
	return mAlbedoMap.Get();
}

ID3D12Resource* GBuffer::NormalMapResource() {
	return mNormalMap.Get();
}

ID3D12Resource* GBuffer::SpecularMapResource() {
	return mSpecularMap.Get();
}

ID3D12Resource* GBuffer::VelocityMapResource() {
	return mVelocityMap.Get();
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE GBuffer::ColorMapSrv() const {
	return mhColorMapGpuSrv;
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE GBuffer::AlbedoMapSrv() const {
	return mhAlbedoMapGpuSrv;
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE GBuffer::NormalMapSrv() const {
	return mhNormalMapGpuSrv;
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE GBuffer::DepthMapSrv() const {
	return mhDepthMapGpuSrv;
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE GBuffer::SpecularMapSrv() const {
	return mhSpecularMapGpuSrv;
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE GBuffer::VelocityMapSrv() const {
	return mhVelocityMapGpuSrv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE GBuffer::ColorMapRtv() const {
	return mhColorMapCpuRtv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE GBuffer::AlbedoMapRtv() const {
	return mhAlbedoMapCpuRtv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE GBuffer::NormalMapRtv() const {
	return mhNormalMapCpuRtv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE GBuffer::SpecularMapRtv() const {
	return mhSpecularMapCpuRtv;
}

constexpr CD3DX12_CPU_DESCRIPTOR_HANDLE GBuffer::VelocityMapRtv() const {
	return mhVelocityMapCpuRtv;
}

#endif // __GBUFFER_INL__