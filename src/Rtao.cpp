#include "Rtao.h"
#include "Logger.h"
#include "D3D12Util.h"
#include "ShaderManager.h"
#include "ShaderTable.h"
#include "HlslCompaction.h"

#include <DirectXColors.h>

#undef max

using namespace DirectX;
using namespace Rtao;

namespace {
	std::string TsppReprojCS = "TsppReprojCS";
	std::string TsppBlendCS = "TsppReprojCS";
	std::string PartialDerivativeCS = "PartialDerivativeCS";
	std::string CalcLocalMeanVarianceCS = "CalcLocalMeanVarianceCS";
	std::string FillInCheckerboardCS = "FillInCheckerboardCS";
	std::string EdgeStoppingFilter_Gaussian3x3CS = "EdgeStoppingFilter_Gaussian3x3CS";
	std::string DisocclusionBlur3x3CS = "DisocclusionBlur3x3CS";

	std::string RtaoRS = "RtaoRS";
	std::string TsppReprojRS = "TsppReprojRS";
	std::string TsppBlendRS = "TsppBlendRS";
	std::string PartialDepthDerivativeRS = "PartialDepthDerivativeRS";
	std::string LocalMeanVarianceRS = "LocalMeanVarianceRS";
	std::string FillInCheckboardRS = "FillInCheckerboardRS";
	std::string AtrousWaveletTransformFilterRS = "AtrousWaveletTransformFilterRS";
	std::string DisocclusionBlurRS = "DisocclusionBlurRS";

	std::string TsppReprojPSO = "TsppReprojPSO";
	std::string TsppBlendPSO = "TsppBlendPSO";
	std::string PartialDepthDerivativePSO = "PartialDepthDerivativePSO";
	std::string LocalMeanVariancePSO = "LocalMeanVariancePSO";
	std::string FillInCheckerboardPSO = "FillInCheckerboardPSO";
	std::string AtrousWaveletTransformFilterPSO = "AtrousWaveletTransformFilterPSO";
	std::string DisocclusionBlurPSO = "DisocclusionBlurPSO";
}

RtaoClass::RtaoClass() {
	mTemporalCurrentFrameResourceIndex = 0;
	mTemporalCurrentFrameTemporalAOCeofficientResourceIndex = 0;

	for (int i = 0; i < AO::Resources::Count; ++i) {
		mAOResources[i] = std::make_unique<GpuResource>();
	}

	for (int i = 0; i < LocalMeanVariance::Resources::Count; ++i) {
		mLocalMeanVarianceResources[i] = std::make_unique<GpuResource>();
	}

	for (int i = 0; i < AOVariance::Resources::Count; ++i) {
		mAOVarianceResources[i] = std::make_unique<GpuResource>();
	}

	for (int i = 0; i < 2; ++i) {
		for (int j = 0; j < TemporalCache::Resources::Count; ++j) {
			mTemporalCaches[i][j] = std::make_unique<GpuResource>();
		}

		mTemporalAOCoefficients[i] = std::make_unique<GpuResource>();
	}

	mPrevFrameNormalDepth = std::make_unique<GpuResource>();
	mTsppCoefficientSquaredMeanRayHitDistance = std::make_unique<GpuResource>();
	mDisocclusionBlurStrength = std::make_unique<GpuResource>();
	mDepthPartialDerivative = std::make_unique<GpuResource>();
};

bool RtaoClass::Initialize(ID3D12Device5* const device, ID3D12GraphicsCommandList* const cmdList, ShaderManager* const manager, UINT width, UINT height) {
	md3dDevice = device;
	mShaderManager = manager;

	mWidth = width;
	mHeight = height;

	CheckReturn(BuildResources(cmdList));

	return true;
}

bool RtaoClass::CompileShaders(const std::wstring& filePath) {
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

	return true;
}

bool RtaoClass::BuildRootSignatures(const StaticSamplers& samplers) {
	// Ray-traced ambient occlusion
	{
		CD3DX12_DESCRIPTOR_RANGE texTables[4];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
		texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
		texTables[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
		texTables[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);

		CD3DX12_ROOT_PARAMETER slotRootParameter[CalcAmbientOcclusion::RootSignatureLayout::Count];
		slotRootParameter[CalcAmbientOcclusion::RootSignatureLayout::ESI_AccelerationStructure].InitAsShaderResourceView(0);
		slotRootParameter[CalcAmbientOcclusion::RootSignatureLayout::ECB_RtaoPass].InitAsConstantBufferView(0);
		slotRootParameter[CalcAmbientOcclusion::RootSignatureLayout::EC_Consts].InitAsConstants(CalcAmbientOcclusion::RootConstantsLayout::Count, 1, 0);
		slotRootParameter[CalcAmbientOcclusion::RootSignatureLayout::ESI_Normal].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[CalcAmbientOcclusion::RootSignatureLayout::ESI_Depth].InitAsDescriptorTable(1, &texTables[1]);
		slotRootParameter[CalcAmbientOcclusion::RootSignatureLayout::EUO_AOCoefficient].InitAsDescriptorTable(1, &texTables[2]);
		slotRootParameter[CalcAmbientOcclusion::RootSignatureLayout::EUO_RayHitDistance].InitAsDescriptorTable(1, &texTables[3]);

		CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_NONE
		);
		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, globalRootSignatureDesc, &mRootSignatures[RtaoRS]));
	}
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

		CD3DX12_ROOT_PARAMETER slotRootParameter[TemporalSupersamplingReverseReproject::RootSignatureLayout::Count];
		slotRootParameter[TemporalSupersamplingReverseReproject::RootSignatureLayout::ECB_CrossBilateralFilter].InitAsConstantBufferView(0);
		slotRootParameter[TemporalSupersamplingReverseReproject::RootSignatureLayout::EC_Consts].InitAsConstants(TemporalSupersamplingReverseReproject::RootConstantsLayout::Count, 1);
		slotRootParameter[TemporalSupersamplingReverseReproject::RootSignatureLayout::ESI_NormalDepth].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[TemporalSupersamplingReverseReproject::RootSignatureLayout::ESI_DepthPartialDerivative].InitAsDescriptorTable(1, &texTables[1]);
		slotRootParameter[TemporalSupersamplingReverseReproject::RootSignatureLayout::ESI_ReprojectedNormalDepth].InitAsDescriptorTable(1, &texTables[2]);
		slotRootParameter[TemporalSupersamplingReverseReproject::RootSignatureLayout::ESI_CachedNormalDepth].InitAsDescriptorTable(1, &texTables[3]);
		slotRootParameter[TemporalSupersamplingReverseReproject::RootSignatureLayout::ESI_Velocity].InitAsDescriptorTable(1, &texTables[4]);
		slotRootParameter[TemporalSupersamplingReverseReproject::RootSignatureLayout::ESI_CachedAOCoefficient].InitAsDescriptorTable(1, &texTables[5]);
		slotRootParameter[TemporalSupersamplingReverseReproject::RootSignatureLayout::ESI_CachedTspp].InitAsDescriptorTable(1, &texTables[6]);
		slotRootParameter[TemporalSupersamplingReverseReproject::RootSignatureLayout::ESI_CachedAOCoefficientSquaredMean].InitAsDescriptorTable(1, &texTables[7]);
		slotRootParameter[TemporalSupersamplingReverseReproject::RootSignatureLayout::ESI_CachedRayHitDistance].InitAsDescriptorTable(1, &texTables[8]);
		slotRootParameter[TemporalSupersamplingReverseReproject::RootSignatureLayout::EUO_CachedTspp].InitAsDescriptorTable(1, &texTables[9]);
		slotRootParameter[TemporalSupersamplingReverseReproject::RootSignatureLayout::EUO_TsppCoefficientSquaredMeanRayHitDistacne].InitAsDescriptorTable(1, &texTables[10]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_NONE
		);
		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSignatureDesc, &mRootSignatures[TsppReprojRS]));
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

		CD3DX12_ROOT_PARAMETER slotRootParameter[TemporalSupersamplingBlendWithCurrentFrame::RootSignatureLayout::Count];
		slotRootParameter[TemporalSupersamplingBlendWithCurrentFrame::RootSignatureLayout::ECB_TsspBlendWithCurrentFrame].InitAsConstantBufferView(0);
		slotRootParameter[TemporalSupersamplingBlendWithCurrentFrame::RootSignatureLayout::ESI_AOCoefficient].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[TemporalSupersamplingBlendWithCurrentFrame::RootSignatureLayout::ESI_LocalMeanVaraince].InitAsDescriptorTable(1, &texTables[1]);
		slotRootParameter[TemporalSupersamplingBlendWithCurrentFrame::RootSignatureLayout::ESI_RayHitDistance].InitAsDescriptorTable(1, &texTables[2]);
		slotRootParameter[TemporalSupersamplingBlendWithCurrentFrame::RootSignatureLayout::ESI_TsppCoefficientSquaredMeanRayHitDistance].InitAsDescriptorTable(1, &texTables[3]);
		slotRootParameter[TemporalSupersamplingBlendWithCurrentFrame::RootSignatureLayout::EUIO_TemporalAOCoefficient].InitAsDescriptorTable(1, &texTables[4]);
		slotRootParameter[TemporalSupersamplingBlendWithCurrentFrame::RootSignatureLayout::EUIO_Tspp].InitAsDescriptorTable(1, &texTables[5]);
		slotRootParameter[TemporalSupersamplingBlendWithCurrentFrame::RootSignatureLayout::EUIO_CoefficientSquaredMean].InitAsDescriptorTable(1, &texTables[6]);
		slotRootParameter[TemporalSupersamplingBlendWithCurrentFrame::RootSignatureLayout::EUIO_RayHitDistance].InitAsDescriptorTable(1, &texTables[7]);
		slotRootParameter[TemporalSupersamplingBlendWithCurrentFrame::RootSignatureLayout::EUO_VarianceMap].InitAsDescriptorTable(1, &texTables[8]);
		slotRootParameter[TemporalSupersamplingBlendWithCurrentFrame::RootSignatureLayout::EUO_BlurStrength].InitAsDescriptorTable(1, &texTables[9]);

		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_NONE
		);
		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, rootSignatureDesc, &mRootSignatures[TsppBlendRS]));
	}
	// CalculateDepthPartialDerivative
	{
		CD3DX12_DESCRIPTOR_RANGE texTables[2];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
		texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

		CD3DX12_ROOT_PARAMETER slotRootParameter[CalcDepthPartialDerivative::RootSignatureLayout::Count];
		slotRootParameter[CalcDepthPartialDerivative::RootSignatureLayout::EC_Consts].InitAsConstants(CalcDepthPartialDerivative::RootConstantsLayout::Count, 0, 0);
		slotRootParameter[CalcDepthPartialDerivative::RootSignatureLayout::ESI_Depth].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[CalcDepthPartialDerivative::RootSignatureLayout::EUO_DepthPartialDerivative].InitAsDescriptorTable(1, &texTables[1]);

		CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_NONE
		);
		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, globalRootSignatureDesc, mRootSignatures[PartialDepthDerivativeRS].GetAddressOf()));
	}
	// CalculateMeanVariance
	{
		CD3DX12_DESCRIPTOR_RANGE texTables[2];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
		texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

		CD3DX12_ROOT_PARAMETER slotRootParameter[CalcLocalMeanVariance::RootSignatureLayout::Count];
		slotRootParameter[CalcLocalMeanVariance::RootSignatureLayout::ECB_LocalMeanVar].InitAsConstantBufferView(0, 0);
		slotRootParameter[CalcLocalMeanVariance::RootSignatureLayout::ESI_AOCoefficient].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[CalcLocalMeanVariance::RootSignatureLayout::EUO_LocalMeanVar].InitAsDescriptorTable(1, &texTables[1]);

		CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_NONE
		);
		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, globalRootSignatureDesc, &mRootSignatures[LocalMeanVarianceRS]));
	}
	// FillInCheckerboard
	{
		CD3DX12_DESCRIPTOR_RANGE texTables[1];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

		CD3DX12_ROOT_PARAMETER slotRootParameter[FillInCheckerboard::RootSignatureLayout::Count];
		slotRootParameter[FillInCheckerboard::RootSignatureLayout::ECB_LocalMeanVar].InitAsConstantBufferView(0, 0);
		slotRootParameter[FillInCheckerboard::RootSignatureLayout::EUIO_LocalMeanVar].InitAsDescriptorTable(1, &texTables[0]);

		CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_NONE
		);
		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, globalRootSignatureDesc, &mRootSignatures[FillInCheckboardRS]));
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

		CD3DX12_ROOT_PARAMETER slotRootParameter[AtrousWaveletTransformFilter::RootSignatureLayout::Count];
		slotRootParameter[AtrousWaveletTransformFilter::RootSignatureLayout::ECB_AtrousFilter].InitAsConstantBufferView(0, 0);
		slotRootParameter[AtrousWaveletTransformFilter::RootSignatureLayout::ESI_TemporalAOCoefficient].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[AtrousWaveletTransformFilter::RootSignatureLayout::ESI_NormalDepth].InitAsDescriptorTable(1, &texTables[1]);
		slotRootParameter[AtrousWaveletTransformFilter::RootSignatureLayout::ESI_Variance].InitAsDescriptorTable(1, &texTables[2]);
		slotRootParameter[AtrousWaveletTransformFilter::RootSignatureLayout::ESI_HitDistance].InitAsDescriptorTable(1, &texTables[3]);
		slotRootParameter[AtrousWaveletTransformFilter::RootSignatureLayout::ESI_DepthPartialDerivative].InitAsDescriptorTable(1, &texTables[4]);
		slotRootParameter[AtrousWaveletTransformFilter::RootSignatureLayout::ESI_Tspp].InitAsDescriptorTable(1, &texTables[5]);
		slotRootParameter[AtrousWaveletTransformFilter::RootSignatureLayout::EUO_TemporalAOCoefficient].InitAsDescriptorTable(1, &texTables[6]);

		CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(
			_countof(slotRootParameter), slotRootParameter,
			0, nullptr,
			D3D12_ROOT_SIGNATURE_FLAG_NONE
		);
		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, globalRootSignatureDesc, &mRootSignatures[AtrousWaveletTransformFilterRS]));
	}
	// Disocclusion blur
	{
		CD3DX12_DESCRIPTOR_RANGE texTables[3];
		texTables[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
		texTables[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);
		texTables[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

		CD3DX12_ROOT_PARAMETER slotRootParameter[DisocclusionBlur::RootSignatureLayout::Count];
		slotRootParameter[DisocclusionBlur::RootSignatureLayout::EC_Consts].InitAsConstants(DisocclusionBlur::RootConstantsLayout::Count, 0);
		slotRootParameter[DisocclusionBlur::RootSignatureLayout::ESI_Depth].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[DisocclusionBlur::RootSignatureLayout::ESI_BlurStrength].InitAsDescriptorTable(1, &texTables[1]);
		slotRootParameter[DisocclusionBlur::RootSignatureLayout::EUIO_AOCoefficient].InitAsDescriptorTable(1, &texTables[2]);

		CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(
			_countof(slotRootParameter), slotRootParameter,
			0, nullptr,
			D3D12_ROOT_SIGNATURE_FLAG_NONE
		);
		CheckReturn(D3D12Util::CreateRootSignature(md3dDevice, globalRootSignatureDesc, mRootSignatures[DisocclusionBlurRS].GetAddressOf()));
	}
	return true;
}

bool RtaoClass::BuildPSO() {
	//
	// Pipeline States
	//
	D3D12_COMPUTE_PIPELINE_STATE_DESC tsppReprojPsoDesc = {};
	tsppReprojPsoDesc.pRootSignature = mRootSignatures[TsppReprojRS].Get();
	{
		auto cs = mShaderManager->GetDxcShader(TsppReprojCS);
		tsppReprojPsoDesc.CS = { reinterpret_cast<BYTE*>(cs->GetBufferPointer()), cs->GetBufferSize() };
	}
	tsppReprojPsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	CheckHRESULT(md3dDevice->CreateComputePipelineState(&tsppReprojPsoDesc, IID_PPV_ARGS(&mPsos[TsppReprojPSO])));

	D3D12_COMPUTE_PIPELINE_STATE_DESC tsppBlendPsoDesc = {};
	tsppBlendPsoDesc.pRootSignature = mRootSignatures[TsppBlendRS].Get();
	{
		auto cs = mShaderManager->GetDxcShader(TsppBlendCS);
		tsppBlendPsoDesc.CS = { reinterpret_cast<BYTE*>(cs->GetBufferPointer()), cs->GetBufferSize() };
	}
	tsppBlendPsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	CheckHRESULT(md3dDevice->CreateComputePipelineState(&tsppBlendPsoDesc, IID_PPV_ARGS(&mPsos[TsppBlendPSO])));

	D3D12_COMPUTE_PIPELINE_STATE_DESC calcPartialDerivativePsoDesc = {};
	calcPartialDerivativePsoDesc.pRootSignature = mRootSignatures[PartialDepthDerivativeRS].Get();
	{
		auto cs = mShaderManager->GetDxcShader(PartialDerivativeCS);
		calcPartialDerivativePsoDesc.CS = { reinterpret_cast<BYTE*>(cs->GetBufferPointer()), cs->GetBufferSize() };
	}
	calcPartialDerivativePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	CheckHRESULT(md3dDevice->CreateComputePipelineState(&calcPartialDerivativePsoDesc, IID_PPV_ARGS(&mPsos[PartialDepthDerivativePSO])));

	D3D12_COMPUTE_PIPELINE_STATE_DESC calcLocalMeanVariancePsoDesc = {};
	calcLocalMeanVariancePsoDesc.pRootSignature = mRootSignatures[LocalMeanVarianceRS].Get();
	{
		auto cs = mShaderManager->GetDxcShader(CalcLocalMeanVarianceCS);
		calcLocalMeanVariancePsoDesc.CS = { reinterpret_cast<BYTE*>(cs->GetBufferPointer()), cs->GetBufferSize() };
	}
	calcLocalMeanVariancePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	CheckHRESULT(md3dDevice->CreateComputePipelineState(&calcLocalMeanVariancePsoDesc, IID_PPV_ARGS(&mPsos[LocalMeanVariancePSO])));

	D3D12_COMPUTE_PIPELINE_STATE_DESC fillInCheckerboardPsoDesc = {};
	fillInCheckerboardPsoDesc.pRootSignature = mRootSignatures[FillInCheckboardRS].Get();
	{
		auto cs = mShaderManager->GetDxcShader(FillInCheckerboardCS);
		fillInCheckerboardPsoDesc.CS = { reinterpret_cast<BYTE*>(cs->GetBufferPointer()), cs->GetBufferSize() };
	}
	fillInCheckerboardPsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	CheckHRESULT(md3dDevice->CreateComputePipelineState(&fillInCheckerboardPsoDesc, IID_PPV_ARGS(&mPsos[FillInCheckerboardPSO])));

	D3D12_COMPUTE_PIPELINE_STATE_DESC atrousWaveletTransformFilterPsoDesc = {};
	atrousWaveletTransformFilterPsoDesc.pRootSignature = mRootSignatures[AtrousWaveletTransformFilterRS].Get();
	{
		auto cs = mShaderManager->GetDxcShader(EdgeStoppingFilter_Gaussian3x3CS);
		atrousWaveletTransformFilterPsoDesc.CS = { reinterpret_cast<BYTE*>(cs->GetBufferPointer()), cs->GetBufferSize() };
	}
	atrousWaveletTransformFilterPsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	CheckHRESULT(md3dDevice->CreateComputePipelineState(&atrousWaveletTransformFilterPsoDesc, IID_PPV_ARGS(&mPsos[AtrousWaveletTransformFilterPSO])));

	D3D12_COMPUTE_PIPELINE_STATE_DESC disocclusionBlurPsoDesc = {};
	disocclusionBlurPsoDesc.pRootSignature = mRootSignatures[DisocclusionBlurRS].Get();
	{
		auto cs = mShaderManager->GetDxcShader(DisocclusionBlur3x3CS);
		disocclusionBlurPsoDesc.CS = { reinterpret_cast<BYTE*>(cs->GetBufferPointer()), cs->GetBufferSize() };
	}
	disocclusionBlurPsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	CheckHRESULT(md3dDevice->CreateComputePipelineState(&disocclusionBlurPsoDesc, IID_PPV_ARGS(&mPsos[DisocclusionBlurPSO])));

	//
	// State Object
	//
	CD3DX12_STATE_OBJECT_DESC rtaoDxrPso = { D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

	auto rtaoLib = rtaoDxrPso.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
	auto rtaoShader = mShaderManager->GetDxcShader("rtao");
	D3D12_SHADER_BYTECODE rtaoLibDxil = CD3DX12_SHADER_BYTECODE(rtaoShader->GetBufferPointer(), rtaoShader->GetBufferSize());
	rtaoLib->SetDXILLibrary(&rtaoLibDxil);
	LPCWSTR rtaoExports[] = { L"RtaoRayGen", L"RtaoClosestHit", L"RtaoMiss" };
	rtaoLib->DefineExports(rtaoExports);

	auto rtaoHitGroup = rtaoDxrPso.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
	rtaoHitGroup->SetClosestHitShaderImport(L"RtaoClosestHit");
	rtaoHitGroup->SetHitGroupExport(L"RtaoHitGroup");
	rtaoHitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

	auto shaderConfig = rtaoDxrPso.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
	UINT payloadSize = sizeof(float) /* tHit */;
	UINT attribSize = sizeof(XMFLOAT2);
	shaderConfig->Config(payloadSize, attribSize);

	auto glbalRootSig = rtaoDxrPso.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
	glbalRootSig->SetRootSignature(mRootSignatures[RtaoRS].Get());

	auto pipelineConfig = rtaoDxrPso.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
	UINT maxRecursionDepth = 1;
	pipelineConfig->Config(maxRecursionDepth);

	CheckHRESULT(md3dDevice->CreateStateObject(rtaoDxrPso, IID_PPV_ARGS(&mDxrPso)));
	CheckHRESULT(mDxrPso->QueryInterface(IID_PPV_ARGS(&mDxrPsoProp)));

	return true;
}

bool RtaoClass::BuildShaderTables() {
	UINT shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

	void* rtaoRayGenShaderIdentifier = mDxrPsoProp->GetShaderIdentifier(L"RtaoRayGen");
	void* rtaoMissShaderIdentifier = mDxrPsoProp->GetShaderIdentifier(L"RtaoMiss");
	void* rtaoHitGroupShaderIdentifier = mDxrPsoProp->GetShaderIdentifier(L"RtaoHitGroup");

	ShaderTable rtaoRayGenShaderTable(md3dDevice, 1, shaderIdentifierSize);
	CheckReturn(rtaoRayGenShaderTable.Initialze());
	rtaoRayGenShaderTable.push_back(ShaderRecord(rtaoRayGenShaderIdentifier, shaderIdentifierSize));
	mShaderTables["rtaoRayGen"] = rtaoRayGenShaderTable.GetResource();

	ShaderTable rtaoMissShaderTable(md3dDevice, 1, shaderIdentifierSize);
	CheckReturn(rtaoMissShaderTable.Initialze());
	rtaoMissShaderTable.push_back(ShaderRecord(rtaoMissShaderIdentifier, shaderIdentifierSize));
	mShaderTables["rtaoMiss"] = rtaoMissShaderTable.GetResource();

	ShaderTable rtaoHitGroupTable(md3dDevice, 1, shaderIdentifierSize);
	CheckReturn(rtaoHitGroupTable.Initialze());
	rtaoHitGroupTable.push_back(ShaderRecord(rtaoHitGroupShaderIdentifier, shaderIdentifierSize));
	mShaderTables["rtaoHitGroup"] = rtaoHitGroupTable.GetResource();

	return true;
}

void RtaoClass::RunCalculatingAmbientOcclusion(
	ID3D12GraphicsCommandList4* const cmdList,
	D3D12_GPU_VIRTUAL_ADDRESS accelStruct,
	D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
	D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
	D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
	D3D12_GPU_DESCRIPTOR_HANDLE uo_aoCoefficient,
	D3D12_GPU_DESCRIPTOR_HANDLE uo_rayHitDistance) {
	cmdList->SetPipelineState1(mDxrPso.Get());
	cmdList->SetComputeRootSignature(mRootSignatures[RtaoRS].Get());

	cmdList->SetComputeRootShaderResourceView(CalcAmbientOcclusion::RootSignatureLayout::ESI_AccelerationStructure, accelStruct);
	cmdList->SetComputeRootConstantBufferView(CalcAmbientOcclusion::RootSignatureLayout::ECB_RtaoPass, cbAddress);

	const UINT values[CalcAmbientOcclusion::RootConstantsLayout::Count] = { mWidth, mHeight };
	cmdList->SetComputeRoot32BitConstants(CalcAmbientOcclusion::RootSignatureLayout::EC_Consts, _countof(values), values, 0);

	cmdList->SetComputeRootDescriptorTable(CalcAmbientOcclusion::RootSignatureLayout::ESI_Normal, si_normal);
	cmdList->SetComputeRootDescriptorTable(CalcAmbientOcclusion::RootSignatureLayout::ESI_Depth, si_depth);
	cmdList->SetComputeRootDescriptorTable(CalcAmbientOcclusion::RootSignatureLayout::EUO_AOCoefficient, uo_aoCoefficient);
	cmdList->SetComputeRootDescriptorTable(CalcAmbientOcclusion::RootSignatureLayout::EUO_RayHitDistance, uo_rayHitDistance);

	D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
	const auto& rayGen = mShaderTables["rtaoRayGen"];
	const auto& miss = mShaderTables["rtaoMiss"];
	const auto& hitGroup = mShaderTables["rtaoHitGroup"];
	dispatchDesc.RayGenerationShaderRecord.StartAddress = rayGen->GetGPUVirtualAddress();
	dispatchDesc.RayGenerationShaderRecord.SizeInBytes = rayGen->GetDesc().Width;
	dispatchDesc.MissShaderTable.StartAddress = miss->GetGPUVirtualAddress();
	dispatchDesc.MissShaderTable.SizeInBytes = miss->GetDesc().Width;
	dispatchDesc.MissShaderTable.StrideInBytes = dispatchDesc.MissShaderTable.SizeInBytes;
	dispatchDesc.HitGroupTable.StartAddress = hitGroup->GetGPUVirtualAddress();
	dispatchDesc.HitGroupTable.SizeInBytes = hitGroup->GetDesc().Width;
	dispatchDesc.HitGroupTable.StrideInBytes = dispatchDesc.HitGroupTable.SizeInBytes;
	dispatchDesc.Width = mWidth;
	dispatchDesc.Height = mHeight;
	dispatchDesc.Depth = 1;
	cmdList->DispatchRays(&dispatchDesc);
}

void RtaoClass::RunCalculatingDepthPartialDerivative(
	ID3D12GraphicsCommandList4* const cmdList,
	D3D12_GPU_DESCRIPTOR_HANDLE i_depth,
	D3D12_GPU_DESCRIPTOR_HANDLE o_depthPartialDerivative,
	UINT width, UINT height) {
	cmdList->SetPipelineState(mPsos[PartialDepthDerivativePSO].Get());
	cmdList->SetComputeRootSignature(mRootSignatures[PartialDepthDerivativeRS].Get());

	const float values[Rtao::CalcDepthPartialDerivative::RootConstantsLayout::Count] = { 1.0f / width, 1.0f / height };
	cmdList->SetComputeRoot32BitConstants(CalcDepthPartialDerivative::RootSignatureLayout::EC_Consts, CalcDepthPartialDerivative::RootConstantsLayout::Count, values, 0);

	cmdList->SetComputeRootDescriptorTable(CalcDepthPartialDerivative::RootSignatureLayout::ESI_Depth, i_depth);
	cmdList->SetComputeRootDescriptorTable(CalcDepthPartialDerivative::RootSignatureLayout::EUO_DepthPartialDerivative, o_depthPartialDerivative);

	cmdList->Dispatch(
		D3D12Util::CeilDivide(width, Default::ThreadGroup::Width),
		D3D12Util::CeilDivide(height, Default::ThreadGroup::Height), 1);
}

void RtaoClass::RunCalculatingLocalMeanVariance(
	ID3D12GraphicsCommandList4* const cmdList,
	D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
	D3D12_GPU_DESCRIPTOR_HANDLE si_aoCoefficient,
	D3D12_GPU_DESCRIPTOR_HANDLE uo_localMeanVariance,
	UINT width, UINT height,
	bool checkerboardSamplingEnabled) {
	cmdList->SetPipelineState(mPsos[LocalMeanVariancePSO].Get());
	cmdList->SetComputeRootSignature(mRootSignatures[LocalMeanVarianceRS].Get());

	cmdList->SetComputeRootConstantBufferView(CalcLocalMeanVariance::RootSignatureLayout::ECB_LocalMeanVar, cbAddress);
	cmdList->SetComputeRootDescriptorTable(CalcLocalMeanVariance::RootSignatureLayout::ESI_AOCoefficient, si_aoCoefficient);
	cmdList->SetComputeRootDescriptorTable(CalcLocalMeanVariance::RootSignatureLayout::EUO_LocalMeanVar, uo_localMeanVariance);

	int pixelStepY = checkerboardSamplingEnabled ? 2 : 1;
	cmdList->Dispatch(
		D3D12Util::CeilDivide(width, Default::ThreadGroup::Width),
		D3D12Util::CeilDivide(height, Default::ThreadGroup::Height * pixelStepY), 1);
}

void RtaoClass::FillInCheckerboard(
	ID3D12GraphicsCommandList4* const cmdList,
	D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
	D3D12_GPU_DESCRIPTOR_HANDLE uio_localMeanVariance) {
	cmdList->SetPipelineState(mPsos[FillInCheckerboardPSO].Get());
	cmdList->SetComputeRootSignature(mRootSignatures[FillInCheckboardRS].Get());

	cmdList->SetComputeRootConstantBufferView(FillInCheckerboard::RootSignatureLayout::ECB_LocalMeanVar, cbAddress);
	cmdList->SetComputeRootDescriptorTable(FillInCheckerboard::RootSignatureLayout::EUIO_LocalMeanVar, uio_localMeanVariance);

	cmdList->Dispatch(
		D3D12Util::CeilDivide(mWidth, Default::ThreadGroup::Width),
		D3D12Util::CeilDivide(mHeight, Default::ThreadGroup::Height * 2), 1);
}

void RtaoClass::ReverseReprojectPreviousFrame(
	ID3D12GraphicsCommandList4* const cmdList,
	D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
	D3D12_GPU_DESCRIPTOR_HANDLE si_normalDepth,
	D3D12_GPU_DESCRIPTOR_HANDLE si_depthPartialDerivative,
	D3D12_GPU_DESCRIPTOR_HANDLE si_reprojNormalDepth,
	D3D12_GPU_DESCRIPTOR_HANDLE si_cachedNormalDepth,
	D3D12_GPU_DESCRIPTOR_HANDLE si_velocity,
	D3D12_GPU_DESCRIPTOR_HANDLE si_cachedAOCoefficient,
	D3D12_GPU_DESCRIPTOR_HANDLE si_cachedTspp,
	D3D12_GPU_DESCRIPTOR_HANDLE si_cachedAOCoefficientSquaredMean,
	D3D12_GPU_DESCRIPTOR_HANDLE si_cachedRayHitDistance,
	D3D12_GPU_DESCRIPTOR_HANDLE uo_cachedTspp,
	D3D12_GPU_DESCRIPTOR_HANDLE uo_tsppCoefficientSquaredMeanRayHitDistance) {
	cmdList->SetPipelineState(mPsos[TsppReprojPSO].Get());
	cmdList->SetComputeRootSignature(mRootSignatures[TsppReprojRS].Get());

	cmdList->SetComputeRootConstantBufferView(TemporalSupersamplingReverseReproject::RootSignatureLayout::ECB_CrossBilateralFilter, cbAddress);
	cmdList->SetComputeRootDescriptorTable(TemporalSupersamplingReverseReproject::RootSignatureLayout::ESI_NormalDepth, si_normalDepth);
	cmdList->SetComputeRootDescriptorTable(TemporalSupersamplingReverseReproject::RootSignatureLayout::ESI_DepthPartialDerivative, si_depthPartialDerivative);
	cmdList->SetComputeRootDescriptorTable(TemporalSupersamplingReverseReproject::RootSignatureLayout::ESI_ReprojectedNormalDepth, si_reprojNormalDepth);
	cmdList->SetComputeRootDescriptorTable(TemporalSupersamplingReverseReproject::RootSignatureLayout::ESI_CachedNormalDepth, si_cachedNormalDepth);
	cmdList->SetComputeRootDescriptorTable(TemporalSupersamplingReverseReproject::RootSignatureLayout::ESI_Velocity, si_velocity);
	cmdList->SetComputeRootDescriptorTable(TemporalSupersamplingReverseReproject::RootSignatureLayout::ESI_CachedAOCoefficient, si_cachedAOCoefficient);
	cmdList->SetComputeRootDescriptorTable(TemporalSupersamplingReverseReproject::RootSignatureLayout::ESI_CachedTspp, si_cachedTspp);
	cmdList->SetComputeRootDescriptorTable(TemporalSupersamplingReverseReproject::RootSignatureLayout::ESI_CachedAOCoefficientSquaredMean, si_cachedAOCoefficientSquaredMean);
	cmdList->SetComputeRootDescriptorTable(TemporalSupersamplingReverseReproject::RootSignatureLayout::ESI_CachedRayHitDistance, si_cachedRayHitDistance);
	cmdList->SetComputeRootDescriptorTable(TemporalSupersamplingReverseReproject::RootSignatureLayout::EUO_CachedTspp, uo_cachedTspp);
	cmdList->SetComputeRootDescriptorTable(TemporalSupersamplingReverseReproject::RootSignatureLayout::EUO_TsppCoefficientSquaredMeanRayHitDistacne, uo_tsppCoefficientSquaredMeanRayHitDistance);

	{
		UINT values[] = { mWidth, mHeight };
		cmdList->SetComputeRoot32BitConstants(
			TemporalSupersamplingReverseReproject::RootSignatureLayout::EC_Consts,
			_countof(values), values,
			TemporalSupersamplingReverseReproject::RootConstantsLayout::ETextureDim_X
		);
	}
	{
		float values[] = { 1.0f / mWidth, 1.0f / mHeight };
		cmdList->SetComputeRoot32BitConstants(
			TemporalSupersamplingReverseReproject::RootSignatureLayout::EC_Consts,
			_countof(values), values,
			TemporalSupersamplingReverseReproject::RootConstantsLayout::EInvTextureDim_X
		);
	}

	cmdList->Dispatch(
		D3D12Util::CeilDivide(mWidth, Default::ThreadGroup::Width),
		D3D12Util::CeilDivide(mHeight, Default::ThreadGroup::Height), 1);
}

void RtaoClass::BlendWithCurrentFrame(
	ID3D12GraphicsCommandList4* const cmdList,
	D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
	D3D12_GPU_DESCRIPTOR_HANDLE si_aoCoefficient,
	D3D12_GPU_DESCRIPTOR_HANDLE si_localMeanVariance,
	D3D12_GPU_DESCRIPTOR_HANDLE si_rayHitDistance,
	D3D12_GPU_DESCRIPTOR_HANDLE si_tsppCoefficientSquaredMeanRayHitDistance,
	D3D12_GPU_DESCRIPTOR_HANDLE uio_temporalAOCoefficient,
	D3D12_GPU_DESCRIPTOR_HANDLE uio_tspp,
	D3D12_GPU_DESCRIPTOR_HANDLE uio_coefficientSquaredMean,
	D3D12_GPU_DESCRIPTOR_HANDLE uio_rayHitDistance,
	D3D12_GPU_DESCRIPTOR_HANDLE uo_variance,
	D3D12_GPU_DESCRIPTOR_HANDLE uo_blurStrength) {
	cmdList->SetPipelineState(mPsos[TsppBlendPSO].Get());
	cmdList->SetComputeRootSignature(mRootSignatures[TsppBlendRS].Get());

	cmdList->SetComputeRootConstantBufferView(TemporalSupersamplingBlendWithCurrentFrame::RootSignatureLayout::ECB_TsspBlendWithCurrentFrame, cbAddress);
	cmdList->SetComputeRootDescriptorTable(TemporalSupersamplingBlendWithCurrentFrame::RootSignatureLayout::ESI_AOCoefficient, si_aoCoefficient);
	cmdList->SetComputeRootDescriptorTable(TemporalSupersamplingBlendWithCurrentFrame::RootSignatureLayout::ESI_LocalMeanVaraince, si_localMeanVariance);
	cmdList->SetComputeRootDescriptorTable(TemporalSupersamplingBlendWithCurrentFrame::RootSignatureLayout::ESI_RayHitDistance, si_rayHitDistance);
	cmdList->SetComputeRootDescriptorTable(TemporalSupersamplingBlendWithCurrentFrame::RootSignatureLayout::ESI_TsppCoefficientSquaredMeanRayHitDistance, si_tsppCoefficientSquaredMeanRayHitDistance);
	cmdList->SetComputeRootDescriptorTable(TemporalSupersamplingBlendWithCurrentFrame::RootSignatureLayout::EUIO_TemporalAOCoefficient, uio_temporalAOCoefficient);
	cmdList->SetComputeRootDescriptorTable(TemporalSupersamplingBlendWithCurrentFrame::RootSignatureLayout::EUIO_Tspp, uio_tspp);
	cmdList->SetComputeRootDescriptorTable(TemporalSupersamplingBlendWithCurrentFrame::RootSignatureLayout::EUIO_CoefficientSquaredMean, uio_coefficientSquaredMean);
	cmdList->SetComputeRootDescriptorTable(TemporalSupersamplingBlendWithCurrentFrame::RootSignatureLayout::EUIO_RayHitDistance, uio_rayHitDistance);
	cmdList->SetComputeRootDescriptorTable(TemporalSupersamplingBlendWithCurrentFrame::RootSignatureLayout::EUO_VarianceMap, uo_variance);
	cmdList->SetComputeRootDescriptorTable(TemporalSupersamplingBlendWithCurrentFrame::RootSignatureLayout::EUO_BlurStrength, uo_blurStrength);

	cmdList->Dispatch(
		D3D12Util::CeilDivide(mWidth, Default::ThreadGroup::Width),
		D3D12Util::CeilDivide(mHeight, Default::ThreadGroup::Height), 1);
}

void RtaoClass::ApplyAtrousWaveletTransformFilter(
	ID3D12GraphicsCommandList4* const cmdList,
	D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
	D3D12_GPU_DESCRIPTOR_HANDLE si_temporalAOCoefficient,
	D3D12_GPU_DESCRIPTOR_HANDLE si_normalDepth,
	D3D12_GPU_DESCRIPTOR_HANDLE si_variance,
	D3D12_GPU_DESCRIPTOR_HANDLE si_hitDistance,
	D3D12_GPU_DESCRIPTOR_HANDLE si_depthPartialDerivative,
	D3D12_GPU_DESCRIPTOR_HANDLE si_tspp,
	D3D12_GPU_DESCRIPTOR_HANDLE uo_temporalAOCoefficient) {
	cmdList->SetPipelineState(mPsos[AtrousWaveletTransformFilterPSO].Get());
	cmdList->SetComputeRootSignature(mRootSignatures[AtrousWaveletTransformFilterRS].Get());

	cmdList->SetComputeRootConstantBufferView(AtrousWaveletTransformFilter::RootSignatureLayout::ECB_AtrousFilter, cbAddress);
	cmdList->SetComputeRootDescriptorTable(AtrousWaveletTransformFilter::RootSignatureLayout::ESI_TemporalAOCoefficient, si_temporalAOCoefficient);
	cmdList->SetComputeRootDescriptorTable(AtrousWaveletTransformFilter::RootSignatureLayout::ESI_NormalDepth, si_normalDepth);
	cmdList->SetComputeRootDescriptorTable(AtrousWaveletTransformFilter::RootSignatureLayout::ESI_Variance, si_variance);
	cmdList->SetComputeRootDescriptorTable(AtrousWaveletTransformFilter::RootSignatureLayout::ESI_HitDistance, si_hitDistance);
	cmdList->SetComputeRootDescriptorTable(AtrousWaveletTransformFilter::RootSignatureLayout::ESI_DepthPartialDerivative, si_depthPartialDerivative);
	cmdList->SetComputeRootDescriptorTable(AtrousWaveletTransformFilter::RootSignatureLayout::ESI_Tspp, si_tspp);
	cmdList->SetComputeRootDescriptorTable(AtrousWaveletTransformFilter::RootSignatureLayout::EUO_TemporalAOCoefficient, uo_temporalAOCoefficient);

	cmdList->Dispatch(
		D3D12Util::CeilDivide(mWidth, Default::ThreadGroup::Width),
		D3D12Util::CeilDivide(mHeight, Default::ThreadGroup::Height), 1);
}

void RtaoClass::BlurDisocclusion(
	ID3D12GraphicsCommandList4* const cmdList,
	ID3D12Resource* aoCoefficient,
	D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
	D3D12_GPU_DESCRIPTOR_HANDLE si_blurStrength,
	D3D12_GPU_DESCRIPTOR_HANDLE uio_aoCoefficient,
	UINT width, UINT height,
	UINT lowTsppBlurPasses) {
	cmdList->SetPipelineState(mPsos[DisocclusionBlurPSO].Get());
	cmdList->SetComputeRootSignature(mRootSignatures[DisocclusionBlurRS].Get());

	UINT values[2] = { width, height };
	cmdList->SetComputeRoot32BitConstants(DisocclusionBlur::RootSignatureLayout::EC_Consts, _countof(values), values, 0);

	cmdList->SetComputeRootDescriptorTable(DisocclusionBlur::RootSignatureLayout::ESI_Depth, si_depth);
	cmdList->SetComputeRootDescriptorTable(DisocclusionBlur::RootSignatureLayout::ESI_BlurStrength, si_blurStrength);
	cmdList->SetComputeRootDescriptorTable(DisocclusionBlur::RootSignatureLayout::EUIO_AOCoefficient, uio_aoCoefficient);

	UINT filterStep = 1;
	for (UINT i = 0; i < lowTsppBlurPasses; ++i) {
		cmdList->SetComputeRoot32BitConstant(DisocclusionBlur::RootSignatureLayout::EC_Consts, filterStep, DisocclusionBlur::RootConstantsLayout::EStep);

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

UINT RtaoClass::MoveToNextFrame() {
	mTemporalCurrentFrameResourceIndex = (mTemporalCurrentFrameResourceIndex + 1) % 2;
	return mTemporalCurrentFrameResourceIndex;
}

UINT RtaoClass::MoveToNextFrameTemporalAOCoefficient() {
	mTemporalCurrentFrameTemporalAOCeofficientResourceIndex = (mTemporalCurrentFrameTemporalAOCeofficientResourceIndex + 1) % 2;
	return mTemporalCurrentFrameTemporalAOCeofficientResourceIndex;
}

void RtaoClass::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu, CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu, UINT descSize) {
	mhAOResourcesCpus[AO::Descriptors::ES_AmbientCoefficient] = hCpu;
	mhAOResourcesGpus[AO::Descriptors::ES_AmbientCoefficient] = hGpu;
	mhAOResourcesCpus[AO::Descriptors::EU_AmbientCoefficient] = hCpu.Offset(1, descSize);
	mhAOResourcesGpus[AO::Descriptors::EU_AmbientCoefficient] = hGpu.Offset(1, descSize);
	mhAOResourcesCpus[AO::Descriptors::ES_RayHitDistance] = hCpu.Offset(1, descSize);
	mhAOResourcesGpus[AO::Descriptors::ES_RayHitDistance] = hGpu.Offset(1, descSize);
	mhAOResourcesCpus[AO::Descriptors::EU_RayHitDistance] = hCpu.Offset(1, descSize);
	mhAOResourcesGpus[AO::Descriptors::EU_RayHitDistance] = hGpu.Offset(1, descSize);

	mhLocalMeanVarianceResourcesCpus[LocalMeanVariance::Descriptors::ES_Raw] = hCpu.Offset(1, descSize);
	mhLocalMeanVarianceResourcesGpus[LocalMeanVariance::Descriptors::ES_Raw] = hGpu.Offset(1, descSize);
	mhLocalMeanVarianceResourcesCpus[LocalMeanVariance::Descriptors::EU_Raw] = hCpu.Offset(1, descSize);
	mhLocalMeanVarianceResourcesGpus[LocalMeanVariance::Descriptors::EU_Raw] = hGpu.Offset(1, descSize);
	mhLocalMeanVarianceResourcesCpus[LocalMeanVariance::Descriptors::ES_Smoothed] = hCpu.Offset(1, descSize);
	mhLocalMeanVarianceResourcesGpus[LocalMeanVariance::Descriptors::ES_Smoothed] = hGpu.Offset(1, descSize);
	mhLocalMeanVarianceResourcesCpus[LocalMeanVariance::Descriptors::EU_Smoothed] = hCpu.Offset(1, descSize);
	mhLocalMeanVarianceResourcesGpus[LocalMeanVariance::Descriptors::EU_Smoothed] = hGpu.Offset(1, descSize);

	mhAOVarianceResourcesCpus[AOVariance::Descriptors::ES_Raw] = hCpu.Offset(1, descSize);
	mhAOVarianceResourcesGpus[AOVariance::Descriptors::ES_Raw] = hGpu.Offset(1, descSize);
	mhAOVarianceResourcesCpus[AOVariance::Descriptors::EU_Raw] = hCpu.Offset(1, descSize);
	mhAOVarianceResourcesGpus[AOVariance::Descriptors::EU_Raw] = hGpu.Offset(1, descSize);
	mhAOVarianceResourcesCpus[AOVariance::Descriptors::ES_Smoothed] = hCpu.Offset(1, descSize);
	mhAOVarianceResourcesGpus[AOVariance::Descriptors::ES_Smoothed] = hGpu.Offset(1, descSize);
	mhAOVarianceResourcesCpus[AOVariance::Descriptors::EU_Smoothed] = hCpu.Offset(1, descSize);
	mhAOVarianceResourcesGpus[AOVariance::Descriptors::EU_Smoothed] = hGpu.Offset(1, descSize);

	for (size_t i = 0; i < 2; ++i) {
		mhTemporalCachesCpus[i][TemporalCache::Descriptors::ES_Tspp] = hCpu.Offset(1, descSize);
		mhTemporalCachesGpus[i][TemporalCache::Descriptors::ES_Tspp] = hGpu.Offset(1, descSize);
		mhTemporalCachesCpus[i][TemporalCache::Descriptors::EU_Tspp] = hCpu.Offset(1, descSize);
		mhTemporalCachesGpus[i][TemporalCache::Descriptors::EU_Tspp] = hGpu.Offset(1, descSize);

		mhTemporalCachesCpus[i][TemporalCache::Descriptors::ES_RayHitDistance] = hCpu.Offset(1, descSize);
		mhTemporalCachesGpus[i][TemporalCache::Descriptors::ES_RayHitDistance] = hGpu.Offset(1, descSize);
		mhTemporalCachesCpus[i][TemporalCache::Descriptors::EU_RayHitDistance] = hCpu.Offset(1, descSize);
		mhTemporalCachesGpus[i][TemporalCache::Descriptors::EU_RayHitDistance] = hGpu.Offset(1, descSize);

		mhTemporalCachesCpus[i][TemporalCache::Descriptors::ES_CoefficientSquaredMean] = hCpu.Offset(1, descSize);
		mhTemporalCachesGpus[i][TemporalCache::Descriptors::ES_CoefficientSquaredMean] = hGpu.Offset(1, descSize);
		mhTemporalCachesCpus[i][TemporalCache::Descriptors::EU_CoefficientSquaredMean] = hCpu.Offset(1, descSize);
		mhTemporalCachesGpus[i][TemporalCache::Descriptors::EU_CoefficientSquaredMean] = hGpu.Offset(1, descSize);
	}

	mhPrevFrameNormalDepthCpuSrv = hCpu.Offset(1, descSize);
	mhPrevFrameNormalDepthGpuSrv = hGpu.Offset(1, descSize);

	mhTsppCoefficientSquaredMeanRayHitDistanceCpuSrv = hCpu.Offset(1, descSize);
	mhTsppCoefficientSquaredMeanRayHitDistanceGpuSrv = hGpu.Offset(1, descSize);
	mhTsppCoefficientSquaredMeanRayHitDistanceCpuUav = hCpu.Offset(1, descSize);
	mhTsppCoefficientSquaredMeanRayHitDistanceGpuUav = hGpu.Offset(1, descSize);

	mhDisocclusionBlurStrengthCpuSrv = hCpu.Offset(1, descSize);
	mhDisocclusionBlurStrengthGpuSrv = hGpu.Offset(1, descSize);
	mhDisocclusionBlurStrengthCpuUav = hCpu.Offset(1, descSize);
	mhDisocclusionBlurStrengthGpuUav = hGpu.Offset(1, descSize);

	for (size_t i = 0; i < 2; ++i) {
		mhTemporalAOCoefficientsCpus[i][TemporalAOCoefficient::Descriptors::Srv] = hCpu.Offset(1, descSize);
		mhTemporalAOCoefficientsGpus[i][TemporalAOCoefficient::Descriptors::Srv] = hGpu.Offset(1, descSize);
		mhTemporalAOCoefficientsCpus[i][TemporalAOCoefficient::Descriptors::Uav] = hCpu.Offset(1, descSize);
		mhTemporalAOCoefficientsGpus[i][TemporalAOCoefficient::Descriptors::Uav] = hGpu.Offset(1, descSize);
	}

	mhDepthPartialDerivativeCpuSrv = hCpu.Offset(1, descSize);
	mhDepthPartialDerivativeGpuSrv = hGpu.Offset(1, descSize);
	mhDepthPartialDerivativeCpuUav = hCpu.Offset(1, descSize);
	mhDepthPartialDerivativeGpuUav = hGpu.Offset(1, descSize);

	BuildDescriptors();

	hCpu.Offset(1, descSize);
	hGpu.Offset(1, descSize);
}

bool RtaoClass::OnResize(ID3D12GraphicsCommandList* cmdList, UINT width, UINT height) {
	if ((mWidth != width) || (mHeight != height)) {
		mWidth = width;
		mHeight = height;

		CheckReturn(BuildResources(cmdList));
		BuildDescriptors();
	}

	return true;
}

void RtaoClass::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

	{
		srvDesc.Format = AOCoefficientMapFormat;
		uavDesc.Format = AOCoefficientMapFormat;

		auto resource = mAOResources[AO::Resources::EAmbientCoefficient]->Resource();
		md3dDevice->CreateShaderResourceView(resource, &srvDesc, mhAOResourcesCpus[AO::Descriptors::ES_AmbientCoefficient]);
		md3dDevice->CreateUnorderedAccessView(resource, nullptr, &uavDesc, mhAOResourcesCpus[AO::Descriptors::EU_AmbientCoefficient]);
	}
	{
		srvDesc.Format = RayHitDistanceFormat;
		uavDesc.Format = RayHitDistanceFormat;

		auto resource = mAOResources[AO::Resources::ERayHitDistance]->Resource();
		md3dDevice->CreateShaderResourceView(resource, &srvDesc, mhAOResourcesCpus[AO::Descriptors::ES_RayHitDistance]);
		md3dDevice->CreateUnorderedAccessView(resource, nullptr, &uavDesc, mhAOResourcesCpus[AO::Descriptors::EU_RayHitDistance]);
	}

	{
		srvDesc.Format = TsppMapFormat;
		uavDesc.Format = TsppMapFormat;
		for (size_t i = 0; i < 2; ++i) {
			auto resource = mTemporalCaches[i][TemporalCache::Resources::ETspp]->Resource();
			auto& cpus = mhTemporalCachesCpus[i];
			md3dDevice->CreateShaderResourceView(resource, &srvDesc, cpus[TemporalCache::Descriptors::ES_Tspp]);
			md3dDevice->CreateUnorderedAccessView(resource, nullptr, &uavDesc, cpus[TemporalCache::Descriptors::EU_Tspp]);
		}
	}
	{
		srvDesc.Format = RayHitDistanceFormat;
		uavDesc.Format = RayHitDistanceFormat;
		for (size_t i = 0; i < 2; ++i) {
			auto resource = mTemporalCaches[i][TemporalCache::Resources::ERayHitDistance]->Resource();
			auto& cpus = mhTemporalCachesCpus[i];
			md3dDevice->CreateShaderResourceView(resource, &srvDesc, cpus[TemporalCache::Descriptors::ES_RayHitDistance]);
			md3dDevice->CreateUnorderedAccessView(resource, nullptr, &uavDesc, cpus[TemporalCache::Descriptors::EU_RayHitDistance]);
		}
	}
	{
		srvDesc.Format = CoefficientSquaredMeanMapFormat;
		uavDesc.Format = CoefficientSquaredMeanMapFormat;
		for (size_t i = 0; i < 2; ++i) {
			auto resource = mTemporalCaches[i][TemporalCache::Resources::ECoefficientSquaredMean]->Resource();
			auto& cpus = mhTemporalCachesCpus[i];
			md3dDevice->CreateShaderResourceView(resource, &srvDesc, cpus[TemporalCache::Descriptors::ES_CoefficientSquaredMean]);
			md3dDevice->CreateUnorderedAccessView(resource, nullptr, &uavDesc, cpus[TemporalCache::Descriptors::EU_CoefficientSquaredMean]);
		}
	}
	{
		srvDesc.Format = LocalMeanVarianceMapFormat;
		uavDesc.Format = LocalMeanVarianceMapFormat;
		auto rawResource = mLocalMeanVarianceResources[LocalMeanVariance::Resources::ERaw]->Resource();
		md3dDevice->CreateShaderResourceView(rawResource, &srvDesc, mhLocalMeanVarianceResourcesCpus[LocalMeanVariance::Descriptors::ES_Raw]);
		md3dDevice->CreateUnorderedAccessView(rawResource, nullptr, &uavDesc, mhLocalMeanVarianceResourcesCpus[LocalMeanVariance::Descriptors::EU_Raw]);

		auto smoothedResource = mLocalMeanVarianceResources[LocalMeanVariance::Resources::ESmoothed]->Resource();
		md3dDevice->CreateShaderResourceView(smoothedResource, &srvDesc, mhLocalMeanVarianceResourcesCpus[LocalMeanVariance::Descriptors::ES_Smoothed]);
		md3dDevice->CreateUnorderedAccessView(smoothedResource, nullptr, &uavDesc, mhLocalMeanVarianceResourcesCpus[LocalMeanVariance::Descriptors::EU_Smoothed]);
	}
	{
		srvDesc.Format = VarianceMapFormat;
		uavDesc.Format = VarianceMapFormat;

		auto rawResource = mAOVarianceResources[AOVariance::Resources::ERaw]->Resource();
		md3dDevice->CreateShaderResourceView(rawResource, &srvDesc, mhAOVarianceResourcesCpus[AOVariance::Descriptors::ES_Raw]);
		md3dDevice->CreateUnorderedAccessView(rawResource, nullptr, &uavDesc, mhAOVarianceResourcesCpus[AOVariance::Descriptors::EU_Raw]);

		auto smoothedResource = mAOVarianceResources[AOVariance::Resources::ESmoothed]->Resource();
		md3dDevice->CreateShaderResourceView(smoothedResource, &srvDesc, mhAOVarianceResourcesCpus[AOVariance::Descriptors::ES_Smoothed]);
		md3dDevice->CreateUnorderedAccessView(smoothedResource, nullptr, &uavDesc, mhAOVarianceResourcesCpus[AOVariance::Descriptors::EU_Smoothed]);
	}
	{
		srvDesc.Format = NormalDepthMapFormat;
		md3dDevice->CreateShaderResourceView(mPrevFrameNormalDepth->Resource(), &srvDesc, mhPrevFrameNormalDepthCpuSrv);

		srvDesc.Format = TsppCoefficientSquaredMeanRayHitDistanceFormat;
		uavDesc.Format = TsppCoefficientSquaredMeanRayHitDistanceFormat;
		md3dDevice->CreateShaderResourceView(mTsppCoefficientSquaredMeanRayHitDistance->Resource(), &srvDesc, mhTsppCoefficientSquaredMeanRayHitDistanceCpuSrv);
		md3dDevice->CreateUnorderedAccessView(mTsppCoefficientSquaredMeanRayHitDistance->Resource(), nullptr, &uavDesc, mhTsppCoefficientSquaredMeanRayHitDistanceCpuUav);
	}
	{
		srvDesc.Format = DisocclusionBlurStrengthMapFormat;
		md3dDevice->CreateShaderResourceView(mDisocclusionBlurStrength->Resource(), &srvDesc, mhDisocclusionBlurStrengthCpuSrv);

		uavDesc.Format = DisocclusionBlurStrengthMapFormat;
		md3dDevice->CreateUnorderedAccessView(mDisocclusionBlurStrength->Resource(), nullptr, &uavDesc, mhDisocclusionBlurStrengthCpuUav);
	}
	{
		srvDesc.Format = AOCoefficientMapFormat;
		uavDesc.Format = AOCoefficientMapFormat;
		for (size_t i = 0; i < 2; ++i) {
			md3dDevice->CreateShaderResourceView(
				mTemporalAOCoefficients[i]->Resource(), &srvDesc,
				mhTemporalAOCoefficientsCpus[i][TemporalAOCoefficient::Descriptors::Srv]
			);
			md3dDevice->CreateUnorderedAccessView(
				mTemporalAOCoefficients[i]->Resource(), nullptr, &uavDesc,
				mhTemporalAOCoefficientsCpus[i][TemporalAOCoefficient::Descriptors::Uav]
			);
		}
	}
	{
		srvDesc.Format = DepthPartialDerivativeMapFormat;
		md3dDevice->CreateShaderResourceView(mDepthPartialDerivative->Resource(), &srvDesc, mhDepthPartialDerivativeCpuSrv);

		uavDesc.Format = DepthPartialDerivativeMapFormat;
		md3dDevice->CreateUnorderedAccessView(mDepthPartialDerivative->Resource(), nullptr, &uavDesc, mhDepthPartialDerivativeCpuUav);
	}
}

bool RtaoClass::BuildResources(ID3D12GraphicsCommandList* cmdList) {
	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	// Ambient occlusion maps are at half resolution.
	texDesc.Width = mWidth;
	texDesc.Height = mHeight;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	{
		texDesc.Format = AOCoefficientMapFormat;
		CheckReturn(mAOResources[AO::Resources::EAmbientCoefficient]->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"AOCoefficientMap"
		));
	}
	{
		texDesc.Format = RayHitDistanceFormat;
		CheckReturn(mAOResources[AO::Resources::ERayHitDistance]->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"RayHitDistanceMap"
		));
	}
	{
		texDesc.Format = TsppMapFormat;
		for (int i = 0; i < 2; ++i) {
			std::wstring name = L"TsppMap_";
			name.append(std::to_wstring(i));
			CheckReturn(mTemporalCaches[i][TemporalCache::Resources::ETspp]->Initialize(
				md3dDevice,
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&texDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				nullptr,
				name.c_str()
			));
		}
	}
	{
		texDesc.Format = RayHitDistanceFormat;
		for (int i = 0; i < 2; ++i) {
			std::wstring name = L"TemporalRayHitDistanceMap_";
			name.append(std::to_wstring(i));
			CheckReturn(mTemporalCaches[i][TemporalCache::Resources::ERayHitDistance]->Initialize(
				md3dDevice,
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&texDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				nullptr,
				name.c_str()
			));
		}
	}
	{
		texDesc.Format = CoefficientSquaredMeanMapFormat;
		for (int i = 0; i < 2; ++i) {
			std::wstring name = L"AOCoefficientSquaredMeanMap_";
			name.append(std::to_wstring(i));
			CheckReturn(mTemporalCaches[i][TemporalCache::Resources::ECoefficientSquaredMean]->Initialize(
				md3dDevice,
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&texDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				nullptr,
				name.c_str()
			));			
		}
	}
	{
		texDesc.Format = LocalMeanVarianceMapFormat;

		CheckReturn(mLocalMeanVarianceResources[LocalMeanVariance::Resources::ERaw]->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"RawLocalMeanVarianceMap"
		));

		CheckReturn(mLocalMeanVarianceResources[LocalMeanVariance::Resources::ESmoothed]->Initialize(
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
		CheckReturn(mAOVarianceResources[AOVariance::Resources::ERaw]->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"RawVarianceMap"
		));

		CheckReturn(mAOVarianceResources[AOVariance::Resources::ESmoothed]->Initialize(
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
		auto _texDesc = texDesc;
		_texDesc.Format = NormalDepthMapFormat;
		_texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		CheckReturn(mPrevFrameNormalDepth->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&_texDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			L"PrevFrameNormalDepthMap"
		));
	}
	{
		texDesc.Format = TsppCoefficientSquaredMeanRayHitDistanceFormat;
		CheckReturn(mTsppCoefficientSquaredMeanRayHitDistance->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"TsppAOCoefficientSquaredMeanRayHitDistanceMap"
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
		texDesc.Format = AOCoefficientMapFormat;
		for (int i = 0; i < 2; ++i) {
			std::wstringstream wsstream;
			wsstream << L"TemporalAOCoefficientMap_" << i;
			CheckReturn(mTemporalAOCoefficients[i]->Initialize(
				md3dDevice,
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&texDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				nullptr,
				wsstream.str().c_str()
			));
		}
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

	//{
	//	const UINT num2DSubresources = texDesc.DepthOrArraySize * texDesc.MipLevels;
	//	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(mPrevFrameNormalDepth->Resource(), 0, num2DSubresources);
	//
	//	CheckReturn(mPrevFrameNormalDepthUploadBuffer->Initialize(
	//		md3dDevice,
	//		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
	//		D3D12_HEAP_FLAG_NONE,
	//		&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
	//		D3D12_RESOURCE_STATE_COPY_SOURCE,
	//		nullptr
	//	));
	//
	//	const UINT size = mWidth * mHeight * 4;
	//	std::vector<BYTE> data(size);
	//
	//	for (UINT i = 0; i < size; i += 4) {
	//		data[i] = data[i + 1] = data[i + 2] = 0;	// rgb-channels(normal) = 0 / 128;
	//		data[i + 3] = 127;							// a-channel(depth) = 127 / 128;
	//	}
	//
	//	D3D12_SUBRESOURCE_DATA subResourceData = {};
	//	subResourceData.pData = data.data();
	//	subResourceData.RowPitch = mWidth * 4;
	//	subResourceData.SlicePitch = subResourceData.RowPitch * mHeight;
	//	
	//	UpdateSubresources(
	//		cmdList,
	//		mPrevFrameNormalDepth->Resource(),
	//		mPrevFrameNormalDepthUploadBuffer->Resource(),
	//		0,
	//		0,
	//		num2DSubresources,
	//		&subResourceData
	//	);
	//
	//	mPrevFrameNormalDepth->Transite(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	//}

	return true;
}