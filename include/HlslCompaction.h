#ifndef __HLSLCOMPACTION_H__
#define __HLSLCOMPACTION_H__

#ifdef HLSL
#include "HlslCompaction.hlsli"
#include "./../../../include/Vertex.h"
#else
#include <DirectXMath.h>
#include <Windows.h>
#include <dxgiformat.h>
#endif

#ifndef NUM_TEXTURE_MAPS 
#define NUM_TEXTURE_MAPS 64
#endif

#ifndef MAX_DESCRIPTOR_SIZE
#define MAX_DESCRIPTOR_SIZE 256
#endif

#ifdef HLSL
struct Material {
	float4 Albedo;
	float3 FresnelR0;
	float Shininess;
	float Metalic;
};
#endif

#include "ShaderArguments.h"
#include "ConstantsBufferDeclarations.h"

#endif // __HLSLCOMPACTION_HLSL__