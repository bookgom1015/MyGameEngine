#include "ShaderManager.h"
#include "Logger.h"

#include <d3dcompiler.h>

using namespace Microsoft::WRL;

ShaderManager::ShaderManager() {}

ShaderManager::~ShaderManager() {}

bool ShaderManager::CompileShader(
		const std::wstring& inFilePath, 
		const D3D_SHADER_MACRO* inDefines, 
		const std::string& inEntryPoint, 
		const std::string& inTarget,
		const std::string& inName) {
#if defined(_DEBUG)  
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif

	ComPtr<ID3DBlob> byteCode = nullptr;
	ComPtr<ID3DBlob> errors;
	HRESULT hr = S_OK;
	hr = D3DCompileFromFile(
		inFilePath.c_str(), 
		inDefines, 
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		inEntryPoint.c_str(), 
		inTarget.c_str(), 
		compileFlags, 
		0, 
		&mShaders[inName], 
		&errors
	);

	std::wstringstream wsstream;
	if (errors != nullptr) wsstream << reinterpret_cast<char*>(errors->GetBufferPointer());
	if (FAILED(hr)) ReturnFalse(wsstream.str());

	return true;
}

ID3DBlob* ShaderManager::GetShader(const std::string& inName) {
	return mShaders[inName].Get();
}