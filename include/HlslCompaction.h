#ifndef __HLSLCOMPACTION_H__
#define __HLSLCOMPACTION_H__

#ifdef HLSL
#include "HlslCompaction.hlsli"
#endif

#ifndef NUM_TEXTURE_MAPS 
#define NUM_TEXTURE_MAPS 64
#endif

#ifndef MAX_DESCRIPTOR_SIZE
#define MAX_DESCRIPTOR_SIZE 256
#endif

#define MaxLights 16

struct Light {
	DirectX::XMFLOAT3 Strength = { 0.5f, 0.5f, 0.5f };
	float FalloffStart = 1.0f;								// point/spot light only
	DirectX::XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };	// directional/spot light only
	float FalloffEnd = 10.0f;								// point/spot light only
	DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };		// point/spot light only
	float SpotPower = 64.0f;								// spot light only
};

struct ObjectConstants {
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 PrevWorld = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
};

struct PassConstants {
	DirectX::XMFLOAT4X4	View = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4	InvView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4	Proj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4	InvProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4	ViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4	InvViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 PrevViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 ViewProjTex = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 ShadowTransform = MathHelper::Identity4x4();
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
			RGB	= 1 << 0,
			RG	= 1 << 1,
			RRR	= 1 << 2,
			GGG	= 1 << 3,
			BBB	= 1 << 4,
			AAA	= 1 << 5
		};
	}
}

#endif // __HLSLCOMPACTION_HLSL__