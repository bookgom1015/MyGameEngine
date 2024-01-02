#include "SVGF.h"
#include "Logger.h"
#include "D3D12Util.h"
#include "ShaderManager.h"
#include "HlslCompaction.h"
#include "GpuResource.h"

using namespace SVGF;
using namespace DirectX;

namespace {
	std::string TsppReprojCS = "TsppReprojCS";
	std::string TsppBlendCS = "TsppBlendCS";
	std::string PartialDerivativeCS = "PartialDerivativeCS";
	std::string CalcLocalMeanVarianceCS = "CalcLocalMeanVarianceCS";
	std::string FillInCheckerboardCS = "FillInCheckerboardCS";
	std::string EdgeStoppingFilter_Gaussian3x3CS = "EdgeStoppingFilter_Gaussian3x3CS";
	std::string DisocclusionBlur3x3CS = "DisocclusionBlur3x3CS";
}

SVGFClass::SVGFClass() {
	for (INT i = 0; i < Resource::LocalMeanVariance::Count; ++i) {
		mLocalMeanVarianceResources[i] = std::make_unique<GpuResource>();
	}
	for (INT i = 0; i < Resource::Variance::Count; ++i) {
		mVarianceResources[i] = std::make_unique<GpuResource>();
	}
	mDepthPartialDerivative = std::make_unique<GpuResource>();
	mDisocclusionBlurStrength = std::make_unique<GpuResource>();
	mTsppValueSquaredMeanRayHitDistance = std::make_unique<GpuResource>();
}

BOOL SVGFClass::Initialize(ID3D12Device5* const device, ShaderManager* const manager, UINT width, UINT height) {
	md3dDevice = device;
	mShaderManager = manager;

	CheckReturn(BuildResources(width, height));

	return TRUE;
}

BOOL SVGFClass::CompileShaders(const std::wstring& filePath) {
	{
		const auto path = filePath + L"TemporalSupersamplingReverseReprojectCS.hlsl";
		auto shaderInfo = D3D12ShaderInfo(path.c_str(), L"CS", L"cs_6_3");
		CheckReturn(mShaderManager->CompileShader(shaderInfo, TsppReprojCS));
	}
	{
		const auto path = filePath + L"TemporalSupersamplingBlendWithCurrentFrameCS.hlsl";
		auto shaderInfo = D3D12ShaderInfo(path.c_str(), L"CS", L"cs_6_3");
		CheckReturn(mShaderManager->CompileShader(shaderInfo, TsppBlendCS));
	}
	{
		const auto path = filePath + L"CalculatePartialDerivativeCS.hlsl";
		auto shaderInfo = D3D12ShaderInfo(path.c_str(), L"CS", L"cs_6_3");
		CheckReturn(mShaderManager->CompileShader(shaderInfo, PartialDerivativeCS));
	}
	{
		const auto path = filePath + L"CalculateLocalMeanVarianceCS.hlsl";
		auto shaderInfo = D3D12ShaderInfo(path.c_str(), L"CS", L"cs_6_3");
		CheckReturn(mShaderManager->CompileShader(shaderInfo, CalcLocalMeanVarianceCS));
	}
	{
		const auto path = filePath + L"FillInCheckerboardCS.hlsl";
		auto shaderInfo = D3D12ShaderInfo(path.c_str(), L"CS", L"cs_6_3");
		CheckReturn(mShaderManager->CompileShader(shaderInfo, FillInCheckerboardCS));
	}
	{
		const auto path = filePath + L"EdgeStoppingFilter_Gaussian3x3CS.hlsl";
		auto shaderInfo = D3D12ShaderInfo(path.c_str(), L"CS", L"cs_6_3");
		CheckReturn(mShaderManager->CompileShader(shaderInfo, EdgeStoppingFilter_Gaussian3x3CS));
	}
	{
		const auto path = filePath + L"DisocclusionBlur3x3CS.hlsl";
		auto shaderInfo = D3D12ShaderInfo(path.c_str(), L"CS", L"cs_6_3");
		CheckReturn(mShaderManager->CompileShader(shaderInfo, DisocclusionBlur3x3CS));
	}

	return TRUE;
}

BOOL SVGFClass::BuildRootSignatures(const StaticSamplers& samplers) {
	// Temporal supersampling reverse reproject
	{
		CD3DX12_DESCRIPTOR_RANGE texTables[11];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
		texTables[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
		texTables[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);
		texTables[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4);
		texTables[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5);
		texTables[6].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 6);
		texTables[7].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 7);
		texTables[8].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 8);
		texTables[9].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
		texTables[10].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);

		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::TemporalSupersamplingReverseReproject::Count];
		slotRootParameter[RootSignature::TemporalSupersamplingReverseReproject::ECB_CrossBilateralFilter].InitAsConstantBufferView(0);
		slotRootParameter[RootSignature::TemporalSupersamplingReverseReproject::EC_Consts].InitAsConstants(
			RootSignature::TemporalSupersamplingReverseReproject::RootConstant::Count, 1);
		slotRootParameter[RootSignature::TemporalSupersamplingReverseReproject::ESI_NormalDepth].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[RootSignature::TemporalSupersamplingReverseReproject::ESI_DepthPartialDerivative].InitAsDescriptorTable(1, &texTables[1]);
		slotRootParameter[RootSignature::TemporalSupersamplingReverseReproject::ESI_ReprojectedNormalDepth].InitAsDescriptorTable(1, &texTables[2]);
		slotRootParameter[RootSignature::TemporalSupersamplingReverseReproject::ESI_CachedNormalDepth].InitAsDescriptorTable(1, &texTables[3]);
		slotRootParameter[RootSignature::TemporalSupersamplingReverseReproject::ESI_Velocity].InitAsDescriptorTable(1, &texTables[4]);
		slotRootParameter[RootSignature::TemporalSupersamplingReverseReproject::ESI_CachedAOCoefficient].InitAsDescriptorTable(1, &texTables[5]);
		slotRootParameter[RootSignature::TemporalSupersamplingReverseReproject::ESI_CachedTspp].InitAsDescriptorTable(1, &texTables[6]);
		slotRootParameter[RootSignature::TemporalSupersamplingReverseReproject::ESI_CachedAOCoefficientSquaredMean].InitAsDescriptorTable(1, &texTables[7]);
		slotRootParameter[RootSignature::TemporalSupersamplingReverseReproject::ESI_CachedRayHitDistance].InitAsDescriptorTable(1, &texTables[8]);
		slotRootParameter[RootSignature::TemporalSupersamplingReverseReproject::EUO_CachedTspp].InitAsDescriptorTable(1, &texTables[9]);
		slotRootParameter[RootSignature::TemporalSupersamplingReverseReproject::EUO_TsppCoefficientSquaredMeanRayHitDistacne].InitAsDescriptorTable(1, &texTables[10]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_NONE
		);
		CheckReturn(D3D12Util::CreateRootSignature(
			md3dDevice, rootSignatureDesc, &mRootSignatures[RootSignature::E_TemporalSupersamplingReverseReproject]));
	}
	// Temporal supersampling blend with current frame
	{
		CD3DX12_DESCRIPTOR_RANGE texTables[10];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
		texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
		texTables[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);
		texTables[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0);
		texTables[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);
		texTables[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1, 0);
		texTables[6].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2, 0);
		texTables[7].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 3, 0);
		texTables[8].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 4, 0);
		texTables[9].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 5, 0);

		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::TemporalSupersamplingBlendWithCurrentFrame::Count];
		slotRootParameter[RootSignature::TemporalSupersamplingBlendWithCurrentFrame::ECB_TsspBlendWithCurrentFrame].InitAsConstantBufferView(0);
		slotRootParameter[RootSignature::TemporalSupersamplingBlendWithCurrentFrame::ESI_AOCoefficient].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[RootSignature::TemporalSupersamplingBlendWithCurrentFrame::ESI_LocalMeanVaraince].InitAsDescriptorTable(1, &texTables[1]);
		slotRootParameter[RootSignature::TemporalSupersamplingBlendWithCurrentFrame::ESI_RayHitDistance].InitAsDescriptorTable(1, &texTables[2]);
		slotRootParameter[RootSignature::TemporalSupersamplingBlendWithCurrentFrame::ESI_TsppCoefficientSquaredMeanRayHitDistance].InitAsDescriptorTable(1, &texTables[3]);
		slotRootParameter[RootSignature::TemporalSupersamplingBlendWithCurrentFrame::EUIO_TemporalAOCoefficient].InitAsDescriptorTable(1, &texTables[4]);
		slotRootParameter[RootSignature::TemporalSupersamplingBlendWithCurrentFrame::EUIO_Tspp].InitAsDescriptorTable(1, &texTables[5]);
		slotRootParameter[RootSignature::TemporalSupersamplingBlendWithCurrentFrame::EUIO_CoefficientSquaredMean].InitAsDescriptorTable(1, &texTables[6]);
		slotRootParameter[RootSignature::TemporalSupersamplingBlendWithCurrentFrame::EUIO_RayHitDistance].InitAsDescriptorTable(1, &texTables[7]);
		slotRootParameter[RootSignature::TemporalSupersamplingBlendWithCurrentFrame::EUO_VarianceMap].InitAsDescriptorTable(1, &texTables[8]);
		slotRootParameter[RootSignature::TemporalSupersamplingBlendWithCurrentFrame::EUO_BlurStrength].InitAsDescriptorTable(1, &texTables[9]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_NONE
		);
		CheckReturn(D3D12Util::CreateRootSignature(
			md3dDevice, rootSignatureDesc, &mRootSignatures[RootSignature::E_TemporalSupersamplingBlendWithCurrentFrame]));
	}
	// CalculateDepthPartialDerivative
	{
		CD3DX12_DESCRIPTOR_RANGE texTables[2];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
		texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::CalcDepthPartialDerivative::Count];
		slotRootParameter[RootSignature::CalcDepthPartialDerivative::EC_Consts].InitAsConstants(
			RootSignature::CalcDepthPartialDerivative::RootConstant::Count, 0, 0);
		slotRootParameter[RootSignature::CalcDepthPartialDerivative::ESI_Depth].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[RootSignature::CalcDepthPartialDerivative::EUO_DepthPartialDerivative].InitAsDescriptorTable(1, &texTables[1]);

		CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_NONE
		);
		CheckReturn(D3D12Util::CreateRootSignature(
			md3dDevice, globalRootSignatureDesc, &mRootSignatures[RootSignature::E_CalcDepthPartialDerivative]));
	}
	// CalculateMeanVariance
	{
		CD3DX12_DESCRIPTOR_RANGE texTables[2];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
		texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::CalcLocalMeanVariance::Count];
		slotRootParameter[RootSignature::CalcLocalMeanVariance::ECB_LocalMeanVar].InitAsConstantBufferView(0, 0);
		slotRootParameter[RootSignature::CalcLocalMeanVariance::ESI_AOCoefficient].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[RootSignature::CalcLocalMeanVariance::EUO_LocalMeanVar].InitAsDescriptorTable(1, &texTables[1]);

		CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_NONE
		);
		CheckReturn(D3D12Util::CreateRootSignature(
			md3dDevice, globalRootSignatureDesc, &mRootSignatures[RootSignature::E_CalcLocalMeanVariance]));
	}
	// FillInCheckerboard
	{
		CD3DX12_DESCRIPTOR_RANGE texTables[1];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::FillInCheckerboard::Count];
		slotRootParameter[RootSignature::FillInCheckerboard::ECB_LocalMeanVar].InitAsConstantBufferView(0, 0);
		slotRootParameter[RootSignature::FillInCheckerboard::EUIO_LocalMeanVar].InitAsDescriptorTable(1, &texTables[0]);

		CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_NONE
		);
		CheckReturn(D3D12Util::CreateRootSignature(
			md3dDevice, globalRootSignatureDesc, &mRootSignatures[RootSignature::E_FillInCheckerboard]));
	}
	// Atrous Wavelet transform filter
	{
		CD3DX12_DESCRIPTOR_RANGE texTables[7];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
		texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
		texTables[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);
		texTables[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0);
		texTables[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 0);
		texTables[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5, 0);
		texTables[6].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::AtrousWaveletTransformFilter::Count];
		slotRootParameter[RootSignature::AtrousWaveletTransformFilter::ECB_AtrousFilter].InitAsConstantBufferView(0, 0);
		slotRootParameter[RootSignature::AtrousWaveletTransformFilter::ESI_TemporalAOCoefficient].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[RootSignature::AtrousWaveletTransformFilter::ESI_NormalDepth].InitAsDescriptorTable(1, &texTables[1]);
		slotRootParameter[RootSignature::AtrousWaveletTransformFilter::ESI_Variance].InitAsDescriptorTable(1, &texTables[2]);
		slotRootParameter[RootSignature::AtrousWaveletTransformFilter::ESI_HitDistance].InitAsDescriptorTable(1, &texTables[3]);
		slotRootParameter[RootSignature::AtrousWaveletTransformFilter::ESI_DepthPartialDerivative].InitAsDescriptorTable(1, &texTables[4]);
		slotRootParameter[RootSignature::AtrousWaveletTransformFilter::ESI_Tspp].InitAsDescriptorTable(1, &texTables[5]);
		slotRootParameter[RootSignature::AtrousWaveletTransformFilter::EUO_TemporalAOCoefficient].InitAsDescriptorTable(1, &texTables[6]);

		CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(
			_countof(slotRootParameter), slotRootParameter,
			0, nullptr,
			D3D12_ROOT_SIGNATURE_FLAG_NONE
		);
		CheckReturn(D3D12Util::CreateRootSignature(
			md3dDevice, globalRootSignatureDesc, &mRootSignatures[RootSignature::E_AtrousWaveletTransformFilter]));
	}
	// Disocclusion blur
	{
		CD3DX12_DESCRIPTOR_RANGE texTables[3];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
		texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
		texTables[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::DisocclusionBlur::Count];
		slotRootParameter[RootSignature::DisocclusionBlur::EC_Consts].InitAsConstants(
			RootSignature::DisocclusionBlur::RootConstant::Count, 0);
		slotRootParameter[RootSignature::DisocclusionBlur::ESI_Depth].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[RootSignature::DisocclusionBlur::ESI_BlurStrength].InitAsDescriptorTable(1, &texTables[1]);
		slotRootParameter[RootSignature::DisocclusionBlur::EUIO_AOCoefficient].InitAsDescriptorTable(1, &texTables[2]);

		CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(
			_countof(slotRootParameter), slotRootParameter,
			0, nullptr,
			D3D12_ROOT_SIGNATURE_FLAG_NONE
		);
		CheckReturn(D3D12Util::CreateRootSignature(
			md3dDevice, globalRootSignatureDesc, &mRootSignatures[RootSignature::E_DisocclusionBlur]));
	}

	return TRUE;
}

BOOL SVGFClass::BuildPSO() {
	// Temporal supersampling reverse reproject
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC tsppReprojPsoDesc = {};
		tsppReprojPsoDesc.pRootSignature = mRootSignatures[RootSignature::E_TemporalSupersamplingReverseReproject].Get();
		{
			auto cs = mShaderManager->GetDxcShader(TsppReprojCS);
			tsppReprojPsoDesc.CS = { reinterpret_cast<BYTE*>(cs->GetBufferPointer()), cs->GetBufferSize() };
		}
		tsppReprojPsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		CheckHRESULT(md3dDevice->CreateComputePipelineState(
			&tsppReprojPsoDesc, IID_PPV_ARGS(&mPsos[PipelineState::E_TemporalSupersamplingReverseReproject])));
	}
	// Temporal supersampling blend with current frame
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC tsppBlendPsoDesc = {};
		tsppBlendPsoDesc.pRootSignature = mRootSignatures[RootSignature::E_TemporalSupersamplingBlendWithCurrentFrame].Get();
		{
			auto cs = mShaderManager->GetDxcShader(TsppBlendCS);
			tsppBlendPsoDesc.CS = { reinterpret_cast<BYTE*>(cs->GetBufferPointer()), cs->GetBufferSize() };
		}
		tsppBlendPsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		CheckHRESULT(md3dDevice->CreateComputePipelineState(
			&tsppBlendPsoDesc, IID_PPV_ARGS(&mPsos[PipelineState::E_TemporalSupersamplingBlendWithCurrentFrame])));
	}
	// CalculateDepthPartialDerivative
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC calcPartialDerivativePsoDesc = {};
		calcPartialDerivativePsoDesc.pRootSignature = mRootSignatures[RootSignature::E_CalcDepthPartialDerivative].Get();
		{
			auto cs = mShaderManager->GetDxcShader(PartialDerivativeCS);
			calcPartialDerivativePsoDesc.CS = { reinterpret_cast<BYTE*>(cs->GetBufferPointer()), cs->GetBufferSize() };
		}
		calcPartialDerivativePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		CheckHRESULT(md3dDevice->CreateComputePipelineState(
			&calcPartialDerivativePsoDesc, IID_PPV_ARGS(&mPsos[PipelineState::E_CalcDepthPartialDerivative])));
	}
	// CalculateMeanVariance
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC calcLocalMeanVariancePsoDesc = {};
		calcLocalMeanVariancePsoDesc.pRootSignature = mRootSignatures[RootSignature::E_CalcLocalMeanVariance].Get();
		{
			auto cs = mShaderManager->GetDxcShader(CalcLocalMeanVarianceCS);
			calcLocalMeanVariancePsoDesc.CS = { reinterpret_cast<BYTE*>(cs->GetBufferPointer()), cs->GetBufferSize() };
		}
		calcLocalMeanVariancePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		CheckHRESULT(md3dDevice->CreateComputePipelineState(
			&calcLocalMeanVariancePsoDesc, IID_PPV_ARGS(&mPsos[PipelineState::E_CalcLocalMeanVariance])));
	}
	// FillInCheckerboard
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC fillInCheckerboardPsoDesc = {};
		fillInCheckerboardPsoDesc.pRootSignature = mRootSignatures[RootSignature::E_FillInCheckerboard].Get();
		{
			auto cs = mShaderManager->GetDxcShader(FillInCheckerboardCS);
			fillInCheckerboardPsoDesc.CS = { reinterpret_cast<BYTE*>(cs->GetBufferPointer()), cs->GetBufferSize() };
		}
		fillInCheckerboardPsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		CheckHRESULT(md3dDevice->CreateComputePipelineState(
			&fillInCheckerboardPsoDesc, IID_PPV_ARGS(&mPsos[PipelineState::E_FillInCheckerboard])));
	}
	// Atrous Wavelet transform filter
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC atrousWaveletTransformFilterPsoDesc = {};
		atrousWaveletTransformFilterPsoDesc.pRootSignature = mRootSignatures[RootSignature::E_AtrousWaveletTransformFilter].Get();
		{
			auto cs = mShaderManager->GetDxcShader(EdgeStoppingFilter_Gaussian3x3CS);
			atrousWaveletTransformFilterPsoDesc.CS = { reinterpret_cast<BYTE*>(cs->GetBufferPointer()), cs->GetBufferSize() };
		}
		atrousWaveletTransformFilterPsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		CheckHRESULT(md3dDevice->CreateComputePipelineState(
			&atrousWaveletTransformFilterPsoDesc, IID_PPV_ARGS(&mPsos[PipelineState::E_AtrousWaveletTransformFilter])));
	}
	// Disocclusion blur
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC disocclusionBlurPsoDesc = {};
		disocclusionBlurPsoDesc.pRootSignature = mRootSignatures[RootSignature::E_DisocclusionBlur].Get();
		{
			auto cs = mShaderManager->GetDxcShader(DisocclusionBlur3x3CS);
			disocclusionBlurPsoDesc.CS = { reinterpret_cast<BYTE*>(cs->GetBufferPointer()), cs->GetBufferSize() };
		}
		disocclusionBlurPsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		CheckHRESULT(md3dDevice->CreateComputePipelineState(
			&disocclusionBlurPsoDesc, IID_PPV_ARGS(&mPsos[PipelineState::E_DisocclusionBlur])));
	}

	return TRUE;
}

void SVGFClass::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu, CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu, UINT descSize) {
	for (UINT i = 0; i < Descriptor::LocalMeanVariance::Count; ++i) {
		mhLocalMeanVarianceResourcesCpus[i] = hCpu.Offset(1, descSize);
		mhLocalMeanVarianceResourcesGpus[i] = hGpu.Offset(1, descSize);
	}

	for (UINT i = 0; i < Descriptor::Variance::Count; ++i) {
		mhVarianceResourcesCpus[i] = hCpu.Offset(1, descSize);
		mhVarianceResourcesGpus[i] = hGpu.Offset(1, descSize);
	}

	mhDepthPartialDerivativeCpuSrv = hCpu.Offset(1, descSize);
	mhDepthPartialDerivativeGpuSrv = hGpu.Offset(1, descSize);
	mhDepthPartialDerivativeCpuUav = hCpu.Offset(1, descSize);
	mhDepthPartialDerivativeGpuUav = hGpu.Offset(1, descSize);

	mhDisocclusionBlurStrengthCpuSrv = hCpu.Offset(1, descSize);
	mhDisocclusionBlurStrengthGpuSrv = hGpu.Offset(1, descSize);
	mhDisocclusionBlurStrengthCpuUav = hCpu.Offset(1, descSize);
	mhDisocclusionBlurStrengthGpuUav = hGpu.Offset(1, descSize);

	mhTsppValueSquaredMeanRayHitDistanceCpuSrv = hCpu.Offset(1, descSize);
	mhTsppValueSquaredMeanRayHitDistanceGpuSrv = hGpu.Offset(1, descSize);
	mhTsppValueSquaredMeanRayHitDistanceCpuUav = hCpu.Offset(1, descSize);
	mhTsppValueSquaredMeanRayHitDistanceGpuUav = hGpu.Offset(1, descSize);

	BuildDescriptors();
}

BOOL SVGFClass::OnResize(UINT width, UINT height) {
	CheckReturn(BuildResources(width, height));
	BuildDescriptors();

	return TRUE;
}

void SVGFClass::RunCalculatingDepthPartialDerivative(
		ID3D12GraphicsCommandList4* const cmdList,
		D3D12_GPU_DESCRIPTOR_HANDLE i_depth,
		UINT width, UINT height) {
	cmdList->SetPipelineState(mPsos[PipelineState::E_CalcDepthPartialDerivative].Get());
	cmdList->SetComputeRootSignature(mRootSignatures[RootSignature::E_CalcDepthPartialDerivative].Get());

	const auto depthPartialDerivative = mDepthPartialDerivative.get();

	depthPartialDerivative->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	D3D12Util::UavBarrier(cmdList, depthPartialDerivative);

	const FLOAT values[RootSignature::CalcDepthPartialDerivative::RootConstant::Count] = { 1.0f / width, 1.0f / height };
	cmdList->SetComputeRoot32BitConstants(
		RootSignature::CalcDepthPartialDerivative::EC_Consts,
		RootSignature::CalcDepthPartialDerivative::RootConstant::Count,
		values, 0);

	cmdList->SetComputeRootDescriptorTable(RootSignature::CalcDepthPartialDerivative::ESI_Depth, i_depth);
	cmdList->SetComputeRootDescriptorTable(RootSignature::CalcDepthPartialDerivative::EUO_DepthPartialDerivative, mhDepthPartialDerivativeGpuUav);

	cmdList->Dispatch(
		D3D12Util::CeilDivide(width, Default::ThreadGroup::Width),
		D3D12Util::CeilDivide(height, Default::ThreadGroup::Height), 1);

	depthPartialDerivative->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	D3D12Util::UavBarrier(cmdList, depthPartialDerivative);
}

void SVGFClass::RunCalculatingLocalMeanVariance(
		ID3D12GraphicsCommandList4* const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
		D3D12_GPU_DESCRIPTOR_HANDLE si_aoCoefficient,
		UINT width, UINT height,
		BOOL checkerboardSamplingEnabled) {
	cmdList->SetPipelineState(mPsos[PipelineState::E_CalcLocalMeanVariance].Get());
	cmdList->SetComputeRootSignature(mRootSignatures[RootSignature::E_CalcLocalMeanVariance].Get());

	const auto rawLocalMeanVariance = mLocalMeanVarianceResources[SVGF::Resource::LocalMeanVariance::E_Raw].get();
	rawLocalMeanVariance->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	D3D12Util::UavBarrier(cmdList, rawLocalMeanVariance);

	const auto uo_localMeanVariance = mhLocalMeanVarianceResourcesGpus[SVGF::Descriptor::LocalMeanVariance::EU_Raw];

	cmdList->SetComputeRootConstantBufferView(RootSignature::CalcLocalMeanVariance::ECB_LocalMeanVar, cbAddress);
	cmdList->SetComputeRootDescriptorTable(RootSignature::CalcLocalMeanVariance::ESI_AOCoefficient, si_aoCoefficient);
	cmdList->SetComputeRootDescriptorTable(RootSignature::CalcLocalMeanVariance::EUO_LocalMeanVar, uo_localMeanVariance);

	INT pixelStepY = checkerboardSamplingEnabled ? 2 : 1;
	cmdList->Dispatch(
		D3D12Util::CeilDivide(width, Default::ThreadGroup::Width),
		D3D12Util::CeilDivide(height, Default::ThreadGroup::Height * pixelStepY), 1);

	rawLocalMeanVariance->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	D3D12Util::UavBarrier(cmdList, rawLocalMeanVariance);
}

void SVGFClass::FillInCheckerboard(
		ID3D12GraphicsCommandList4* const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
		UINT width, UINT height) {
	cmdList->SetPipelineState(mPsos[PipelineState::E_FillInCheckerboard].Get());
	cmdList->SetComputeRootSignature(mRootSignatures[RootSignature::E_FillInCheckerboard].Get());

	const auto rawLocalMeanVariance = mLocalMeanVarianceResources[SVGF::Resource::LocalMeanVariance::E_Raw].get();
	rawLocalMeanVariance->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	D3D12Util::UavBarrier(cmdList, rawLocalMeanVariance);

	const auto uio_localMeanVariance = mhLocalMeanVarianceResourcesGpus[SVGF::Descriptor::LocalMeanVariance::EU_Raw];

	cmdList->SetComputeRootConstantBufferView(RootSignature::FillInCheckerboard::ECB_LocalMeanVar, cbAddress);
	cmdList->SetComputeRootDescriptorTable(RootSignature::FillInCheckerboard::EUIO_LocalMeanVar, uio_localMeanVariance);

	cmdList->Dispatch(
		D3D12Util::CeilDivide(width, Default::ThreadGroup::Width),
		D3D12Util::CeilDivide(height, Default::ThreadGroup::Height * 2), 1);

	rawLocalMeanVariance->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	D3D12Util::UavBarrier(cmdList, rawLocalMeanVariance);
}

void SVGFClass::ReverseReprojectPreviousFrame(
		ID3D12GraphicsCommandList4* const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
		D3D12_GPU_DESCRIPTOR_HANDLE si_normalDepth,
		D3D12_GPU_DESCRIPTOR_HANDLE si_reprojNormalDepth,
		D3D12_GPU_DESCRIPTOR_HANDLE si_cachedNormalDepth,
		D3D12_GPU_DESCRIPTOR_HANDLE si_velocity,
		D3D12_GPU_DESCRIPTOR_HANDLE si_cachedAOCoefficient,
		D3D12_GPU_DESCRIPTOR_HANDLE si_cachedTspp,
		D3D12_GPU_DESCRIPTOR_HANDLE si_cachedAOCoefficientSquaredMean,
		D3D12_GPU_DESCRIPTOR_HANDLE si_cachedRayHitDistance,
		D3D12_GPU_DESCRIPTOR_HANDLE uo_cachedTspp,
		UINT width, UINT height) {
	cmdList->SetPipelineState(mPsos[PipelineState::E_TemporalSupersamplingReverseReproject].Get());
	cmdList->SetComputeRootSignature(mRootSignatures[RootSignature::E_TemporalSupersamplingReverseReproject].Get());

	cmdList->SetComputeRootConstantBufferView(RootSignature::TemporalSupersamplingReverseReproject::ECB_CrossBilateralFilter, cbAddress);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingReverseReproject::ESI_NormalDepth, si_normalDepth);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingReverseReproject::ESI_DepthPartialDerivative, mhDepthPartialDerivativeGpuSrv);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingReverseReproject::ESI_ReprojectedNormalDepth, si_reprojNormalDepth);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingReverseReproject::ESI_CachedNormalDepth, si_cachedNormalDepth);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingReverseReproject::ESI_Velocity, si_velocity);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingReverseReproject::ESI_CachedAOCoefficient, si_cachedAOCoefficient);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingReverseReproject::ESI_CachedTspp, si_cachedTspp);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingReverseReproject::ESI_CachedAOCoefficientSquaredMean, si_cachedAOCoefficientSquaredMean);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingReverseReproject::ESI_CachedRayHitDistance, si_cachedRayHitDistance);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingReverseReproject::EUO_CachedTspp, uo_cachedTspp);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingReverseReproject::EUO_TsppCoefficientSquaredMeanRayHitDistacne, mhTsppValueSquaredMeanRayHitDistanceGpuUav);

	{
		UINT values[] = { width, height };
		cmdList->SetComputeRoot32BitConstants(
			RootSignature::TemporalSupersamplingReverseReproject::EC_Consts,
			_countof(values), values,
			RootSignature::TemporalSupersamplingReverseReproject::RootConstant::E_TextureDim_X
		);
	}
	{
		FLOAT values[] = { 1.0f / width, 1.0f / height };
		cmdList->SetComputeRoot32BitConstants(
			RootSignature::TemporalSupersamplingReverseReproject::EC_Consts,
			_countof(values), values,
			RootSignature::TemporalSupersamplingReverseReproject::RootConstant::E_InvTextureDim_X
		);
	}

	cmdList->Dispatch(
		D3D12Util::CeilDivide(width, Default::ThreadGroup::Width),
		D3D12Util::CeilDivide(height, Default::ThreadGroup::Height), 1);
}

void SVGFClass::BlendWithCurrentFrame(
		ID3D12GraphicsCommandList4* const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
		D3D12_GPU_DESCRIPTOR_HANDLE si_aoCoefficient,
		D3D12_GPU_DESCRIPTOR_HANDLE si_rayHitDistance,
		D3D12_GPU_DESCRIPTOR_HANDLE uio_temporalAOCoefficient,
		D3D12_GPU_DESCRIPTOR_HANDLE uio_tspp,
		D3D12_GPU_DESCRIPTOR_HANDLE uio_coefficientSquaredMean,
		D3D12_GPU_DESCRIPTOR_HANDLE uio_rayHitDistance,
		UINT width, UINT height) {
	cmdList->SetPipelineState(mPsos[PipelineState::E_TemporalSupersamplingBlendWithCurrentFrame].Get());
	cmdList->SetComputeRootSignature(mRootSignatures[RootSignature::E_TemporalSupersamplingBlendWithCurrentFrame].Get());

	const auto rawVariance = mVarianceResources[SVGF::Resource::Variance::E_Raw].get();
	const auto disocclusionBlurStrength = mDisocclusionBlurStrength.get();

	std::vector<GpuResource*> resources = { rawVariance, disocclusionBlurStrength };

	rawVariance->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	disocclusionBlurStrength->Transite(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	D3D12Util::UavBarriers(cmdList, resources.data(), resources.size());

	cmdList->SetComputeRootConstantBufferView(RootSignature::TemporalSupersamplingBlendWithCurrentFrame::ECB_TsspBlendWithCurrentFrame, cbAddress);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingBlendWithCurrentFrame::ESI_AOCoefficient, si_aoCoefficient);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingBlendWithCurrentFrame::ESI_LocalMeanVaraince, mhLocalMeanVarianceResourcesGpus[SVGF::Descriptor::LocalMeanVariance::ES_Raw]);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingBlendWithCurrentFrame::ESI_RayHitDistance, si_rayHitDistance);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingBlendWithCurrentFrame::ESI_TsppCoefficientSquaredMeanRayHitDistance, mhTsppValueSquaredMeanRayHitDistanceGpuSrv);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingBlendWithCurrentFrame::EUIO_TemporalAOCoefficient, uio_temporalAOCoefficient);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingBlendWithCurrentFrame::EUIO_Tspp, uio_tspp);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingBlendWithCurrentFrame::EUIO_CoefficientSquaredMean, uio_coefficientSquaredMean);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingBlendWithCurrentFrame::EUIO_RayHitDistance, uio_rayHitDistance);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingBlendWithCurrentFrame::EUO_VarianceMap, mhVarianceResourcesGpus[SVGF::Descriptor::Variance::EU_Raw]);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingBlendWithCurrentFrame::EUO_BlurStrength, mhDisocclusionBlurStrengthGpuUav);

	cmdList->Dispatch(
		D3D12Util::CeilDivide(width, Default::ThreadGroup::Width),
		D3D12Util::CeilDivide(height, Default::ThreadGroup::Height), 1);

	rawVariance->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	disocclusionBlurStrength->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	D3D12Util::UavBarriers(cmdList, resources.data(), resources.size());
}

void SVGFClass::ApplyAtrousWaveletTransformFilter(
		ID3D12GraphicsCommandList4* const cmdList,
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
		D3D12_GPU_DESCRIPTOR_HANDLE si_temporalAOCoefficient,
		D3D12_GPU_DESCRIPTOR_HANDLE si_normalDepth,
		D3D12_GPU_DESCRIPTOR_HANDLE si_hitDistance,
		D3D12_GPU_DESCRIPTOR_HANDLE si_tspp,
		D3D12_GPU_DESCRIPTOR_HANDLE uo_temporalAOCoefficient,
		UINT width, UINT height,
		bool useSmoothingVar) {
	cmdList->SetPipelineState(mPsos[PipelineState::E_AtrousWaveletTransformFilter].Get());
	cmdList->SetComputeRootSignature(mRootSignatures[RootSignature::E_AtrousWaveletTransformFilter].Get());

	const auto si_variance = mhVarianceResourcesGpus[useSmoothingVar ? SVGF::Descriptor::Variance::ES_Smoothed : SVGF::Descriptor::Variance::ES_Raw];

	cmdList->SetComputeRootConstantBufferView(RootSignature::AtrousWaveletTransformFilter::ECB_AtrousFilter, cbAddress);
	cmdList->SetComputeRootDescriptorTable(RootSignature::AtrousWaveletTransformFilter::ESI_TemporalAOCoefficient, si_temporalAOCoefficient);
	cmdList->SetComputeRootDescriptorTable(RootSignature::AtrousWaveletTransformFilter::ESI_NormalDepth, si_normalDepth);
	cmdList->SetComputeRootDescriptorTable(RootSignature::AtrousWaveletTransformFilter::ESI_Variance, si_variance);
	cmdList->SetComputeRootDescriptorTable(RootSignature::AtrousWaveletTransformFilter::ESI_HitDistance, si_hitDistance);
	cmdList->SetComputeRootDescriptorTable(RootSignature::AtrousWaveletTransformFilter::ESI_DepthPartialDerivative, mhDepthPartialDerivativeGpuSrv);
	cmdList->SetComputeRootDescriptorTable(RootSignature::AtrousWaveletTransformFilter::ESI_Tspp, si_tspp);
	cmdList->SetComputeRootDescriptorTable(RootSignature::AtrousWaveletTransformFilter::EUO_TemporalAOCoefficient, uo_temporalAOCoefficient);

	cmdList->Dispatch(
		D3D12Util::CeilDivide(width, Default::ThreadGroup::Width),
		D3D12Util::CeilDivide(height, Default::ThreadGroup::Height), 1);
}

void SVGFClass::BlurDisocclusion(
		ID3D12GraphicsCommandList4* const cmdList,
		GpuResource* aoCoefficient,
		D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
		D3D12_GPU_DESCRIPTOR_HANDLE uio_aoCoefficient,
		UINT width, UINT height,
		UINT lowTsppBlurPasses) {
	cmdList->SetPipelineState(mPsos[PipelineState::E_DisocclusionBlur].Get());
	cmdList->SetComputeRootSignature(mRootSignatures[RootSignature::E_DisocclusionBlur].Get());

	UINT values[2] = { width, height };
	cmdList->SetComputeRoot32BitConstants(RootSignature::DisocclusionBlur::EC_Consts, _countof(values), values, 0);

	cmdList->SetComputeRootDescriptorTable(RootSignature::DisocclusionBlur::ESI_Depth, si_depth);
	cmdList->SetComputeRootDescriptorTable(RootSignature::DisocclusionBlur::ESI_BlurStrength, mhDisocclusionBlurStrengthGpuSrv);
	cmdList->SetComputeRootDescriptorTable(RootSignature::DisocclusionBlur::EUIO_AOCoefficient, uio_aoCoefficient);

	UINT filterStep = 1;
	for (UINT i = 0; i < lowTsppBlurPasses; ++i) {
		cmdList->SetComputeRoot32BitConstant(
			RootSignature::DisocclusionBlur::EC_Consts, filterStep, RootSignature::DisocclusionBlur::RootConstant::E_Step);

		// Account for interleaved Group execution
		UINT widthCS = filterStep * Default::ThreadGroup::Width * D3D12Util::CeilDivide(width, filterStep * Default::ThreadGroup::Width);
		UINT heightCS = filterStep * Default::ThreadGroup::Height * D3D12Util::CeilDivide(height, filterStep * Default::ThreadGroup::Height);

		// Dispatch.
		XMUINT2 groupSize(D3D12Util::CeilDivide(widthCS, Default::ThreadGroup::Width), D3D12Util::CeilDivide(heightCS, Default::ThreadGroup::Height));
		cmdList->Dispatch(groupSize.x, groupSize.y, 1);
		D3D12Util::UavBarrier(cmdList, aoCoefficient);

		filterStep *= 2;
	}
}

void SVGFClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

	{
		srvDesc.Format = LocalMeanVarianceMapFormat;
		uavDesc.Format = LocalMeanVarianceMapFormat;
		auto rawResource = mLocalMeanVarianceResources[Resource::LocalMeanVariance::E_Raw]->Resource();
		md3dDevice->CreateShaderResourceView(rawResource, &srvDesc, mhLocalMeanVarianceResourcesCpus[Descriptor::LocalMeanVariance::ES_Raw]);
		md3dDevice->CreateUnorderedAccessView(rawResource, nullptr, &uavDesc, mhLocalMeanVarianceResourcesCpus[Descriptor::LocalMeanVariance::EU_Raw]);

		auto smoothedResource = mLocalMeanVarianceResources[Resource::LocalMeanVariance::E_Smoothed]->Resource();
		md3dDevice->CreateShaderResourceView(smoothedResource, &srvDesc, mhLocalMeanVarianceResourcesCpus[Descriptor::LocalMeanVariance::ES_Smoothed]);
		md3dDevice->CreateUnorderedAccessView(smoothedResource, nullptr, &uavDesc, mhLocalMeanVarianceResourcesCpus[Descriptor::LocalMeanVariance::EU_Smoothed]);
	}
	{
		srvDesc.Format = VarianceMapFormat;
		uavDesc.Format = VarianceMapFormat;

		auto rawResource = mVarianceResources[Resource::Variance::E_Raw]->Resource();
		md3dDevice->CreateShaderResourceView(rawResource, &srvDesc, mhVarianceResourcesCpus[Descriptor::Variance::ES_Raw]);
		md3dDevice->CreateUnorderedAccessView(rawResource, nullptr, &uavDesc, mhVarianceResourcesCpus[Descriptor::Variance::EU_Raw]);

		auto smoothedResource = mVarianceResources[Resource::Variance::E_Smoothed]->Resource();
		md3dDevice->CreateShaderResourceView(smoothedResource, &srvDesc, mhVarianceResourcesCpus[Descriptor::Variance::ES_Smoothed]);
		md3dDevice->CreateUnorderedAccessView(smoothedResource, nullptr, &uavDesc, mhVarianceResourcesCpus[Descriptor::Variance::EU_Smoothed]);
	}
	{
		srvDesc.Format = DepthPartialDerivativeMapFormat;
		md3dDevice->CreateShaderResourceView(mDepthPartialDerivative->Resource(), &srvDesc, mhDepthPartialDerivativeCpuSrv);

		uavDesc.Format = DepthPartialDerivativeMapFormat;
		md3dDevice->CreateUnorderedAccessView(mDepthPartialDerivative->Resource(), nullptr, &uavDesc, mhDepthPartialDerivativeCpuUav);
	}
	{
		srvDesc.Format = DisocclusionBlurStrengthMapFormat;
		md3dDevice->CreateShaderResourceView(mDisocclusionBlurStrength->Resource(), &srvDesc, mhDisocclusionBlurStrengthCpuSrv);

		uavDesc.Format = DisocclusionBlurStrengthMapFormat;
		md3dDevice->CreateUnorderedAccessView(mDisocclusionBlurStrength->Resource(), nullptr, &uavDesc, mhDisocclusionBlurStrengthCpuUav);
	}
	{
		srvDesc.Format = TsppF1ValueSquaredMeanRayHitDistanceFormat;
		md3dDevice->CreateShaderResourceView(mTsppValueSquaredMeanRayHitDistance->Resource(), &srvDesc, mhTsppValueSquaredMeanRayHitDistanceCpuSrv);

		uavDesc.Format = TsppF1ValueSquaredMeanRayHitDistanceFormat;
		md3dDevice->CreateUnorderedAccessView(mTsppValueSquaredMeanRayHitDistance->Resource(), nullptr, &uavDesc, mhTsppValueSquaredMeanRayHitDistanceCpuUav);
	}
}

BOOL SVGFClass::BuildResources(UINT width, UINT height) {
	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	{
		texDesc.Format = LocalMeanVarianceMapFormat;

		CheckReturn(mLocalMeanVarianceResources[Resource::LocalMeanVariance::E_Raw]->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"RawLocalMeanVarianceMap"
		));
		CheckReturn(mLocalMeanVarianceResources[Resource::LocalMeanVariance::E_Smoothed]->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"SmoothedLocalMeanVarianceMap"
		));
	}
	{
		texDesc.Format = VarianceMapFormat;

		CheckReturn(mVarianceResources[Resource::Variance::E_Raw]->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"RawVarianceMap"
		));
		CheckReturn(mVarianceResources[Resource::Variance::E_Smoothed]->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"SmoothedVarianceMap"
		));
	}
	{
		texDesc.Format = DepthPartialDerivativeMapFormat;

		CheckReturn(mDepthPartialDerivative->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"DepthPartialDerivativeMap"
		));
	}
	{
		texDesc.Format = DisocclusionBlurStrengthMapFormat;

		CheckReturn(mDisocclusionBlurStrength->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"DisocclusionBlurStrengthMap"
		));
	}
	{
		texDesc.Format = TsppF1ValueSquaredMeanRayHitDistanceFormat;

		CheckReturn(mTsppValueSquaredMeanRayHitDistance->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"TsppF1ValueSquaredMeanRayHitDistanceMap"
		));
	}

	return TRUE;
}