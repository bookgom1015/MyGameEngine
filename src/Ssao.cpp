#include "Ssao.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "FrameResource.h"
#include "HlslCompaction.h"

#include <DirectXColors.h>

using namespace DirectX;
using namespace DirectX::PackedVector;

using namespace Ssao;

SsaoClass::SsaoClass() {
	mRandomVectorMap = std::make_unique<GpuResource>();
	mRandomVectorMapUploadBuffer = std::make_unique<GpuResource>();
	mAOCoefficientMaps[0] = std::make_unique<GpuResource>();
	mAOCoefficientMaps[1] = std::make_unique<GpuResource>();
}

bool SsaoClass::Initialize(
		ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, ShaderManager*const manager,
		UINT width, UINT height, UINT divider) {
	md3dDevice = device;
	mShaderManager = manager;

	mWidth = width / divider;
	mHeight = height / divider;

	mDivider = divider;

	mViewport = { 0.0f, 0.0f, static_cast<float>(mWidth), static_cast<float>(mHeight), 0.0f, 1.0f };
	mScissorRect = { 0, 0, static_cast<int>(mWidth), static_cast<int>(mHeight) };

	CheckReturn(BuildResources());
	BuildOffsetVectors();
	CheckReturn(BuildRandomVectorTexture(cmdList));

	return true;
}

bool SsaoClass::CompileShaders(const std::wstring& filePath) {
	const std::wstring actualPath = filePath + L"Ssao.hlsl";
	auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
	auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
	CheckReturn(mShaderManager->CompileShader(vsInfo, "SsaoVS"));
	CheckReturn(mShaderManager->CompileShader(psInfo, "SsaoPS"));

	return true;
}

bool SsaoClass::BuildRootSignature(const StaticSamplers& samplers) {
	CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::Count];

	CD3DX12_DESCRIPTOR_RANGE texTables[3];
	texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
	texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
	texTables[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);

	slotRootParameter[RootSignature::ECB_Pass].InitAsConstantBufferView(0);
	slotRootParameter[RootSignature::ESI_Normal].InitAsDescriptorTable(1, &texTables[0]);
	slotRootParameter[RootSignature::ESI_Depth].InitAsDescriptorTable(1, &texTables[1]);
	slotRootParameter[RootSignature::ESI_RandomVector].InitAsDescriptorTable(1, &texTables[2]);

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignature));

	return true;
}

bool SsaoClass::BuildPso() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::QuadPsoDesc();
	psoDesc.pRootSignature = mRootSignature.Get();
	{
		auto vs = mShaderManager->GetDxcShader("SsaoVS");
		auto ps = mShaderManager->GetDxcShader("SsaoPS");
		psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
		psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	psoDesc.RTVFormats[0] = AOCoefficientMapFormat;

	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));

	return true;
}

void SsaoClass::Run(
		ID3D12GraphicsCommandList*const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS passCBAddress,
		D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
		D3D12_GPU_DESCRIPTOR_HANDLE si_depth) {
	cmdList->SetPipelineState(mPSO.Get());
	cmdList->SetGraphicsRootSignature(mRootSignature.Get());

	cmdList->RSSetViewports(1, &mViewport);
	cmdList->RSSetScissorRects(1, &mScissorRect);

	const auto aoCeffMap = mAOCoefficientMaps[0].get();
	aoCeffMap->Transite(cmdList,D3D12_RESOURCE_STATE_RENDER_TARGET);

	auto aoCeffMap0Rtv = mhAOCoefficientMapCpuRtvs[0];
	cmdList->ClearRenderTargetView(aoCeffMap0Rtv, AOCoefficientMapClearValues, 0, nullptr);
	cmdList->OMSetRenderTargets(1, &aoCeffMap0Rtv, true, nullptr);

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


void SsaoClass::GetOffsetVectors(DirectX::XMFLOAT4 offsets[14]) {
	std::copy(&mOffsets[0], &mOffsets[14], &offsets[0]);
}

void SsaoClass::BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
		UINT descSize, UINT rtvDescSize) {
	mhRandomVectorMapCpuSrv = hCpu;
	mhRandomVectorMapGpuSrv = hGpu;

	mhAOCoefficientMapCpuSrvs[0] = hCpu.Offset(1, descSize);
	mhAOCoefficientMapGpuSrvs[0] = hGpu.Offset(1, descSize);
	mhAOCoefficientMapCpuRtvs[0] = hCpuRtv;

	mhAOCoefficientMapCpuSrvs[1] = hCpu.Offset(1, descSize);
	mhAOCoefficientMapGpuSrvs[1] = hGpu.Offset(1, descSize);
	mhAOCoefficientMapCpuRtvs[1] = hCpuRtv.Offset(1, rtvDescSize);

	hCpu.Offset(1, descSize);
	hGpu.Offset(1, descSize);
	hCpuRtv.Offset(1, rtvDescSize);

	BuildDescriptors();
}

bool SsaoClass::OnResize(UINT width, UINT height) {
	width /= mDivider;
	height /= mDivider;
	if ((mWidth != width) || (mHeight != height)) {
		mWidth = width;
		mHeight = height;

		mViewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
		mScissorRect = { 0, 0, static_cast<int>(width), static_cast<int>(height) };

		CheckReturn(BuildResources());
		BuildDescriptors();
	}

	return true;
}

void SsaoClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = RandomVectorMapFormat;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	md3dDevice->CreateShaderResourceView(mRandomVectorMap->Resource(), &srvDesc, mhRandomVectorMapCpuSrv);

	srvDesc.Format = AOCoefficientMapFormat;
	for (int i = 0; i < 2; ++i) {
		md3dDevice->CreateShaderResourceView(mAOCoefficientMaps[i]->Resource(), &srvDesc, mhAOCoefficientMapCpuSrvs[i]);
	}

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = AOCoefficientMapFormat;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;
	for (int i = 0; i < 2; ++i) {
		md3dDevice->CreateRenderTargetView(mAOCoefficientMaps[i]->Resource(), &rtvDesc, mhAOCoefficientMapCpuRtvs[i]);
	}
}

bool SsaoClass::BuildResources() {
	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	// Ambient occlusion maps are at half resolution.
	texDesc.Width = mWidth;
	texDesc.Height = mHeight;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = AOCoefficientMapFormat;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	CD3DX12_CLEAR_VALUE optClear(AOCoefficientMapFormat, AOCoefficientMapClearValues);

	for (int i = 0; i < 2; ++i) {
		std::wstringstream wsstream;
		wsstream << L"AOCoefficientMap_" << i;
		CheckHRESULT(mAOCoefficientMaps[i]->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&optClear,
			wsstream.str().c_str()
		));
	}

	return true;
}

void SsaoClass::BuildOffsetVectors() {
	// Start with 14 uniformly distributed vectors.  We choose the 8 corners of the cube
	// and the 6 center points along each cube face.  We always alternate the points on 
	// opposites sides of the cubes.  This way we still get the vectors spread out even
	// if we choose to use less than 14 samples.

	// 8 cube corners
	mOffsets[0] = XMFLOAT4(+1.0f, +1.0f, +1.0f, 0.0f);
	mOffsets[1] = XMFLOAT4(-1.0f, -1.0f, -1.0f, 0.0f);

	mOffsets[2] = XMFLOAT4(-1.0f, +1.0f, +1.0f, 0.0f);
	mOffsets[3] = XMFLOAT4(+1.0f, -1.0f, -1.0f, 0.0f);

	mOffsets[4] = XMFLOAT4(+1.0f, +1.0f, -1.0f, 0.0f);
	mOffsets[5] = XMFLOAT4(-1.0f, -1.0f, +1.0f, 0.0f);

	mOffsets[6] = XMFLOAT4(-1.0f, +1.0f, -1.0f, 0.0f);
	mOffsets[7] = XMFLOAT4(+1.0f, -1.0f, +1.0f, 0.0f);

	// 6 centers of cube faces
	mOffsets[8] = XMFLOAT4(-1.0f, 0.0f, 0.0f, 0.0f);
	mOffsets[9] = XMFLOAT4(+1.0f, 0.0f, 0.0f, 0.0f);

	mOffsets[10] = XMFLOAT4(0.0f, -1.0f, 0.0f, 0.0f);
	mOffsets[11] = XMFLOAT4(0.0f, +1.0f, 0.0f, 0.0f);

	mOffsets[12] = XMFLOAT4(0.0f, 0.0f, -1.0f, 0.0f);
	mOffsets[13] = XMFLOAT4(0.0f, 0.0f, +1.0f, 0.0f);

	for (int i = 0; i < 14; ++i) {
		// Create random lengths in [0.25, 1.0].
		float s = MathHelper::RandF(0.25f, 1.0f);

		XMVECTOR v = s * XMVector4Normalize(XMLoadFloat4(&mOffsets[i]));

		XMStoreFloat4(&mOffsets[i], v);
	}
}

bool SsaoClass::BuildRandomVectorTexture(ID3D12GraphicsCommandList* cmdList) {
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

	CheckReturn(mRandomVectorMap->Initialize(
		md3dDevice,
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		L"RandomVectorMap"
	));

	//
	// In order to copy CPU memory data into our default buffer,
	//  we need to create an intermediate upload heap. 
	//

	const UINT num2DSubresources = texDesc.DepthOrArraySize * texDesc.MipLevels;
	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(mRandomVectorMap->Resource(), 0, num2DSubresources);

	CheckReturn(mRandomVectorMapUploadBuffer->Initialize(
		md3dDevice,
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
		D3D12_RESOURCE_STATE_COPY_SOURCE,
		nullptr
	));

	XMCOLOR initData[256 * 256];
	for (int i = 0; i < 256; ++i) {
		for (int j = 0; j < 256; ++j) {
			// Random vector in [0,1].  We will decompress in shader to [-1,1].
			XMFLOAT3 v(MathHelper::RandF(), MathHelper::RandF(), MathHelper::RandF());

			initData[i * 256 + j] = XMCOLOR(v.x, v.y, v.z, 0.0f);
		}
	}

	D3D12_SUBRESOURCE_DATA subResourceData = {};
	subResourceData.pData = initData;
	subResourceData.RowPitch = 256 * sizeof(XMCOLOR);
	subResourceData.SlicePitch = subResourceData.RowPitch * 256;

	//
	// Schedule to copy the data to the default resource, and change states.
	// Note that mCurrSol is put in the GENERIC_READ state so it can be 
	// read by a shader.
	//

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

	return true;
}