#ifndef __SKYSPHERE_HLSL__
#define __SKYSPHERE_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "./../../../include/Vertex.h"
#include "ShadingHelpers.hlsli"
#include "Samplers.hlsli"

ConstantBuffer<ConstantBuffer_Pass>			 cb_Pass	: register(b0);
ConstantBuffer<ConstantBuffer_Object>		 cb_Obj		: register(b1);

TextureCube<IrradianceMap::EnvCubeMapFormat> gi_Cube	: register(t0);

VERTEX_IN

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float3 PosL		: POSITION;
};

VertexOut VS(VertexIn vin) {
	VertexOut vout;

	// Use local vertex position as cubemap lookup vector.
	vout.PosL = vin.PosL;

	// Transform to world space.
	float4 posW = mul(float4(vin.PosL, 1.f), cb_Obj.World);
	// Always center sky about camera.
	posW.xyz += cb_Pass.EyePosW;

	// Set z = w so that z/w = 1 (i.e., skydome always on far plane).
	vout.PosH = mul(posW, cb_Pass.ViewProj).xyww;

	return vout;
}

HDR_FORMAT PS(VertexOut pin) : SV_Target {
	return gi_Cube.SampleLevel(gsamLinearWrap, normalize(pin.PosL), 0);
}

#endif // __SKYSPHERE_HLSL__