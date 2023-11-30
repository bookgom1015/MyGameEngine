#include "AccelerationStructure.h"
#include "Logger.h"
#include "D3D12Util.h"

AccelerationStructureBuffer::~AccelerationStructureBuffer() {
	if (InstanceDesc != nullptr) InstanceDesc->Unmap(0, nullptr);
	mMappedData = nullptr;
}

bool AccelerationStructureBuffer::BuildTLAS(
		ID3D12Device5* const device,
		ID3D12GraphicsCommandList4* const cmdList,
		const std::vector<D3D12_RAYTRACING_INSTANCE_DESC>& instanceDescs) {
	// Copy the instance data to the buffer
	CheckHRESULT(InstanceDesc->Map(0, nullptr, &mMappedData));
	std::memcpy(mMappedData, instanceDescs.data(), instanceDescs.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC));

	// Get the size requirements for the TLAS buffers
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.InstanceDescs = InstanceDesc->GetGPUVirtualAddress();
	inputs.NumDescs = static_cast<UINT>(instanceDescs.size());
	inputs.Flags = buildFlags;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo = {};
	device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuildInfo);

	prebuildInfo.ResultDataMaxSizeInBytes = Align(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, prebuildInfo.ResultDataMaxSizeInBytes);
	prebuildInfo.ScratchDataSizeInBytes = Align(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, prebuildInfo.ScratchDataSizeInBytes);

	// Set TLAS size
	ResultDataMaxSizeInBytes = prebuildInfo.ResultDataMaxSizeInBytes;

	// Create TLAS sratch buffer
	D3D12BufferCreateInfo bufferInfo(prebuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
	bufferInfo.Alignment = std::max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
	CheckReturn(D3D12Util::CreateBuffer(device, bufferInfo, Scratch.GetAddressOf()));

	// Create the TLAS buffer
	bufferInfo.Size = prebuildInfo.ResultDataMaxSizeInBytes;
	bufferInfo.State = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
	CheckReturn(D3D12Util::CreateBuffer(device, bufferInfo, Result.GetAddressOf()));

	// Describe and build the TLAS
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
	buildDesc.Inputs = inputs;
	buildDesc.ScratchAccelerationStructureData = Scratch->GetGPUVirtualAddress();
	buildDesc.DestAccelerationStructureData = Result->GetGPUVirtualAddress();
	cmdList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

	// Wait for the TLAS build to complete
	D3D12Util::UavBarrier(cmdList, Result.Get());

	return true;
}

void AccelerationStructureBuffer::UpdateTLAS(
		ID3D12GraphicsCommandList4* const cmdList,
		const std::vector<D3D12_RAYTRACING_INSTANCE_DESC>& instanceDescs) {
	std::memcpy(mMappedData, instanceDescs.data(), instanceDescs.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC));

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.InstanceDescs = InstanceDesc->GetGPUVirtualAddress();
	inputs.NumDescs = static_cast<UINT>(instanceDescs.size());
	inputs.Flags = buildFlags;

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
	buildDesc.Inputs = inputs;
	buildDesc.ScratchAccelerationStructureData = Scratch->GetGPUVirtualAddress();
	buildDesc.DestAccelerationStructureData = Result->GetGPUVirtualAddress();
	cmdList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);
}