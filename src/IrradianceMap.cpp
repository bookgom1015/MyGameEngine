#include "IrradianceMap.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "D3D12Util.h"
#include "RenderItem.h"
#include "DxMesh.h"
#include "DDSTextureLoader.h"
#include "ResourceUploadBatch.h"
#include "GpuResource.h"

using namespace DirectX;
using namespace IrradianceMap;

namespace {
	const std::string ConvRectToCubeVS = "ConvRectToCubeVS";
	const std::string ConvRectToCubePS = "ConvRectToCubePS";

	const std::string DrawConvertedCubeVS = "DrawConvertedCubeVS";
	const std::string DrawConvertedCubePS = "DrawConvertedCubePS";

	const std::string DrawEquirectangularCubeVS = "DrawEquirectangularCubeVS";
	const std::string DrawEquirectangularCubePS = "DrawEquirectangularCubePS";
}

namespace {
	const UINT CubeMapSize = 1024;
}

IrradianceMapClass::IrradianceMapClass() {
	mEquirectangularMap = std::make_unique<GpuResource>();
	mConvertedCubeMap = std::make_unique<GpuResource>();

	mhConvertedCubeMapCpuRtvs.resize(CubeMapFace::Count);

	mViewport = { 0.0f, 0.0f, static_cast<float>(CubeMapSize), static_cast<float>(CubeMapSize), 0.0f, 1.0f };
	mScissorRect = { 0, 0, static_cast<int>(CubeMapSize), static_cast<int>(CubeMapSize) };

	bNeedToUpdate = false;

	PipelineStateType = PipelineState::E_DrawConvertedCube;
}

bool IrradianceMapClass::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* const cmdList, ShaderManager* const manager) {
	md3dDevice = device;
	mShaderManager = manager;

	BuildResources(cmdList);

	return true;
}

bool IrradianceMapClass::CompileShaders(const std::wstring& filePath) {
	{
		const std::wstring actualPath = filePath + L"ConvertEquirectangularToCubeMap.hlsl";
		auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
		auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
		CheckReturn(mShaderManager->CompileShader(vsInfo, ConvRectToCubeVS));
		CheckReturn(mShaderManager->CompileShader(psInfo, ConvRectToCubePS));
	}
	{
		const std::wstring actualPath = filePath + L"DrawIrradianceCubeMap.hlsl";
		{
			DxcDefine defines[] = {
			{ L"SPHERICAL", L"1" }
			};

			auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3", defines, _countof(defines));
			auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3", defines, _countof(defines));
			CheckReturn(mShaderManager->CompileShader(vsInfo, DrawEquirectangularCubeVS));
			CheckReturn(mShaderManager->CompileShader(psInfo, DrawEquirectangularCubePS));
		}
		{
			auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
			auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
			CheckReturn(mShaderManager->CompileShader(vsInfo, DrawConvertedCubeVS));
			CheckReturn(mShaderManager->CompileShader(psInfo, DrawConvertedCubePS));
		}
	}

	return true;
}

bool IrradianceMapClass::BuildRootSignature(const StaticSamplers& samplers) {
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[ConvEquirectToCube::RootSignatureLayout::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[1];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

		slotRootParameter[ConvEquirectToCube::RootSignatureLayout::ECB_ConvEquirectToCube].InitAsConstantBufferView(0);
		slotRootParameter[ConvEquirectToCube::RootSignatureLayout::EC_Consts].InitAsConstants(
			ConvEquirectToCube::RootConstantsLayout::Count,1);
		slotRootParameter[ConvEquirectToCube::RootSignatureLayout::ESI_Equirectangular].InitAsDescriptorTable(1, &texTables[0]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures[RootSignatureLayout::E_ConvEquirectToConv]));
	}
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[DrawCube::RootSignatureLayout::Count];

		CD3DX12_DESCRIPTOR_RANGE texTables[2];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
		texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);

		slotRootParameter[DrawCube::RootSignatureLayout::ECB_Pass].InitAsConstantBufferView(0);
		slotRootParameter[DrawCube::RootSignatureLayout::ECB_Obj].InitAsConstantBufferView(1);
		slotRootParameter[DrawCube::RootSignatureLayout::ESI_Cube].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[DrawCube::RootSignatureLayout::ESI_Equirectangular].InitAsDescriptorTable(1, &texTables[1]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);

		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignatures[RootSignatureLayout::E_DrawCube]));
	}

	return true;
}

bool IrradianceMapClass::BuildPso(D3D12_INPUT_LAYOUT_DESC inputLayout) {
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::DefaultPsoDesc(inputLayout, DXGI_FORMAT_UNKNOWN);
		psoDesc.pRootSignature = mRootSignatures[RootSignatureLayout::E_ConvEquirectToConv].Get();
		{
			auto vs = mShaderManager->GetDxcShader(ConvRectToCubeVS);
			auto ps = mShaderManager->GetDxcShader(ConvRectToCubePS);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
		psoDesc.NumRenderTargets = 1;
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
		psoDesc.RTVFormats[0] = D3D12Util::HDRMapFormat;
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::E_ConvEquirectToConv])));
	}
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = D3D12Util::DefaultPsoDesc(inputLayout, DXGI_FORMAT_UNKNOWN);
		psoDesc.pRootSignature = mRootSignatures[RootSignatureLayout::E_DrawCube].Get();
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = D3D12Util::SDRMapFormat;
		psoDesc.DepthStencilState.DepthEnable = FALSE;

		{
			auto vs = mShaderManager->GetDxcShader(DrawConvertedCubeVS);
			auto ps = mShaderManager->GetDxcShader(DrawConvertedCubePS);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
		CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::E_DrawConvertedCube])));

		{
			auto vs = mShaderManager->GetDxcShader(DrawEquirectangularCubeVS);
			auto ps = mShaderManager->GetDxcShader(DrawEquirectangularCubePS);
			psoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
		}
		CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSOs[PipelineState::E_DrawEquirectangularCube])));
	}

	return true;
}

void IrradianceMapClass::BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuRtv,
		UINT descSize, UINT rtvDescSize) {
	mhEquirectangularMapCpuSrv = hCpuSrv;
	mhEquirectangularMapGpuSrv = hGpuSrv;

	mhConvertedCubeMapCpuSrv = hCpuSrv.Offset(1, descSize);
	mhConvertedCubeMapGpuSrv = hGpuSrv.Offset(1, descSize);

	for (int i = 0; i < CubeMapFace::Count; ++i) {
		mhConvertedCubeMapCpuRtvs[i] = hCpuRtv;
		hCpuRtv.Offset(1, rtvDescSize);
	}

	BuildDescriptors();

	hCpuSrv.Offset(1, descSize);
	hGpuSrv.Offset(1, descSize);
}


bool IrradianceMapClass::SetEquirectangularMap(ID3D12CommandQueue* const queue,  const std::string& file) {
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

	bNeedToUpdate = true;

	return true;
}

bool IrradianceMapClass::Update(
		ID3D12DescriptorHeap* descHeap,
		ID3D12GraphicsCommandList* const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cbConvEquirectToCube,
		RenderItem* box) {
	if (!bNeedToUpdate) return true;

	Convert(descHeap, cmdList, cbConvEquirectToCube, box);

	return true;
}

bool IrradianceMapClass::DrawCubeMap(
		ID3D12GraphicsCommandList* const cmdList,
		D3D12_VIEWPORT viewport,
		D3D12_RECT scissorRect,
		GpuResource* backBuffer,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
		D3D12_GPU_VIRTUAL_ADDRESS cbPassAddress,
		D3D12_GPU_VIRTUAL_ADDRESS cbObjAddress,
		UINT objCBByteSize,
		RenderItem* box) {
	cmdList->SetPipelineState(mPSOs[PipelineStateType].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[RootSignatureLayout::E_DrawCube].Get());

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->OMSetRenderTargets(1, &ro_backBuffer, true, nullptr);

	cmdList->SetGraphicsRootConstantBufferView(DrawCube::RootSignatureLayout::ECB_Pass, cbPassAddress);

	if (PipelineStateType == PipelineState::E_DrawConvertedCube)
		cmdList->SetGraphicsRootDescriptorTable(DrawCube::RootSignatureLayout::ESI_Cube, mhConvertedCubeMapGpuSrv);
	else
		cmdList->SetGraphicsRootDescriptorTable(DrawCube::RootSignatureLayout::ESI_Equirectangular, mhEquirectangularMapGpuSrv);

	D3D12_GPU_VIRTUAL_ADDRESS boxObjCBAddress = cbObjAddress + box->ObjCBIndex * objCBByteSize;
	cmdList->SetGraphicsRootConstantBufferView(DrawCube::RootSignatureLayout::ECB_Obj, boxObjCBAddress);

	cmdList->IASetVertexBuffers(0, 1, &box->Geometry->VertexBufferView());
	cmdList->IASetIndexBuffer(&box->Geometry->IndexBufferView());
	cmdList->IASetPrimitiveTopology(box->PrimitiveType);

	cmdList->DrawIndexedInstanced(box->IndexCount, 1, box->StartIndexLocation, box->BaseVertexLocation, 0);

	backBuffer->Transite(cmdList, D3D12_RESOURCE_STATE_PRESENT);

	return true;
}


void IrradianceMapClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	srvDesc.TextureCube.MipLevels = 1;
	srvDesc.Format = D3D12Util::HDRMapFormat;
	md3dDevice->CreateShaderResourceView(mConvertedCubeMap->Resource(), &srvDesc, mhConvertedCubeMapCpuSrv);

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
	rtvDesc.Texture2DArray.MipSlice = 0;
	rtvDesc.Texture2DArray.PlaneSlice = 0;
	rtvDesc.Texture2DArray.ArraySize = 1;
	rtvDesc.Format = D3D12Util::HDRMapFormat;
	
	for (int i = 0; i < CubeMapFace::Count; ++i) {
		rtvDesc.Texture2DArray.FirstArraySlice = i;
		md3dDevice->CreateRenderTargetView(mConvertedCubeMap->Resource(), &rtvDesc, mhConvertedCubeMapCpuRtvs[i]);
	}
}

bool IrradianceMapClass::BuildResources(ID3D12GraphicsCommandList* const cmdList) {
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
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	CheckReturn(mConvertedCubeMap->Initialize(
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

void IrradianceMapClass::Convert(
		ID3D12DescriptorHeap* descHeap,
		ID3D12GraphicsCommandList* const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cbConvEquirectToCube,
		RenderItem* box) {
	ID3D12DescriptorHeap* descriptorHeaps[] = { descHeap };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	cmdList->SetPipelineState(mPSOs[PipelineState::E_ConvEquirectToConv].Get());
	cmdList->SetGraphicsRootSignature(mRootSignatures[RootSignatureLayout::E_ConvEquirectToConv].Get());

	cmdList->RSSetViewports(1, &mViewport);
	cmdList->RSSetScissorRects(1, &mScissorRect);

	mConvertedCubeMap->Transite(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->SetGraphicsRootConstantBufferView(ConvEquirectToCube::RootSignatureLayout::ECB_ConvEquirectToCube, cbConvEquirectToCube);
	cmdList->SetGraphicsRootDescriptorTable(ConvEquirectToCube::RootSignatureLayout::ESI_Equirectangular, mhEquirectangularMapGpuSrv);

	for (UINT i = 0; i < CubeMapFace::Count; ++i) {
		cmdList->OMSetRenderTargets(1, &mhConvertedCubeMapCpuRtvs[i], true, nullptr);

		cmdList->SetGraphicsRoot32BitConstant(ConvEquirectToCube::RootSignatureLayout::EC_Consts, i, 0);

		cmdList->IASetVertexBuffers(0, 1, &box->Geometry->VertexBufferView());
		cmdList->IASetIndexBuffer(&box->Geometry->IndexBufferView());
		cmdList->IASetPrimitiveTopology(box->PrimitiveType);

		cmdList->DrawIndexedInstanced(box->IndexCount, 1, box->StartIndexLocation, box->BaseVertexLocation, 0);
	}

	mConvertedCubeMap->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	bNeedToUpdate = false;
}