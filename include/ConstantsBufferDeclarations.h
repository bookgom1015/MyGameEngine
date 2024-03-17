#pragma once

#include "Light.h"

struct ConstantBuffer_Object {
	DirectX::XMFLOAT4X4 World;
	DirectX::XMFLOAT4X4 PrevWorld;
	DirectX::XMFLOAT4X4 TexTransform;
	DirectX::XMFLOAT4	Center;
	DirectX::XMFLOAT4	Extents;
};

struct ConstantBuffer_Material {
	DirectX::XMFLOAT4	Albedo;

	FLOAT				Roughness;
	FLOAT				Metalic;
	FLOAT				Specular;
	FLOAT				ConstantPad0;

	DirectX::XMFLOAT4X4	MatTransform;

	INT					DiffuseSrvIndex;
	INT					NormalSrvIndex;
	INT					AlphaSrvIndex;
	FLOAT				MatConstPad0;
};

struct ConstantBuffer_Pass {
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
	UINT				LightCount;

	DirectX::XMFLOAT2	JitteredOffset;
	FLOAT				ConstantPad1;
	FLOAT				ConstantPad2;

	DirectX::XMFLOAT4	AmbientLight;

	Light				Lights[MaxLights];
};

struct ConstantBuffer_SSAO {
	DirectX::XMFLOAT4X4	View;
	DirectX::XMFLOAT4X4	Proj;
	DirectX::XMFLOAT4X4	InvProj;
	DirectX::XMFLOAT4X4	ProjTex;
	DirectX::XMFLOAT4	OffsetVectors[14];

	FLOAT				OcclusionRadius;
	FLOAT				OcclusionFadeStart;
	FLOAT				OcclusionFadeEnd;
	FLOAT				SurfaceEpsilon;

	UINT				SampleCount;
	FLOAT				ConstantPads[3];
};

struct ConstantBuffer_Blur {
	DirectX::XMFLOAT4X4	Proj;
	DirectX::XMFLOAT4	BlurWeights[3];

	FLOAT				BlurRadius;
	FLOAT				ConstantPads[3];

};

struct ConstantBuffer_DoF {
	DirectX::XMFLOAT4X4	Proj;
	DirectX::XMFLOAT4X4	InvProj;
	FLOAT				FocusRange;
	FLOAT				FocusingSpeed;
	FLOAT				DeltaTime;
	FLOAT				ConstantPad0;
};

struct ConstantBuffer_SSR {
	DirectX::XMFLOAT4X4	View;
	DirectX::XMFLOAT4X4	InvView;
	DirectX::XMFLOAT4X4	Proj;
	DirectX::XMFLOAT4X4	InvProj;

	DirectX::XMFLOAT3	EyePosW;
	FLOAT				MaxDistance;

	FLOAT				RayLength;
	FLOAT				NoiseIntensity;
	INT					StepCount;
	INT					BackStepCount;

	FLOAT				DepthThreshold;
	FLOAT				Thickness;
	FLOAT				Resolution;
	FLOAT				ConstantPad0;
};

struct ConvertEquirectangularToCubeConstantBuffer {
	DirectX::XMFLOAT4X4 Proj;
	DirectX::XMFLOAT4X4 View[6];
};

struct RtaoConstants {
	DirectX::XMFLOAT4X4	View;
	DirectX::XMFLOAT4X4	InvView;
	DirectX::XMFLOAT4X4	Proj;
	DirectX::XMFLOAT4X4	InvProj;

	FLOAT OcclusionRadius;
	FLOAT OcclusionFadeStart;
	FLOAT OcclusionFadeEnd;
	FLOAT SurfaceEpsilon;

	UINT FrameCount;
	UINT SampleCount;
	FLOAT ConstantPad[2];
};

struct CrossBilateralFilterConstants {
	FLOAT	DepthSigma;
	UINT	DepthNumMantissaBits;
	FLOAT	ConstantPad0;
	FLOAT	ConstantPad1;
};

struct CalcLocalMeanVarianceConstants {
	DirectX::XMUINT2 TextureDim;
	UINT	KernelWidth;
	UINT	KernelRadius;

	BOOL	CheckerboardSamplingEnabled;
	BOOL	EvenPixelActivated;
	UINT	PixelStepY;
	FLOAT	ConstantPad0;
};

struct TemporalSupersamplingBlendWithCurrentFrameConstants {
	FLOAT StdDevGamma;
	BOOL ClampCachedValues;
	FLOAT ClampingMinStdDevTolerance;
	FLOAT ConstnatPad0;

	FLOAT ClampDifferenceToTsppScale;
	BOOL ForceUseMinSmoothingFactor;
	FLOAT MinSmoothingFactor;
	UINT MinTsppToUseTemporalVariance;

	UINT BlurStrengthMaxTspp;
	FLOAT BlurDecayStrength;
	BOOL CheckerboardEnabled;
	BOOL CheckerboardEvenPixelActivated;
};

struct AtrousWaveletTransformFilterConstantBuffer {
	DirectX::XMUINT2 TextureDim;
	FLOAT DepthWeightCutoff;
	BOOL UsingBilateralDownsamplingBuffers;

	BOOL UseAdaptiveKernelSize;
	FLOAT KernelRadiusLerfCoef;
	UINT MinKernelWidth;
	UINT MaxKernelWidth;

	FLOAT RayHitDistanceToKernelWidthScale;
	FLOAT RayHitDistanceToKernelSizeScaleExponent;
	BOOL PerspectiveCorrectDepthInterpolation;
	FLOAT MinVarianceToDenoise;

	FLOAT ValueSigma;
	FLOAT DepthSigma;
	FLOAT NormalSigma;
	FLOAT FovY;

	UINT DepthNumMantissaBits;
	FLOAT ConstantPads[3];
};

struct RaytracedReflectionConstantBuffer {
	DirectX::XMFLOAT4X4	View;
	DirectX::XMFLOAT4X4	InvView;
	DirectX::XMFLOAT4X4	Proj;
	DirectX::XMFLOAT4X4	InvProj;
	DirectX::XMFLOAT4X4	ViewProj;

	DirectX::XMFLOAT3 EyePosW;
	FLOAT ReflectionRadius;

	DirectX::XMUINT2 TextureDim;
	FLOAT ConstantPads[2];
};

struct DebugMapSampleDesc {
	DirectX::XMFLOAT4 MinColor;
	DirectX::XMFLOAT4 MaxColor;

	FLOAT Denominator;
	FLOAT ConstantPad0;
	FLOAT ConstantPad1;
	FLOAT ConstantPad2;
};

struct ConstantBuffer_DebugMap {
	DebugMapSampleDesc SampleDescs[DebugMap::MapSize];
};