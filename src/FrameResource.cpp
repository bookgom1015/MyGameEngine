#include "FrameResource.h"
#include "Logger.h"

FrameResource::FrameResource(ID3D12Device* inDevice, UINT inPassCount, UINT inObjectCount, UINT inMaterialCount) :
	PassCount(inPassCount),
	ObjectCount(inObjectCount),
	MaterialCount(inMaterialCount) {
	Device = inDevice;
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

	return true;
}