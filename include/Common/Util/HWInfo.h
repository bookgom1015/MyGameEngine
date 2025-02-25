#pragma once

#include <Windows.h>

namespace HWInfo {
	struct Processor {
		UINT64 Physical;
		UINT64 Logical;
	};

	BOOL GetProcessorInfo(Processor& info);
};