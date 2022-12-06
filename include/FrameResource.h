#pragma once

#include "UploadBuffer.h"
#include "MathHelper.h"

#define MaxLights 16

struct Light {
	DirectX::XMFLOAT3 Strength = { 0.5f, 0.5f, 0.5f };
	float FalloffStart = 1.0f;						// point/spot light only
	DirectX::XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };	// directional/spot light only
	float FalloffEnd = 10.0f;					// point/spot light only
	DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };		// point/spot light only
	float SpotPower = 64.0f;					// spot light only
};

struct ObjectConstants {
	DirectX::XMFLOAT4X4 World;
	DirectX::XMFLOAT4X4 TexTransform;
};

struct PassConstants {
	DirectX::XMFLOAT4X4	View;
	DirectX::XMFLOAT4X4	InvView;
	DirectX::XMFLOAT4X4	Proj;
	DirectX::XMFLOAT4X4	InvProj;
	DirectX::XMFLOAT4X4	ViewProj;
	DirectX::XMFLOAT4X4	InvViewProj;
	DirectX::XMFLOAT3	EyePosW;
	float				PassConstantsPad1;
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

struct FrameResource {
public:
	FrameResource(
		ID3D12Device* inDevice,
		UINT inPassCount,
		UINT inObjectCount,
		UINT inMaterialCount);
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

	UINT64 Fence;

	ID3D12Device* Device;
	UINT PassCount;
	UINT ObjectCount;
	UINT MaterialCount;
};