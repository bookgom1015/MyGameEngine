#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "Common.glsli"

layout (location = 0) in vec3 iPosL;
layout (location = 1) in vec3 iNormalL;
layout (location = 2) in vec2 iTexC;

layout (location = 0) out vec2 oTexC;

void main() {
	gl_Position = ubp.Proj * ubp.View * ubo.model * vec4(iPosL, 1.0);
	
	oTexC = iTexC;
}