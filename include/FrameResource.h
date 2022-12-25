#pragma once

#include "UploadBuffer.h"
#include "MathHelper.h"
#include "Light.h"

struct ObjectConstants {
	DirectX::XMFLOAT4X4 World			= MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 PrevWorld		= MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 TexTransform	= MathHelper::Identity4x4();
};

struct PassConstants {
	DirectX::XMFLOAT4X4	View			= MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4	InvView			= MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4	Proj			= MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4	InvProj			= MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4	ViewProj		= MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4	InvViewProj		= MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 PrevViewProj	= MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 ViewProjTex		= MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 ShadowTransform	= MathHelper::Identity4x4();
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
	INT					DiffuseSrvIndex;
	INT					NormalSrvIndex;
	INT					AlphaSrvIndex;
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

struct FrameResource {
public:
	FrameResource(
		ID3D12Device* pDevice,
		UINT passCount,
		UINT objectCount,
		UINT materialCount);
	virtual ~FrameResource() = default;

private:
	FrameResource(const FrameResource& src) = delete;
	FrameResource(FrameResource&& src) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	FrameResource& operator=(FrameResource&& rhs) = delete;

public:
	bool Initialize();

public:
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

	UploadBuffer<PassConstants> PassCB;
	UploadBuffer<ObjectConstants> ObjectCB;
	UploadBuffer<MaterialConstants> MaterialCB;
	UploadBuffer<SsaoConstants> SsaoCB;
	UploadBuffer<BlurConstants> BlurCB;

	UINT64 Fence;

	ID3D12Device* Device;
	UINT PassCount;
	UINT ObjectCount;
	UINT MaterialCount;
};