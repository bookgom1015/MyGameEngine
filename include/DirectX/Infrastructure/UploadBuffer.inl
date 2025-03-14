#ifndef __UPLOADBUFFER_INL__
#define __UPLOADBUFFER_INL__

template <typename T>
UploadBuffer<T>::~UploadBuffer() {
	if (mUploadBuffer != nullptr) mUploadBuffer->Unmap(0, nullptr);
	mMappedData = nullptr;
}

template <typename T>
BOOL UploadBuffer<T>::Initialize(ID3D12Device* const device, UINT elementCount, BOOL isConstantBuffer) {
	mIsConstantBuffer = isConstantBuffer;
	mElementByteSize = sizeof(T);

	// Constant buffer elements need to be multiples of 256 bytes.
	// This is because the hardware can only view constant data 
	// at m*256 byte offsets and of n*256 byte lengths. 
	// typedef struct D3D12_CONSTANT_BUFFER_VIEW_DESC {
	// UINT64 OffsetInBytes; // multiple of 256
	// UINT   SizeInBytes;   // multiple of 256
	// } D3D12_CONSTANT_BUFFER_VIEW_DESC;
	if (isConstantBuffer)
		mElementByteSize = D3D12Util::CalcConstantBufferByteSize(sizeof(T));

	if (FAILED(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(mElementByteSize * elementCount),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mUploadBuffer)))) return FALSE;

	if (FAILED(mUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mMappedData)))) return FALSE;

	// We do not need to unmap until we are done with the resource.  However, we must not write to
	// the resource while it is in use by the GPU (so we must use synchronization techniques).

	return TRUE;
}

template <typename T>
ID3D12Resource* UploadBuffer<T>::Resource() const {
	return mUploadBuffer.Get();
}

template <typename T>
void UploadBuffer<T>::CopyData(INT elementIndex, const T& data) {
	std::memcpy(&mMappedData[elementIndex * mElementByteSize], &data, sizeof(T));
}

#endif // __UPLOADBUFFER_INL__