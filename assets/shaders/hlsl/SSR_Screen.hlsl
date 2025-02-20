// [ References ]
//  - https://blog.naver.com/PostView.nhn?blogId=daehuck&logNo=220450852220&parentCategoryNo
//   -- http://www.kode80.com/blog/2015/03/11/screen-space-reflections-in-unity-5/
//   -- http://www.crytek.com/download/fmx2013_c3_art_tech_donzallaz_sousa.pdf
//   -- https://books.google.co.kr/books?id=30ZOCgAAQBAJ&pg=PA65&lpg=PA65&dq=rendering+in+thief&source=bl&ots=2YfyfYHFKM&sig=_bczBXj1hRGHEfx81_vciIWV9Mo&hl=ko&sa=X&ved=0CD4Q6AEwBGoVChMIwtWnsYSoxwIViKWUCh2AbQMR#v=onepage&q=rendering%20in%20thief&f=false
//   -- https://forum.beyond3d.com/threads/screen-space-reflections.47780/

#ifndef __SSR_SCREEN_HLSL__
#define __SSR_SCREEN_HLSL__

#ifndef HLSL
#define HLSL
#endif

#include "./../../../include/HlslCompaction.h"
#include "ShadingHelpers.hlsli"
#include "Samplers.hlsli"

ConstantBuffer<ConstantBuffer_SSR>			cb_SSR			: register(b0);

Texture2D<SDR_FORMAT>						gi_BackBuffer	: register(t0);
Texture2D<GBuffer::PositionMapFormat>		gi_Position		: register(t1);
Texture2D<GBuffer::NormalMapFormat>			gi_Normal		: register(t2);
Texture2D<DepthStencilBuffer::BufferFormat>	gi_Depth		: register(t3);
Texture2D<GBuffer::RMSMapFormat>			gi_RMS			: register(t4);

#include "CoordinatesFittedToScreen.hlsli"

struct VertexOut {
	float4 PosH		: SV_POSITION;
	float3 PosV		: POSITION;
	float2 TexC		: TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID) {
	VertexOut vout;

	vout.TexC = gTexCoords[vid];
	vout.PosH = float4(2.f * vout.TexC.x - 1.f, 1.f - 2.f * vout.TexC.y, 0.f, 1.f);

	const float4 ph = mul(vout.PosH, cb_SSR.InvProj);
	vout.PosV = ph.xyz / ph.w;

	return vout;
}

SSR::SSRMapFormat PS(VertexOut pin) : SV_Target {	
	uint2 size;
	gi_BackBuffer.GetDimensions(size.x, size.y);
	
	const float4 posW = gi_Position.Sample(gsamLinearClamp, pin.TexC);
	if (GBuffer::IsInvalidPosition(posW)) return 0.f;
	
	const float3 posV = mul(posW, cb_SSR.View).xyz;
	
	const float3 normalW = normalize(gi_Normal.Sample(gsamLinearClamp, pin.TexC).xyz);
	const float3 normalV = normalize(mul(normalW, (float3x3)cb_SSR.View));
	
	const float3 fromEyeV = normalize(posV);
	const float3 toLightV = reflect(fromEyeV, normalV);
	
	float3 startV = posV;
	float3 endV = startV + toLightV * cb_SSR.MaxDistance;
	if (endV.z < 0.f) return 0.f;
	
	float4 endH = mul(float4(endV, 1.f), cb_SSR.Proj);
	endH /= endH.w;
	
	float2 startFrag = pin.TexC;
	startFrag *= size;
	
	float2 endFrag = endH.xy * 0.5f + 0.5f;
	endFrag.y = 1.f - endFrag.y;
	endFrag *= size;
	
	float2 _delta = endFrag - startFrag;
	float useX = abs(_delta.x) >= abs(_delta.y) ? 1.f : 0.f;
	float delta = lerp(abs(_delta.y), abs(_delta.x), useX) * clamp(cb_SSR.Resolution, 0.f, 1.f);
	float2 increment = _delta / max(delta, 0.001f);
	
	float search_prev = 0.f;
	float search_curr = 0.f;
	
	uint hit_p0 = 0;
	uint hit_p1 = 0;
	
	float viewDist = startV.z;
	float depth = cb_SSR.Thickness;
	
	float2 frag = startFrag;
	float2 texc = frag / size;
	float3 currV = startV;
	
	const uint maxStep = min(delta, 64);
	
	{
		for (uint i = 0; i < maxStep; ++i) {
			frag += increment;
			texc = frag / size;
			{
				if (texc.x < 0.f || texc.x > 1.f || texc.y < 0.f || texc.y > 1.f) continue;
				const float4 _posW = gi_Position.Sample(gsamLinearClamp, texc);
				if (GBuffer::IsInvalidPosition(_posW)) continue;
	
				currV = mul(_posW, cb_SSR.View).xyz;
			}
	
			search_curr = lerp((frag.y - startFrag.y) / _delta.y, (frag.x - startFrag.x) / _delta.x, useX);
			search_curr = clamp(search_curr, 0.f, 1.f);
	
			viewDist = (startV.z * endV.z) / lerp(endV.z, startV.z, search_curr);
			depth = viewDist - currV.z;
	
			if (depth > 0.f && depth < cb_SSR.Thickness) {
				hit_p0 = 1.f;
				break;
			}
			else {
				search_prev = search_curr;
			}
		}
	}
	
	search_curr = search_prev + ((search_curr - search_prev) * 0.5f);
	if (hit_p0 == 0.f) return 0.f;
	
	{
		for (uint i = 0; i < 8; ++i) {
			frag = lerp(startFrag, endFrag, search_curr);
			texc = frag / size;
			{
				const float4 _posW = gi_Position.Sample(gsamLinearClamp, texc);
				if (GBuffer::IsInvalidPosition(_posW)) continue;
	
				currV = mul(_posW, cb_SSR.View).xyz;
			}
	
			viewDist = (startV.z * endV.z) / lerp(endV.z, startV.z, search_curr);
			depth = viewDist - currV.z;
	
			if (depth > 0 && depth < cb_SSR.Thickness) {
				hit_p1 = 1.f;
				search_curr = search_prev + ((search_curr - search_prev) * 0.5f);
			}
			else {
				const float temp = search_curr;
				search_curr = search_curr + ((search_curr - search_prev) * 0.5f);
				search_prev = temp;
			}
		}
	}
	
	return gi_BackBuffer.Sample(gsamLinearClamp, texc);
}

#endif // __SSR_SCREEN_HLSL__