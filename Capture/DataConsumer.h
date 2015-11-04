#pragma once

#include <Windows.h>
#include <wrl\implements.h>
#include <mfapi.h>
#include "DataCollector.h"
#include "..\LibTDE\TimeDelayEstimation.h"

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Storage;

namespace Wasapi
{
	static Windows::Foundation::IAsyncAction^ WorkItem = nullptr;

	public delegate void UIHandler(int, int, int, int, int, int, UINT64, UINT64, int);

	ref class DataConsumer sealed
	{
	public:
		DataConsumer(size_t nDevices, DataCollector^ collector, UIHandler^ func);

		void Start();
		void Stop();

	internal:
		HRESULT Finish();

	private:

		enum class Status
		{
			ONLY_ONE_SAMPLE,
			EXCESS_DATA,
			DISCONTINUITY,
			SILENCE,
			DATA_AVAILABLE
		};

	private:
		~DataConsumer();

		void HeartBeat(int status, int delta, int msg0, int msg1, int msg2, int msg3, UINT64 msg4, UINT64 msg5);

		Status HandlePackets();
		bool ProcessData();

		void FlushPackets();
		void FlushBuffer();
		void FlushCollector();

		void AddData(size_t device, DWORD cbBytes, const BYTE* pData, UINT64 pos1, UINT64 pos2);
		bool CalculateTDE(size_t pos);
		void StoreData(String^ fileName, String^ data);

		void Worker();

		size_t m_numberOfDevices;
		DataCollector^ m_collector;

		std::vector<DeviceInfo> m_devices;
		std::vector<AudioDataPacket*> m_audioDataFirst;
		std::vector<AudioDataPacket*> m_audioDataLast;

		std::vector<std::vector<std::vector<TimeDelayEstimation::AudioDataItem>>> m_buffer;	

		UIHandler^ m_uiHandler;

		uint32 m_counter;
		ULONGLONG m_tick;

		uint32 m_minAudioThreshold;
		uint32 m_maxAudioThreshold;
		uint32 m_packetCounter;
		uint32 m_discontinuityCounter;
		uint32 m_dataRemovalCounter;

		size_t m_delayWindow;
		size_t m_bufferSize;
		size_t m_tdeWindow;
		bool m_storeSample;
	};
}
