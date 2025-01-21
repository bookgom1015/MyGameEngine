#include "VolumetricLight.h"

using namespace VolumetricLight;

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
		CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpuUav,
		CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpuUav,
		UINT descSize, UINT uavDescSize) {
	mhVolumetricLightMapCpuSrv = hCpu.Offset(1, descSize);
	mhVolumetricLightMapGpuSrv = hGpu.Offset(1, descSize);

	mhVolumetricLightMapCpuUav = hCpuUav.Offset(1, uavDescSize);
	mhVolumetricLightMapGpuUav = hGpuUav.Offset(1, uavDescSize);
}