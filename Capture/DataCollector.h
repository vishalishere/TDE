#pragma once

#include <Windows.h>
#include <wrl\implements.h>
#include <mfapi.h>
#include "DeviceInfo.h"
#include "AudioDataPacket.h"

namespace Wasapi
{
	ref class DataCollector sealed
	{
	public:
		DataCollector(size_t nDevices);

	internal:
		HRESULT Initialize(size_t device, int channelsPerDevice, int nSamplesPerSec, int bitsPerSample, WAVEFORMATEX *mixFormat);
		HRESULT Finish();

		void DeviceError(size_t device);

		void AddData(size_t device, BYTE* pData, DWORD cbBytes, UINT64 u64QPCPosition, bool bDiscontinuity, bool bSilence);
		DeviceInfo RemoveData(size_t device, AudioDataPacket** first, AudioDataPacket** last, size_t *count, bool* error);
		void StoreData(bool store);

	private:
		~DataCollector();
		void FlushPackets();

	private:
		CRITICAL_SECTION m_CritSec;
		size_t m_numberOfDevices;
		bool m_store;
		bool m_error;

		std::vector<AudioDataPacket*> m_audioDataFirst;
		std::vector<AudioDataPacket*> m_audioDataLast;
		std::vector<DeviceInfo> m_devices;
		std::vector<size_t> m_packetCounts;
	};
}
