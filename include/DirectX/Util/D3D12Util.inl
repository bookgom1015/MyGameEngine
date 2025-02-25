#ifndef __D3D12UTIL_INL__
#define __D3D12UTIL_INL__

D3D12_GRAPHICS_PIPELINE_STATE_DESC D3D12Util::DefaultPsoDesc(D3D12_INPUT_LAYOUT_DESC inputLayout, DXGI_FORMAT dsvFormat) {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = inputLayout;
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.DSVFormat = dsvFormat;
	return psoDesc;
}

D3D12_GRAPHICS_PIPELINE_STATE_DESC D3D12Util::QuadPsoDesc() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = { nullptr, 0 };
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.NumRenderTargets = 1;
	psoDesc.DepthStencilState.DepthEnable = FALSE;
	psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	return psoDesc;
}

UINT D3D12Util::CeilDivide(UINT value, UINT divisor) {
	return (value + divisor - 1) / divisor;
}

UINT D3D12Util::CeilLogWithBase(UINT value, UINT base) {
	return static_cast<UINT>(ceil(log(value) / log(base)));
}

FLOAT D3D12Util::Clamp(FLOAT a, FLOAT _min, FLOAT _max) {
	return std::max(_min, std::min(_max, a));
}

FLOAT D3D12Util::Lerp(FLOAT a, FLOAT b, FLOAT t) {
	return a + t * (b - a);
}

FLOAT D3D12Util::RelativeCoef(FLOAT a, FLOAT _min, FLOAT _max) {
	FLOAT _a = Clamp(a, _min, _max);
	return (_a - _min) / (_max - _min);
}

UINT D3D12Util::NumMantissaBitsInFloatFormat(UINT FloatFormatBitLength) {
	switch (FloatFormatBitLength) {
	case 32: return 23;
	case 16: return 10;
	case 11: return 6;
	case 10: return 5;
	}
	return 0;
}

#endif // __D3D12UTIL_INL__