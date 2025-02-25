#pragma once

#include "DirectX/Util/D3D12Util.h"

template<typename T>
class UploadBuffer {
public:
	UploadBuffer() = default;
	virtual ~UploadBuffer();

public:
	BOOL Initialize(ID3D12Device* const device, UINT elementCount, BOOL isConstantBuffer);

	ID3D12Resource* Resource() const;

	void CopyData(INT elementIndex, const T& data);

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> mUploadBuffer;
	BYTE* mMappedData = nullptr;

	UINT mElementByteSize = 0;
	BOOL mIsConstantBuffer = FALSE;

	BOOL bIsDirty = TRUE;
};

#include "UploadBuffer.inl"