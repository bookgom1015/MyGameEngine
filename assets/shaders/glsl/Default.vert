#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "Common.glsli"

layout (location = 0) in vec3 iPosL;
layout (location = 1) in vec3 iNormalL;
layout (location = 2) in vec2 iTexC;

layout (location = 0) out vec3 oPosW;
layout (location = 1) out vec3 oNormalW;
layout (location = 2) out vec2 oTexC;
layout (location = 3) out vec4 oShadowPosH;

void main() {
	gl_Position = ubp.Proj * ubp.View * ubo.model * vec4(iPosL, 1.0);

	vec4 posW = ubo.model * vec4(iPosL, 1.0);
	oPosW = posW.xyz;

	oNormalW = mat3(ubo.model) * iNormalL;

	oTexC = iTexC;

	oShadowPosH = ubp.ShadowTransform * posW;
}