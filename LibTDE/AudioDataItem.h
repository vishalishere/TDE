#pragma once

#include <Windows.h>

namespace TimeDelayEstimation {

	struct AudioDataItem
	{
		INT16 value;
		UINT64 timestamp;
		DWORD index;

		AudioDataItem() : value(0), timestamp(0), index(0) {}
		AudioDataItem(INT16 v, UINT64 ts, DWORD i) : value(v), timestamp(ts), index(i) {}
	};
}
