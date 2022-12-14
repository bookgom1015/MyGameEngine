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

	return true;
}