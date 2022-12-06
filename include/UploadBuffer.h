#pragma once

#include "D3D12Util.h"
#include "d3dx12.h"

template<typename T>
class UploadBuffer {
public:
	UploadBuffer() = default;
	virtual ~UploadBuffer();

public:
	bool Initialize(ID3D12Device* device, UINT elementCount, bool isConstantBuffer);

	ID3D12Resource* Resource() const;

	void CopyData(int elementIndex, const T& data);

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> mUploadBuffer;
	BYTE* mMappedData = nullptr;

	UINT mElementByteSize = 0;
	bool mIsConstantBuffer = false;
};

#include "UploadBuffer.inl"