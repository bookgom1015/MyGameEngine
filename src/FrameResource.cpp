#include "FrameResource.h"
#include "Logger.h"

FrameResource::FrameResource(ID3D12Device* pDevice, UINT passCount, UINT objectCount, UINT materialCount) :
	PassCount(passCount),
	ObjectCount(objectCount),
	MaterialCount(materialCount) {
	Device = pDevice;
	Fence = 0;
}

bool FrameResource::Initialize() {
	CheckHRESULT(Device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(CmdListAlloc.GetAddressOf())
	));

	CheckReturn(PassCB.Initialize(Device, PassCount, true));
	CheckReturn(ObjectCB.Initialize(Device, ObjectCount, true));
	CheckReturn(MaterialCB.Initialize(Device, MaterialCount, true));
	CheckReturn(SsaoCB.Initialize(Device, 1, true));
	CheckReturn(BlurCB.Initialize(Device, 1, true));
	CheckReturn(DofCB.Initialize(Device, 1, true));
	CheckReturn(SsrCB.Initialize(Device, 1, true));
	CheckReturn(ConvEquirectToCubeCB.Initialize(Device, 1, true))
	CheckReturn(RtaoCB.Initialize(Device, 1, true));
	CheckReturn(CrossBilateralFilterCB.Initialize(Device, 1, true));
	CheckReturn(CalcLocalMeanVarCB.Initialize(Device, 1, true));
	CheckReturn(TsppBlendCB.Initialize(Device, 1, true));
	CheckReturn(AtrousFilterCB.Initialize(Device, 1, true));
	CheckReturn(DebugMapCB.Initialize(Device, 1, true));
	CheckReturn(RrCB.Initialize(Device, 1, true));

	return true;
}