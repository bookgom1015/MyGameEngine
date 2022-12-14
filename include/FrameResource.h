#pragma once

#include "UploadBuffer.h"
#include "MathHelper.h"
#include "Light.h"

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
	DirectX::XMFLOAT4X4 ShadowTransform;
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

	UINT64 Fence;

	ID3D12Device* Device;
	UINT PassCount;
	UINT ObjectCount;
	UINT MaterialCount;
};