#ifndef __RTAO_INL__
#define __RTAO_INL__

const RTAO::AOResourcesType& RTAO::RTAOClass::AOResources() const {
	return mAOResources;
}
const RTAO::AOResourcesGpuDescriptors& RTAO::RTAOClass::AOResourceGpuDescriptors() const {
	return mhAOResourcesGpus;
}

const RTAO::TemporalCachesType& RTAO::RTAOClass::TemporalCaches() const {
	return mTemporalCaches;
}
const RTAO::TemporalCachesGpuDescriptors& RTAO::RTAOClass::TemporalCacheGpuDescriptors() const {
	return mhTemporalCachesGpus;
}

const RTAO::TemporalAOCoefficientsType& RTAO::RTAOClass::TemporalAOCoefficients() {
	return mTemporalAOCoefficients;
}
const RTAO::TemporalAOCoefficientsGpuDescriptors& RTAO::RTAOClass::TemporalAOCoefficientGpuDescriptors() const {
	return mhTemporalAOCoefficientsGpus;
}

constexpr UINT RTAO::RTAOClass::TemporalCurrentFrameResourceIndex() const {
	return mTemporalCurrentFrameResourceIndex;
}

constexpr UINT RTAO::RTAOClass::TemporalCurrentFrameTemporalAOCoefficientResourceIndex() const {
	return mTemporalCurrentFrameTemporalAOCeofficientResourceIndex;
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE RTAO::RTAOClass::ResolvedAOCoefficientSrv() const {
	return mhTemporalAOCoefficientsGpus[mTemporalCurrentFrameTemporalAOCeofficientResourceIndex][RTAO::Descriptor::TemporalAOCoefficient::Srv];
}

#endif // __RTAO_INL__