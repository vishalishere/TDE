#pragma once

#include <Windows.h>
#include <wrl\implements.h>
#include <mfapi.h>
#include "DeviceInfo.h"
#include "AudioItem.h"

namespace Wasapi
{
	ref class DataCollector sealed
	{
	public:
		DataCollector(size_t nDevices);

	internal:
		HRESULT Initialize(size_t device, int channelsPerDevice, int nSamplesPerSec, int bitsPerSample, WAVEFORMATEX *mixFormat);
		HRESULT Finish();

		void AddData(size_t device, BYTE* pData, DWORD cbBytes, UINT64 u64QPCPosition, bool bDiscontinuity);
		DeviceInfo RemoveData(size_t device, AudioItem** first, AudioItem** last, size_t *count);
		void StoreData(bool store);

	private:
		~DataCollector();
		void FlushPackets();

	private:
		CRITICAL_SECTION m_CritSec;
		size_t m_numberOfDevices;
		bool m_store;

		std::vector<AudioItem*> m_audioDataFirst;
		std::vector<AudioItem*> m_audioDataLast;
		std::vector<DeviceInfo> m_devices;
		std::vector<size_t> m_packetCounts;
	};
}
