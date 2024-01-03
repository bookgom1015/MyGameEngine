#ifndef __RaytracedReflection_INL__
#define __RaytracedReflection_INL__

D3D12_GPU_DESCRIPTOR_HANDLE RaytracedReflection::RaytracedReflectionClass::ReflectionMapSrv() const {
	return mhReflectionMapGpus[RaytracedReflection::Descriptor::Reflection::ES_Reflection];
}

const RaytracedReflection::ReflectionType& RaytracedReflection::RaytracedReflectionClass::Reflections() const {
	return mReflectionMaps;
}

const RaytracedReflection::ReflectionGpuDescriptors& RaytracedReflection::RaytracedReflectionClass::ReflectionGpuDescriptors() const {
	return mhReflectionMapGpus;
}

const RaytracedReflection::TemporalCachesType& RaytracedReflection::RaytracedReflectionClass::TemporalCaches() const {
	return mTemporalCaches;
}

const RaytracedReflection::TemporalCachesGpuDescriptors& RaytracedReflection::RaytracedReflectionClass::TemporalCacheGpuDescriptors() const {
	return mhTemporalCachesGpus;
}

const RaytracedReflection::TemporalReflectionType& RaytracedReflection::RaytracedReflectionClass::TemporalReflections() {
	return mTemporalReflectionMaps;
}

const RaytracedReflection::TemporalReflectionGpuDescriptors& RaytracedReflection::RaytracedReflectionClass::TemporalReflectionGpuDescriptors() const {
	return mhTemporalReflectionMapGpus;
}

constexpr UINT RaytracedReflection::RaytracedReflectionClass::TemporalCurrentFrameResourceIndex() const {
	return mTemporalCurrentFrameResourceIndex;
}

constexpr UINT RaytracedReflection::RaytracedReflectionClass::TemporalCurrentFrameTemporalReflectionResourceIndex() const {
	return mTemporalCurrentFrameTemporalReflectionResourceIndex;
}

#endif // __RaytracedReflection_INL__