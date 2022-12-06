#pragma once

#include <d3d12.h>
#include <dxcapi.h>

#include <string>
#include <unordered_map>
#include <wrl.h>

class ShaderManager {
public:
	ShaderManager();
	virtual ~ShaderManager();

public:
	bool CompileShader(
		const std::wstring& inFilePath,
		const D3D_SHADER_MACRO* inDefines,
		const std::string& inEntryPoint,
		const std::string& inTarget,
		const std::string& inName);

	ID3DBlob* GetShader(const std::string& inName);

private:
	bool bIsCleanedUp = false;

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> mShaders;
};