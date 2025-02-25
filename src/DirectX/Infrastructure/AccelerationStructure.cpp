#include "DirectX/Infrastructure/AccelerationStructure.h"
#include "Common/Debug/Logger.h"
#include "Common/Mesh/Vertex.h"
#include "DirectX/Util/D3D12Util.h"
#include "DxMesh.h"

AccelerationStructureBuffer::~AccelerationStructureBuffer() {
	if (InstanceDesc != nullptr) InstanceDesc->Unmap(0, nullptr);
	mMappedData = nullptr;
}

BOOL AccelerationStructureBuffer::BuildBLAS(
		ID3D12Device5* const device,
		ID3D12GraphicsCommandList4* const cmdList,
		MeshGeometry* const geo) {
	D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
	geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
	geometryDesc.Triangles.VertexCount = static_cast<UINT>(geo->VertexBufferCPU->GetBufferSize() / sizeof(Vertex));
	geometryDesc.Triangles.VertexBuffer.StartAddress = geo->VertexBufferGPU->GetGPUVirtualAddress();
	geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
	geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
	geometryDesc.Triangles.IndexCount = static_cast<UINT>(geo->IndexBufferCPU->GetBufferSize() / sizeof(UINT));
	geometryDesc.Triangles.IndexBuffer = geo->IndexBufferGPU->GetGPUVirtualAddress();
	geometryDesc.Triangles.Transform3x4 = 0;
	// Mark the geometry as opaque. 
	// PERFORMANCE TIP: mark geometry as opaque whenever applicable as it can enable important ray processing optimizations.
	// Note: When rays encounter opaque geometry an any hit shader will not be executed whether it is present or not.
	geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

	// Get the size requirements for the BLAS buffers
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.pGeometryDescs = &geometryDesc;
	inputs.NumDescs = 1;
	inputs.Flags = buildFlags;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo = {};
	device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuildInfo);

	prebuildInfo.ScratchDataSizeInBytes = Align(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, prebuildInfo.ScratchDataSizeInBytes);
	prebuildInfo.ResultDataMaxSizeInBytes = Align(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, prebuildInfo.ResultDataMaxSizeInBytes);

	// Create the BLAS scratch buffer
	D3D12BufferCreateInfo bufferInfo(prebuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
	bufferInfo.Alignment = std::max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
	CheckReturn(D3D12Util::CreateBuffer(device, bufferInfo, Scratch.GetAddressOf()));

	// Create the BLAS buffer
	bufferInfo.Size = prebuildInfo.ResultDataMaxSizeInBytes;
	bufferInfo.State = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
	CheckReturn(D3D12Util::CreateBuffer(device, bufferInfo, Result.GetAddressOf()));

	// Describe and build the bottom level acceleration structure
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
	buildDesc.Inputs = inputs;
	buildDesc.ScratchAccelerationStructureData = Scratch->GetGPUVirtualAddress();
	buildDesc.DestAccelerationStructureData = Result->GetGPUVirtualAddress();

	cmdList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

	D3D12Util::UavBarrier(cmdList, Result.Get());

	return TRUE;
}

BOOL AccelerationStructureBuffer::BuildTLAS(
		ID3D12Device5* const device,
		ID3D12GraphicsCommandList4* const cmdList,
		const std::vector<D3D12_RAYTRACING_INSTANCE_DESC>& instanceDescs) {
	// Create the TLAS instance buffer
	D3D12BufferCreateInfo instanceBufferInfo;
	instanceBufferInfo.Size = instanceDescs.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC);
	instanceBufferInfo.HeapType = D3D12_HEAP_TYPE_UPLOAD;
	instanceBufferInfo.Flags = D3D12_RESOURCE_FLAG_NONE;
	instanceBufferInfo.State = D3D12_RESOURCE_STATE_GENERIC_READ;
	CheckReturn(D3D12Util::CreateBuffer(device, instanceBufferInfo, InstanceDesc.GetAddressOf()));

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

	return TRUE;
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