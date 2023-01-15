#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "Common.glsli"

const vec2 gTexCoords[6] = {
	vec2(0.0, 1.0),
	vec2(0.0, 0.0),
	vec2(1.0, 0.0),
	vec2(0.0, 1.0),
	vec2(1.0, 0.0),
	vec2(1.0, 1.0)
};

const vec2 positions[3] = {
	vec2(0.0, -1.0),
	vec2(1.0, 1.0),
	vec2(-1.0, 1.0)
};

layout (location = 0) out vec2 oTexC;
layout (location = 1) out int oInstID;

void main() {
	vec2 texC = gTexCoords[gl_VertexIndex];
	int instID = gl_InstanceIndex;

	vec2 pos = vec2(2.0 * texC.x - 1.0, 2.0 * texC.y - 1.0);

	if (instID == 0) pos = pos * 0.2 + vec2(0.8, -0.8);
	else if (instID == 1) pos = pos * 0.2 + vec2(0.8, -0.4);
	else if (instID == 2) pos = pos * 0.2 + vec2(0.8, 0.0);
	else if (instID == 3) pos = pos * 0.2 + vec2(0.8, 0.4);
	else if (instID == 4) pos = pos * 0.2 + vec2(0.8, 0.8);

	oTexC = texC;
	oInstID = instID;

	gl_Position = vec4(pos, 0.0, 1.0);	
}