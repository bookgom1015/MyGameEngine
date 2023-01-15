#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "Common.glsli"

layout (location = 0) in vec2 iTexC;
layout (location = 1) in flat int iInstID;

layout (location = 0) out vec4 oColor;

void main() {
	oColor = vec4(texture(sShadowMap, iTexC).rrr, 1.0);
}