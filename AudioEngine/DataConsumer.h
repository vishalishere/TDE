#pragma once

#include <Windows.h>
#include <wrl\implements.h>
#include <mfapi.h>
#include "DataCollector.h"
#include "..\LibTDE\TimeDelayEstimation.h"

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Storage;

namespace AudioEngine
{
	static Windows::Foundation::IAsyncAction^ WorkItem = nullptr;

	public enum class HeartBeatType
	{
		DATA,
		BUFFERING,
		DEVICE_ERROR,
		SILENCE,
		INVALID,
		NODEVICE
	};

	public delegate void UIDelegate(uint32, int, int, int, int, int, UINT64, UINT64, uint32);

	ref class DataConsumer sealed
	{
	public:
		DataConsumer(size_t nDevices, DataCollector^ collector, UIDelegate^ func);

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

		void HeartBeat(HeartBeatType status, int delta = 0, int msg0 = 0, int msg1 = 0, int msg2 = 0, long msg3 = 0, UINT64 msg4 = 0, UINT64 msg5 = 0);

		Status HandlePackets();
		bool ProcessData();

		void FlushPackets();
		void FlushBuffer();
		void FlushCollector();

		void AddData(size_t device, DWORD cbBytes, const BYTE* pData, UINT64 pos1, UINT64 pos2);
		bool CalculateTDE(size_t pos, uint32 threshold);
		void StoreData(String^ fileName, String^ data);

		void Worker();

		size_t m_numberOfDevices;
		DataCollector^ m_collector;

		std::vector<DeviceInfo> m_devices;
		std::vector<AudioDataPacket*> m_audioDataFirst;
		std::vector<AudioDataPacket*> m_audioDataLast;

		std::vector<std::vector<std::vector<TimeDelayEstimation::AudioDataItem>>> m_buffer;	

		UIDelegate^ m_uiHandler;

		uint32 m_counter;
		ULONGLONG m_tick;

		uint32 m_minAudioThreshold;
		uint32 m_maxAudioThreshold;
		size_t m_packetCounter;
		uint32 m_discontinuityCounter;
		uint32 m_dataRemovalCounter;

		size_t m_delayWindow;
		size_t m_bufferSize;
		size_t m_tdeWindow;
		bool m_storeSample;
	};
}
