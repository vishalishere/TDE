#pragma once

#include <Windows.h>

namespace TimeDelayEstimation {

	struct AudioDataItem
	{
		INT16 value;
		UINT64 timestamp;
		DWORD index;
	};
}
