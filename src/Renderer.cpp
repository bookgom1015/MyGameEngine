#include "Renderer.h"

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