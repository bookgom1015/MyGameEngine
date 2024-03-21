#include "FrameResource.h"
#include "Logger.h"

FrameResource::FrameResource(ID3D12Device* pDevice, UINT passCount, UINT objectCount, UINT materialCount) :
	PassCount(passCount),
	ObjectCount(objectCount),
	MaterialCount(materialCount) {
	Device = pDevice;
	Fence = 0;
}

BOOL FrameResource::Initialize() {
	CheckHRESULT(Device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(CmdListAlloc.GetAddressOf())
	));

	CheckReturn(CB_Pass.Initialize(Device, PassCount, true));
	CheckReturn(CB_Object.Initialize(Device, ObjectCount, true));
	CheckReturn(CB_Material.Initialize(Device, MaterialCount, true));
	CheckReturn(CB_SSAO.Initialize(Device, 1, true));
	CheckReturn(CB_Blur.Initialize(Device, 1, true));
	CheckReturn(CB_DoF.Initialize(Device, 1, true));
	CheckReturn(CB_SSR.Initialize(Device, 1, true));
	CheckReturn(CB_Irradiance.Initialize(Device, 1, true))
	CheckReturn(CB_RTAO.Initialize(Device, 1, true));
	CheckReturn(CB_CrossBilateralFilter.Initialize(Device, 1, true));
	CheckReturn(CB_CalcLocalMeanVar.Initialize(Device, 1, true));
	CheckReturn(CB_TSPPBlend.Initialize(Device, 1, true));
	CheckReturn(CB_AtrousFilter.Initialize(Device, 1, true));
	CheckReturn(CB_DebugMap.Initialize(Device, 1, true));

	return true;
}