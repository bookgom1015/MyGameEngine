#pragma once

#include <d3d12.h>
#include <dxcapi.h>
#include <dxc/Support/dxcapi.use.h>

#include <string>
#include <unordered_map>
#include <vector>
#include <set>
#include <mutex>
#include <wrl.h>

struct D3D12ShaderInfo {
	LPCWSTR		FileName = nullptr;
	LPCWSTR		EntryPoint = nullptr;
	LPCWSTR		TargetProfile = nullptr;
	LPCWSTR*	Arguments = nullptr;
	DxcDefine*	Defines = nullptr;
	UINT32		ArgCount = 0;
	UINT32		DefineCount = 0;

	D3D12ShaderInfo() = default;
	D3D12ShaderInfo(LPCWSTR fileName, LPCWSTR entryPoint, LPCWSTR profile) {
		FileName = fileName;
		EntryPoint = entryPoint;
		TargetProfile = profile;
	}
	D3D12ShaderInfo(LPCWSTR fileName, LPCWSTR entryPoint, LPCWSTR profile, DxcDefine* defines, UINT32 defCount) {
		FileName = fileName;
		EntryPoint = entryPoint;
		TargetProfile = profile;
		Defines = defines;
		DefineCount = defCount;
	}
};

class ShaderManager {
public:
	ShaderManager() = default;
	virtual ~ShaderManager();

public:
	BOOL Initialize(UINT numThreads = 1);
	void CleanUp();

	BOOL CompileShader(
		const std::wstring& filePath,
		const D3D_SHADER_MACRO* defines,
		const std::string& entryPoint,
		const std::string& target,
		const CHAR* name);
	BOOL CompileShader(
		const std::wstring& filePath,
		const D3D_SHADER_MACRO* defines,
		const std::string& entryPoint,
		const std::string& target,
		const std::string& name);

	BOOL CompileShader(const D3D12ShaderInfo& shaderInfo, const CHAR* name);
	BOOL CompileShader(const D3D12ShaderInfo& shaderInfo, const std::string& name);

	ID3DBlob* GetShader(const CHAR* name);
	ID3DBlob* GetShader(const std::string& name);
	IDxcBlob* GetDxcShader(const CHAR* name);
	IDxcBlob* GetDxcShader(const std::string& name);

private:
	BOOL bIsCleanedUp = FALSE;

	dxc::DxcDllSupport mDxcDllHelper;
	std::vector<Microsoft::WRL::ComPtr<IDxcUtils>> mUtils;
	std::vector<Microsoft::WRL::ComPtr<IDxcCompiler3>> mCompilers;
	std::vector<std::unique_ptr<std::mutex>> mCompileMutexes;

	std::unordered_map<const CHAR*, Microsoft::WRL::ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<const CHAR*, Microsoft::WRL::ComPtr<IDxcBlob>> mDxcShaders;

	std::set<std::string> mExportedPDBs;
	std::mutex mExportMutex;

	UINT mThreadCount = 1;
};