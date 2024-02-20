#ifndef __SHADERARGUMENTS_INL__
#define __SHADERARGUMENTS_INL__

#ifdef HLSL
bool GBuffer::IsInvalidVelocity(float2 val) {
	return val.x > 100.0f;
}

bool GBuffer::IsValidVelocity(float2 val) {
	return val.x <= 100.0f;
}

bool GBuffer::IsInvalidDepth(float val) {
	return val == InvalidDepthValue;
}

bool GBuffer::IsValidDepth(float val) {
	return val != InvalidDepthValue;
}

bool GBuffer::IsInvalidPosition(float4 val) {
	return val.w == -1.0f;
}

bool GBuffer::IsValidPosition(float4 val) {
	return val.w != -1.0f;
}

#endif

#endif // __SHADERARGUMENTS_INL__