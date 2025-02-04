#include "VolumetricLight.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "D3D12Util.h"
#include "HlslCompaction.h"
#include "Light.h"

using namespace VolumetricLight;

namespace {
	const CHAR* const CS_CalcScatteringAndDensity	= "CS_CalcScatteringAndDensity";
	const CHAR* const CS_AccumulateScattering		= "CS_AccumulateScattering";

	const CHAR* const VS_ApplyFog = "VS_ApplyFog";
	const CHAR* const PS_ApplyFog = "PS_ApplyFog";
}

VolumetricLightClass::VolumetricLightClass() {
	mFrustumMap = std::make_unique<GpuResource>();
	mDebugMap = std::make_unique<GpuResource>();
}

BOOL VolumetricLightClass::Initialize(
		ID3D12Device* device, ShaderManager* const manager,
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

BOOL VolumetricLightClass::CompileShaders(const std::wstring& filePath) {
	{
		const std::wstring fullPath = filePath + L"CalculateScatteringAndDensityCS.hlsl";
		auto csInfo = D3D12ShaderInfo(fullPath.c_str(), L"CS", L"cs_6_3");
		CheckReturn(mShaderManager->CompileShader(csInfo, CS_CalcScatteringAndDensity));
	}
	{
		const std::wstring fullPath = filePath + L"AccumulateScattering.hlsl";
		auto csInfo = D3D12ShaderInfo(fullPath.c_str(), L"CS", L"cs_6_3");
		CheckReturn(mShaderManager->CompileShader(csInfo, CS_AccumulateScattering));
	}
	{
		const std::wstring fullPath = filePath + L"ApplyFog.hlsl";
		auto vsInfo = D3D12ShaderInfo(fullPath.c_str(), L"VS", L"vs_6_3");
		auto psInfo = D3D12ShaderInfo(fullPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, VS_ApplyFog));
		CheckReturn(mShaderManager->CompileShader(psInfo, PS_ApplyFog));
	}

	return TRUE;
}

BOOL VolumetricLightClass::BuildRootSignature(const StaticSamplers& samplers) {
	// CalcScatteringAndDensity
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::CalcScatteringAndDensity::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[4]; UINT index = 0;
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, MaxLights, 0, 0);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, MaxLights, 0, 1);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, MaxLights, 0, 2);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

		index = 0;

		slotRootParameter[RootSignature::CalcScatteringAndDensity::ECB_Pass].InitAsConstantBufferView(0);
		slotRootParameter[RootSignature::CalcScatteringAndDensity::EC_Consts].InitAsConstants(RootSignature::CalcScatteringAndDensity::RootConstant::Count, 1);
		slotRootParameter[RootSignature::CalcScatteringAndDensity::ESI_ZDepth].InitAsDescriptorTable(1, &texTables[index++]);
		slotRootParameter[RootSignature::CalcScatteringAndDensity::ESI_ZDepthCube].InitAsDescriptorTable(1, &texTables[index++]);
		slotRootParameter[RootSignature::CalcScatteringAndDensity::ESI_FaceIDCube].InitAsDescriptorTable(1, &texTables[index++]);
		slotRootParameter[RootSignature::CalcScatteringAndDensity::EUO_FrustumVolume].InitAsDescriptorTable(1, &texTables[index++]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures[RootSignature::E_CalcScatteringAndDensity]));
	}
	// AccumulateScattering
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::AccumulateScattering::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[1]; UINT index = 0;
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

		index = 0;

		slotRootParameter[RootSignature::AccumulateScattering::EC_Consts].InitAsConstants(RootSignature::AccumulateScattering::RootConstant::Count, 0);
		slotRootParameter[RootSignature::AccumulateScattering::EUIO_FrustumVolume].InitAsDescriptorTable(1, &texTables[index++]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures[RootSignature::E_AccumulateScattering]));
	}
	// ApplyFog
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::ApplyFog::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[2]; UINT index = 0;
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
		texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);

		index = 0;

		slotRootParameter[RootSignature::ApplyFog::ECB_Pass].InitAsConstantBufferView(0);
		slotRootParameter[RootSignature::ApplyFog::ESI_Position].InitAsDescriptorTable(1, &texTables[index++]);
		slotRootParameter[RootSignature::ApplyFog::ESI_FrustumVolume].InitAsDescriptorTable(1, &texTables[index++]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures[RootSignature::E_ApplyFog]));
	}

	return TRUE;
}

BOOL VolumetricLightClass::BuildPSO() {
	// CalcScatteringAndDensity
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = mRootSignatures[RootSignature::E_CalcScatteringAndDensity].Get();
		psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		{
			auto cs = mShaderManager->GetDxcShader(CS_CalcScatteringAndDensity);
			psoDesc.CS = { reinterpret_cast<BYTE*>(cs->GetBufferPointer()), cs->GetBufferSize() };
		}
		CheckHRESULT(md3dDevice->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::EC_CalcScatteringAndDensity])));
	}
	// AccumulateScattering
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = mRootSignatures[RootSignature::E_AccumulateScattering].Get();
		psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		{
			auto cs = mShaderManager->GetDxcShader(CS_AccumulateScattering);
			psoDesc.CS = { reinterpret_cast<BYTE*>(cs->GetBufferPointer()), cs->GetBufferSize() };
		}
		CheckHRESULT(md3dDevice->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::EC_AccumulateScattering])));
	}
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::QuadPsoDesc();
		psoDesc.pRootSignature = mRootSignatures[RootSignature::E_ApplyFog].Get();
		{
			auto vs = mShaderManager->GetDxcShader(VS_ApplyFog);
			auto ps = mShaderManager->GetDxcShader(PS_ApplyFog);
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

		CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::EG_ApplyFog])));
	}

	return TRUE;
}

void VolumetricLightClass::Run(
		ID3D12GraphicsCommandList* const cmdList,
		const D3D12_VIEWPORT& viewport,
		const D3D12_RECT& scissorRect,
		GpuResource* backBuffer,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
		D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
		D3D12_GPU_DESCRIPTOR_HANDLE si_zdepth,
		D3D12_GPU_DESCRIPTOR_HANDLE si_zdepthCube,
		D3D12_GPU_DESCRIPTOR_HANDLE si_faceIDCube,
		D3D12_GPU_DESCRIPTOR_HANDLE si_position,
		FLOAT nearZ, FLOAT farZ,
		FLOAT depth_exp, 
		FLOAT uniformDensity, 
		FLOAT anisotropicCoeiff,
		FLOAT densityScale) {
	CalculateScatteringAndDensity(cmdList, cb_pass, si_zdepth, si_zdepthCube, si_faceIDCube, nearZ, farZ, depth_exp, uniformDensity, anisotropicCoeiff);
	AccumulateScattering(cmdList, nearZ, farZ, densityScale);
	ApplyFog(cmdList, viewport, scissorRect, backBuffer, ro_backBuffer, cb_pass, si_position);
}

void VolumetricLightClass::BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
		UINT descSize, UINT rtvDescSize) {
	mhFrustumMapCpuSrv = hCpu.Offset(1, descSize);
	mhFrustumMapGpuSrv = hGpu.Offset(1, descSize);

	mhFrustumMapCpuUav = hCpu.Offset(1, descSize);
	mhFrustumMapGpuUav = hGpu.Offset(1, descSize);

	mhDebugMapCpuRtv = hCpuRtv.Offset(1, rtvDescSize);

	BuildDescriptors();
}

void VolumetricLightClass::BuildDescriptors() {
	{
		const auto& frustum = mFrustumMap->Resource();

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
		srvDesc.Format = FrustumMapFormat;
		srvDesc.Texture3D.MostDetailedMip = 0;
		srvDesc.Texture3D.MipLevels = 1;
		srvDesc.Texture3D.ResourceMinLODClamp = 0.0f;

		md3dDevice->CreateShaderResourceView(frustum, &srvDesc, mhFrustumMapCpuSrv);

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
		uavDesc.Format = FrustumMapFormat;
		uavDesc.Texture3D.MipSlice = 0;
		uavDesc.Texture3D.WSize = mTexDepth;
		uavDesc.Texture3D.FirstWSlice = 0;

		md3dDevice->CreateUnorderedAccessView(frustum, nullptr, &uavDesc, mhFrustumMapCpuUav);
	}
	{
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Format = DebugMapFormat;
		rtvDesc.Texture2D.MipSlice = 0;
		rtvDesc.Texture2D.PlaneSlice = 0;
		md3dDevice->CreateRenderTargetView(mDebugMap->Resource(), &rtvDesc, mhDebugMapCpuRtv);
	}
}

BOOL VolumetricLightClass::BuildResources() {
	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Alignment = 0;
	texDesc.MipLevels = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	// Scattering and density map
	{
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
		texDesc.Width = mTexWidth;
		texDesc.Height = mTexHeight;
		texDesc.DepthOrArraySize = mTexDepth;
		texDesc.Format = FrustumMapFormat;
		texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		CheckReturn(mFrustumMap->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"VolumetricLight_FrustumMap"
		));
	}
	// Bebug map
	{
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Width = mClientWidth;
		texDesc.Height = mClientHeight;
		texDesc.DepthOrArraySize = 1;
		texDesc.Format = DebugMapFormat;
		texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		CheckReturn(mDebugMap->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			L"VolumetricLight_DebugMap"
		));
	}

	return TRUE;
}

void VolumetricLightClass::CalculateScatteringAndDensity(
		ID3D12GraphicsCommandList* const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
		D3D12_GPU_DESCRIPTOR_HANDLE si_zdepth,
		D3D12_GPU_DESCRIPTOR_HANDLE si_zdepthCube,
		D3D12_GPU_DESCRIPTOR_HANDLE si_faceIDCube,
		FLOAT nearZ, FLOAT farZ,
		FLOAT depth_exp, 
		FLOAT uniformDensity, 
		FLOAT anisotropicCoeiff) {
	cmdList->SetPipelineState(mPSOs[PipelineState::EC_CalcScatteringAndDensity].Get());
	cmdList->SetComputeRootSignature(mRootSignatures[RootSignature::E_CalcScatteringAndDensity].Get());

	mFrustumMap->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	D3D12Util::UavBarrier(cmdList, mFrustumMap.get());

	cmdList->SetComputeRootConstantBufferView(RootSignature::CalcScatteringAndDensity::ECB_Pass, cb_pass);

	FLOAT values[RootSignature::CalcScatteringAndDensity::RootConstant::Count] = { nearZ, farZ, depth_exp, uniformDensity, anisotropicCoeiff };
	cmdList->SetComputeRoot32BitConstants(RootSignature::CalcScatteringAndDensity::EC_Consts, _countof(values), values, 0);

	cmdList->SetComputeRootDescriptorTable(RootSignature::CalcScatteringAndDensity::ESI_ZDepth, si_zdepth);
	cmdList->SetComputeRootDescriptorTable(RootSignature::CalcScatteringAndDensity::ESI_ZDepthCube, si_zdepthCube);
	cmdList->SetComputeRootDescriptorTable(RootSignature::CalcScatteringAndDensity::ESI_FaceIDCube, si_faceIDCube);
	cmdList->SetComputeRootDescriptorTable(RootSignature::CalcScatteringAndDensity::EUO_FrustumVolume, mhFrustumMapGpuUav);

	cmdList->Dispatch(
		D3D12Util::CeilDivide(static_cast<UINT>(mTexWidth), CalcScatteringAndDensity::ThreadGroup::Width),
		D3D12Util::CeilDivide(static_cast<UINT>(mTexHeight), CalcScatteringAndDensity::ThreadGroup::Height),
		D3D12Util::CeilDivide(static_cast<UINT>(mTexDepth), CalcScatteringAndDensity::ThreadGroup::Depth)
	);

	mFrustumMap->Transite(cmdList, D3D12_RESOURCE_STATE_GENERIC_READ);
	D3D12Util::UavBarrier(cmdList, mFrustumMap.get());
}

void VolumetricLightClass::AccumulateScattering(
		ID3D12GraphicsCommandList* const cmdList,
		FLOAT nearZ, FLOAT farZ, FLOAT densityScale) {
	cmdList->SetPipelineState(mPSOs[PipelineState::EC_AccumulateScattering].Get());
	cmdList->SetComputeRootSignature(mRootSignatures[RootSignature::E_AccumulateScattering].Get());

	mFrustumMap->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	D3D12Util::UavBarrier(cmdList, mFrustumMap.get());

	FLOAT values[RootSignature::AccumulateScattering::RootConstant::Count] = { nearZ, farZ, 4, densityScale };
	cmdList->SetComputeRoot32BitConstants(RootSignature::AccumulateScattering::EC_Consts, _countof(values), values, 0);

	cmdList->SetComputeRootDescriptorTable(RootSignature::AccumulateScattering::EUIO_FrustumVolume, mhFrustumMapGpuUav);

	cmdList->Dispatch(
		D3D12Util::CeilDivide(static_cast<UINT>(mTexWidth), AccumulateScattering::ThreadGroup::Width),
		D3D12Util::CeilDivide(static_cast<UINT>(mTexHeight), AccumulateScattering::ThreadGroup::Height),
		1
	);

	mFrustumMap->Transite(cmdList, D3D12_RESOURCE_STATE_GENERIC_READ);
	D3D12Util::UavBarrier(cmdList, mFrustumMap.get());
}

void VolumetricLightClass::ApplyFog(
		ID3D12GraphicsCommandList* const cmdList,
		const D3D12_VIEWPORT& viewport,
		const D3D12_RECT& scissorRect,
		GpuResource* backBuffer,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
		D3D12_GPU_VIRTUAL_ADDRESS cb_pass,
		D3D12_GPU_DESCRIPTOR_HANDLE si_position) {
	cmdList->SetPipelineState(mPSOs[PipelineState::EG_ApplyFog].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[RootSignature::E_ApplyFog].Get());

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	mDebugMap->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvs[] = { ro_backBuffer, mhDebugMapCpuRtv };
	cmdList->OMSetRenderTargets(_countof(rtvs), rtvs, FALSE, nullptr);

	cmdList->SetGraphicsRootConstantBufferView(RootSignature::ApplyFog::ECB_Pass, cb_pass);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::ApplyFog::ESI_Position, si_position);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::ApplyFog::ESI_FrustumVolume, mhFrustumMapGpuSrv);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	mDebugMap->Transite(cmdList, D3D12_RESOURCE_STATE_GENERIC_READ);
}