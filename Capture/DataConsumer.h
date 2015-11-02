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

	public delegate void UIHandler(int, int, int, int, int, int, int, int, int);

	ref class DataConsumer sealed
	{
	public:
		DataConsumer(size_t nDevices, DataCollector^ collector, UIHandler^ func);

		void Start();
		void Stop();

	internal:
		HRESULT Finish();

	private:
		~DataConsumer();

		int HandlePackets();

		void ProcessData();

		void FlushPackets();
		void FlushBuffer();
		void FlushCollector();

		void AddData(size_t device, DWORD cbBytes, const BYTE* pData, UINT64 pos1, UINT64 pos2);

		long CalculateTDE(size_t pos);

		void StoreData(String^ fileName, String^ data);

		void Worker();

		size_t m_numberOfDevices;
		DataCollector^ m_collector;

		std::vector<DeviceInfo> m_devices;
		std::vector<AudioItem*> m_audioDataFirst;
		std::vector<AudioItem*> m_audioDataLast;

		std::vector<std::vector<std::vector<TimeDelayEstimation::AudioDataItem>>> m_buffer;	

		UIHandler^ m_uiHandler;
		int m_counter;

		ULONGLONG m_tick;
	};
}
