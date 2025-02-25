#include "DirectX/Shading/VolumetricLight.h"
#include "Common/Debug/Logger.h"
#include "Common/Helper/MathHelper.h"
#include "Common/Light/Light.h"
#include "DirectX/Util/D3D12Util.h"
#include "DirectX/Util/ShaderManager.h"
#include "HlslCompaction.h"

using namespace VolumetricLight;

namespace {
	const CHAR* const CS_CalcScatteringAndDensity	= "CS_CalcScatteringAndDensity";
	const CHAR* const CS_AccumulateScattering		= "CS_AccumulateScattering";

	const CHAR* const VS_ApplyFog = "VS_ApplyFog";
	const CHAR* const PS_ApplyFog = "PS_ApplyFog";
}

VolumetricLightClass::VolumetricLightClass() {
	for (size_t i = 0, end = mFrustumVolumeMaps.size(); i < end; ++i) 
		mFrustumVolumeMaps[i] = std::make_unique<GpuResource>();
	mFrustumVolumeUploadBuffer = std::make_unique<GpuResource>();
}

BOOL VolumetricLightClass::Initialize(
		Locker<ID3D12Device5>* const device, ShaderManager* const manager,
		UINT clientW, UINT clientH,
		UINT texW, UINT texH, UINT texD) {
	md3dDevice = device;
	mShaderManager = manager;

	mClientWidth = clientW;
	mClientHeight = clientH;

	mTexWidth = texW;
	mTexHeight = texH;
	mTexDepth = texD;

	CheckReturn(BuildResources());

	return TRUE;
}

BOOL VolumetricLightClass::PrepareUpdate(ID3D12GraphicsCommandList* const cmdList) {	
	const auto dest = mFrustumVolumeMaps[1].get();
	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(dest->Resource(), 0, 1);	

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		CheckReturn(mFrustumVolumeUploadBuffer->Initialize(
			device,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
			D3D12_RESOURCE_STATE_COMMON,
			nullptr
		));
	}

	const UINT size = static_cast<UINT64>(mTexWidth) * static_cast<UINT64>(mTexHeight) * static_cast<UINT64>(mTexDepth) * 4;
	std::vector<std::uint16_t> data(size);
	
	for (UINT i = 0; i < size; i += 4) {
		data[static_cast<UINT64>(i) + 0] = DirectX::PackedVector::XMConvertFloatToHalf(0.f);
		data[static_cast<UINT64>(i) + 1] = DirectX::PackedVector::XMConvertFloatToHalf(0.f);
		data[static_cast<UINT64>(i) + 2] = DirectX::PackedVector::XMConvertFloatToHalf(0.f);
		data[static_cast<UINT64>(i) + 3] = DirectX::PackedVector::XMConvertFloatToHalf(VolumetricLight::InvalidFrustumVolumeAlphaValue);
	}
	
	D3D12_SUBRESOURCE_DATA subResourceData = {};
	subResourceData.pData = data.data();
	subResourceData.RowPitch = static_cast<UINT64>(mTexWidth) * 4 * sizeof(std::uint16_t);
	subResourceData.SlicePitch = subResourceData.RowPitch * mTexHeight;

	mFrustumVolumeUploadBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_SOURCE);
	dest->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);
		
	UpdateSubresources(
		cmdList,
		dest->Resource(),
		mFrustumVolumeUploadBuffer->Resource(),
		0,
		0,
		1,
		&subResourceData
	);
	
	dest->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	return TRUE;
}

BOOL VolumetricLightClass::CompileShaders(const std::wstring& filePath) {
	{
		const std::wstring fullPath = filePath + L"CalculateScatteringAndDensityCS.hlsl";
		const auto csInfo = D3D12ShaderInfo(fullPath.c_str(), L"CS", L"cs_6_3");
		CheckReturn(mShaderManager->CompileShader(csInfo, CS_CalcScatteringAndDensity));
	}
	{
		const std::wstring fullPath = filePath + L"AccumulateScattering.hlsl";
		const auto csInfo = D3D12ShaderInfo(fullPath.c_str(), L"CS", L"cs_6_3");
		CheckReturn(mShaderManager->CompileShader(csInfo, CS_AccumulateScattering));
	}
	{
		const std::wstring fullPath = filePath + L"ApplyFog.hlsl";
		const auto vsInfo = D3D12ShaderInfo(fullPath.c_str(), L"VS", L"vs_6_3");
		const auto psInfo = D3D12ShaderInfo(fullPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, VS_ApplyFog));
		CheckReturn(mShaderManager->CompileShader(psInfo, PS_ApplyFog));
	}

	return TRUE;
}

BOOL VolumetricLightClass::BuildRootSignature(const StaticSamplers& samplers) {
	D3D12Util::Descriptor::RootSignature::Builder builder;
	// CalcScatteringAndDensity
	{
		CD3DX12_DESCRIPTOR_RANGE texTables[5] = {}; UINT index = 0;
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1,			0, 0);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, MaxLights, 1, 0);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, MaxLights, 0, 1);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, MaxLights, 0, 2);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1,			0, 0);

		index = 0;

		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::CalcScatteringAndDensity::Count] = {};
		slotRootParameter[RootSignature::CalcScatteringAndDensity::ECB_Pass].InitAsConstantBufferView(0);
		slotRootParameter[RootSignature::CalcScatteringAndDensity::EC_Consts].InitAsConstants(RootConstant::CalcScatteringAndDensity::Count, 1);
		slotRootParameter[RootSignature::CalcScatteringAndDensity::ESI_PrevFrustumVolume].InitAsDescriptorTable(1, &texTables[index++]);
		slotRootParameter[RootSignature::CalcScatteringAndDensity::ESI_ZDepth].InitAsDescriptorTable(1, &texTables[index++]);
		slotRootParameter[RootSignature::CalcScatteringAndDensity::ESI_ZDepthCube].InitAsDescriptorTable(1, &texTables[index++]);
		slotRootParameter[RootSignature::CalcScatteringAndDensity::ESI_FaceIDCube].InitAsDescriptorTable(1, &texTables[index++]);
		slotRootParameter[RootSignature::CalcScatteringAndDensity::EUO_FrustumVolume].InitAsDescriptorTable(1, &texTables[index++]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		builder.Enqueue(rootSigDesc, IID_PPV_ARGS(&mRootSignatures[RootSignature::E_CalcScatteringAndDensity]));
	}
	// AccumulateScattering
	{
		CD3DX12_DESCRIPTOR_RANGE texTables[1] = {}; UINT index = 0;
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

		index = 0;

		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::AccumulateScattering::Count] = {};
		slotRootParameter[RootSignature::AccumulateScattering::EC_Consts].InitAsConstants(RootConstant::AccumulateScattering::Count, 0);
		slotRootParameter[RootSignature::AccumulateScattering::EUIO_FrustumVolume].InitAsDescriptorTable(1, &texTables[index++]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		builder.Enqueue(rootSigDesc, IID_PPV_ARGS(&mRootSignatures[RootSignature::E_AccumulateScattering]));
	}
	// ApplyFog
	{
		CD3DX12_DESCRIPTOR_RANGE texTables[2] = {}; UINT index = 0;
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);

		index = 0;

		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::ApplyFog::Count] = {};
		slotRootParameter[RootSignature::ApplyFog::ECB_Pass].InitAsConstantBufferView(0);
		slotRootParameter[RootSignature::ApplyFog::ESI_Position].InitAsDescriptorTable(1, &texTables[index++]);
		slotRootParameter[RootSignature::ApplyFog::ESI_FrustumVolume].InitAsDescriptorTable(1, &texTables[index++]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		builder.Enqueue(rootSigDesc, IID_PPV_ARGS(&mRootSignatures[RootSignature::E_ApplyFog]));
	}

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		CheckReturn(builder.Build(device));
	}

	return TRUE;
}

BOOL VolumetricLightClass::BuildPSO() {
	D3D12Util::Descriptor::PipelineState::Builder builder;
	// CalcScatteringAndDensity
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = mRootSignatures[RootSignature::E_CalcScatteringAndDensity].Get();
		psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		{
			const auto cs = mShaderManager->GetDxcShader(CS_CalcScatteringAndDensity);
			psoDesc.CS = { reinterpret_cast<BYTE*>(cs->GetBufferPointer()), cs->GetBufferSize() };
		}
		builder.Enqueue(psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::EC_CalcScatteringAndDensity]));
	}
	// AccumulateScattering
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = mRootSignatures[RootSignature::E_AccumulateScattering].Get();
		psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		{
			const auto cs = mShaderManager->GetDxcShader(CS_AccumulateScattering);
			psoDesc.CS = { reinterpret_cast<BYTE*>(cs->GetBufferPointer()), cs->GetBufferSize() };
		}
		builder.Enqueue(psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::EC_AccumulateScattering]));
	}
	// ApplyFog
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::QuadPsoDesc();
		psoDesc.pRootSignature = mRootSignatures[RootSignature::E_ApplyFog].Get();
		{
			const auto vs = mShaderManager->GetDxcShader(VS_ApplyFog);
			const auto ps = mShaderManager->GetDxcShader(PS_ApplyFog);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
		psoDesc.NumRenderTargets = 2;
		psoDesc.RTVFormats[0] = HDR_FORMAT;
		psoDesc.RTVFormats[1] = DebugMapFormat;

		D3D12_RENDER_TARGET_BLEND_DESC blendDesc;
		blendDesc.BlendEnable = TRUE;
		blendDesc.LogicOpEnable = FALSE;
		blendDesc.SrcBlend = D3D12_BLEND_INV_SRC_ALPHA;
		blendDesc.DestBlend = D3D12_BLEND_SRC_ALPHA;
		blendDesc.BlendOp = D3D12_BLEND_OP_ADD;
		blendDesc.SrcBlendAlpha = D3D12_BLEND_ZERO;
		blendDesc.DestBlendAlpha = D3D12_BLEND_ONE;
		blendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		blendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
		blendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		psoDesc.BlendState.RenderTarget[0] = blendDesc;
		//psoDesc.BlendState.RenderTarget[1] = blendDesc;

		builder.Enqueue(psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::EG_ApplyFog]));
	}

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		CheckReturn(builder.Build(device));
	}

	return TRUE;
}

void VolumetricLightClass::Run(
		ID3D12GraphicsCommandList* const cmdList,
		const D3D12_VIEWPORT& viewport,
		const D3D12_RECT& scissorRect,
		GpuResource* const backBuffer,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
		D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
		D3D12_GPU_DESCRIPTOR_HANDLE si_zdepth,
		D3D12_GPU_DESCRIPTOR_HANDLE si_zdepthCube,
		D3D12_GPU_DESCRIPTOR_HANDLE si_faceIDCube,
		D3D12_GPU_DESCRIPTOR_HANDLE si_position,
		FLOAT nearZ, FLOAT farZ, FLOAT depth_exp, 
		FLOAT uniformDensity, 
		FLOAT anisotropicCoeiff,
		FLOAT densityScale) {
	GpuResource* currVolume;
	GpuResource* prevVolume;
	D3D12_GPU_DESCRIPTOR_HANDLE currVolumeGpuUav;
	D3D12_GPU_DESCRIPTOR_HANDLE currVolumeGpuSrv;
	D3D12_GPU_DESCRIPTOR_HANDLE prevVolumeGpuSrv;

	if (mCurrentFrame) {
		currVolume = mFrustumVolumeMaps[1].get();
		prevVolume = mFrustumVolumeMaps[0].get();
		currVolumeGpuUav = mhFrustumVolumeMapGpuUavs[1];
		currVolumeGpuSrv = mhFrustumVolumeMapGpuSrvs[1];
		prevVolumeGpuSrv = mhFrustumVolumeMapGpuSrvs[0];
	}
	else {
		currVolume = mFrustumVolumeMaps[0].get();
		prevVolume = mFrustumVolumeMaps[1].get();
		currVolumeGpuUav = mhFrustumVolumeMapGpuUavs[0];
		currVolumeGpuSrv = mhFrustumVolumeMapGpuSrvs[0];
		prevVolumeGpuSrv = mhFrustumVolumeMapGpuSrvs[1];
	}

	CalculateScatteringAndDensity(
		cmdList, 
		currVolume, 
		prevVolume, 
		currVolumeGpuUav, 
		prevVolumeGpuSrv, 
		cb_pass, 
		si_zdepth, 
		si_zdepthCube, 
		si_faceIDCube, 
		nearZ, farZ, depth_exp, 
		uniformDensity, 
		anisotropicCoeiff);
	AccumulateScattering(cmdList, currVolume, currVolumeGpuUav, nearZ, farZ, depth_exp, densityScale);
	ApplyFog(cmdList, viewport, scissorRect, backBuffer, ro_backBuffer, cb_pass, currVolumeGpuSrv, si_position);

	mCurrentFrame = ++mFrameCount % 2 != 0;
}

void VolumetricLightClass::AllocateDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
		UINT descSize) {
	for (size_t i = 0; i < 2; ++i) {
		mhFrustumVolumeMapCpuSrvs[i] = hCpu.Offset(1, descSize);
		mhFrustumVolumeMapGpuSrvs[i] = hGpu.Offset(1, descSize);

		mhFrustumVolumeMapCpuUavs[i] = hCpu.Offset(1, descSize);
		mhFrustumVolumeMapGpuUavs[i] = hGpu.Offset(1, descSize);
	}
}

BOOL VolumetricLightClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
	srvDesc.Format = FrustumMapFormat;
	srvDesc.Texture3D.MostDetailedMip = 0;
	srvDesc.Texture3D.MipLevels = 1;
	srvDesc.Texture3D.ResourceMinLODClamp = 0.0f;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
	uavDesc.Format = FrustumMapFormat;
	uavDesc.Texture3D.MipSlice = 0;
	uavDesc.Texture3D.WSize = mTexDepth;
	uavDesc.Texture3D.FirstWSlice = 0;

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		for (size_t i = 0; i < 2; ++i) {
			const auto& frustum = mFrustumVolumeMaps[i]->Resource();

			device->CreateShaderResourceView(frustum, &srvDesc, mhFrustumVolumeMapCpuSrvs[i]);
			device->CreateUnorderedAccessView(frustum, nullptr, &uavDesc, mhFrustumVolumeMapCpuUavs[i]);
		}
	}

	return TRUE;
}

BOOL VolumetricLightClass::BuildResources() {
	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
	texDesc.Format = FrustumMapFormat;
	texDesc.Width = mTexWidth;
	texDesc.Height = mTexHeight;
	texDesc.DepthOrArraySize = mTexDepth;
	texDesc.Alignment = 0;
	texDesc.MipLevels = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		for (size_t i = 0; i < 2; ++i) {
			const auto volume = mFrustumVolumeMaps[i].get();

			std::wstring name(L"VolumetricLight_FrustumMap_");
			name.append(std::to_wstring(i));

			CheckReturn(volume->Initialize(
				device,
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&texDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				nullptr,
				name.c_str()
			));
		}
	}

	return TRUE;
}

void VolumetricLightClass::CalculateScatteringAndDensity(
		ID3D12GraphicsCommandList* const cmdList,
		GpuResource* const currFrustumVolume,
		GpuResource* const prevFrustumVolume,
		D3D12_GPU_DESCRIPTOR_HANDLE uo_currFrustumVolume,
		D3D12_GPU_DESCRIPTOR_HANDLE si_prevFrustumVolume,
		D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
		D3D12_GPU_DESCRIPTOR_HANDLE si_zdepth,
		D3D12_GPU_DESCRIPTOR_HANDLE si_zdepthCube,
		D3D12_GPU_DESCRIPTOR_HANDLE si_faceIDCube,
		FLOAT nearZ, FLOAT farZ, FLOAT depth_exp, 
		FLOAT uniformDensity, 
		FLOAT anisotropicCoeiff) {
	cmdList->SetPipelineState(mPSOs[PipelineState::EC_CalcScatteringAndDensity].Get());
	cmdList->SetComputeRootSignature(mRootSignatures[RootSignature::E_CalcScatteringAndDensity].Get());

	currFrustumVolume->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	D3D12Util::UavBarrier(cmdList, currFrustumVolume);

	cmdList->SetComputeRootConstantBufferView(RootSignature::CalcScatteringAndDensity::ECB_Pass, cb_pass);

	RootConstant::CalcScatteringAndDensity::Struct rc;
	rc.gNearZ = nearZ;
	rc.gFarZ = farZ;
	rc.gDepthExponent = depth_exp;
	rc.gUniformDensity = uniformDensity;
	rc.gAnisotropicCoefficient = anisotropicCoeiff;
	rc.gFrameCount = mFrameCount;

	std::array<std::uint32_t, RootConstant::CalcScatteringAndDensity::Count> consts;
	std::memcpy(consts.data(), &rc, sizeof(RootConstant::CalcScatteringAndDensity::Struct));

	cmdList->SetComputeRoot32BitConstants(RootSignature::CalcScatteringAndDensity::EC_Consts, RootConstant::CalcScatteringAndDensity::Count, consts.data(), 0);

	cmdList->SetComputeRootDescriptorTable(RootSignature::CalcScatteringAndDensity::ESI_ZDepth, si_zdepth);
	cmdList->SetComputeRootDescriptorTable(RootSignature::CalcScatteringAndDensity::ESI_ZDepthCube, si_zdepthCube);
	cmdList->SetComputeRootDescriptorTable(RootSignature::CalcScatteringAndDensity::ESI_FaceIDCube, si_faceIDCube);
	cmdList->SetComputeRootDescriptorTable(RootSignature::CalcScatteringAndDensity::EUO_FrustumVolume, si_prevFrustumVolume);

	cmdList->Dispatch(
		D3D12Util::CeilDivide(static_cast<UINT>(mTexWidth), ThreadGroup::CalcScatteringAndDensity::Width),
		D3D12Util::CeilDivide(static_cast<UINT>(mTexHeight), ThreadGroup::CalcScatteringAndDensity::Height),
		D3D12Util::CeilDivide(static_cast<UINT>(mTexDepth), ThreadGroup::CalcScatteringAndDensity::Depth)
	);

	currFrustumVolume->Transite(cmdList, D3D12_RESOURCE_STATE_GENERIC_READ);
	D3D12Util::UavBarrier(cmdList, currFrustumVolume);
}

void VolumetricLightClass::AccumulateScattering(
		ID3D12GraphicsCommandList* const cmdList,
		GpuResource* const currFrustumVolume,
		D3D12_GPU_DESCRIPTOR_HANDLE uio_currFrustumVolume,
		FLOAT nearZ, FLOAT farZ, FLOAT depth_exp, 
		FLOAT densityScale) {
	cmdList->SetPipelineState(mPSOs[PipelineState::EC_AccumulateScattering].Get());
	cmdList->SetComputeRootSignature(mRootSignatures[RootSignature::E_AccumulateScattering].Get());

	currFrustumVolume->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	D3D12Util::UavBarrier(cmdList, currFrustumVolume);

	RootConstant::AccumulateScattering::Struct rc;
	rc.gNearZ = nearZ;
	rc.gFarZ = farZ;
	rc.gDepthExponent = depth_exp;
	rc.gDensityScale = densityScale;

	std::array<std::uint32_t, RootConstant::AccumulateScattering::Count> consts;
	std::memcpy(consts.data(), &rc, sizeof(RootConstant::AccumulateScattering::Struct));

	cmdList->SetComputeRoot32BitConstants(RootSignature::AccumulateScattering::EC_Consts, RootConstant::AccumulateScattering::Count, consts.data(), 0);
	cmdList->SetComputeRootDescriptorTable(RootSignature::AccumulateScattering::EUIO_FrustumVolume, uio_currFrustumVolume);

	cmdList->Dispatch(
		D3D12Util::CeilDivide(static_cast<UINT>(mTexWidth), ThreadGroup::AccumulateScattering::Width),
		D3D12Util::CeilDivide(static_cast<UINT>(mTexHeight), ThreadGroup::AccumulateScattering::Height),
		1
	);

	currFrustumVolume->Transite(cmdList, D3D12_RESOURCE_STATE_GENERIC_READ);
	D3D12Util::UavBarrier(cmdList, currFrustumVolume);
}

void VolumetricLightClass::ApplyFog(
		ID3D12GraphicsCommandList* const cmdList,
		const D3D12_VIEWPORT& viewport,
		const D3D12_RECT& scissorRect,
		GpuResource* const backBuffer,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
		D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
		D3D12_GPU_DESCRIPTOR_HANDLE si_currFrustumVolume,
		D3D12_GPU_DESCRIPTOR_HANDLE si_position) {
	cmdList->SetPipelineState(mPSOs[PipelineState::EG_ApplyFog].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[RootSignature::E_ApplyFog].Get());

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->OMSetRenderTargets(1, &ro_backBuffer, TRUE, nullptr);

	cmdList->SetGraphicsRootConstantBufferView(RootSignature::ApplyFog::ECB_Pass, cb_pass);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::ApplyFog::ESI_Position, si_position);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::ApplyFog::ESI_FrustumVolume, si_currFrustumVolume);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);
}
