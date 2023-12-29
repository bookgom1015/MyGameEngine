#ifndef __RTAO_INL__
#define __RTAO_INL__

const Rtao::AOResourcesType& Rtao::RtaoClass::AOResources() const {
	return mAOResources;
}
const Rtao::AOResourcesGpuDescriptors& Rtao::RtaoClass::AOResourcesGpuDescriptors() const {
	return mhAOResourcesGpus;
}

const Rtao::TemporalCachesType& Rtao::RtaoClass::TemporalCaches() const {
	return mTemporalCaches;
}
const Rtao::TemporalCachesGpuDescriptors& Rtao::RtaoClass::TemporalCachesGpuDescriptors() const {
	return mhTemporalCachesGpus;
}

const Rtao::TemporalAOCoefficientsType& Rtao::RtaoClass::TemporalAOCoefficients() {
	return mTemporalAOCoefficients;
}
const Rtao::TemporalAOCoefficientsGpuDescriptors& Rtao::RtaoClass::TemporalAOCoefficientsGpuDescriptors() const {
	return mhTemporalAOCoefficientsGpus;
}

constexpr UINT Rtao::RtaoClass::TemporalCurrentFrameResourceIndex() const {
	return mTemporalCurrentFrameResourceIndex;
}

constexpr UINT Rtao::RtaoClass::TemporalCurrentFrameTemporalAOCoefficientResourceIndex() const {
	return mTemporalCurrentFrameTemporalAOCeofficientResourceIndex;
}

constexpr CD3DX12_GPU_DESCRIPTOR_HANDLE Rtao::RtaoClass::ResolvedAOCoefficientSrv() const {
	return mhTemporalAOCoefficientsGpus[mTemporalCurrentFrameTemporalAOCeofficientResourceIndex][Rtao::Descriptor::TemporalAOCoefficient::Srv];
}

#endif // __RTAO_INL__