#include "CubeMapConverter.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "D3D12Util.h"
#include "RenderItem.h"
#include "DxMesh.h"
#include "DDSTextureLoader.h"
#include "ResourceUploadBatch.h"
#include "GpuResource.h"

using namespace DirectX;
using namespace CubeMapConverter;

namespace {
	const std::string ConvRectToCubeVS = "ConvRectToCubeVS";
	const std::string ConvRectToCubePS = "ConvRectToCubePS";
}

namespace {
	const UINT CubeMapSize = 1024;
}

CubeMapConverterClass::CubeMapConverterClass() {
	mEquirectangularMap = std::make_unique<GpuResource>();
	mCubeMap = std::make_unique<GpuResource>();
}

bool CubeMapConverterClass::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* const cmdList, ShaderManager* const manager) {
	md3dDevice = device;
	mShaderManager = manager;

	BuildResources(cmdList);

	return true;
}

bool CubeMapConverterClass::CompileShaders(const std::wstring& filePath) {
	const std::wstring actualPath = filePath + L"ConvertEquirectangularToCubeMap.hlsl";
	auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
	auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
	CheckReturn(mShaderManager->CompileShader(vsInfo, ConvRectToCubeVS));
	CheckReturn(mShaderManager->CompileShader(psInfo, ConvRectToCubePS));

	return true;
}

bool CubeMapConverterClass::BuildRootSignature(const StaticSamplers& samplers) {
	CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignatureLayout::Count];

	CD3DX12_DESCRIPTOR_RANGE texTables[1];
	texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

	slotRootParameter[RootSignatureLayout::ECB_Pass].InitAsConstantBufferView(0);
	slotRootParameter[RootSignatureLayout::ECB_Obj].InitAsConstantBufferView(1);
	slotRootParameter[RootSignatureLayout::ESI_Equirectangular].InitAsDescriptorTable(1, &texTables[0]);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignature));

	return true;
}

bool CubeMapConverterClass::BuildPso(D3D12_INPUT_LAYOUT_DESC inputLayout) {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skyPsoDesc = D3D12Util::DefaultPsoDesc(inputLayout, DXGI_FORMAT_UNKNOWN);
	skyPsoDesc.pRootSignature = mRootSignature.Get();
	{
		auto vs = mShaderManager->GetDxcShader(ConvRectToCubeVS);
		auto ps = mShaderManager->GetDxcShader(ConvRectToCubePS);
		skyPsoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
		skyPsoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	skyPsoDesc.NumRenderTargets = 1;
	skyPsoDesc.RTVFormats[0] = D3D12Util::SDRMapFormat;
	skyPsoDesc.DepthStencilState.DepthEnable = FALSE;

	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&skyPsoDesc, IID_PPV_ARGS(&mPSO)));

	return true;
}

void CubeMapConverterClass::Run(
		ID3D12GraphicsCommandList* const cmdList,
		D3D12_VIEWPORT viewport,
		D3D12_RECT scissorRect,
		GpuResource* backBuffer,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
		D3D12_GPU_VIRTUAL_ADDRESS cbPassAddress,
		D3D12_GPU_VIRTUAL_ADDRESS cbObjAddress,
		UINT objCBByteSize,
		const std::vector<RenderItem*>& ritems) {
	cmdList->SetPipelineState(mPSO.Get());
	cmdList->SetGraphicsRootSignature(mRootSignature.Get());

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->OMSetRenderTargets(1, &ro_backBuffer, true, nullptr);

	cmdList->SetGraphicsRootConstantBufferView(RootSignatureLayout::ECB_Pass, cbPassAddress);
	cmdList->SetGraphicsRootConstantBufferView(RootSignatureLayout::ECB_Obj, cbObjAddress);
	cmdList->SetGraphicsRootDescriptorTable(RootSignatureLayout::ESI_Equirectangular, mhEquirectangularMapGpuSrv);

	DrawRenderItems(cmdList, ritems, cbObjAddress, objCBByteSize);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PRESENT);
}


void CubeMapConverterClass::BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
		UINT descSize) {
	mhEquirectangularMapCpuSrv = hCpu;
	mhEquirectangularMapGpuSrv = hGpu;

	mhCubeMapCpuSrv = hCpu.Offset(1, descSize);
	mhCubeMapGpuSrv = hGpu.Offset(1, descSize);

	BuildDescriptors();

	hCpu.Offset(1, descSize);
	hGpu.Offset(1, descSize);
}


bool CubeMapConverterClass::SetEquirectangularMap(ID3D12CommandQueue* const queue, const std::string& file) {
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
	tex->Resource->SetName(L"EquirectangularMap");

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	srvDesc.TextureCube.MipLevels = resource->GetDesc().MipLevels;
	srvDesc.Format = resource->GetDesc().Format;

	mEquirectangularMap->Swap(resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	md3dDevice->CreateShaderResourceView(mEquirectangularMap->Resource(), &srvDesc, mhEquirectangularMapCpuSrv);

	return true;
}


void CubeMapConverterClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	srvDesc.TextureCube.MipLevels = 1;
	srvDesc.Format = D3D12Util::HDRMapFormat;
	md3dDevice->CreateShaderResourceView(mCubeMap->Resource(), &srvDesc, mhCubeMapCpuSrv);
}

bool CubeMapConverterClass::BuildResources(ID3D12GraphicsCommandList* const cmdList) {
	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = CubeMapSize;
	texDesc.Height = CubeMapSize;
	texDesc.DepthOrArraySize = 6;
	texDesc.MipLevels = 1;
	texDesc.Format = D3D12Util::HDRMapFormat;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	CheckReturn(mCubeMap->Initialize(
		md3dDevice,
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		L"ConvertedCubeMap"
	));

	return true;
}

void CubeMapConverterClass::DrawRenderItems(
		ID3D12GraphicsCommandList* const cmdList,
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