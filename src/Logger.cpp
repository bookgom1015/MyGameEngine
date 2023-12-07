#include "Logger.h"

BOOL Logger::LogHelper::StaticInit() {
	DWORD writtenBytes = 0;
	WORD bom = 0xFEFF;

	return WriteFile(LogHelper::ghLogFile, &bom, 2, &writtenBytes, NULL);
}

HANDLE Logger::LogHelper::ghLogFile = CreateFile(
	L"./log.txt",
	GENERIC_WRITE,
	FILE_SHARE_WRITE,
	NULL,
	CREATE_ALWAYS,
	FILE_ATTRIBUTE_NORMAL,
	NULL
);

std::mutex Logger::LogHelper::gLogFileMutex;