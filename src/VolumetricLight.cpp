#include "VolumetricLight.h"
#include "Logger.h"
#include "ShaderManager.h"

using namespace VolumetricLight;

namespace {
	const CHAR* const CS_CalcScatteringAndDensity = "CS_CalcScatteringAndDensity";
}

VolumetricLightClass::VolumetricLightClass() {
	mVolumetricLightMap = std::make_unique<GpuResource>();
}

BOOL VolumetricLightClass::Initialize(
		ID3D12Device* device, ShaderManager* const manager,
		UINT width, UINT height, UINT depth) {
	md3dDevice = device;
	mShaderManager = manager;

	mWidth = width;
	mHeight = height;
	mDepth = depth;

	return TRUE;
}

BOOL VolumetricLightClass::CompileShaders(const std::wstring& filePath) {
	{
		const std::wstring actualPath = filePath + L"CalculateScatteringAndDensityCS.hlsl";
		auto csInfo = D3D12ShaderInfo(actualPath.c_str(), L"CS", L"cs_6_3");
		CheckReturn(mShaderManager->CompileShader(csInfo, CS_CalcScatteringAndDensity));
	}

	return TRUE;
}

BOOL VolumetricLightClass::BuildRootSignature(const StaticSamplers& samplers) {
	return TRUE;
}

BOOL VolumetricLightClass::BuildPSO() {
	return TRUE;
}

void VolumetricLightClass::BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu,
		UINT descSize) {
	mhVolumetricLightMapCpuSrv = hCpu.Offset(1, descSize);
	mhVolumetricLightMapGpuSrv = hGpu.Offset(1, descSize);

	mhVolumetricLightMapCpuUav = hCpu.Offset(1, descSize);
	mhVolumetricLightMapGpuUav = hGpu.Offset(1, descSize);
}