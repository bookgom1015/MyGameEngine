#ifndef __SHADOW_HLSL__
#define __SHADOW_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "./../../../include/Vertex.h"
#include "ShadingHelpers.hlsli"
#include "Samplers.hlsli"

[maxvertexcount(3)]
void GS(inout TriangleStream<GeoOut> triStream) {

}

#endif // __SHADOW_HLSL__