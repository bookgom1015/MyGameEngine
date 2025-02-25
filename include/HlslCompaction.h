#ifndef __HLSLCOMPACTION_H__
#define __HLSLCOMPACTION_H__

#ifdef HLSL
#include "HlslCompaction.hlsli"
#include "./../../../include/Common/Mesh/Vertex.h"
#else
#include <DirectXMath.h>
#include <dxgiformat.h>
#include <windef.h>
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

#include "DirectX/Infrastructure/ShadingConvention.h"
#include "DirectX/Infrastructure/ConstantsBufferDeclarations.h"

#endif // __HLSLCOMPACTION_HLSL__