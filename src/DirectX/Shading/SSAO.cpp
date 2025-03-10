#include "DirectX/Shading/SSAO.h"
#include "Common/Debug/Logger.h"
#include "DirectX/Infrastructure/FrameResource.h"
#include "DirectX/Util/ShaderManager.h"
#include "HlslCompaction.h"

#include <DirectXColors.h>

using namespace DirectX;
using namespace DirectX::PackedVector;

using namespace SSAO;

namespace {
	const CHAR* const VS_SSAO = "VS_SSAO";
	const CHAR* const PS_SSAO = "PS_SSAO";
}

SSAOClass::SSAOClass() {
	mRandomVectorMap = std::make_unique<GpuResource>();
	mRandomVectorMapUploadBuffer = std::make_unique<GpuResource>();
	mAOCoefficientMaps[0] = std::make_unique<GpuResource>();
	mAOCoefficientMaps[1] = std::make_unique<GpuResource>();
}

BOOL SSAOClass::Initialize(Locker<ID3D12Device5>* const device, ShaderManager* const manager, UINT width, UINT height, Resolution::Type type) {
	md3dDevice = device;
	mShaderManager = manager;
	mResolutionType = type;

	CheckReturn(BuildResources(width, height));

	BuildOffsetVectors();

	return TRUE;
}

BOOL SSAOClass::PrepareUpdate(ID3D12GraphicsCommandList* const cmdList) {
	CheckReturn(BuildRandomVectorTexture(cmdList));

	return TRUE;
}

BOOL SSAOClass::CompileShaders(const std::wstring& filePath) {
	const std::wstring actualPath = filePath + L"SSAO.hlsl";
	const auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
	const auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
	CheckReturn(mShaderManager->CompileShader(vsInfo, VS_SSAO));
	CheckReturn(mShaderManager->CompileShader(psInfo, PS_SSAO));

	return TRUE;
}

BOOL SSAOClass::BuildRootSignature(const StaticSamplers& samplers) {
	CD3DX12_DESCRIPTOR_RANGE texTables[3] = {}; UINT index = 0;
	texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
	texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
	texTables[index++].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);

	index = 0;

	CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::Count] = {};
	slotRootParameter[RootSignature::ECB_Pass].InitAsConstantBufferView(0);
	slotRootParameter[RootSignature::ESI_Normal].InitAsDescriptorTable(		 1, &texTables[index++]);
	slotRootParameter[RootSignature::ESI_Depth].InitAsDescriptorTable(		 1, &texTables[index++]);
	slotRootParameter[RootSignature::ESI_RandomVector].InitAsDescriptorTable(1, &texTables[index++]);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);
	
	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		CheckReturn(D3D12Util::CreateRootSignature(device, rootSigDesc, &mRootSignature, L"SSAO_RS_Default"));
	}

	return TRUE;
}

BOOL SSAOClass::BuildPSO() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::QuadPsoDesc();
	psoDesc.pRootSignature = mRootSignature.Get();
	{
		const auto vs = mShaderManager->GetDxcShader(VS_SSAO);
		const auto ps = mShaderManager->GetDxcShader(PS_SSAO);
		psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
		psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	psoDesc.RTVFormats[0] = AOCoefficientMapFormat;

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		CheckReturn(D3D12Util::CreateGraphicsPipelineState(device, psoDesc, IID_PPV_ARGS(&mPSO), L"SSAO_GPS_Default"));
	}

	return TRUE;
}

void SSAOClass::Run(
		ID3D12GraphicsCommandList* const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS passCBAddress,
		D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
		D3D12_GPU_DESCRIPTOR_HANDLE si_depth) {
	cmdList->SetPipelineState(mPSO.Get());
	cmdList->SetGraphicsRootSignature(mRootSignature.Get());

	cmdList->RSSetViewports(1, &mViewport);
	cmdList->RSSetScissorRects(1, &mScissorRect);

	const auto aoCeffMap = mAOCoefficientMaps[0].get();
	aoCeffMap->Transite(cmdList,D3D12_RESOURCE_STATE_RENDER_TARGET);

	const auto aoCeffMap0Rtv = mhAOCoefficientMapCpuRtvs[0];
	cmdList->ClearRenderTargetView(aoCeffMap0Rtv, AOCoefficientMapClearValues, 0, nullptr);
	cmdList->OMSetRenderTargets(1, &aoCeffMap0Rtv, TRUE, nullptr);

	cmdList->SetGraphicsRootConstantBufferView(RootSignature::ECB_Pass, passCBAddress);

	cmdList->SetGraphicsRootDescriptorTable(RootSignature::ESI_Normal, si_normal);
	cmdList->SetGraphicsRootDescriptorTable(RootSignature::ESI_Depth, si_depth);

	cmdList->SetGraphicsRootDescriptorTable(RootSignature::ESI_RandomVector, mhRandomVectorMapGpuSrv);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	aoCeffMap->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void SSAOClass::GetOffsetVectors(DirectX::XMFLOAT4 offsets[14]) {
	std::copy(&mOffsets[0], &mOffsets[14], &offsets[0]);
}

void SSAOClass::AllocateDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
		UINT descSize, UINT rtvDescSize) {
	mhRandomVectorMapCpuSrv = hCpu.Offset(1, descSize);
	mhRandomVectorMapGpuSrv = hGpu.Offset(1, descSize);

	for (UINT i = 0; i < 2; ++i) {
		mhAOCoefficientMapCpuSrvs[i] = hCpu.Offset(1, descSize);
		mhAOCoefficientMapGpuSrvs[i] = hGpu.Offset(1, descSize);
		mhAOCoefficientMapCpuRtvs[i] = hCpuRtv.Offset(1, rtvDescSize);
	}
}

BOOL SSAOClass::BuildDescriptors() {
	D3D12Util::Descriptor::View::Builder builder;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = RandomVectorMapFormat;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	builder.Enqueue(mRandomVectorMap->Resource(), srvDesc, mhRandomVectorMapCpuSrv);

	srvDesc.Format = AOCoefficientMapFormat;
	for (UINT i = 0; i < 2; ++i) {
		builder.Enqueue(mAOCoefficientMaps[i]->Resource(), srvDesc, mhAOCoefficientMapCpuSrvs[i]);
	}

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = AOCoefficientMapFormat;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;
	for (UINT i = 0; i < 2; ++i) {
		builder.Enqueue(mAOCoefficientMaps[i]->Resource(), rtvDesc, mhAOCoefficientMapCpuRtvs[i]);
	}

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		builder.Build(device);
	}

	return TRUE;
}

BOOL SSAOClass::OnResize(UINT width, UINT height) {
	CheckReturn(BuildResources(width, height));
	BuildDescriptors();

	return TRUE;
}

BOOL SSAOClass::BuildResources(UINT width, UINT height) {
	const CD3DX12_CLEAR_VALUE optClear(AOCoefficientMapFormat, AOCoefficientMapClearValues);

	D3D12Util::Descriptor::Resource::Builder builder;
	{
		UINT actWidth;
		UINT actHeight;
		if (mResolutionType == Resolution::E_Fullscreen) {
			actWidth = width;
			actHeight = height;
		}
		else {
			actWidth = static_cast<UINT>(width * 0.5f);
			actHeight = static_cast<UINT>(height * 0.5f);
		}

		mViewport = { 0.f, 0.f, static_cast<FLOAT>(actWidth), static_cast<FLOAT>(actHeight), 0.f, 1.f };
		mScissorRect = { 0, 0, static_cast<INT>(actWidth), static_cast<INT>(actHeight) };

		D3D12_RESOURCE_DESC texDesc;
		ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Alignment = 0;
		texDesc.Width = actWidth;
		texDesc.Height = actHeight;
		texDesc.DepthOrArraySize = 1;
		texDesc.MipLevels = 1;
		texDesc.Format = AOCoefficientMapFormat;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		for (UINT i = 0; i < 2; ++i) {
			std::wstringstream wsstream;
			wsstream << L"SSAO_AOCoefficientMap_" << i;

			builder.Enqueue(
				mAOCoefficientMaps[i].get(),
				CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				texDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				&optClear,
				wsstream.str().c_str()
			);
		}
	}
	{
		D3D12_RESOURCE_DESC texDesc;
		ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Alignment = 0;
		texDesc.Width = 256;
		texDesc.Height = 256;
		texDesc.DepthOrArraySize = 1;
		texDesc.MipLevels = 1;
		texDesc.Format = RandomVectorMapFormat;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		builder.Enqueue(
			mRandomVectorMap.get(),
			CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"SSAO_RandomVectorMap"
		);
	}

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		CheckReturn(builder.Build(device));
	}

	return TRUE;
}

void SSAOClass::BuildOffsetVectors() {
	// Start with 14 uniformly distributed vectors.  We choose the 8 corners of the cube
	// and the 6 center points along each cube face.  We always alternate the points on 
	// opposites sides of the cubes.  This way we still get the vectors spread out even
	// if we choose to use less than 14 samples.

	// 8 cube corners
	mOffsets[0] = XMFLOAT4(+1.f, +1.f, +1.f, 0.f);
	mOffsets[1] = XMFLOAT4(-1.f, -1.f, -1.f, 0.f);

	mOffsets[2] = XMFLOAT4(-1.f, +1.f, +1.f, 0.f);
	mOffsets[3] = XMFLOAT4(+1.f, -1.f, -1.f, 0.f);

	mOffsets[4] = XMFLOAT4(+1.f, +1.f, -1.f, 0.f);
	mOffsets[5] = XMFLOAT4(-1.f, -1.f, +1.f, 0.f);

	mOffsets[6] = XMFLOAT4(-1.f, +1.f, -1.f, 0.f);
	mOffsets[7] = XMFLOAT4(+1.f, -1.f, +1.f, 0.f);

	// 6 centers of cube faces
	mOffsets[8] = XMFLOAT4(-1.f,  0.f,  0.f, 0.f);
	mOffsets[9] = XMFLOAT4(+1.f,  0.f,  0.f, 0.f);

	mOffsets[10] = XMFLOAT4(0.f, -1.f,  0.f, 0.f);
	mOffsets[11] = XMFLOAT4(0.f, +1.f,  0.f, 0.f);

	mOffsets[12] = XMFLOAT4(0.f,  0.f, -1.f, 0.f);
	mOffsets[13] = XMFLOAT4(0.f,  0.f, +1.f, 0.f);

	for (UINT i = 0; i < 14; ++i) {
		// Create random lengths in [0.25, 1.0].
		const FLOAT s = MathHelper::RandF(0.25f, 1.f);
		const XMVECTOR v = s * XMVector4Normalize(XMLoadFloat4(&mOffsets[i]));

		XMStoreFloat4(&mOffsets[i], v);
	}
}

BOOL SSAOClass::BuildRandomVectorTexture(ID3D12GraphicsCommandList* const cmdList) {
	//
	// In order to copy CPU memory data into our default buffer,
	//  we need to create an intermediate upload heap. 
	//
	const UINT num2DSubresources = 1;
	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(mRandomVectorMap->Resource(), 0, num2DSubresources);

	{
		ID3D12Device5* device;
		auto lock = md3dDevice->TakeOut(device);

		CheckReturn(mRandomVectorMapUploadBuffer->Initialize(
			device,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
			D3D12_RESOURCE_STATE_COMMON,
			nullptr
		));
	}

	std::vector<XMCOLOR> initData(256 * 256);
	for (INT i = 0; i < 256; ++i) {
		for (INT j = 0; j < 256; ++j) {
			// Random vector in [0,1].  We will decompress in shader to [-1,1].
			XMFLOAT3 v(MathHelper::RandF(), MathHelper::RandF(), MathHelper::RandF());

			initData[static_cast<INT64>(i) * 256 + static_cast<INT64>(j)] = XMCOLOR(v.x, v.y, v.z, 0.f);
		}
	}

	D3D12_SUBRESOURCE_DATA subResourceData = {};
	subResourceData.pData = initData.data();
	subResourceData.RowPitch = 256 * sizeof(XMCOLOR);
	subResourceData.SlicePitch = subResourceData.RowPitch * 256;

	//
	// Schedule to copy the data to the default resource, and change states.
	// Note that mCurrSol is put in the GENERIC_READ state so it can be 
	// read by a shader.
	//
	mRandomVectorMapUploadBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_SOURCE);
	mRandomVectorMap->Transite(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);

	UpdateSubresources(
		cmdList,
		mRandomVectorMap->Resource(),
		mRandomVectorMapUploadBuffer->Resource(),
		0,
		0,
		num2DSubresources,
		&subResourceData
	);
	mRandomVectorMap->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	return TRUE;
}