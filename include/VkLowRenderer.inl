#ifndef __VKLOWRENDERER_INL__
#define __VKLOWRENDERER_INL__

constexpr VkSampleCountFlagBits VkLowRenderer::GetMSAASamples() const {
	return mMSAASamples;
}

#endif // __VKLOWRENDERER_INL__