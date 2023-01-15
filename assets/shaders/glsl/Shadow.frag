#version 450
#extension GL_ARB_separate_shader_objects : enable

#ifndef NUM_DIR_LIGHTS
#define NUM_DIR_LIGHTS 1
#endif

#ifndef NUM_POINT_LIGHTS
#define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
#define NUM_SPOT_LIGHTS 0
#endif

#include "Common.glsli"

layout (location = 0) in vec2 iTexC;

void main() {
	//vec4 diffuse = texture(texSampler, iTexC);
	//if (diffuse.a < 0.1f) discard;
}