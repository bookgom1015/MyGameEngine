#include "Common/Util/HWInfo.h"
#include "Common/Debug/Logger.h"

#include <vector>

BOOL HWInfo::GetProcessorInfo(Processor& info) {
    DWORD length = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &length);

    if (length == 0) {
        WLogln(L"Failed to get required buffer size.");
        return FALSE;
    }

    std::vector<uint8_t> buffer(length);
    if (!GetLogicalProcessorInformationEx(RelationProcessorCore, reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data()), &length)) {
        WLogln(L"Failed to retrieve processor information.");
        return FALSE;
    }
    
    info.Physical = 0;
    info.Logical = 0;
    
    for (size_t offset = 0; offset < length;) {
        auto* infoEX = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data() + offset);
    
        if (infoEX->Relationship == RelationProcessorCore) {
            ++info.Physical;
            info.Logical += __popcnt64(infoEX->Processor.GroupMask->Mask);
        }
    
        offset += infoEX->Size;
    }

	return TRUE;
}