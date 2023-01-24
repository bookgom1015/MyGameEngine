#ifndef __APPLYINGSSR_HLSL__
#define __APPLYINGSSR_HLSL__

#include "Samplers.hlsli"
#include "LightingUtil.hlsli"
#include "SsrCommon.hlsli"

TextureCube	gCubeMap		: register(t0);
Texture2D	gBackBuffer		: register(t1);
Texture2D	gNormalMap		: register(t2);
Texture2D	gDepthMap		: register(t3);
Texture2D	gSpecularMap	: register(t4);
Texture2D	gSsrMap			: register(t5);

static const float2 gTexCoords[6] = {
	float2(0.0f, 1.0f),
	float2(0.0f, 0.0f),
	float2(1.0f, 0.0f),
	float2(0.0f, 1.0f),
	float2(1.0f, 0.0f),
	float2(1.0f, 1.0f)
};

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float3 PosV		: POSITION;
	float2 TexC		: TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout;

	vout.TexC = gTexCoords[vid];

	// Quad covering screen in NDC space.
	vout.PosH = float4(2.0f * vout.TexC.x - 1.0f, 1.0f - 2.0f * vout.TexC.y, 0.0f, 1.0f);

	// Transform quad corners to view space near plane.
	float4 ph = mul(vout.PosH, gInvProj);
	vout.PosV = ph.xyz / ph.w;

	return vout;
}

float NdcDepthToViewDepth(float z_ndc) {
	// z_ndc = A + B/viewZ, where gProj[2,2]=A and gProj[3,2]=B.
	float viewZ = gProj[3][2] / (z_ndc - gProj[2][2]);
	return viewZ;
}

float3 BoxCubeMapLookup(float3 rayOrigin, float3 unitRayDir, float3 boxCenter, float3 boxExtents) {
	// Set the origin to box of center.
	float3 p = rayOrigin - boxCenter;

	// The formula for AABB's i-th plate ray versus plane intersection is as follows.
	//
	// t1 = (-dot(n_i, p) + h_i) / dot(n_i, d) = (-p_i + h_i) / d_i
	// t2 = (-dot(n_i, p) - h_i) / dot(n_i, d) = (-p_i - h_i) / d_i
	float3 t1 = (-p + boxExtents) / unitRayDir;
	float3 t2 = (-p - boxExtents) / unitRayDir;

	// Find the maximum value for each coordinate component.
	// We assume that ray is inside the box, so we only need to find the maximum value of the intersection parameter. 
	float3 tmax = max(t1, t2);

	// Find the minimum value of all components for tmax.
	float t = min(min(tmax.x, tmax.y), tmax.z);

	// To use a lookup vector for a cube map, 
	// create coordinate relative to center of box.
	return p + t * unitRayDir;
}

float4 PS(VertexOut pin) : SV_Target {
	// Get viewspace normal and z-coord of this pixel.  
	float pz = gDepthMap.Sample(gsamDepthMap, pin.TexC).r;
	pz = NdcDepthToViewDepth(pz);

	//
	// Reconstruct full view space position (x,y,z).
	// Find t such that p = t*pin.PosV.
	// p.z = t*pin.PosV.z
	// t = p.z / pin.PosV.z
	//
	float3 posV = (pz / pin.PosV.z) * pin.PosV;
	float4 posW = mul(float4(posV, 1.0f), gInvView);

	// Vector from point being lit to eye. 
	float3 toEyeW = normalize(gEyePosW - posW.xyz);

	float3 normalW = gNormalMap.Sample(gsamLinearClamp, pin.TexC).xyz;

	// Add in specular reflections.
	float3 r = reflect(-toEyeW, normalW);
	float3 lookup = BoxCubeMapLookup(posW.xyz, r, (float3)0.0f, (float3)100.0f);

	float3 cube = gCubeMap.Sample(gsamLinearWrap, lookup).rgb;

	float3 color = gBackBuffer.Sample(gsamLinearClamp, pin.TexC).rgb;
	float4 ssr = gSsrMap.Sample(gsamLinearClamp, pin.TexC);

	float4 specular = gSpecularMap.Sample(gsamLinearClamp, pin.TexC);
	float3 fresnelFactor = SchlickFresnel(specular.rgb, normalW, r);
	float shiness = 1.0f - specular.a;

	float3 reflection = ssr.a * ssr.rgb + (1.0f - ssr.a) * cube;

	float3 litColor = color + fresnelFactor * shiness * reflection;

	return float4(litColor, 1.0f);
}

#endif // __APPLYINGSSR_HLSL__