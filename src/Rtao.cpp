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
	std::string RtaoCS								= "RtaoCS";
	std::string TsppReprojCS						= "TsppReprojCS";
	std::string TsppBlendCS							= "TsppBlendCS";
	std::string PartialDerivativeCS					= "PartialDerivativeCS";
	std::string CalcLocalMeanVarianceCS				= "CalcLocalMeanVarianceCS";
	std::string FillInCheckerboardCS				= "FillInCheckerboardCS";
	std::string EdgeStoppingFilter_Gaussian3x3CS	= "EdgeStoppingFilter_Gaussian3x3CS";
	std::string DisocclusionBlur3x3CS				= "DisocclusionBlur3x3CS";
}

RtaoClass::RtaoClass() {
	mTemporalCurrentFrameResourceIndex = 0;
	mTemporalCurrentFrameTemporalAOCeofficientResourceIndex = 0;

	for (int i = 0; i < Resource::AO::Count; ++i) {
		mAOResources[i] = std::make_unique<GpuResource>();
	}

	for (int i = 0; i < Resource::LocalMeanVariance::Count; ++i) {
		mLocalMeanVarianceResources[i] = std::make_unique<GpuResource>();
	}

	for (int i = 0; i < Resource::AOVariance::Count; ++i) {
		mAOVarianceResources[i] = std::make_unique<GpuResource>();
	}

	for (int i = 0; i < 2; ++i) {
		for (int j = 0; j < Resource::TemporalCache::Count; ++j) {
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
		const auto path = filePath + L"Rtao.hlsl";
		auto shaderInfo = D3D12ShaderInfo(path.c_str(), L"", L"lib_6_3");
		CheckReturn(mShaderManager->CompileShader(shaderInfo, RtaoCS));
	}
	{
		const auto path = filePath + L"TemporalSupersamplingReverseReprojectCS.hlsl";
		auto shaderInfo = D3D12ShaderInfo(path.c_str(), L"CS", L"cs_6_3");

		//std::vector<LPCWSTR> args = { L"-Zi" };
		//shaderInfo.ArgCount = args.size();
		//shaderInfo.Arguments = args.data();

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

		CD3DX12_ROOT_PARAMETER slotRootParameter[RootSignature::CalcAmbientOcclusion::Count];
		slotRootParameter[RootSignature::CalcAmbientOcclusion::ESI_AccelerationStructure].InitAsShaderResourceView(0);
		slotRootParameter[RootSignature::CalcAmbientOcclusion::ECB_RtaoPass].InitAsConstantBufferView(0);
		slotRootParameter[RootSignature::CalcAmbientOcclusion::EC_Consts].InitAsConstants(
			RootSignature::CalcAmbientOcclusion::RootConstant::Count, 1, 0);
		slotRootParameter[RootSignature::CalcAmbientOcclusion::ESI_Normal].InitAsDescriptorTable(1, &texTables[0]);
		slotRootParameter[RootSignature::CalcAmbientOcclusion::ESI_Depth].InitAsDescriptorTable(1, &texTables[1]);
		slotRootParameter[RootSignature::CalcAmbientOcclusion::EUO_AOCoefficient].InitAsDescriptorTable(1, &texTables[2]);
		slotRootParameter[RootSignature::CalcAmbientOcclusion::EUO_RayHitDistance].InitAsDescriptorTable(1, &texTables[3]);

		CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(
			_countof(slotRootParameter), slotRootParameter,
			static_cast<UINT>(samplers.size()), samplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_NONE
		);
		CheckReturn(D3D12Util::CreateRootSignature(
			md3dDevice, globalRootSignatureDesc, &mRootSignatures[RootSignature::E_CalcAmbientOcclusion]));
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
	return true;
}

bool RtaoClass::BuildPSO() {
	//
	// Pipeline States
	//
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
	
	//
	// State Object
	//
	{
		CD3DX12_STATE_OBJECT_DESC rtaoDxrPso = { D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };
	
		auto rtaoLib = rtaoDxrPso.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
		auto rtaoShader = mShaderManager->GetDxcShader(RtaoCS);
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
		glbalRootSig->SetRootSignature(mRootSignatures[RootSignature::E_CalcAmbientOcclusion].Get());
	
		auto pipelineConfig = rtaoDxrPso.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
		UINT maxRecursionDepth = 1;
		pipelineConfig->Config(maxRecursionDepth);
	
		CheckHRESULT(md3dDevice->CreateStateObject(rtaoDxrPso, IID_PPV_ARGS(&mDxrPso)));
		CheckHRESULT(mDxrPso->QueryInterface(IID_PPV_ARGS(&mDxrPsoProp)));
	}

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

void RtaoClass::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu, CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu, UINT descSize) {
	mhAOResourcesCpus[Descriptor::AO::ES_AmbientCoefficient] = hCpu;
	mhAOResourcesGpus[Descriptor::AO::ES_AmbientCoefficient] = hGpu;
	mhAOResourcesCpus[Descriptor::AO::EU_AmbientCoefficient] = hCpu.Offset(1, descSize);
	mhAOResourcesGpus[Descriptor::AO::EU_AmbientCoefficient] = hGpu.Offset(1, descSize);
	mhAOResourcesCpus[Descriptor::AO::ES_RayHitDistance] = hCpu.Offset(1, descSize);
	mhAOResourcesGpus[Descriptor::AO::ES_RayHitDistance] = hGpu.Offset(1, descSize);
	mhAOResourcesCpus[Descriptor::AO::EU_RayHitDistance] = hCpu.Offset(1, descSize);
	mhAOResourcesGpus[Descriptor::AO::EU_RayHitDistance] = hGpu.Offset(1, descSize);

	mhLocalMeanVarianceResourcesCpus[Descriptor::LocalMeanVariance::ES_Raw] = hCpu.Offset(1, descSize);
	mhLocalMeanVarianceResourcesGpus[Descriptor::LocalMeanVariance::ES_Raw] = hGpu.Offset(1, descSize);
	mhLocalMeanVarianceResourcesCpus[Descriptor::LocalMeanVariance::EU_Raw] = hCpu.Offset(1, descSize);
	mhLocalMeanVarianceResourcesGpus[Descriptor::LocalMeanVariance::EU_Raw] = hGpu.Offset(1, descSize);
	mhLocalMeanVarianceResourcesCpus[Descriptor::LocalMeanVariance::ES_Smoothed] = hCpu.Offset(1, descSize);
	mhLocalMeanVarianceResourcesGpus[Descriptor::LocalMeanVariance::ES_Smoothed] = hGpu.Offset(1, descSize);
	mhLocalMeanVarianceResourcesCpus[Descriptor::LocalMeanVariance::EU_Smoothed] = hCpu.Offset(1, descSize);
	mhLocalMeanVarianceResourcesGpus[Descriptor::LocalMeanVariance::EU_Smoothed] = hGpu.Offset(1, descSize);

	mhAOVarianceResourcesCpus[Descriptor::AOVariance::ES_Raw] = hCpu.Offset(1, descSize);
	mhAOVarianceResourcesGpus[Descriptor::AOVariance::ES_Raw] = hGpu.Offset(1, descSize);
	mhAOVarianceResourcesCpus[Descriptor::AOVariance::EU_Raw] = hCpu.Offset(1, descSize);
	mhAOVarianceResourcesGpus[Descriptor::AOVariance::EU_Raw] = hGpu.Offset(1, descSize);
	mhAOVarianceResourcesCpus[Descriptor::AOVariance::ES_Smoothed] = hCpu.Offset(1, descSize);
	mhAOVarianceResourcesGpus[Descriptor::AOVariance::ES_Smoothed] = hGpu.Offset(1, descSize);
	mhAOVarianceResourcesCpus[Descriptor::AOVariance::EU_Smoothed] = hCpu.Offset(1, descSize);
	mhAOVarianceResourcesGpus[Descriptor::AOVariance::EU_Smoothed] = hGpu.Offset(1, descSize);

	for (size_t i = 0; i < 2; ++i) {
		mhTemporalCachesCpus[i][Descriptor::TemporalCache::ES_Tspp] = hCpu.Offset(1, descSize);
		mhTemporalCachesGpus[i][Descriptor::TemporalCache::ES_Tspp] = hGpu.Offset(1, descSize);
		mhTemporalCachesCpus[i][Descriptor::TemporalCache::EU_Tspp] = hCpu.Offset(1, descSize);
		mhTemporalCachesGpus[i][Descriptor::TemporalCache::EU_Tspp] = hGpu.Offset(1, descSize);

		mhTemporalCachesCpus[i][Descriptor::TemporalCache::ES_RayHitDistance] = hCpu.Offset(1, descSize);
		mhTemporalCachesGpus[i][Descriptor::TemporalCache::ES_RayHitDistance] = hGpu.Offset(1, descSize);
		mhTemporalCachesCpus[i][Descriptor::TemporalCache::EU_RayHitDistance] = hCpu.Offset(1, descSize);
		mhTemporalCachesGpus[i][Descriptor::TemporalCache::EU_RayHitDistance] = hGpu.Offset(1, descSize);

		mhTemporalCachesCpus[i][Descriptor::TemporalCache::ES_CoefficientSquaredMean] = hCpu.Offset(1, descSize);
		mhTemporalCachesGpus[i][Descriptor::TemporalCache::ES_CoefficientSquaredMean] = hGpu.Offset(1, descSize);
		mhTemporalCachesCpus[i][Descriptor::TemporalCache::EU_CoefficientSquaredMean] = hCpu.Offset(1, descSize);
		mhTemporalCachesGpus[i][Descriptor::TemporalCache::EU_CoefficientSquaredMean] = hGpu.Offset(1, descSize);
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
		mhTemporalAOCoefficientsCpus[i][Descriptor::TemporalAOCoefficient::Srv] = hCpu.Offset(1, descSize);
		mhTemporalAOCoefficientsGpus[i][Descriptor::TemporalAOCoefficient::Srv] = hGpu.Offset(1, descSize);
		mhTemporalAOCoefficientsCpus[i][Descriptor::TemporalAOCoefficient::Uav] = hCpu.Offset(1, descSize);
		mhTemporalAOCoefficientsGpus[i][Descriptor::TemporalAOCoefficient::Uav] = hGpu.Offset(1, descSize);
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

void RtaoClass::RunCalculatingAmbientOcclusion(
	ID3D12GraphicsCommandList4* const cmdList,
	D3D12_GPU_VIRTUAL_ADDRESS accelStruct,
	D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
	D3D12_GPU_DESCRIPTOR_HANDLE si_normal,
	D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
	D3D12_GPU_DESCRIPTOR_HANDLE uo_aoCoefficient,
	D3D12_GPU_DESCRIPTOR_HANDLE uo_rayHitDistance) {
	cmdList->SetPipelineState1(mDxrPso.Get());
	cmdList->SetComputeRootSignature(mRootSignatures[RootSignature::E_CalcAmbientOcclusion].Get());

	cmdList->SetComputeRootShaderResourceView(RootSignature::CalcAmbientOcclusion::ESI_AccelerationStructure, accelStruct);
	cmdList->SetComputeRootConstantBufferView(RootSignature::CalcAmbientOcclusion::ECB_RtaoPass, cbAddress);

	const UINT values[RootSignature::CalcAmbientOcclusion::RootConstant::Count] = { mWidth, mHeight };
	cmdList->SetComputeRoot32BitConstants(RootSignature::CalcAmbientOcclusion::EC_Consts, _countof(values), values, 0);

	cmdList->SetComputeRootDescriptorTable(RootSignature::CalcAmbientOcclusion::ESI_Normal, si_normal);
	cmdList->SetComputeRootDescriptorTable(RootSignature::CalcAmbientOcclusion::ESI_Depth, si_depth);
	cmdList->SetComputeRootDescriptorTable(RootSignature::CalcAmbientOcclusion::EUO_AOCoefficient, uo_aoCoefficient);
	cmdList->SetComputeRootDescriptorTable(RootSignature::CalcAmbientOcclusion::EUO_RayHitDistance, uo_rayHitDistance);

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
	cmdList->SetPipelineState(mPsos[PipelineState::E_CalcDepthPartialDerivative].Get());
	cmdList->SetComputeRootSignature(mRootSignatures[RootSignature::E_CalcDepthPartialDerivative].Get());

	const float values[RootSignature::CalcDepthPartialDerivative::RootConstant::Count] = { 1.0f / width, 1.0f / height };
	cmdList->SetComputeRoot32BitConstants(
		RootSignature::CalcDepthPartialDerivative::EC_Consts, 
		RootSignature::CalcDepthPartialDerivative::RootConstant::Count,
		values, 0);

	cmdList->SetComputeRootDescriptorTable(RootSignature::CalcDepthPartialDerivative::ESI_Depth, i_depth);
	cmdList->SetComputeRootDescriptorTable(RootSignature::CalcDepthPartialDerivative::EUO_DepthPartialDerivative, o_depthPartialDerivative);

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
	cmdList->SetPipelineState(mPsos[PipelineState::E_CalcLocalMeanVariance].Get());
	cmdList->SetComputeRootSignature(mRootSignatures[RootSignature::E_CalcLocalMeanVariance].Get());

	cmdList->SetComputeRootConstantBufferView(RootSignature::CalcLocalMeanVariance::ECB_LocalMeanVar, cbAddress);
	cmdList->SetComputeRootDescriptorTable(RootSignature::CalcLocalMeanVariance::ESI_AOCoefficient, si_aoCoefficient);
	cmdList->SetComputeRootDescriptorTable(RootSignature::CalcLocalMeanVariance::EUO_LocalMeanVar, uo_localMeanVariance);

	int pixelStepY = checkerboardSamplingEnabled ? 2 : 1;
	cmdList->Dispatch(
		D3D12Util::CeilDivide(width, Default::ThreadGroup::Width),
		D3D12Util::CeilDivide(height, Default::ThreadGroup::Height * pixelStepY), 1);
}

void RtaoClass::FillInCheckerboard(
	ID3D12GraphicsCommandList4* const cmdList,
	D3D12_GPU_VIRTUAL_ADDRESS cbAddress,
	D3D12_GPU_DESCRIPTOR_HANDLE uio_localMeanVariance) {
	cmdList->SetPipelineState(mPsos[PipelineState::E_FillInCheckerboard].Get());
	cmdList->SetComputeRootSignature(mRootSignatures[RootSignature::E_FillInCheckerboard].Get());

	cmdList->SetComputeRootConstantBufferView(RootSignature::FillInCheckerboard::ECB_LocalMeanVar, cbAddress);
	cmdList->SetComputeRootDescriptorTable(RootSignature::FillInCheckerboard::EUIO_LocalMeanVar, uio_localMeanVariance);

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
	cmdList->SetPipelineState(mPsos[PipelineState::E_TemporalSupersamplingReverseReproject].Get());
	cmdList->SetComputeRootSignature(mRootSignatures[RootSignature::E_TemporalSupersamplingReverseReproject].Get());

	cmdList->SetComputeRootConstantBufferView(RootSignature::TemporalSupersamplingReverseReproject::ECB_CrossBilateralFilter, cbAddress);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingReverseReproject::ESI_NormalDepth, si_normalDepth);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingReverseReproject::ESI_DepthPartialDerivative, si_depthPartialDerivative);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingReverseReproject::ESI_ReprojectedNormalDepth, si_reprojNormalDepth);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingReverseReproject::ESI_CachedNormalDepth, si_cachedNormalDepth);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingReverseReproject::ESI_Velocity, si_velocity);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingReverseReproject::ESI_CachedAOCoefficient, si_cachedAOCoefficient);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingReverseReproject::ESI_CachedTspp, si_cachedTspp);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingReverseReproject::ESI_CachedAOCoefficientSquaredMean, si_cachedAOCoefficientSquaredMean);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingReverseReproject::ESI_CachedRayHitDistance, si_cachedRayHitDistance);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingReverseReproject::EUO_CachedTspp, uo_cachedTspp);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingReverseReproject::EUO_TsppCoefficientSquaredMeanRayHitDistacne, uo_tsppCoefficientSquaredMeanRayHitDistance);

	{
		UINT values[] = { mWidth, mHeight };
		cmdList->SetComputeRoot32BitConstants(
			RootSignature::TemporalSupersamplingReverseReproject::EC_Consts,
			_countof(values), values,
			RootSignature::TemporalSupersamplingReverseReproject::RootConstant::E_TextureDim_X
		);
	}
	{
		float values[] = { 1.0f / mWidth, 1.0f / mHeight };
		cmdList->SetComputeRoot32BitConstants(
			RootSignature::TemporalSupersamplingReverseReproject::EC_Consts,
			_countof(values), values,
			RootSignature::TemporalSupersamplingReverseReproject::RootConstant::E_InvTextureDim_X
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
	cmdList->SetPipelineState(mPsos[PipelineState::E_TemporalSupersamplingBlendWithCurrentFrame].Get());
	cmdList->SetComputeRootSignature(mRootSignatures[RootSignature::E_TemporalSupersamplingBlendWithCurrentFrame].Get());

	cmdList->SetComputeRootConstantBufferView(RootSignature::TemporalSupersamplingBlendWithCurrentFrame::ECB_TsspBlendWithCurrentFrame, cbAddress);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingBlendWithCurrentFrame::ESI_AOCoefficient, si_aoCoefficient);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingBlendWithCurrentFrame::ESI_LocalMeanVaraince, si_localMeanVariance);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingBlendWithCurrentFrame::ESI_RayHitDistance, si_rayHitDistance);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingBlendWithCurrentFrame::ESI_TsppCoefficientSquaredMeanRayHitDistance, si_tsppCoefficientSquaredMeanRayHitDistance);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingBlendWithCurrentFrame::EUIO_TemporalAOCoefficient, uio_temporalAOCoefficient);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingBlendWithCurrentFrame::EUIO_Tspp, uio_tspp);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingBlendWithCurrentFrame::EUIO_CoefficientSquaredMean, uio_coefficientSquaredMean);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingBlendWithCurrentFrame::EUIO_RayHitDistance, uio_rayHitDistance);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingBlendWithCurrentFrame::EUO_VarianceMap, uo_variance);
	cmdList->SetComputeRootDescriptorTable(RootSignature::TemporalSupersamplingBlendWithCurrentFrame::EUO_BlurStrength, uo_blurStrength);

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
	cmdList->SetPipelineState(mPsos[PipelineState::E_AtrousWaveletTransformFilter].Get());
	cmdList->SetComputeRootSignature(mRootSignatures[RootSignature::E_AtrousWaveletTransformFilter].Get());

	cmdList->SetComputeRootConstantBufferView(RootSignature::AtrousWaveletTransformFilter::ECB_AtrousFilter, cbAddress);
	cmdList->SetComputeRootDescriptorTable(RootSignature::AtrousWaveletTransformFilter::ESI_TemporalAOCoefficient, si_temporalAOCoefficient);
	cmdList->SetComputeRootDescriptorTable(RootSignature::AtrousWaveletTransformFilter::ESI_NormalDepth, si_normalDepth);
	cmdList->SetComputeRootDescriptorTable(RootSignature::AtrousWaveletTransformFilter::ESI_Variance, si_variance);
	cmdList->SetComputeRootDescriptorTable(RootSignature::AtrousWaveletTransformFilter::ESI_HitDistance, si_hitDistance);
	cmdList->SetComputeRootDescriptorTable(RootSignature::AtrousWaveletTransformFilter::ESI_DepthPartialDerivative, si_depthPartialDerivative);
	cmdList->SetComputeRootDescriptorTable(RootSignature::AtrousWaveletTransformFilter::ESI_Tspp, si_tspp);
	cmdList->SetComputeRootDescriptorTable(RootSignature::AtrousWaveletTransformFilter::EUO_TemporalAOCoefficient, uo_temporalAOCoefficient);

	cmdList->Dispatch(
		D3D12Util::CeilDivide(mWidth, Default::ThreadGroup::Width),
		D3D12Util::CeilDivide(mHeight, Default::ThreadGroup::Height), 1);
}

void RtaoClass::BlurDisocclusion(
	ID3D12GraphicsCommandList4* const cmdList,
	GpuResource* aoCoefficient,
	D3D12_GPU_DESCRIPTOR_HANDLE si_depth,
	D3D12_GPU_DESCRIPTOR_HANDLE si_blurStrength,
	D3D12_GPU_DESCRIPTOR_HANDLE uio_aoCoefficient,
	UINT width, UINT height,
	UINT lowTsppBlurPasses) {
	cmdList->SetPipelineState(mPsos[PipelineState::E_DisocclusionBlur].Get());
	cmdList->SetComputeRootSignature(mRootSignatures[RootSignature::E_DisocclusionBlur].Get());

	UINT values[2] = { width, height };
	cmdList->SetComputeRoot32BitConstants(RootSignature::DisocclusionBlur::EC_Consts, _countof(values), values, 0);

	cmdList->SetComputeRootDescriptorTable(RootSignature::DisocclusionBlur::ESI_Depth, si_depth);
	cmdList->SetComputeRootDescriptorTable(RootSignature::DisocclusionBlur::ESI_BlurStrength, si_blurStrength);
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

UINT RtaoClass::MoveToNextFrame() {
	mTemporalCurrentFrameResourceIndex = (mTemporalCurrentFrameResourceIndex + 1) % 2;
	return mTemporalCurrentFrameResourceIndex;
}

UINT RtaoClass::MoveToNextFrameTemporalAOCoefficient() {
	mTemporalCurrentFrameTemporalAOCeofficientResourceIndex = (mTemporalCurrentFrameTemporalAOCeofficientResourceIndex + 1) % 2;
	return mTemporalCurrentFrameTemporalAOCeofficientResourceIndex;
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

		auto resource = mAOResources[Resource::AO::E_AmbientCoefficient]->Resource();
		md3dDevice->CreateShaderResourceView(resource, &srvDesc, mhAOResourcesCpus[Descriptor::AO::ES_AmbientCoefficient]);
		md3dDevice->CreateUnorderedAccessView(resource, nullptr, &uavDesc, mhAOResourcesCpus[Descriptor::AO::EU_AmbientCoefficient]);
	}
	{
		srvDesc.Format = RayHitDistanceFormat;
		uavDesc.Format = RayHitDistanceFormat;

		auto resource = mAOResources[Resource::AO::E_RayHitDistance]->Resource();
		md3dDevice->CreateShaderResourceView(resource, &srvDesc, mhAOResourcesCpus[Descriptor::AO::ES_RayHitDistance]);
		md3dDevice->CreateUnorderedAccessView(resource, nullptr, &uavDesc, mhAOResourcesCpus[Descriptor::AO::EU_RayHitDistance]);
	}

	{
		srvDesc.Format = TsppMapFormat;
		uavDesc.Format = TsppMapFormat;
		for (size_t i = 0; i < 2; ++i) {
			auto resource = mTemporalCaches[i][Resource::TemporalCache::E_Tspp]->Resource();
			auto& cpus = mhTemporalCachesCpus[i];
			md3dDevice->CreateShaderResourceView(resource, &srvDesc, cpus[Descriptor::TemporalCache::ES_Tspp]);
			md3dDevice->CreateUnorderedAccessView(resource, nullptr, &uavDesc, cpus[Descriptor::TemporalCache::EU_Tspp]);
		}
	}
	{
		srvDesc.Format = RayHitDistanceFormat;
		uavDesc.Format = RayHitDistanceFormat;
		for (size_t i = 0; i < 2; ++i) {
			auto resource = mTemporalCaches[i][Resource::TemporalCache::E_RayHitDistance]->Resource();
			auto& cpus = mhTemporalCachesCpus[i];
			md3dDevice->CreateShaderResourceView(resource, &srvDesc, cpus[Descriptor::TemporalCache::ES_RayHitDistance]);
			md3dDevice->CreateUnorderedAccessView(resource, nullptr, &uavDesc, cpus[Descriptor::TemporalCache::EU_RayHitDistance]);
		}
	}
	{
		srvDesc.Format = CoefficientSquaredMeanMapFormat;
		uavDesc.Format = CoefficientSquaredMeanMapFormat;
		for (size_t i = 0; i < 2; ++i) {
			auto resource = mTemporalCaches[i][Resource::TemporalCache::E_CoefficientSquaredMean]->Resource();
			auto& cpus = mhTemporalCachesCpus[i];
			md3dDevice->CreateShaderResourceView(resource, &srvDesc, cpus[Descriptor::TemporalCache::ES_CoefficientSquaredMean]);
			md3dDevice->CreateUnorderedAccessView(resource, nullptr, &uavDesc, cpus[Descriptor::TemporalCache::EU_CoefficientSquaredMean]);
		}
	}
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

		auto rawResource = mAOVarianceResources[Resource::AOVariance::E_Raw]->Resource();
		md3dDevice->CreateShaderResourceView(rawResource, &srvDesc, mhAOVarianceResourcesCpus[Descriptor::AOVariance::ES_Raw]);
		md3dDevice->CreateUnorderedAccessView(rawResource, nullptr, &uavDesc, mhAOVarianceResourcesCpus[Descriptor::AOVariance::EU_Raw]);

		auto smoothedResource = mAOVarianceResources[Resource::AOVariance::E_Smoothed]->Resource();
		md3dDevice->CreateShaderResourceView(smoothedResource, &srvDesc, mhAOVarianceResourcesCpus[Descriptor::AOVariance::ES_Smoothed]);
		md3dDevice->CreateUnorderedAccessView(smoothedResource, nullptr, &uavDesc, mhAOVarianceResourcesCpus[Descriptor::AOVariance::EU_Smoothed]);
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
				mhTemporalAOCoefficientsCpus[i][Descriptor::TemporalAOCoefficient::Srv]
			);
			md3dDevice->CreateUnorderedAccessView(
				mTemporalAOCoefficients[i]->Resource(), nullptr, &uavDesc,
				mhTemporalAOCoefficientsCpus[i][Descriptor::TemporalAOCoefficient::Uav]
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
		CheckReturn(mAOResources[Resource::AO::E_AmbientCoefficient]->Initialize(
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
		CheckReturn(mAOResources[Resource::AO::E_RayHitDistance]->Initialize(
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
			CheckReturn(mTemporalCaches[i][Resource::TemporalCache::E_Tspp]->Initialize(
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
			CheckReturn(mTemporalCaches[i][Resource::TemporalCache::E_RayHitDistance]->Initialize(
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
			CheckReturn(mTemporalCaches[i][Resource::TemporalCache::E_CoefficientSquaredMean]->Initialize(
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
		CheckReturn(mAOVarianceResources[Resource::AOVariance::E_Raw]->Initialize(
			md3dDevice,
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			L"RawVarianceMap"
		));

		CheckReturn(mAOVarianceResources[Resource::AOVariance::E_Smoothed]->Initialize(
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