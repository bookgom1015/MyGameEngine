#pragma once

#include <string>
#include <queue>
#include <wrl.h>
#include <DirectX/d3dx12.h>
#include <DirectX/DDS.h>

#undef min
#undef max

#ifndef ReleaseCom
	#define ReleaseCom(x) { if (x){ x->Release(); x = NULL; } }
#endif

#ifndef Align
	#define Align(alignment, val) (((val + alignment - 1) / alignment) * alignment)
#endif

class GpuResource;

struct D3D12BufferCreateInfo {
	UINT64					Size	  = 0;
	UINT64					Alignment = 0;
	D3D12_HEAP_TYPE			HeapType  = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_HEAP_FLAGS		HeapFlags = D3D12_HEAP_FLAG_NONE;
	D3D12_RESOURCE_FLAGS	Flags	  = D3D12_RESOURCE_FLAG_NONE;
	D3D12_RESOURCE_STATES	State	  = D3D12_RESOURCE_STATE_COMMON;

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

namespace D3D12Util {
	const size_t SizeOfUint = sizeof(UINT);

	UINT CalcConstantBufferByteSize(UINT byteSize);

	BOOL LoadBinary(const std::wstring& filename, Microsoft::WRL::ComPtr<ID3DBlob>& blob);

	BOOL CreateDefaultBuffer(
		ID3D12Device* const device,
		ID3D12GraphicsCommandList* const cmdList,
		const void* const initData,
		UINT64 byteSize,
		Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer,
		Microsoft::WRL::ComPtr<ID3D12Resource>& defaultBuffer);

	BOOL CreateRootSignature(ID3D12Device* const device, const D3D12_ROOT_SIGNATURE_DESC& rootSignatureDesc, ID3D12RootSignature** const rootSignature);
	BOOL CreateRootSignature(ID3D12Device* const device, const D3D12_ROOT_SIGNATURE_DESC& rootSignatureDesc, const IID& riid, void** const rootSignature);

	BOOL CreateBuffer(ID3D12Device* const device, D3D12BufferCreateInfo& info, ID3D12Resource** resource, ID3D12InfoQueue* infoQueue = nullptr);
	BOOL CreateConstantBuffer(ID3D12Device* const device, ID3D12Resource** resource, UINT64 size);

	D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(ID3D12DescriptorHeap* const descHeap, INT index, UINT descriptorSize);
	D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(ID3D12DescriptorHeap* const descHeap, INT index, UINT descriptorSize);

	template <typename T>
	__forceinline constexpr UINT CalcNumUintValues() {
		return static_cast<UINT>((sizeof(T) - 1) / SizeOfUint + 1);
	}

	void UavBarrier(ID3D12GraphicsCommandList* const cmdList, ID3D12Resource* resource);
	void UavBarriers(ID3D12GraphicsCommandList* const cmdList, ID3D12Resource* resources[], size_t length);
	void UavBarrier(ID3D12GraphicsCommandList* const cmdList, GpuResource* resource);
	void UavBarriers(ID3D12GraphicsCommandList* const cmdList, GpuResource* resources[], size_t length);
	
	__forceinline D3D12_GRAPHICS_PIPELINE_STATE_DESC DefaultPsoDesc(D3D12_INPUT_LAYOUT_DESC inputLayout, DXGI_FORMAT dsvFormat);
	__forceinline D3D12_GRAPHICS_PIPELINE_STATE_DESC QuadPsoDesc();

	__forceinline UINT CeilDivide(UINT value, UINT divisor);
	__forceinline UINT CeilLogWithBase(UINT value, UINT base);
	__forceinline FLOAT Clamp(FLOAT a, FLOAT _min, FLOAT _max);
	__forceinline FLOAT Lerp(FLOAT a, FLOAT b, FLOAT t);
	__forceinline FLOAT RelativeCoef(FLOAT a, FLOAT _min, FLOAT _max);
	__forceinline UINT NumMantissaBitsInFloatFormat(UINT FloatFormatBitLength);

	namespace Descriptor {
		namespace View {
			enum Type {
				E_SRV = 0,
				E_UAV,
				E_RTV,
				E_DSV,
				Count
			};

			struct Struct {
				Struct() = default;
				Struct(ID3D12Resource* const resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
				Struct(ID3D12Resource* const resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC& desc, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
				Struct(ID3D12Resource* const resource, const D3D12_RENDER_TARGET_VIEW_DESC& desc, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
				Struct(ID3D12Resource* const resource, const D3D12_DEPTH_STENCIL_VIEW_DESC& desc, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);

				union {
					D3D12_SHADER_RESOURCE_VIEW_DESC		SRV;
					D3D12_UNORDERED_ACCESS_VIEW_DESC	UAV;
					D3D12_RENDER_TARGET_VIEW_DESC		RTV;
					D3D12_DEPTH_STENCIL_VIEW_DESC		DSV;
				};
				Type						Type;
				ID3D12Resource* Resource;
				D3D12_CPU_DESCRIPTOR_HANDLE	Handle;
			};

			class Builder {
			public:
				Builder() = default;
				virtual ~Builder() = default;

			public:
				void Enqueue(const Struct& desc);
				void Enqueue(ID3D12Resource* const resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
				void Enqueue(ID3D12Resource* const resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC& desc, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
				void Enqueue(ID3D12Resource* const resource, const D3D12_RENDER_TARGET_VIEW_DESC& desc, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
				void Enqueue(ID3D12Resource* const resource, const D3D12_DEPTH_STENCIL_VIEW_DESC& desc, const D3D12_CPU_DESCRIPTOR_HANDLE& handle);
				void Build(ID3D12Device5* const device);

			private:
				std::queue<Struct> mDescriptors;
			};
		}

		namespace Resource {
			struct Struct {
				Struct() = default;
				Struct(
					GpuResource* const resource,
					const D3D12_HEAP_PROPERTIES& heapProp,
					const D3D12_HEAP_FLAGS& heapFlag,
					const D3D12_RESOURCE_DESC& rscDesc,
					const D3D12_RESOURCE_STATES& initialState,
					const D3D12_CLEAR_VALUE* const optClear,
					LPCWSTR name);

				GpuResource* Resource;
				D3D12_HEAP_PROPERTIES		HeapProp;
				D3D12_HEAP_FLAGS			HeapFlag;
				D3D12_RESOURCE_DESC			RscDesc;
				D3D12_RESOURCE_STATES		InitialState;
				const D3D12_CLEAR_VALUE* OptClear;
				std::wstring				Name;
			};

			class Builder {
			public:
				Builder() = default;
				virtual ~Builder() = default;

			public:
				void Enqueue(const Struct& desc);
				void Enqueue(
					GpuResource* const resource,
					const D3D12_HEAP_PROPERTIES& heapProp,
					const D3D12_HEAP_FLAGS& heapFlag,
					const D3D12_RESOURCE_DESC& rscDesc,
					const D3D12_RESOURCE_STATES& initialState,
					const D3D12_CLEAR_VALUE* const optClear,
					LPCWSTR name);

				BOOL Build(ID3D12Device5* const device);

			private:
				std::queue<Struct> mDescriptors;
			};
		}

		namespace RootSignature {
			struct Struct {
				Struct() = default;
				Struct(const D3D12_ROOT_SIGNATURE_DESC& desc, const IID& riid, void** ptr);

				D3D12_ROOT_SIGNATURE_DESC Desc;
				IID RIID;
				void** Ptr;
			};

			class Builder {
			public:
				Builder() = default;
				virtual ~Builder() = default;

			public:
				void Enqueue(const Struct& desc);
				void Enqueue(const D3D12_ROOT_SIGNATURE_DESC& desc, const IID& riid, void** ptr);

				BOOL Build(ID3D12Device5* const device);

			private:
				std::queue<Struct> mDescriptors;
			};
		}

		namespace PipelineState {
			enum Type {
				E_Grapchis = 0,
				E_Compute,
				Count
			};

			struct Struct {
				Struct() = default;
				Struct(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc, const IID& riid, void** ptr);
				Struct(const D3D12_COMPUTE_PIPELINE_STATE_DESC& desc, const IID& riid, void** ptr);

				union {
					D3D12_GRAPHICS_PIPELINE_STATE_DESC Graphics;
					D3D12_COMPUTE_PIPELINE_STATE_DESC Compute;
				};
				IID RIID;
				void** Ptr;
				Type Type;
			};

			class Builder {
			public:
				Builder() = default;
				virtual ~Builder() = default;

			public:
				void Enqueue(const Struct& desc);
				void Enqueue(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc, const IID& riid, void** ptr);
				void Enqueue(const D3D12_COMPUTE_PIPELINE_STATE_DESC& desc, const IID& riid, void** ptr);

				BOOL Build(ID3D12Device5* const device);

			private:
				std::queue<Struct> mDescriptors;
			};
		}
	}
};

#include "D3D12Util.inl"