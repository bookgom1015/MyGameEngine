#include "ShaderManager.h"
#include "Logger.h"
#include "RenderMacros.h"

#include <d3dcompiler.h>
#include <fstream>
#include <filesystem>

using namespace Microsoft::WRL;

ShaderManager::~ShaderManager() {
	if (!bIsCleanedUp) {
		CleanUp();
	}
}

bool ShaderManager::Initialize() {
	CheckHRESULT(mDxcDllHelper.Initialize());
	DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&mUtils));
	DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&mCompiler));
	return true;
}

void ShaderManager::CleanUp() {
	//mDxcDllHelper.Cleanup();
	bIsCleanedUp = true;
}

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

	HRESULT hr = S_OK;

	ComPtr<ID3DBlob> byteCode = nullptr;
	ComPtr<ID3DBlob> errors;
	hr = D3DCompileFromFile(inFilePath.c_str(), inDefines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		inEntryPoint.c_str(), inTarget.c_str(), compileFlags, 0, &mShaders[inName], &errors);

	std::wstringstream wsstream;
	if (errors != nullptr)
		wsstream << reinterpret_cast<char*>(errors->GetBufferPointer());

	if (FAILED(hr)) {
		ReturnFalse(wsstream.str());
	}

	return true;
}

bool ShaderManager::CompileShader(const D3D12ShaderInfo& shaderInfo, const std::string& name) {
	std::ifstream fin(shaderInfo.FileName, std::ios::ate | std::ios::binary);
	if (!fin.is_open()) {
		std::wstring msg(L"Failed to open shader file: ");
		msg.append(shaderInfo.FileName);
		ReturnFalse(msg);
	}

	size_t fileSize = static_cast<size_t>(fin.tellg());
	
	std::vector<char> data(fileSize);

	fin.seekg(0);
	fin.read(data.data(), fileSize);
	fin.close();

	IDxcBlobEncoding* shaderText = nullptr;
	mUtils->CreateBlob(data.data(), static_cast<UINT32>(fileSize), 0, &shaderText);

	ComPtr<IDxcIncludeHandler> includeHandler;
	mUtils->CreateDefaultIncludeHandler(&includeHandler);
		
	DxcBuffer sourceBuffer;
	sourceBuffer.Ptr = shaderText->GetBufferPointer();
	sourceBuffer.Size = shaderText->GetBufferSize();
	sourceBuffer.Encoding = 0;

	std::vector<LPCWSTR> arguments;

	// Strip reflection data and pdbs
	arguments.push_back(L"-Qembed_debug");
	//arguments.push_back(L"-Qstrip_debug");
	//arguments.push_back(L"-Qstrip_reflect");

	arguments.push_back(DXC_ARG_WARNINGS_ARE_ERRORS); // -WX
	arguments.push_back(DXC_ARG_DEBUG); // -Zi

	ComPtr<IDxcCompilerArgs> compilerArgs;
	mUtils->BuildArguments(
		shaderInfo.FileName,
		shaderInfo.EntryPoint, 
		shaderInfo.TargetProfile, 
		arguments.data(), 
		static_cast<UINT32>(arguments.size()),
		shaderInfo.Defines, 
		shaderInfo.DefineCount, 
		&compilerArgs);

	IDxcResult* result;
	mCompiler->Compile(
		&sourceBuffer, 
		compilerArgs->GetArguments(), 
		static_cast<UINT32>(compilerArgs->GetCount()), 
		includeHandler.Get(), 
		IID_PPV_ARGS(&result));
	
	HRESULT hr;
	CheckHRESULT(result->GetStatus(&hr));
	if (FAILED(hr)) {
		IDxcBlobEncoding* error;
		CheckHRESULT(result->GetErrorBuffer(&error));

		auto bufferSize = error->GetBufferSize();
		std::vector<char> infoLog(bufferSize + 1);
		std::memcpy(infoLog.data(), error->GetBufferPointer(), bufferSize);
		infoLog[bufferSize] = 0;

		std::string errorMsg = "Shader Compiler Error:\n";
		errorMsg.append(infoLog.data());

		std::wstring errorMsgW;
		errorMsgW.assign(errorMsg.begin(), errorMsg.end());

		ReturnFalse(errorMsgW);
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
			fileName.push_back(static_cast<char>(ch));

		std::ofstream fout;
		fout.open(fileName, std::ios::beg | std::ios::binary | std::ios::trunc);
		if (!fout.is_open()) {
			std::wstring msg = L"Failed to open file for PDB: ";
			msg.append(fileNameWithExtW);
			ReturnFalse(msg);
		}

		fout.write(reinterpret_cast<const char*>(pdbBlob->GetBufferPointer()), pdbBlob->GetBufferSize());
		fout.close();
	}
#endif 

	CheckHRESULT(result->GetResult(&mDxcShaders[name]));

	return true;
}

ID3DBlob* ShaderManager::GetShader(const std::string& inName) {
	return mShaders[inName].Get();
}

IDxcBlob* ShaderManager::GetDxcShader(const std::string& inName) {
	return mDxcShaders[inName].Get();
}