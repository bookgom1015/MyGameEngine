#ifndef __DEBUGMAP_INL__
#define __DEBUGMAP_INL__

constexpr DebugSampleDesc Debug::DebugClass::SampleDesc(UINT index) const {
	return mSampleDescs[index];
}

#endif // __DEBUGMAP_INL__