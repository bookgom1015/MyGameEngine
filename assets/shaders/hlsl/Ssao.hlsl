#ifndef __SSAO_HLSL__
#define __SSAO_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "ShadingHelpers.hlsli"
#include "Samplers.hlsli"

ConstantBuffer<SsaoConstants> cbSsao : register(b0);

// Nonnumeric values cannot be added to a cbuffer.
Texture2D<GBuffer::NormalMapFormat>			gi_Normal			: register(t0);
Texture2D<GBuffer::DepthMapFormat>			gi_Depth			: register(t1);
Texture2D<Ssao::RandomVectorMapFormat>		gi_RandomVector		: register(t2);

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH : SV_POSITION;
	float3 PosV : POSITION;
	float2 TexC : TEXCOORD0;
};

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout;

	vout.TexC = gTexCoords[vid];

	vout.PosH = float4(2.0f * vout.TexC.x - 1.0f, 1.0f - 2.0f * vout.TexC.y, 0.0f, 1.0f);

	// Transform quad corners to view space near plane.
	float4 ph = mul(vout.PosH, cbSsao.InvProj);
	vout.PosV = ph.xyz / ph.w;

	return vout;
}

float NdcDepthToViewDepth(float z_ndc) {
	// z_ndc = A + B/viewZ, where gProj[2,2]=A and gProj[3,2]=B.
	float viewZ = cbSsao.Proj[3][2] / (z_ndc - cbSsao.Proj[2][2]);
	return viewZ;
}

// Determines how much the sample point q occludes the point p as a function of distZ.
float OcclusionFunction(float distZ) {
	//
	// If depth(q) is "behind" depth(p), then q cannot occlude p.  Moreover, if 
	// depth(q) and depth(p) are sufficiently close, then we also assume q cannot
	// occlude p because q needs to be in front of p by Epsilon to occlude p.
	//
	// We use the following function to determine the occlusion.  
	// 
	//
	//       1.0     -------------\\
	//               |           |  \\
	//               |           |    \\
	//               |           |      \\
	//               |           |        \\
	//               |           |          \\
	//               |           |            \\
	//  ------|------|-----------|-------------|---------|--> zv
	//        0     Eps          z0            z1        
	//
	float occlusion = 0.0f;
	if (distZ > cbSsao.SurfaceEpsilon) {
		float fadeLength = cbSsao.OcclusionFadeEnd - cbSsao.OcclusionFadeStart;

		// Linearly decrease occlusion from 1 to 0 as distZ goes from gOcclusionFadeStart to gOcclusionFadeEnd.	
		occlusion = saturate((cbSsao.OcclusionFadeEnd - distZ) / fadeLength);
	}

	return occlusion;
}

float4 PS(VertexOut pin) : SV_Target{
	// p -- the point we are computing the ambient occlusion for.
	// n -- normal vector at p.
	// q -- a random offset from p.
	// r -- a potential occluder that might occlude p.
	float pz = gi_Depth.SampleLevel(gsamDepthMap, pin.TexC, 0.0f);
	pz = NdcDepthToViewDepth(pz);

	float3 n = gi_Normal.Sample(gsamLinearClamp, pin.TexC).xyz;
	n = mul(n, (float3x3)cbSsao.View);

	//
	// Reconstruct full view space position (x,y,z).
	// Find t such that p = t*pin.PosV.
	// p.z = t*pin.PosV.z
	// t = p.z / pin.PosV.z
	//
	const float3 p = (pz / pin.PosV.z) * pin.PosV;

	// Extract random vector and map from [0,1] --> [-1, +1].
	const float3 randVec = 2.0f * gi_RandomVector.SampleLevel(gsamLinearWrap, 4.0f * pin.TexC, 0.0f) - 1.0f;

	float occlusionSum = 0.0f;

	// Sample neighboring points about p in the hemisphere oriented by n.
	[loop]
	for (int i = 0; i < cbSsao.SampleCount; ++i) {
		// Are offset vectors are fixed and uniformly distributed (so that our offset vectors
		// do not clump in the same direction).  If we reflect them about a random vector
		// then we get a random uniform distribution of offset vectors.
		const float3 offset = reflect(cbSsao.OffsetVectors[i].xyz, randVec);

		// Flip offset vector if it is behind the plane defined by (p, n).
		const float flip = sign(dot(offset, n));

		// Sample a point near p within the occlusion radius.
		const float3 q = p + flip * cbSsao.OcclusionRadius * offset;

		// Project q and generate projective tex-coords.  
		float4 projQ = mul(float4(q, 1.0f), cbSsao.ProjTex);
		projQ /= projQ.w;

		// Find the nearest depth value along the ray from the eye to q (this is not
		// the depth of q, as q is just an arbitrary point near p and might
		// occupy empty space).  To find the nearest depth we look it up in the depthmap.

		float rz = gi_Depth.SampleLevel(gsamDepthMap, projQ.xy, 0.0f).r;
		rz = NdcDepthToViewDepth(rz);

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
		const float dp = max(dot(n, normalize(r - p)), 0.0f);

		const float occlusion = dp * OcclusionFunction(distZ);

		occlusionSum += occlusion;
	}

	occlusionSum /= cbSsao.SampleCount;

	const float access = 1.0f - occlusionSum;

	// Sharpen the contrast of the SSAO map to make the SSAO affect more dramatic.
	return saturate(pow(access, 6.0f));
}

#endif // __SSAO_HLSL__