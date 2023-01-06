#include "Renderer.h"

#include <assert.h>
#include <algorithm>
#include <memory>
#include <wrl.h>

void Renderer::SetCamera(Camera* cam) {
	mCamera = cam;
}

void Renderer::EnableDebugging(bool state) {
	bDebuggingEnabled = state;
}

void Renderer::EnableShadow(bool state) {
	bShadowEnabled = state;
}

void Renderer::EnableSsao(bool state) {
	bSsaoEnabled = state;
}

void Renderer::EnableTaa(bool state) {
	if (!bTaaEnabled && state) bInitiatingTaa = true;
	bTaaEnabled = state;
}

void Renderer::EnableMotionBlur(bool state) {
	bMotionBlurEnabled = state;
}

void Renderer::EnableDepthOfField(bool state) {
	bDepthOfFieldEnabled = state;
}

void Renderer::ShowImGui(bool state) {
	bShowImGui = state;
}