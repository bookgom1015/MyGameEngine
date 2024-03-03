#ifndef __DEBUGCOLLISION_HLSL__
#define __DEBUGCOLLISION_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"

ConstantBuffer<ConstantBuffer_Pass>	cb_Pass	: register(b0);
ConstantBuffer<ObjectConstants>		cb_Obj	: register(b1);

struct GeoOut {
	float4 PosH    : SV_POSITION;
	uint   PrimID  : SV_PrimitiveID;
};

void VS(uint vid : SV_VertexID) {}

#define NUM 16

[maxvertexcount(NUM)]
void GS(inout TriangleStream<GeoOut> triStream) {	
	float3 center = cb_Obj.Center.xyz;
	float3 extents = cb_Obj.Extents.xyz;

	const float3 V0 = extents;
	const float3 V1 = float3(-extents.x, extents.yz);
	const float3 V2 = float3(extents.x, -extents.y, extents.z);
	const float3 V3 = float3(extents.xy, -extents.z);
	const float3 V4 = float3(-extents.xy, extents.z);
	const float3 V5 = float3(-extents.x, extents.y, -extents.z);
	const float3 V6 = float3(extents.x, -extents.yz);
	const float3 V7 = -extents;

	float3 offsets[NUM] = { V0, V2, V3, V6, V5, V7, V6, V4, V2, V0, V4, V1, V7, V5, V1, V3 };
	
	GeoOut gout = (GeoOut)0;
	for (int i = 0; i < NUM; ++i) {
		float3 posL = center + offsets[i];
		float4 posW = mul(float4(posL, 1), cb_Obj.World);
	
		gout.PosH = mul(posW, cb_Pass.ViewProj);
	
		triStream.Append(gout);
	}
}

float4 PS(GeoOut pin) : SV_Target{	
	return float4(0, 1, 0, 1);
}

#endif // __DEBUGCOLLISION_HLSL__