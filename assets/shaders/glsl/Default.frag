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

layout (location = 0) in vec3 iPosW;
layout (location = 1) in vec3 iNormalW;
layout (location = 2) in vec2 iTexC;
layout (location = 3) in vec4 iShadowPosH;

layout (location = 0) out vec4 oColor;

void main() {
	vec4 diffuse = texture(sOutputMap, iTexC);
	//if (diffuse.a < 0.1) discard;

	vec3 normalW = normalize(iNormalW);

	vec3 toEyeW = normalize(ubp.EyePosW - iPosW);

	vec4 ambient = ubp.AmbientLight * diffuse;

	const float shiness = 1.0 - 0.5;
	Material mat = { vec4(1.0), vec3(0.5), shiness };

	vec3 shadowFactor = vec3(1.0);
	//shadowFactor[0] = CalcShadowFactor(iShadowPosH);

	vec4 directLight = ComputeLighting(ubp.Lights, mat, iPosW, normalW, toEyeW, shadowFactor);

	vec4 litColor = ambient + directLight;
	litColor.a = diffuse.a;

	oColor = litColor;
}