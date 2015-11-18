#pragma once

#include <Windows.h>
#include <wrl\implements.h>
#include <mfapi.h>
#include "DataCollector.h"
#include "..\LibTDE\TimeDelayEstimation.h"

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Storage;

#define MIN_AUDIO_THRESHOLD 500
#define MAX_AUDIO_THRESHOLD	32000
#define BUFFER_SIZE 44100
#define DELAY_WINDOW_SIZE 50
#define TDE_WINDOW 300
#define START_OFFSET -60
#define STORE_SAMPLE 0
#define SAMPLE_FILE "SAMPLE.TXT"

namespace LibAudio
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

	public ref class TDEParameters sealed
	{
	public:
		TDEParameters() :
			minDevices(2),
			device0(0),
			device1(1),
			channel0(0),
			channel1(0),
			bufferSize(BUFFER_SIZE),
			tdeWindow(TDE_WINDOW),
			delayWindow(DELAY_WINDOW_SIZE),
			minAudioThreshold(MIN_AUDIO_THRESHOLD),
			maxAudioThreshold(MAX_AUDIO_THRESHOLD),
			startOffset(START_OFFSET),
			storeSample(STORE_SAMPLE),
			sampleFile(SAMPLE_FILE)
		{}

		TDEParameters(uint32 md, uint32 d0, uint32 d1, uint32 c0, uint32 c1) :
			minDevices(md),
			device0(d0),
			device1(d1),
			channel0(c0),
			channel1(c1),
			bufferSize(BUFFER_SIZE),
			tdeWindow(TDE_WINDOW),
			delayWindow(DELAY_WINDOW_SIZE),
			minAudioThreshold(MIN_AUDIO_THRESHOLD),
			maxAudioThreshold(MAX_AUDIO_THRESHOLD),
			startOffset(START_OFFSET),
			storeSample(STORE_SAMPLE),
			sampleFile(SAMPLE_FILE)
		{}

		TDEParameters(uint32 md, uint32 d0, uint32 d1, uint32 c0, uint32 c1, size_t bs, size_t ts, size_t ds, uint32 minAT, uint32 maxAT, int so, bool ss, Platform::String^ file) : 
			minDevices(md),
			device0(d0),
			device1(d1),
			channel0(c0),
			channel1(c1),
			bufferSize(bs), 
			tdeWindow(ts),
			delayWindow(ds),
			minAudioThreshold(minAT),
			maxAudioThreshold(maxAT),
			startOffset(so),
			storeSample(ss),
			sampleFile(file)
			{}

		uint32 MinDevices() { return minDevices; }

		uint32 Device0() { return device0; }
		uint32 Device1() { return device1; }
		uint32 Channel0() { return channel0;  }
		uint32 Channel1() { return channel1;  }

		size_t BufferSize() { return bufferSize; }
		size_t TDEWindow()  { return tdeWindow; }
		size_t DelayWindow() { return delayWindow; }
		int StartOffset() { return startOffset; }

		uint32 MinAudioThreshold() { return minAudioThreshold; }
		uint32 MaxAudioThreshold() { return maxAudioThreshold; }
		bool StoreSample() { return storeSample; }

		Platform::String^ SampleFile() { return sampleFile; }
		 
	private:
		uint32 minDevices;
		
		uint32 device0;
		uint32 device1;
		
		uint32 channel0;
		uint32 channel1;

		size_t bufferSize;
		size_t tdeWindow;
		size_t delayWindow;

		uint32 minAudioThreshold;
		uint32 maxAudioThreshold;

		uint32 startOffset;
		bool storeSample;
		Platform::String^ sampleFile;
	};

	public delegate void UIDelegate(uint32, int, int, int, int, int, UINT64, UINT64, uint32);

	ref class DataConsumer sealed
	{
	public:
		DataConsumer(size_t nDevices, DataCollector^ collector, UIDelegate^ func, TDEParameters^ parameters);

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
		void StoreData(String^ data);

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

		size_t m_packetCounter;
		uint32 m_discontinuityCounter;
		uint32 m_dataRemovalCounter;

		TDEParameters^ m_params;
	};
}
