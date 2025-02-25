#include "DirectX/Util/D3D12Util.h"
#include "Common/Debug/Logger.h"
#include "DirectX/Infrastructure/GpuResource.h"

#include <d3dcompiler.h>
#include <fstream>

using namespace Microsoft::WRL;
using namespace D3D12Util::Descriptor;

UINT D3D12Util::CalcConstantBufferByteSize(UINT byteSize) {
	// Constant buffers must be a multiple of the minimum hardware
	// allocation size (usually 256 bytes).  So round up to nearest
	// multiple of 256.  We do this by adding 255 and then masking off
	// the lower 2 bytes which store all bits < 256.
	// Example: Suppose byteSize = 300.
	// (300 + 255) & ~255
	// 555 & ~255
	// 0x022B & ~0x00ff
	// 0x022B & 0xff00
	// 0x0200
	// 512
	return (byteSize + 255) & ~255;
}

BOOL D3D12Util::LoadBinary(const std::wstring& filename, ComPtr<ID3DBlob>& blob) {
	std::ifstream fin(filename, std::ios::binary);

	fin.seekg(0, std::ios_base::end);
	std::ifstream::pos_type size = static_cast<INT>(fin.tellg());
	fin.seekg(0, std::ios_base::beg);

	CheckHRESULT(D3DCreateBlob(size, blob.GetAddressOf()));

	fin.read(reinterpret_cast<char*>(blob->GetBufferPointer()), size);
	fin.close();

	return TRUE;
}

BOOL D3D12Util::CreateDefaultBuffer(
		ID3D12Device* const device,
		ID3D12GraphicsCommandList* const cmdList,
		const void* const initData,
		UINT64 byteSize,
		ComPtr<ID3D12Resource>& uploadBuffer,
		ComPtr<ID3D12Resource>& defaultBuffer) {
	// Create the actual default buffer resource.
	CheckHRESULT(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(byteSize),
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(defaultBuffer.GetAddressOf()))
	);

	// In order to copy CPU memory data into our default buffer, we need to create
	// an intermediate upload heap. 
	CheckHRESULT(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(byteSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(uploadBuffer.GetAddressOf()))
	);

	// Describe the data we want to copy into the default buffer.
	D3D12_SUBRESOURCE_DATA subResourceData = {};
	subResourceData.pData = initData;
	subResourceData.RowPitch = byteSize;
	subResourceData.SlicePitch = subResourceData.RowPitch;

	// Schedule to copy the data to the default buffer resource.  At a high level, the helper function UpdateSubresources
	// will copy the CPU memory into the intermediate upload heap.  Then, using ID3D12CommandList::CopySubresourceRegion,
	// the intermediate upload heap data will be copied to mBuffer.
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
	UpdateSubresources<1>(cmdList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));

	// Note: uploadBuffer has to be kept alive after the above function calls because
	// the command list has not been executed yet that performs the actual copy.
	// The caller can Release the uploadBuffer after it knows the copy has been executed.

	return TRUE;
}

BOOL D3D12Util::CreateRootSignature(ID3D12Device* const device, const D3D12_ROOT_SIGNATURE_DESC& rootSignatureDesc, ID3D12RootSignature** const rootSignature) {
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(
		&rootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(),
		errorBlob.GetAddressOf()
	);

	std::wstringstream wsstream;
	if (errorBlob != nullptr)
		wsstream << reinterpret_cast<char*>(errorBlob->GetBufferPointer());

	if (FAILED(hr))
		ReturnFalse(wsstream.str());

	CheckHRESULT(device->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(rootSignature)
	));

	return TRUE;
}

BOOL D3D12Util::CreateRootSignature(ID3D12Device* const device, const D3D12_ROOT_SIGNATURE_DESC& rootSignatureDesc, const IID& riid, void** const rootSignature) {
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(
		&rootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(),
		errorBlob.GetAddressOf()
	);

	std::wstringstream wsstream;
	if (errorBlob != nullptr)
		wsstream << reinterpret_cast<char*>(errorBlob->GetBufferPointer());

	if (FAILED(hr))
		ReturnFalse(wsstream.str());

	CheckHRESULT(device->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		riid,
		rootSignature
	));

	return TRUE;
}

BOOL D3D12Util::CreateBuffer(ID3D12Device* const device, D3D12BufferCreateInfo& info, ID3D12Resource** resource, ID3D12InfoQueue* infoQueue) {
	D3D12_HEAP_PROPERTIES heapDesc = {};
	heapDesc.Type = info.HeapType;
	heapDesc.CreationNodeMask = 1;
	heapDesc.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = info.Alignment;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Width = info.Size;
	resourceDesc.Flags = info.Flags;

	if (infoQueue != nullptr) {
		CheckHRESULT(device->CreateCommittedResource(&heapDesc, info.HeapFlags, &resourceDesc, info.State, nullptr, IID_PPV_ARGS(resource)));
		//CheckHResult(pInfoQueue, pDevice->CreateCommittedResource(&heapDesc, inInfo.HeapFlags, &resourceDesc, inInfo.State, nullptr, IID_PPV_ARGS(ppResource)));
	}
	else {
		CheckHRESULT(device->CreateCommittedResource(&heapDesc, info.HeapFlags, &resourceDesc, info.State, nullptr, IID_PPV_ARGS(resource)));
	}

	return TRUE;
}

BOOL D3D12Util::CreateConstantBuffer(ID3D12Device* const device, ID3D12Resource** resource, UINT64 size) {
	D3D12BufferCreateInfo bufferInfo((size + 255) & ~255, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
	CheckReturn(CreateBuffer(device, bufferInfo, resource));

	return TRUE;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12Util::GetCpuHandle(ID3D12DescriptorHeap* const descHeap, INT index, UINT descriptorSize) {
	auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(descHeap->GetCPUDescriptorHandleForHeapStart());
	handle.Offset(index, descriptorSize);
	return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE D3D12Util::GetGpuHandle(ID3D12DescriptorHeap* const descHeap, INT index, UINT descriptorSize) {
	auto handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(descHeap->GetGPUDescriptorHandleForHeapStart());
	handle.Offset(index, descriptorSize);
	return handle;
}

void D3D12Util::UavBarrier(ID3D12GraphicsCommandList* const cmdList, ID3D12Resource* resource) {
	D3D12_RESOURCE_BARRIER uavBarrier;
	uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarrier.UAV.pResource = resource;
	uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	cmdList->ResourceBarrier(1, &uavBarrier);
}

void D3D12Util::UavBarriers(ID3D12GraphicsCommandList* const cmdList, ID3D12Resource* resources[], size_t length) {
	std::vector<D3D12_RESOURCE_BARRIER> uavBarriers;
	for (size_t i = 0; i < length; ++i) {
		D3D12_RESOURCE_BARRIER uavBarrier;
		uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		uavBarrier.UAV.pResource = resources[i];
		uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		uavBarriers.push_back(uavBarrier);
	}
	cmdList->ResourceBarrier(static_cast<UINT>(uavBarriers.size()), uavBarriers.data());
}

void D3D12Util::UavBarrier(ID3D12GraphicsCommandList* const cmdList, GpuResource* resource) {
	D3D12_RESOURCE_BARRIER uavBarrier;
	uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarrier.UAV.pResource = resource->Resource();
	uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	cmdList->ResourceBarrier(1, &uavBarrier);
}

void D3D12Util::UavBarriers(ID3D12GraphicsCommandList* const cmdList, GpuResource* resources[], size_t length) {
	std::vector<D3D12_RESOURCE_BARRIER> uavBarriers;
	for (size_t i = 0; i < length; ++i) {
		D3D12_RESOURCE_BARRIER uavBarrier;
		uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		uavBarrier.UAV.pResource = resources[i]->Resource();
		uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		uavBarriers.push_back(uavBarrier);
	}
	cmdList->ResourceBarrier(static_cast<UINT>(uavBarriers.size()), uavBarriers.data());
}

View::Struct::Struct(ID3D12Resource* const resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc, const D3D12_CPU_DESCRIPTOR_HANDLE& handle) {
	Type = Type::E_SRV;
	Resource = resource;
	SRV = desc;
	Handle = handle;
}

View::Struct::Struct(ID3D12Resource* const resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC& desc, const D3D12_CPU_DESCRIPTOR_HANDLE& handle) {
	Type = Type::E_UAV;
	Resource = resource;
	UAV = desc;
	Handle = handle;
}

View::Struct::Struct(ID3D12Resource* const resource, const D3D12_RENDER_TARGET_VIEW_DESC& desc, const D3D12_CPU_DESCRIPTOR_HANDLE& handle) {
	Type = Type::E_RTV;
	Resource = resource;
	RTV = desc;
	Handle = handle;
}

View::Struct::Struct(ID3D12Resource* const resource, const D3D12_DEPTH_STENCIL_VIEW_DESC& desc, const D3D12_CPU_DESCRIPTOR_HANDLE& handle) {
	Type = Type::E_DSV;
	Resource = resource;
	DSV = desc;
	Handle = handle;
}


void View::Builder::Enqueue(const Struct& desc) {
	mDescriptors.push(desc);
}

void View::Builder::Enqueue(ID3D12Resource* const resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc, const D3D12_CPU_DESCRIPTOR_HANDLE& handle) {
	mDescriptors.emplace(resource, desc, handle);
}

void View::Builder::Enqueue(ID3D12Resource* const resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC& desc, const D3D12_CPU_DESCRIPTOR_HANDLE& handle) {
	mDescriptors.emplace(resource, desc, handle);
}

void View::Builder::Enqueue(ID3D12Resource* const resource, const D3D12_RENDER_TARGET_VIEW_DESC& desc, const D3D12_CPU_DESCRIPTOR_HANDLE& handle) {
	mDescriptors.emplace(resource, desc, handle);
}

void View::Builder::Enqueue(ID3D12Resource* const resource, const D3D12_DEPTH_STENCIL_VIEW_DESC& desc, const D3D12_CPU_DESCRIPTOR_HANDLE& handle) {
	mDescriptors.emplace(resource, desc, handle);
}


void View::Builder::Build(ID3D12Device5* const device) {
	while (!mDescriptors.empty()) {
		const auto& desc = mDescriptors.front();

		if (desc.Type == Type::E_DSV) device->CreateDepthStencilView(desc.Resource, &desc.DSV, desc.Handle);
		else if (desc.Type == Type::E_RTV) device->CreateRenderTargetView(desc.Resource, &desc.RTV, desc.Handle);
		else if (desc.Type == Type::E_UAV) device->CreateUnorderedAccessView(desc.Resource, nullptr, &desc.UAV, desc.Handle);
		else device->CreateShaderResourceView(desc.Resource, &desc.SRV, desc.Handle);

		mDescriptors.pop();
	}
}

Resource::Struct::Struct(
		GpuResource* const resource,
		const D3D12_HEAP_PROPERTIES& heapProp,
		const D3D12_HEAP_FLAGS& heapFlag,
		const D3D12_RESOURCE_DESC& rscDesc,
		const D3D12_RESOURCE_STATES& initialState,
		const D3D12_CLEAR_VALUE* const optClear,
		LPCWSTR name) {
	Resource = resource;
	HeapProp = heapProp;
	HeapFlag = heapFlag;
	RscDesc = rscDesc;
	InitialState = initialState;
	OptClear = optClear;
	Name = std::wstring(name);
}

void Resource::Builder::Enqueue(const Struct& desc) {
	mDescriptors.push(desc);
}

void Resource::Builder::Enqueue(
		GpuResource* const resource,
		const D3D12_HEAP_PROPERTIES& heapProp,
		const D3D12_HEAP_FLAGS& heapFlag,
		const D3D12_RESOURCE_DESC& rscDesc,
		const D3D12_RESOURCE_STATES& initialState,
		const D3D12_CLEAR_VALUE* const optClear,
		LPCWSTR name) {
	mDescriptors.emplace(resource, heapProp, heapFlag, rscDesc, initialState, optClear, name);
}

BOOL Resource::Builder::Build(ID3D12Device5* const device) {
	while (!mDescriptors.empty()) {
		const auto& desc = mDescriptors.front();

		CheckReturn(desc.Resource->Initialize(
			device,
			&desc.HeapProp,
			desc.HeapFlag,
			&desc.RscDesc,
			desc.InitialState,
			desc.OptClear,
			desc.Name.c_str()
		));

		mDescriptors.pop();
	}

	return TRUE;
}

RootSignature::Struct::Struct(const D3D12_ROOT_SIGNATURE_DESC& desc, const IID& riid, void** ptr) {
	Desc = desc;
	RIID = riid;
	Ptr = ptr;
}

void RootSignature::Builder::Enqueue(const Struct& desc) {
	mDescriptors.push(desc);
}

void RootSignature::Builder::Enqueue(const D3D12_ROOT_SIGNATURE_DESC& desc, const IID& riid, void** ptr) {
	mDescriptors.emplace(desc, riid, ptr);
}

BOOL RootSignature::Builder::Build(ID3D12Device5* const device) {
	while (!mDescriptors.empty()) {
		auto desc = mDescriptors.front();

		CheckReturn(D3D12Util::CreateRootSignature(device, desc.Desc, desc.RIID, desc.Ptr));

		mDescriptors.pop();
	}

	return TRUE;
}


PipelineState::Struct::Struct(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc, const IID& riid, void** ptr) {
	Graphics = desc;
	RIID = riid;
	Ptr = ptr;
	Type = Type::E_Grapchis;
}

PipelineState::Struct::Struct(const D3D12_COMPUTE_PIPELINE_STATE_DESC& desc, const IID& riid, void** ptr) {
	Compute = desc;
	RIID = riid;
	Ptr = ptr;
	Type = Type::E_Compute;
}

void PipelineState::Builder::Enqueue(const Struct& desc) {
	mDescriptors.push(desc);
}

void PipelineState::Builder::Enqueue(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc, const IID& riid, void** ptr) {
	mDescriptors.emplace(desc, riid, ptr);
}

void PipelineState::Builder::Enqueue(const D3D12_COMPUTE_PIPELINE_STATE_DESC& desc, const IID& riid, void** ptr) {
	mDescriptors.emplace(desc, riid, ptr);
}

BOOL PipelineState::Builder::Build(ID3D12Device5* const device) {
	while (!mDescriptors.empty()) {
		auto& desc = mDescriptors.front();
		if (desc.Type == Type::E_Compute) {
			CheckHRESULT(device->CreateComputePipelineState(&desc.Compute, desc.RIID, desc.Ptr));
		}
		else {
			CheckHRESULT(device->CreateGraphicsPipelineState(&desc.Graphics, desc.RIID, desc.Ptr));
		}

		mDescriptors.pop();
	}

	return TRUE;
}