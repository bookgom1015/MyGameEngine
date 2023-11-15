#pragma once

#define MaxLights 16

struct ObjectConstants {
	DirectX::XMFLOAT4X4 World;
	DirectX::XMFLOAT4X4 PrevWorld;
	DirectX::XMFLOAT4X4 TexTransform;
	DirectX::XMFLOAT4	Center;
	DirectX::XMFLOAT4	Extents;
};

struct Light {
	DirectX::XMFLOAT3 Strength;
	float FalloffStart;				// point/spot light only
	DirectX::XMFLOAT3 Direction;	// directional/spot light only
	float FalloffEnd;				// point/spot light only
	DirectX::XMFLOAT3 Position;		// point/spot light only
	float SpotPower;				// spot light only
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
	DirectX::XMFLOAT4	Albedo;
	float				Roughness;
	float				Metalic;
	float				Specular;
	float				ConstantPad0;
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

	UINT				SampleCount;
	float				ConstantPads[3];
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

struct ConvertEquirectangularToCubeConstantBuffer {
	DirectX::XMFLOAT4X4 Proj;
	DirectX::XMFLOAT4X4 View[6];
};

struct RtaoConstants {
	DirectX::XMFLOAT4X4	View;
	DirectX::XMFLOAT4X4	InvView;
	DirectX::XMFLOAT4X4	Proj;
	DirectX::XMFLOAT4X4	InvProj;

	float OcclusionRadius;
	float OcclusionFadeStart;
	float OcclusionFadeEnd;
	float SurfaceEpsilon;

	UINT FrameCount;
	UINT SampleCount;
	float ConstantPad[2];
};

struct CrossBilateralFilterConstants {
	float	DepthSigma;
	UINT	DepthNumMantissaBits;
	float	ConstantPad0;
	float	ConstantPad1;
};

struct CalcLocalMeanVarianceConstants {
	DirectX::XMUINT2 TextureDim;
	UINT	KernelWidth;
	UINT	KernelRadius;

	BOOL	CheckerboardSamplingEnabled;
	BOOL	EvenPixelActivated;
	UINT	PixelStepY;
	float	ConstantPad0;
};

struct TemporalSupersamplingBlendWithCurrentFrameConstants {
	float StdDevGamma;
	BOOL ClampCachedValues;
	float ClampingMinStdDevTolerance;
	float ConstnatPad0;

	float ClampDifferenceToTsppScale;
	BOOL ForceUseMinSmoothingFactor;
	float MinSmoothingFactor;
	UINT MinTsppToUseTemporalVariance;

	UINT BlurStrengthMaxTspp;
	float BlurDecayStrength;
	BOOL CheckerboardEnabled;
	BOOL CheckerboardEvenPixelActivated;
};

struct AtrousWaveletTransformFilterConstantBuffer {
	DirectX::XMUINT2 TextureDim;
	float DepthWeightCutoff;
	bool UsingBilateralDownsamplingBuffers;

	BOOL UseAdaptiveKernelSize;
	float KernelRadiusLerfCoef;
	UINT MinKernelWidth;
	UINT MaxKernelWidth;

	float RayHitDistanceToKernelWidthScale;
	float RayHitDistanceToKernelSizeScaleExponent;
	BOOL PerspectiveCorrectDepthInterpolation;
	float MinVarianceToDenoise;

	float ValueSigma;
	float DepthSigma;
	float NormalSigma;
	float FovY;

	UINT DepthNumMantissaBits;
	float ConstantPads[3];
};

struct RaytracedReflectionConstantBuffer {
	DirectX::XMFLOAT4X4	View;
	DirectX::XMFLOAT4X4	InvView;
	DirectX::XMFLOAT4X4	Proj;
	DirectX::XMFLOAT4X4	InvProj;
	DirectX::XMFLOAT4X4	ViewProj;

	DirectX::XMFLOAT3 EyePosW;
	float ReflectionRadius;

	DirectX::XMUINT2 TextureDim;
	float ConstantPads[2];
};

struct DebugMapSampleDesc {
	DirectX::XMFLOAT4 MinColor;
	DirectX::XMFLOAT4 MaxColor;

	float Denominator;
	float ConstantPad0;
	float ConstantPad1;
	float ConstantPad2;
};

struct DebugMapConstantBuffer {
	DebugMapSampleDesc SampleDescs[DebugMap::MapSize];
};