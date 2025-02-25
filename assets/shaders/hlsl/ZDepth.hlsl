// [ References ]
//  - https://learnopengl.com/Advanced-Lighting/Shadows/Point-Shadows

#ifndef __ZDEPTH_HLSL__
#define __ZDEPTH_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "ShadingHelpers.hlsli"
#include "Samplers.hlsli"

ConstantBuffer<ConstantBuffer_Pass>		cb_Pass	: register(b0);
ConstantBuffer<ConstantBuffer_Object>	cb_Obj	: register(b1);
ConstantBuffer<ConstantBuffer_Material>	cb_Mat	: register(b2);

Shadow_ZDepth_RootConstants(b3)

Texture2D gi_TexMaps[NUM_TEXTURE_MAPS] : register(t0);

VERTEX_IN

struct VertexOut {
	float4	PosW		: SV_POSITION;
	float2	TexC		: TEXCOORD;
};

struct GeoOut {
	float4	PosH		: SV_POSITION;
	float2	TexC		: TEXCOORD;
	uint	ArrayIndex	: SV_RenderTargetArrayIndex;
};

VertexOut VS(VertexIn vin) {
	VertexOut vout = (VertexOut)0;

	vout.PosW = mul(float4(vin.PosL, 1.f), cb_Obj.World);

	const float4 texC = mul(float4(vin.TexC, 0.f, 1.f), cb_Obj.TexTransform);
	vout.TexC = mul(texC, cb_Mat.MatTransform).xy;

	return vout;
}

float4x4 GetViewProjMatrix(in Light light, in uint face) {
	switch (face) {
	case 0: return light.Mat0;
	case 1: return light.Mat1;
	case 2: return light.Mat2;
	case 3: return light.Mat3;
	case 4: return light.Mat4;
	case 5: return light.Mat5;
	default: return (float4x4)0.f;
	}
}

[maxvertexcount(18)]
void GS(triangle VertexOut gin[3], inout TriangleStream<GeoOut> triStream) {
	GeoOut gout = (GeoOut)0;
	
	Light light = cb_Pass.Lights[gLightIndex];

	// Directional light
	if (light.Type == LightType::E_Directional) {
		[unroll]
		for (uint i = 0; i < 3; ++i) {
			gout.PosH = mul(gin[i].PosW, light.Mat0);

			const float4 texc = mul(float4(gin[i].TexC, 0.f, 1.f), cb_Obj.TexTransform);
			gout.TexC = mul(texc, cb_Mat.MatTransform).xy;

			triStream.Append(gout);
		}
	}
	// Point light or Spot light
	else {
		[loop]
		for (uint face = 0; face < 6; ++face) {
			gout.ArrayIndex = face;

			const float4x4 viewProj = GetViewProjMatrix(light, face);

			[unroll]
			for (uint i = 0; i < 3; ++i) {				
				gout.PosH = mul(gin[i].PosW, viewProj);
				gout.TexC = gin[i].TexC;

				triStream.Append(gout);
			}

			triStream.RestartStrip();
		}
	}	
}

Shadow::FaceIDCubeMapFormat PS(GeoOut pin) : SV_Target {
	float4 albedo = cb_Mat.Albedo;
	if (cb_Mat.DiffuseSrvIndex != -1) albedo *= gi_TexMaps[cb_Mat.DiffuseSrvIndex].Sample(gsamAnisotropicWrap, pin.TexC);

#ifdef ALPHA_TEST
	// Discard pixel if texture alpha < 0.1.  We do this test as soon 
	// as possible in the shader so that we can potentially exit the
	// shader early, thereby skipping the rest of the shader code.
	clip(albedo.a - 0.1f);
#endif

	return (float)pin.ArrayIndex;
}

#endif // __ZDEPTH_HLSL__