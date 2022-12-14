#pragma once

#include <Windows.h>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#ifndef Log
#define Log(x, ...)												\
	{															\
		std::vector<std::string> texts = { x, __VA_ARGS__ };	\
		std::stringstream _sstream;								\
																\
		for (const auto& text : texts)							\
			_sstream << text;									\
																\
		Logger::LogFunc(_sstream.str());						\
	}
#endif

#ifndef Logln
#define Logln(x, ...)											\
	{															\
		std::vector<std::string> texts = { x, __VA_ARGS__ };	\
		std::stringstream _sstream;								\
																\
		for (const auto& text : texts)							\
			_sstream << text;									\
		_sstream << '\n';										\
																\
		Logger::LogFunc(_sstream.str());						\
	}
#endif

#ifndef WLog
#define WLog(x, ...)											\
	{															\
		std::vector<std::wstring> texts = { x, __VA_ARGS__ };	\
		std::wstringstream _wsstream;							\
																\
		for (const auto& text : texts)							\
			_wsstream << text;									\
																\
		Logger::LogFunc(_wsstream.str());						\
	}
#endif

#ifndef WLogln
#define WLogln(x, ...)											\
	{															\
		std::vector<std::wstring> texts = { x, __VA_ARGS__ };	\
		std::wstringstream _wsstream;							\
																\
		for (const auto& text : texts)							\
			_wsstream << text;									\
		_wsstream << L'\n';										\
																\
		Logger::LogFunc(_wsstream.str());						\
	}
#endif

#ifndef ReturnFalse
#define ReturnFalse(__msg)															\
	{																				\
		std::wstringstream __wsstream_RIF;											\
		__wsstream_RIF << __FILE__ << L"; line: " << __LINE__ << L"; " << __msg;	\
		WLogln(__wsstream_RIF.str());												\
		return false;																\
	}
#endif

#ifndef CheckReturn
#define CheckReturn(__statement)															\
	{																						\
		try {																				\
			bool __result = __statement;													\
			if (!__result) {																\
				std::wstringstream __wsstream_CR;											\
				__wsstream_CR << __FILE__ << L"; line: " << __LINE__ << L"; ";				\
				WLogln(__wsstream_CR.str());												\
				return false;																\
			}																				\
		}																					\
		catch (const std::exception& e) {													\
			std::wstringstream __wsstream_CR;												\
				__wsstream_CR << __FILE__ << L"; line: " << __LINE__ << L"; " << e.what();	\
				WLogln(__wsstream_CR.str());												\
			return false;																	\
		}																					\
	}
#endif

#ifndef CheckHRESULT
#define CheckHRESULT(__statement)																					\
	{																												\
		try {																										\
			HRESULT __result = __statement;																			\
			if (FAILED(__result)) {																					\
				std::wstringstream __wsstream_CR;																	\
				__wsstream_CR << __FILE__ << L"; line: " << __LINE__ << L"; HRESULT: 0x" << std::hex << __result;	\
				WLogln(__wsstream_CR.str());																		\
				return false;																						\
			}																										\
		}																											\
		catch (const std::exception& e) {																			\
			std::wstringstream __wsstream_CR;																		\
				__wsstream_CR << __FILE__ << L"; line: " << __LINE__ << L"; " << e.what();							\
				WLogln(__wsstream_CR.str());																		\
			return false;																							\
		}																											\
	}
#endif

namespace Logger {
	class LogHelper {
	public:
		static HANDLE ghLogFile;

		static std::mutex gLogFileMutex;
	};

	inline void LogFunc(const std::string& text) {
		std::wstring wstr;
		wstr.assign(text.begin(), text.end());

		DWORD writtenBytes = 0;

		LogHelper::gLogFileMutex.lock();

		WriteFile(
			LogHelper::ghLogFile,
			wstr.c_str(),
			static_cast<DWORD>(wstr.length() * sizeof(wchar_t)),
			&writtenBytes,
			NULL
		);

		LogHelper::gLogFileMutex.unlock();
	}

	inline void LogFunc(const std::wstring& text) {
		DWORD writtenBytes = 0;

		LogHelper::gLogFileMutex.lock();

		WriteFile(
			LogHelper::ghLogFile,
			text.c_str(),
			static_cast<DWORD>(text.length() * sizeof(wchar_t)),
			&writtenBytes,
			NULL
		);

		LogHelper::gLogFileMutex.unlock();
	}

	inline void SetTextToWnd(HWND hWnd, LPCWSTR newText) {
		SetWindowText(hWnd, newText);
	}

	inline void AppendTextToWnd(HWND hWnd, LPCWSTR newText) {
		int finalLength = GetWindowTextLength(hWnd) + lstrlen(newText) + 1;
		wchar_t* buf = reinterpret_cast<wchar_t*>(std::malloc(finalLength * sizeof(wchar_t)));

		GetWindowText(hWnd, buf, finalLength);

		wcscat_s(buf, finalLength, newText);

		SetWindowText(hWnd, buf);

		std::free(buf);
	}
};