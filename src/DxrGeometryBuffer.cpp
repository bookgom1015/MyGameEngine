#include "DxrGeometryBuffer.h"
#include "DxMesh.h"
#include "Vertex.h"

using namespace DxrGeometryBuffer;

BOOL DxrGeometryBufferClass::Initialize(ID3D12Device5* const device) {
	md3dDevice = device;

	mNumGeometries = 0;

	return true;
}

void DxrGeometryBufferClass::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& hCpu, CD3DX12_GPU_DESCRIPTOR_HANDLE& hGpu, UINT descSize) {
	mhCpuSrv = hCpu;
	mhGpuSrv = hGpu;
	mDescSize = descSize;

	hCpu.Offset(GeometryDescriptorCount, descSize);
	hGpu.Offset(GeometryDescriptorCount, descSize);
}

void DxrGeometryBufferClass::AddGeometry(MeshGeometry* geo) {
	if (mNumGeometries >= DxrGeometryBuffer::GeometryBufferCount) return;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = 0;
	srvDesc.Buffer.StructureByteStride = sizeof(Vertex);
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	const auto hVerticesCpuSrv = CD3DX12_CPU_DESCRIPTOR_HANDLE(mhCpuSrv, mNumGeometries, mDescSize);
	const auto hIndicesCpuSrv = CD3DX12_CPU_DESCRIPTOR_HANDLE(mhCpuSrv, GeometryBufferCount + mNumGeometries, mDescSize);

	md3dDevice->CreateShaderResourceView(geo->VertexBufferGPU.Get(), &srvDesc, hVerticesCpuSrv);
	md3dDevice->CreateShaderResourceView(geo->IndexBufferGPU.Get(), &srvDesc, hIndicesCpuSrv);

	++mNumGeometries;
}