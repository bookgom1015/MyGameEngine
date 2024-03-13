#ifndef __SSAO_HLSL__
#define __SSAO_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "ShadingHelpers.hlsli"
#include "Samplers.hlsli"

ConstantBuffer<ConstantBuffer_SSAO> cb_SSAO : register(b0);

// Nonnumeric values cannot be added to a cbuffer.
Texture2D<GBuffer::NormalMapFormat>			gi_Normal			: register(t0);
Texture2D<GBuffer::DepthMapFormat>			gi_Depth			: register(t1);
Texture2D<SSAO::RandomVectorMapFormat>		gi_RandomVector		: register(t2);

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH : SV_POSITION;
	float3 PosV : POSITION;
	float2 TexC : TEXCOORD0;
};

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout;

	vout.TexC = gTexCoords[vid];
	vout.PosH = float4(2 * vout.TexC.x - 1, 1 - 2 * vout.TexC.y, 0, 1);

	float4 ph = mul(vout.PosH, cb_SSAO.InvProj);
	vout.PosV = ph.xyz / ph.w;

	return vout;
}

SSAO::AOCoefficientMapFormat PS(VertexOut pin) : SV_Target{
	// p -- the point we are computing the ambient occlusion for.
	// n -- normal vector at p.
	// q -- a random offset from p.
	// r -- a potential occluder that might occlude p.
	float pz = gi_Depth.SampleLevel(gsamDepthMap, pin.TexC, 0);
	pz = NdcDepthToViewDepth(pz, cb_SSAO.Proj);

	float3 n = normalize(gi_Normal.Sample(gsamLinearClamp, pin.TexC).xyz);
	n = mul(n, (float3x3)cb_SSAO.View);

	//
	// Reconstruct full view space position (x,y,z).
	// Find t such that p = t*pin.PosV.
	// p.z = t*pin.PosV.z
	// t = p.z / pin.PosV.z
	//
	const float3 p = (pz / pin.PosV.z) * pin.PosV;

	// Extract random vector and map from [0,1] --> [-1, +1].
	const float3 randVec = 2 * gi_RandomVector.SampleLevel(gsamLinearWrap, 4 * pin.TexC, 0) - 1;

	float occlusionSum = 0;

	// Sample neighboring points about p in the hemisphere oriented by n.
	[loop]
	for (uint i = 0; i < cb_SSAO.SampleCount; ++i) {
		// Are offset vectors are fixed and uniformly distributed (so that our offset vectors
		// do not clump in the same direction).  If we reflect them about a random vector
		// then we get a random uniform distribution of offset vectors.
		const float3 offset = reflect(cb_SSAO.OffsetVectors[i].xyz, randVec);

		// Flip offset vector if it is behind the plane defined by (p, n).
		const float flip = sign(dot(offset, n));

		// Sample a point near p within the occlusion radius.
		const float3 q = p + flip * cb_SSAO.OcclusionRadius * offset;

		// Project q and generate projective tex-coords.  
		float4 projQ = mul(float4(q, 1), cb_SSAO.ProjTex);
		projQ /= projQ.w;

		// Find the nearest depth value along the ray from the eye to q (this is not
		// the depth of q, as q is just an arbitrary point near p and might
		// occupy empty space).  To find the nearest depth we look it up in the depthmap.

		float rz = gi_Depth.SampleLevel(gsamDepthMap, projQ.xy, 0).r;
		rz = NdcDepthToViewDepth(rz, cb_SSAO.Proj);

		// Reconstruct full view space position r = (rx,ry,rz).  We know r
		// lies on the ray of q, so there exists a t such that r = t*q.
		// r.z = t*q.z ==> t = r.z / q.z
		const float3 r = (rz / q.z) * q;

		//
		// Test whether r occludes p.
		//   * The product dot(n, normalize(r - p)) measures how much in front
		//     of the plane(p,n) the occluder point r is.  The more in front it is, the
		//     more occlusion weight we give it.  This also prevents self shadowing where 
		//     a point r on an angled plane (p,n) could give a false occlusion since they
		//     have different depth values with respect to the eye.
		//   * The weight of the occlusion is scaled based on how far the occluder is from
		//     the point we are computing the occlusion of.  If the occluder r is far away
		//     from p, then it does not occlude it.
		// 
		const float distZ = p.z - r.z;
		const float dp = max(dot(n, normalize(r - p)), 0);

		const float occlusion = dp * OcclusionFunction(distZ, cb_SSAO.SurfaceEpsilon, cb_SSAO.OcclusionFadeStart, cb_SSAO.OcclusionFadeEnd);

		occlusionSum += occlusion;
	}

	occlusionSum /= cb_SSAO.SampleCount;

	const float access = 1 - occlusionSum;

	// Sharpen the contrast of the SSAO map to make the SSAO affect more dramatic.
	return saturate(pow(access, 6));
}

#endif // __SSAO_HLSL__