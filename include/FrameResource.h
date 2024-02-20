#pragma once

#include "MathHelper.h"
#include "HlslCompaction.h"
#include "UploadBuffer.h"

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
	BOOL Initialize();

public:
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

	UploadBuffer<PassConstants> PassCB;
	UploadBuffer<ObjectConstants> ObjectCB;
	UploadBuffer<MaterialConstants> MaterialCB;
	UploadBuffer<SsaoConstants> SsaoCB;
	UploadBuffer<BlurConstants> BlurCB;
	UploadBuffer<DofConstants> DofCB;
	UploadBuffer<ConstantBuffer_SSR> CB_SSR;
	UploadBuffer<ConvertEquirectangularToCubeConstantBuffer> ConvEquirectToCubeCB;
	UploadBuffer<RtaoConstants> RtaoCB;
	UploadBuffer<CrossBilateralFilterConstants> CrossBilateralFilterCB;
	UploadBuffer<CalcLocalMeanVarianceConstants> CalcLocalMeanVarCB;
	UploadBuffer<TemporalSupersamplingBlendWithCurrentFrameConstants> TsppBlendCB;
	UploadBuffer<AtrousWaveletTransformFilterConstantBuffer> AtrousFilterCB;
	UploadBuffer<DebugMapConstantBuffer> DebugMapCB;
	UploadBuffer<RaytracedReflectionConstantBuffer> RrCB;

	UINT64 Fence;

	ID3D12Device* Device;
	UINT PassCount;
	UINT ObjectCount;
	UINT MaterialCount;
};