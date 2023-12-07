#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <vector>

struct MeshGeometry;

class AccelerationStructureBuffer {
public:
	AccelerationStructureBuffer() = default;
	virtual ~AccelerationStructureBuffer();

public:
	BOOL BuildBLAS(
		ID3D12Device5* const device,
		ID3D12GraphicsCommandList4* const cmdList,
		MeshGeometry* const geo);
	BOOL BuildTLAS(
		ID3D12Device5* const device,
		ID3D12GraphicsCommandList4* const cmdList,
		const std::vector<D3D12_RAYTRACING_INSTANCE_DESC>& instanceDescs);
	void UpdateTLAS(
		ID3D12GraphicsCommandList4* const cmdList,
		const std::vector<D3D12_RAYTRACING_INSTANCE_DESC>& instanceDescs);

public:
	Microsoft::WRL::ComPtr<ID3D12Resource> Scratch;
	Microsoft::WRL::ComPtr<ID3D12Resource> Result;
	Microsoft::WRL::ComPtr<ID3D12Resource> InstanceDesc;	// only used in top-level AS
	UINT64 ResultDataMaxSizeInBytes;

private:
	void* mMappedData = nullptr;
};