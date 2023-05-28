#include "Debug.h"
#include "Logger.h"
#include "ShaderManager.h"
#include "D3D12Util.h"

using namespace Debug;

bool DebugClass::Initialize(ID3D12Device* device, ShaderManager*const manager, UINT width, UINT height, DXGI_FORMAT backBufferFormat) {
	md3dDevice = device;
	mShaderManager = manager;

	mWidth = width;
	mHeight = height;

	mViewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
	mScissorRect = { 0, 0, static_cast<int>(width), static_cast<int>(height) };

	mBackBufferFormat = backBufferFormat;

	return true;
}

bool DebugClass::CompileShaders(const std::wstring& filePath) {
	const std::wstring actualPath = filePath + L"Debug.hlsl";
	auto vsInfo = D3D12ShaderInfo(actualPath.c_str(), L"VS", L"vs_6_3");
	auto psInfo = D3D12ShaderInfo(actualPath.c_str(), L"PS", L"ps_6_3");
	CheckReturn(mShaderManager->CompileShader(vsInfo, "DebugVS"));
	CheckReturn(mShaderManager->CompileShader(psInfo, "DebugPS"));

	return true;
}

bool DebugClass::BuildRootSignature(const StaticSamplers& samplers) {
	CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignatureLayout::Count];
	
	slotRootParameter[RootSignatureLayout::EC_Consts].InitAsConstants(RootConstantsLayout::Count, 0);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSigDesc, &mRootSignature));

	return true;
}

bool DebugClass::BuildPso() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC debugPsoDesc = D3D12Util::QuadPsoDesc();
	debugPsoDesc.pRootSignature = mRootSignature.Get();
	{
		auto vs = mShaderManager->GetDxcShader("DebugVS");
		auto ps = mShaderManager->GetDxcShader("DebugPS");
		debugPsoDesc.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
		debugPsoDesc.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
	}
	debugPsoDesc.RTVFormats[0] = mBackBufferFormat;
	CheckHRESULT(md3dDevice->CreateGraphicsPipelineState(&debugPsoDesc, IID_PPV_ARGS(&mPSO)));

	return true;
}

void DebugClass::Run(
		ID3D12GraphicsCommandList*const cmdList,
		D3D12_CPU_DESCRIPTOR_HANDLE ro_backBuffer,
		D3D12_CPU_DESCRIPTOR_HANDLE dio_dsv,
		DebugShaderParams::SampleMask::Type mask0,
		DebugShaderParams::SampleMask::Type mask1, 
		DebugShaderParams::SampleMask::Type mask2, 
		DebugShaderParams::SampleMask::Type mask3, 
		DebugShaderParams::SampleMask::Type mask4) {
	cmdList->SetPipelineState(mPSO.Get());
	cmdList->SetGraphicsRootSignature(mRootSignature.Get());
	
	cmdList->RSSetViewports(1, &mViewport);
	cmdList->RSSetScissorRects(1, &mScissorRect);
	
	cmdList->OMSetRenderTargets(1, &ro_backBuffer, true, &dio_dsv);

	UINT values[RootConstantsLayout::Count] = { 
		static_cast<UINT>(mask0), static_cast<UINT>(mask1), static_cast<UINT>(mask2), static_cast<UINT>(mask3), static_cast<UINT>(mask4) 
	};
	cmdList->SetGraphicsRoot32BitConstants(RootSignatureLayout::EC_Consts, _countof(values), values, RootConstantsLayout::ESampleMask0);

	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 10, 0, 0);
}

bool DebugClass::OnResize(UINT width, UINT height) {
	if ((mWidth != width) || (mHeight != height)) {
		mWidth = width;
		mHeight = height;

		mViewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
		mScissorRect = { 0, 0, static_cast<int>(width), static_cast<int>(height) };
	}

	return true;
}