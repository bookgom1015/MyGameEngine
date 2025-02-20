#include "ShaderTable.h"
#include "Logger.h"
#include "RenderMacros.h"

#include <d3dx12.h>

using namespace Microsoft::WRL;

GpuUploadBuffer::~GpuUploadBuffer() {
	if (mResource.Get())
		mResource->Unmap(0, nullptr);
}

ComPtr<ID3D12Resource> GpuUploadBuffer::GetResource() {
	return mResource;
}

BOOL GpuUploadBuffer::Allocate(ID3D12Device* const pDevice, UINT bufferSize, LPCWSTR resourceName) {
	CheckHRESULT(pDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mResource)));
	//mResource->SetName(resourceName);

	return TRUE;
}

BOOL GpuUploadBuffer::MapCpuWriteOnly(std::uint8_t*& pData) {
	// We don't unmap this until the app closes. Keeping buffer mapped for the lifetime of the resource is okay.
	// We do not intend to read from this resource on the CPU.
	CheckHRESULT(mResource->Map(0, &CD3DX12_RANGE(0, 0), reinterpret_cast<void**>(&pData)));

	return TRUE;
}

ShaderRecord::ShaderRecord(void* const pShaderIdentifier, UINT shaderIdentifierSize) :
	mShaderIdentifier(pShaderIdentifier, shaderIdentifierSize) {}

ShaderRecord::ShaderRecord(void* const pShaderIdentifier, UINT shaderIdentifierSize, void* const pLocalRootArguments, UINT localRootArgumentsSize) :
	mShaderIdentifier(pShaderIdentifier, shaderIdentifierSize),
	mLocalRootArguments(pLocalRootArguments, localRootArgumentsSize) {}

void ShaderRecord::CopyTo(void* const dest) const {
	uint8_t* byteDest = static_cast<uint8_t*>(dest);
	std::memcpy(byteDest, mShaderIdentifier.Ptr, mShaderIdentifier.Size);
	if (mLocalRootArguments.Ptr) std::memcpy(byteDest + mShaderIdentifier.Size, mLocalRootArguments.Ptr, mLocalRootArguments.Size);
}

ShaderRecord::PointerWithSize::PointerWithSize() :
	Ptr(nullptr), Size(0) {}

ShaderRecord::PointerWithSize::PointerWithSize(void* const ptr, UINT size) :
	Ptr(ptr), Size(size) {}

ShaderTable::ShaderTable(ID3D12Device* const device, UINT numShaderRecords, UINT shaderRecordSize, LPCWSTR resourceName) : mDevice(device) {
	mName = resourceName != nullptr ? std::wstring(resourceName) : L"";
	mShaderRecordSize = Align(shaderRecordSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
	mShaderRecords.reserve(numShaderRecords);
	mBufferSize = numShaderRecords * mShaderRecordSize;
}

BOOL ShaderTable::Initialze() {
	CheckReturn(Allocate(mDevice, mBufferSize, mName.length() > 0 ? mName.c_str() : nullptr));
	CheckReturn(MapCpuWriteOnly(mMappedShaderRecords));

	return TRUE;
}

BOOL ShaderTable::push_back(const ShaderRecord& shaderRecord) {
	CheckReturn(mShaderRecords.size() < mShaderRecords.capacity());

	mShaderRecords.push_back(shaderRecord);
	shaderRecord.CopyTo(mMappedShaderRecords);
	mMappedShaderRecords += mShaderRecordSize;

	return TRUE;
}

std::uint8_t* ShaderTable::GetMappedShaderRecords() {
	return mMappedShaderRecords;
}

UINT ShaderTable::GetShaderRecordSize() {
	return mShaderRecordSize;
}

void ShaderTable::DebugPrint(std::unordered_map<void*, std::wstring>& shaderIdToStringMap) {
	std::wstringstream wsstream;
	wsstream << L"|--------------------------------------------------------------------\n";
	wsstream << L"|Shader table - " << mName << L": " << mShaderRecordSize << L" | " << mShaderRecords.size() * mShaderRecordSize << L" bytes" << std::endl;
	for (UINT i = 0; i < mShaderRecords.size(); ++i) {
		wsstream << L"| [" << i << L"]: " << shaderIdToStringMap[mShaderRecords[i].mShaderIdentifier.Ptr] << L", ";
		wsstream << mShaderRecords[i].mShaderIdentifier.Size << L" + " << mShaderRecords[i].mLocalRootArguments.Size << L" bytes" << std::endl;
	}
	wsstream << L"|--------------------------------------------------------------------";
	WLogln(wsstream.str());
}