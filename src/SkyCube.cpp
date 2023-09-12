#include "SkyCube.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "D3D12Util.h"
#include "RenderItem.h"
#include "DxMesh.h"
#include "DDSTextureLoader.h"
#include "ResourceUploadBatch.h"
#include "GpuResource.h"

using namespace DirectX;
using namespace SkyCube;

namespace {
	const std::string SkyVS = "SkyVS";
	const std::string SkyPS = "SkyPS";
}

bool SkyCubeClass::Initialize(
		ID3D12Device* device, ID3D12GraphicsCommandList*const cmdList,
		ShaderManager*const manager, UINT width, UINT height, DXGI_FORMAT hdrMapFormat) {
	md3dDevice = device;
	mShaderManager = manager;

	mWidth = width;
	mHeight = height;

	mHDRMapFormat = hdrMapFormat;

	CheckReturn(BuildResources(cmdList));

	return true;
}

bool SkyCubeClass::CompileShaders(const std::wstring& filePath) {
	const std::wstring actualPath = filePath + L"Sky.hlsl";
	auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
	auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
	CheckReturn(mShaderManager->CompileShader(vsInfo, "SkyVS"));
	CheckReturn(mShaderManager->CompileShader(psInfo, "SkyPS"));

	return true;
}

bool SkyCubeClass::BuildRootSignature(const StaticSamplers& samplers) {
	CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignatureLayout::Count];

	CD3DX12_DESCRIPTOR_RANGE texTables[1];
	texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

	slotRootParameter[RootSignatureLayout::ECB_Pass].InitAsConstantBufferView(0);
	slotRootParameter[RootSignatureLayout::ECB_Obj].InitAsConstantBufferView(1);
	slotRootParameter[RootSignatureLayout::ESI_Cube].InitAsDescriptorTable(1, &texTables[0]);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignature));

	return true;
}

bool SkyCubeClass::BuildPso(D3D12_INPUT_LAYOUT_DESC inputLayout, DXGI_FORMAT dsvFormat) {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skyPsoDesc = D3D12Util::DefaultPsoDesc(inputLayout, dsvFormat);
	skyPsoDesc.pRootSignature = mRootSignature.Get();
	{
		auto vs = mShaderManager->GetDxcShader(SkyVS);
		auto ps = mShaderManager->GetDxcShader(SkyPS);
		skyPsoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
		skyPsoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	skyPsoDesc.NumRenderTargets = 1;
	skyPsoDesc.RTVFormats[0] = mHDRMapFormat;
	skyPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
	skyPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	skyPsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	skyPsoDesc.DepthStencilState.StencilEnable = FALSE;

	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&skyPsoDesc, IID_PPV_ARGS(&mPSO)));

	return true;
}

void SkyCubeClass::Run(
		ID3D12GraphicsCommandList*const cmdList,
		D3D12_VIEWPORT viewport,
		D3D12_RECT scissorRect,
		GpuResource* backBuffer,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
		D3D12_CPU_DESCRIPTOR_HANDLE dio_dsv,
		D3D12_GPU_VIRTUAL_ADDRESS cbPassAddress,
		D3D12_GPU_VIRTUAL_ADDRESS cbObjAddress,
		UINT objCBByteSize,
		const std::vector<RenderItem*>& ritems) {
	cmdList->SetPipelineState(mPSO.Get());
	cmdList->SetGraphicsRootSignature(mRootSignature.Get());

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	
	cmdList->OMSetRenderTargets(1, &ro_backBuffer, true, &dio_dsv);

	cmdList->SetGraphicsRootConstantBufferView(RootSignatureLayout::ECB_Pass, cbPassAddress);
	cmdList->SetGraphicsRootConstantBufferView(RootSignatureLayout::ECB_Obj, cbObjAddress);
	cmdList->SetGraphicsRootDescriptorTable(RootSignatureLayout::ESI_Cube, mhGpuSrv);

	DrawRenderItems(cmdList, ritems, cbObjAddress, objCBByteSize);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PRESENT);
}

bool SkyCubeClass::SetCubeMap(ID3D12CommandQueue*const queue, const std::string& file) {
	auto tex = std::make_unique<Texture>();

	std::wstring filename;
	filename.assign(file.begin(), file.end());

	auto index = filename.rfind(L'.');
	filename = filename.replace(filename.begin() + index, filename.end(), L".dds");

	ResourceUploadBatch resourceUpload(md3dDevice);

	resourceUpload.Begin();

	HRESULT status = DirectX::CreateDDSTextureFromFile(
		md3dDevice,
		resourceUpload,
		filename.c_str(),
		tex->Resource.ReleaseAndGetAddressOf()
	);

	auto finished = resourceUpload.End(queue);
	finished.wait();

	if (FAILED(status)) {
		std::wstringstream wsstream;
		wsstream << "Returned " << std::hex << status << "; when creating texture:  " << filename;
		WLogln(wsstream.str());
		return false;
	}

	auto& resource = tex->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	srvDesc.TextureCube.MipLevels = resource->GetDesc().MipLevels;
	srvDesc.Format = resource->GetDesc().Format;

	resource.Swap(mCubeMap);
	resource.Reset();

	md3dDevice->CreateShaderResourceView(mCubeMap.Get(), &srvDesc, mhCpuSrv);
	
	return true;
}

void SkyCubeClass::BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
		UINT descSize) {
	mhCpuSrv = hCpu;
	mhGpuSrv = hGpu;

	hCpu.Offset(1, descSize);
	hGpu.Offset(1, descSize);

	BuildDescriptors();
}

void SkyCubeClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = mHDRMapFormat;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.PlaneSlice = 0;
	md3dDevice->CreateShaderResourceView(mCubeMap.Get(), &srvDesc, mhCpuSrv);
}

bool SkyCubeClass::BuildResources(ID3D12GraphicsCommandList*const cmdList) {
	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = 1;
	texDesc.Height = 1;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = mHDRMapFormat;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	
	CheckHRESULT(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&mCubeMap))
	);
	mCubeMap->SetName(L"CubeMap");

	{
		const UINT num2DSubresources = texDesc.DepthOrArraySize * texDesc.MipLevels;
		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(mCubeMap.Get(), 0, num2DSubresources);

		CheckHRESULT(md3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
			D3D12_RESOURCE_STATE_COPY_SOURCE,
			nullptr,
			IID_PPV_ARGS(&mCubeMapUploadBuffer)
		));

		const UINT size = 4;
		std::vector<BYTE> data(size);

		for (UINT i = 0; i < size; ++i) {
			data[i] = 0;	// rgba-channels = 0 = black
		}

		D3D12_SUBRESOURCE_DATA subResourceData = {};
		subResourceData.pData = data.data();
		subResourceData.RowPitch = size;
		subResourceData.SlicePitch = subResourceData.RowPitch * 1;

		UpdateSubresources(
			cmdList,
			mCubeMap.Get(),
			mCubeMapUploadBuffer.Get(),
			0,
			0,
			num2DSubresources,
			&subResourceData
		);
		cmdList->ResourceBarrier(
			1,
			&CD3DX12_RESOURCE_BARRIER::Transition(
				mCubeMap.Get(),
				D3D12_RESOURCE_STATE_COPY_DEST,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			)
		);
	}

	return true;
}

void SkyCubeClass::DrawRenderItems(
		ID3D12GraphicsCommandList*const cmdList, 
		const std::vector<RenderItem*>& ritems,
		D3D12_GPU_VIRTUAL_ADDRESS cbObjAddress,
		UINT objCBByteSize) {
	for (size_t i = 0; i < ritems.size(); ++i) {
		auto& ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geometry->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geometry->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		D3D12_GPU_VIRTUAL_ADDRESS currRitemObjCBAddress = cbObjAddress + ri->ObjCBIndex * objCBByteSize;
		cmdList->SetGraphicsRootConstantBufferView(RootSignatureLayout::ECB_Obj, currRitemObjCBAddress);
		
		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}