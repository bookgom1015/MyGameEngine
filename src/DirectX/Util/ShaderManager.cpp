#include "DirectX/Util/ShaderManager.h"
#include "Common/Debug/Logger.h"
#include "DirectX/Util/D3D12Util.h"

#include <d3dcompiler.h>
#include <fstream>
#include <filesystem>

using namespace Microsoft::WRL;

ShaderManager::~ShaderManager() {
	if (!bIsCleanedUp) CleanUp();
}

BOOL ShaderManager::Initialize(UINT numThreads) {
	CheckHRESULT(mDxcDllHelper.Initialize());

	mThreadCount = numThreads;

	mUtils.resize(numThreads);
	mCompilers.resize(numThreads);
	mCompileMutexes.resize(numThreads);

	for (UINT i = 0; i < numThreads; ++i) {
		DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&mUtils[i]));
		DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&mCompilers[i]));
		mCompileMutexes[i] = std::make_unique<std::mutex>();
	}

	return TRUE;
}

void ShaderManager::CleanUp() {
	mDxcDllHelper.Cleanup();
	bIsCleanedUp = TRUE;
}

BOOL ShaderManager::CompileShader(
	const std::wstring& filePath,
	const D3D_SHADER_MACRO* defines,
	const std::string& entryPoint,
	const std::string& target,
	const CHAR* name) {
#if defined(_DEBUG)  
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif

	HRESULT hr = S_OK;

	ComPtr<ID3DBlob> byteCode = nullptr;
	ComPtr<ID3DBlob> errors;
	hr = D3DCompileFromFile(filePath.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		entryPoint.c_str(), target.c_str(), compileFlags, 0, &mShaders[name], &errors);

	std::wstringstream wsstream;
	if (errors != nullptr)
		wsstream << reinterpret_cast<CHAR*>(errors->GetBufferPointer());

	if (FAILED(hr)) {
		ReturnFalse(wsstream.str());
	}

	return TRUE;
}

BOOL ShaderManager::CompileShader(
	const std::wstring& filePath,
	const D3D_SHADER_MACRO* defines,
	const std::string& entryPoint,
	const std::string& target,
	const std::string& name) {
	return CompileShader(filePath, defines, entryPoint, target, name.c_str());
}

BOOL ShaderManager::CompileShader(const D3D12ShaderInfo& shaderInfo, const CHAR* name) {
	std::ifstream fin(shaderInfo.FileName, std::ios::ate | std::ios::binary);
	if (!fin.is_open()) {
		std::wstring msg(L"Failed to open shader file: ");
		msg.append(shaderInfo.FileName);
		ReturnFalse(msg);
	}

	size_t fileSize = static_cast<size_t>(fin.tellg());
	std::vector<CHAR> data(fileSize);

	fin.seekg(0);
	fin.read(data.data(), fileSize);
	fin.close();

	IDxcResult* result;
	{
		std::unique_lock<std::mutex> lock_compile;
		UINT tid = 0;
		for (UINT i = 0; i < mThreadCount; ++i) {
			lock_compile = std::unique_lock<std::mutex>(*mCompileMutexes[i], std::defer_lock);
			if (!lock_compile.try_lock()) continue;

			tid = i;

			break;
		}

		auto& utils = mUtils[tid];
		auto& compiler = mCompilers[tid];

		ComPtr<IDxcBlobEncoding> shaderText;
		CheckHRESULT(utils->CreateBlob(data.data(), static_cast<UINT32>(fileSize), 0, &shaderText));

		ComPtr<IDxcIncludeHandler> includeHandler;
		CheckHRESULT(utils->CreateDefaultIncludeHandler(&includeHandler));

		DxcBuffer sourceBuffer;
		sourceBuffer.Ptr = shaderText->GetBufferPointer();
		sourceBuffer.Size = shaderText->GetBufferSize();
		sourceBuffer.Encoding = 0;

		std::vector<LPCWSTR> arguments;

#ifdef _DEBUG
		arguments.push_back(L"-Qembed_debug");
		arguments.push_back(DXC_ARG_WARNINGS_ARE_ERRORS); // -WX
		arguments.push_back(DXC_ARG_DEBUG); // -Zi
#else
		// Strip reflection data and pdbs
		//arguments.push_back(L"-Qstrip_debug");
		//arguments.push_back(L"-Qstrip_reflect");
#endif

		ComPtr<IDxcCompilerArgs> compilerArgs;
		CheckHRESULT(utils->BuildArguments(
			shaderInfo.FileName,
			shaderInfo.EntryPoint,
			shaderInfo.TargetProfile,
			arguments.data(),
			static_cast<UINT32>(arguments.size()),
			shaderInfo.Defines,
			shaderInfo.DefineCount,
			&compilerArgs));

		CheckHRESULT(compiler->Compile(
			&sourceBuffer,
			compilerArgs->GetArguments(),
			static_cast<UINT32>(compilerArgs->GetCount()),
			includeHandler.Get(),
			IID_PPV_ARGS(&result)));

		HRESULT hr;
		CheckHRESULT(result->GetStatus(&hr));
		if (FAILED(hr)) {
			IDxcBlobEncoding* error;
			CheckHRESULT(result->GetErrorBuffer(&error));

			auto bufferSize = error->GetBufferSize();
			std::vector<CHAR> infoLog(bufferSize + 1);
			std::memcpy(infoLog.data(), error->GetBufferPointer(), bufferSize);
			infoLog[bufferSize] = 0;

			std::string errorMsg = "Shader Compiler Error:\n";
			errorMsg.append(infoLog.data());

			std::wstring errorMsgW;
			errorMsgW.assign(errorMsg.begin(), errorMsg.end());

			ReturnFalse(errorMsgW);
		}
	}

#if _DEBUG
	{
		ComPtr<IDxcBlob> pdbBlob;
		ComPtr<IDxcBlobUtf16> debugDataPath;
		result->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&pdbBlob), &debugDataPath);

		std::wstring fileNameW = shaderInfo.FileName;
		auto extIdx = fileNameW.rfind(L'.');
		std::wstring fileNameWithExtW = fileNameW.substr(0, extIdx);
		//fileNameWithExtW.append(L"_");
		//fileNameWithExtW.append(shaderInfo.EntryPoint);
		fileNameWithExtW.append(L".pdb");
		auto delimIdx = fileNameWithExtW.rfind(L'\\');

		{
			std::wstring filePathW = fileNameWithExtW.substr(0, delimIdx);
			filePathW.append(L"\\debug");

			std::filesystem::path debugDir(filePathW);
			if (!std::filesystem::exists(debugDir)) std::filesystem::create_directory(debugDir);
		}

		fileNameWithExtW.insert(delimIdx, L"\\debug");

		std::string fileName;
		for (auto ch : fileNameWithExtW)
			fileName.push_back(static_cast<CHAR>(ch));


		{
			std::unique_lock lock_export(mExportMutex);

			auto nameIdx = mExportedPDBs.find(fileName);
			if (nameIdx == mExportedPDBs.end()) {
				mExportedPDBs.insert(fileName);

				lock_export.unlock();

				std::ofstream fout;
				fout.open(fileName, std::ios::beg | std::ios::binary | std::ios::trunc);
				if (!fout.is_open()) {
					std::wstring msg = L"Failed to open file for PDB: ";
					msg.append(fileNameWithExtW);
					ReturnFalse(msg);
				}

				fout.write(reinterpret_cast<const CHAR*>(pdbBlob->GetBufferPointer()), pdbBlob->GetBufferSize());
				fout.close();
			}
		}
	}
#endif 

	CheckHRESULT(result->GetResult(&mDxcShaders[name]));

	return TRUE;
}

BOOL ShaderManager::CompileShader(const D3D12ShaderInfo& shaderInfo, const std::string& name) {
	return CompileShader(shaderInfo, name.c_str());
}

ID3DBlob* ShaderManager::GetShader(const CHAR* name) {
	return mShaders[name].Get();
}

ID3DBlob* ShaderManager::GetShader(const std::string& name) {
	return GetShader(name.c_str());
}

IDxcBlob* ShaderManager::GetDxcShader(const CHAR* name) {
	return mDxcShaders[name].Get();
}

IDxcBlob* ShaderManager::GetDxcShader(const std::string& name) {
	return GetDxcShader(name.c_str());
}