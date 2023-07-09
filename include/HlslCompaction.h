#ifndef __HLSLCOMPACTION_H__
#define __HLSLCOMPACTION_H__

#ifdef HLSL
#include "HlslCompaction.hlsli"
#include "./../../../include/Vertex.h"
#endif

#ifndef NUM_TEXTURE_MAPS 
#define NUM_TEXTURE_MAPS 64
#endif

#ifndef MAX_DESCRIPTOR_SIZE
#define MAX_DESCRIPTOR_SIZE 256
#endif

#define MaxLights 16

struct Light {
	DirectX::XMFLOAT3 Strength;
	float FalloffStart;				// point/spot light only
	DirectX::XMFLOAT3 Direction;	// directional/spot light only
	float FalloffEnd;				// point/spot light only
	DirectX::XMFLOAT3 Position;		// point/spot light only
	float SpotPower;				// spot light only
};

#ifdef HLSL
struct Material {
	float4 DiffuseAlbedo;
	float3 FresnelR0;
	float Shininess;
};
#endif

struct ObjectConstants {
	DirectX::XMFLOAT4X4 World;
	DirectX::XMFLOAT4X4 PrevWorld;
	DirectX::XMFLOAT4X4 TexTransform;
};

struct PassConstants {
	DirectX::XMFLOAT4X4	View;
	DirectX::XMFLOAT4X4	InvView;
	DirectX::XMFLOAT4X4	Proj;
	DirectX::XMFLOAT4X4	InvProj;
	DirectX::XMFLOAT4X4	ViewProj;
	DirectX::XMFLOAT4X4	InvViewProj;
	DirectX::XMFLOAT4X4 PrevViewProj;
	DirectX::XMFLOAT4X4 ViewProjTex;
	DirectX::XMFLOAT4X4 ShadowTransform;
	DirectX::XMFLOAT3	EyePosW;
	float				PassConstantsPad0;
	DirectX::XMFLOAT2	JitteredOffset;
	float				PassConstantsPad1;
	float				PassConstantsPad2;
	DirectX::XMFLOAT4	AmbientLight;
	Light				Lights[MaxLights];
};

struct MaterialConstants {
	DirectX::XMFLOAT4	DiffuseAlbedo;
	DirectX::XMFLOAT3	FresnelR0;
	float				Roughness;
	DirectX::XMFLOAT4X4	MatTransform;
	int					DiffuseSrvIndex;
	int					NormalSrvIndex;
	int					AlphaSrvIndex;
	float				MatConstPad0;
};

struct SsaoConstants {
	DirectX::XMFLOAT4X4	View;
	DirectX::XMFLOAT4X4	Proj;
	DirectX::XMFLOAT4X4	InvProj;
	DirectX::XMFLOAT4X4	ProjTex;
	DirectX::XMFLOAT4	OffsetVectors[14];
	float				OcclusionRadius;
	float				OcclusionFadeStart;
	float				OcclusionFadeEnd;
	float				SurfaceEpsilon;
};

struct BlurConstants {
	DirectX::XMFLOAT4X4	Proj;
	DirectX::XMFLOAT4	BlurWeights[3];
	float				BlurRadius;
	float				ConstantPad0;
	float				ConstantPad1;
	float				ConstantPad2;

};

struct DofConstants {
	DirectX::XMFLOAT4X4	Proj;
	DirectX::XMFLOAT4X4	InvProj;
	float				FocusRange;
	float				FocusingSpeed;
	float				DeltaTime;
	float				ConstantPad0;
};

struct SsrConstants {
	DirectX::XMFLOAT4X4	View;
	DirectX::XMFLOAT4X4	InvView;
	DirectX::XMFLOAT4X4	Proj;
	DirectX::XMFLOAT4X4	InvProj;
	DirectX::XMFLOAT3	EyePosW;
	float				MaxDistance;
	float				RayLength;
	float				NoiseIntensity;
	int					NumSteps;
	int					NumBackSteps;
	float				DepthThreshold;
	float				ConstantPad0;
	float				ConstantPad1;
	float				ConstantPad2;
};

namespace DebugShaderParams {
	static const int MapCount = 5;

	namespace SampleMask {
		enum Type {
			RGB	= 0,
			RG	= 1 << 0,
			RRR	= 1 << 1,
			GGG	= 1 << 2,
			BBB	= 1 << 3,
			AAA	= 1 << 4
		};
	}
}

#endif // __HLSLCOMPACTION_HLSL__