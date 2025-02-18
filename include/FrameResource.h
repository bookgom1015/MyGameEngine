#pragma once

#include "MathHelper.h"
#include "HlslCompaction.h"
#include "UploadBuffer.h"

struct FrameResource {
public:
	FrameResource(
		ID3D12Device* pDevice,
		UINT passCount,
		UINT shadowCount,
		UINT objectCount,
		UINT materialCount);
	virtual ~FrameResource() = default;

private:
	FrameResource(const FrameResource& src) = delete;
	FrameResource(FrameResource&& src) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	FrameResource& operator=(FrameResource&& rhs) = delete;

public:
	BOOL Initialize();

public:
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

	UploadBuffer<ConstantBuffer_Pass> CB_Pass;
	UploadBuffer<ConstantBuffer_Object> CB_Object;
	UploadBuffer<ConstantBuffer_Material> CB_Material;
	UploadBuffer<ConstantBuffer_SSAO> CB_SSAO;
	UploadBuffer<ConstantBuffer_Blur> CB_Blur;
	UploadBuffer<ConstantBuffer_DoF> CB_DoF;
	UploadBuffer<ConstantBuffer_SSR> CB_SSR;
	UploadBuffer<ConstantBuffer_Irradiance> CB_Irradiance;
	UploadBuffer<ConstantBuffer_RTAO> CB_RTAO;
	UploadBuffer<ConstantBuffer_CrossBilateralFilter> CB_CrossBilateralFilter;
	UploadBuffer<ConstantBuffer_CalcLocalMeanVariance> CB_CalcLocalMeanVar;
	UploadBuffer<ConstantBuffer_TemporalSupersamplingBlendWithCurrentFrame> CB_TSPPBlend;
	UploadBuffer<ConstantBuffer_AtrousWaveletTransformFilter> CB_AtrousFilter;
	UploadBuffer<ConstantBuffer_Debug> CB_Debug;

	UINT64 Fence;

	ID3D12Device* Device;
	UINT PassCount;
	UINT ShadowCount;
	UINT ObjectCount;
	UINT MaterialCount;
};