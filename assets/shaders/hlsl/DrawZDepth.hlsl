#ifndef __DRAWZDEPTH_HLSL__
#define __DRAWZDEPTH_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "./../../../include/Vertex.h"
#include "ShadingHelpers.hlsli"
#include "Samplers.hlsli"

ConstantBuffer<ConstantBuffer_Pass>		cb_Pass	: register(b0);
ConstantBuffer<ConstantBuffer_Object>	cb_Obj	: register(b1);
ConstantBuffer<ConstantBuffer_Material>	cb_Mat	: register(b2);

cbuffer cbRootConstants : register(b3) {
	uint gLightIndex;
}

Texture2D gi_TexMaps[NUM_TEXTURE_MAPS]	: register(t0);

VERTEX_IN

struct VertexOut {
	float4	PosW		: SV_POSITION;
	float2	TexC		: TEXCOORD;
};

struct GeoOut {
	float4	PosH		: SV_POSITION;
	float4	PosV		: POSITION;
	float2	TexC		: TEXCOORD;
	uint	ArrayIndex	: SV_RenderTargetArrayIndex;
};

struct PixelOut {
	Shadow::VSDepthCubeMapFormat	VSDepth	: SV_TARGET0;
	Shadow::FaceIDCubeMapFormat		FaceID	: SV_TARGET1;
};

VertexOut VS(VertexIn vin) {
	VertexOut vout = (VertexOut)0;

	vout.PosW = mul(float4(vin.PosL, 1), cb_Obj.World);

	float4 texC = mul(float4(vin.TexC, 0, 1), cb_Obj.TexTransform);
	vout.TexC = mul(texC, cb_Mat.MatTransform).xy;

	return vout;
}

float4x4 GetViewMatrix(Light light, int face) {
	switch (face) {
	case 0: return light.View0;
	case 1: return light.View1;
	case 2: return light.View2;
	case 3: return light.View3;
	case 4: return light.View4;
	case 5: return light.View5;
	default: return (float4x4)0;
	}
}

[maxvertexcount(18)]
void GS(triangle VertexOut gin[3], inout TriangleStream<GeoOut> triStream) {
	GeoOut gout = (GeoOut)0;
	
	Light light = cb_Pass.Lights[gLightIndex];

	if (light.Type == LightType::E_Point) {
		for (int face = 0; face < 6; ++face) {
			gout.ArrayIndex = face;

			const float4x4 view = GetViewMatrix(light, face);

			for (int i = 0; i < 3; ++i) {				
				const float4 posV = mul(gin[i].PosW, view);
				gout.PosV = posV;
				gout.PosH = mul(posV, light.Proj);

				gout.TexC = gin[i].TexC;

				triStream.Append(gout);
			}
			triStream.RestartStrip();
		}
	}
	else {
		[unroll]
		for (int i = 0; i < 3; ++i) {
			gout.PosH = mul(gin[i].PosW, light.View0);

			float4 texC = mul(float4(gin[i].TexC, 0, 1), cb_Obj.TexTransform);
			gout.TexC = mul(texC, cb_Mat.MatTransform).xy;

			triStream.Append(gout);
		}
	}
}

PixelOut PS(GeoOut pin) {
	PixelOut ps = (PixelOut)0;

	float4 albedo = cb_Mat.Albedo;
	if (cb_Mat.DiffuseSrvIndex != -1) albedo *= gi_TexMaps[cb_Mat.DiffuseSrvIndex].Sample(gsamAnisotropicWrap, pin.TexC);

#ifdef ALPHA_TEST
	// Discard pixel if texture alpha < 0.1.  We do this test as soon 
	// as possible in the shader so that we can potentially exit the
	// shader early, thereby skipping the rest of the shader code.
	clip(albedo.a - 0.1f);
#endif

	ps.VSDepth = pin.PosV.z;
	ps.FaceID = (float)pin.ArrayIndex;

	return ps;
}

#endif // __DRAWZDEPTH_HLSL__