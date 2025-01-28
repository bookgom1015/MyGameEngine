#pragma once

#include <string>
#include <wrl.h>
#include <d3dx12.h>
#include <DDS.h>

#ifndef ReleaseCom
#define ReleaseCom(x) { if (x){ x->Release(); x = NULL; } }
#endif

#ifndef Align
#define Align(alignment, val) (((val + alignment - 1) / alignment) * alignment)
#endif

#undef min
#undef max

struct D3D12BufferCreateInfo {
	UINT64					Size = 0;
	UINT64					Alignment = 0;
	D3D12_HEAP_TYPE			HeapType = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_HEAP_FLAGS		HeapFlags = D3D12_HEAP_FLAG_NONE;
	D3D12_RESOURCE_FLAGS	Flags = D3D12_RESOURCE_FLAG_NONE;
	D3D12_RESOURCE_STATES	State = D3D12_RESOURCE_STATE_COMMON;

	D3D12BufferCreateInfo() {}
	D3D12BufferCreateInfo(UINT64 size, D3D12_RESOURCE_FLAGS flags) :
		Size(size), Flags(flags) {}
	D3D12BufferCreateInfo(UINT64 size, D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES state) :
		Size(size), HeapType(heapType), State(state) {}
	D3D12BufferCreateInfo(UINT64 size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state) :
		Size(size), Flags(flags), State(state) {}
	D3D12BufferCreateInfo(UINT64 size, UINT64 alignment, D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state) :
		Size(size), Alignment(alignment), HeapType(heapType), Flags(flags), State(state) {}
	D3D12BufferCreateInfo(UINT64 size, UINT64 alignment, D3D12_HEAP_TYPE heapType, D3D12_HEAP_FLAGS heapFlags, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state) :
		Size(size), Alignment(alignment), HeapType(heapType), HeapFlags(heapFlags), Flags(flags), State(state) {}
};

class GpuResource;

class D3D12Util {
public:
	static UINT CalcConstantBufferByteSize(UINT byteSize);

	static BOOL LoadBinary(const std::wstring& filename, Microsoft::WRL::ComPtr<ID3DBlob>& blob);

	static BOOL CreateDefaultBuffer(
		ID3D12Device* const device,
		ID3D12GraphicsCommandList* const cmdList,
		const void* initData,
		UINT64 byteSize,
		Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer,
		Microsoft::WRL::ComPtr<ID3D12Resource>& defaultBuffer);

	static BOOL CreateRootSignature(ID3D12Device* const device, const D3D12_ROOT_SIGNATURE_DESC& rootSignatureDesc, ID3D12RootSignature** rootSignature);

	static BOOL CreateBuffer(ID3D12Device* const device, D3D12BufferCreateInfo& info, ID3D12Resource** resource, ID3D12InfoQueue* infoQueue = nullptr);
	static BOOL CreateConstantBuffer(ID3D12Device* const device, ID3D12Resource** resource, UINT64 size);

	static D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(ID3D12DescriptorHeap* const descHeap, INT index, UINT descriptorSize);
	static D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(ID3D12DescriptorHeap* const descHeap, INT index, UINT descriptorSize);

	template <typename T>
	static __forceinline constexpr UINT CalcNumUintValues() {
		return static_cast<UINT>((sizeof(T) - 1) / SizeOfUint + 1);
	}

	static void UavBarrier(ID3D12GraphicsCommandList* const cmdList, ID3D12Resource* resource);
	static void UavBarriers(ID3D12GraphicsCommandList* const cmdList, ID3D12Resource* resources[], size_t length);
	static void UavBarrier(ID3D12GraphicsCommandList* const cmdList, GpuResource* resource);
	static void UavBarriers(ID3D12GraphicsCommandList* const cmdList, GpuResource* resources[], size_t length);
	
	static __forceinline D3D12_GRAPHICS_PIPELINE_STATE_DESC DefaultPsoDesc(D3D12_INPUT_LAYOUT_DESC inputLayout, DXGI_FORMAT dsvFormat);
	static __forceinline D3D12_GRAPHICS_PIPELINE_STATE_DESC QuadPsoDesc();

	static __forceinline UINT CeilDivide(UINT value, UINT divisor);
	static __forceinline UINT CeilLogWithBase(UINT value, UINT base);
	static __forceinline FLOAT Clamp(FLOAT a, FLOAT _min, FLOAT _max);
	static __forceinline FLOAT Lerp(FLOAT a, FLOAT b, FLOAT t);
	static __forceinline FLOAT RelativeCoef(FLOAT a, FLOAT _min, FLOAT _max);
	static __forceinline UINT NumMantissaBitsInFloatFormat(UINT FloatFormatBitLength);

private:
	static const size_t SizeOfUint;
};

#include "D3D12Util.inl"