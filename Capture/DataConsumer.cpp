#include "pch.h"
#include "DataConsumer.h"

#include <ppltasks.h>
#include <sstream>

using namespace Windows::Storage;
using namespace Windows::System::Threading;
using namespace Platform;
using namespace Wasapi;

using namespace concurrency;

#define MIN_AUDIO_THRESHOLD 400
#define MAX_AUDIO_THRESHOLD	400
#define BUFFER_SIZE 5000

#define DELAY_WINDOW_BEGIN 5
#define DELAY_WINDOW_SIZE 50
#define DELAY_WINDOW_END 96
#define TDE_WINDOW 200

DataConsumer::DataConsumer(size_t nDevices, DataCollector^ collector, UIHandler^ func) : m_numberOfDevices(nDevices), m_collector(collector), m_uiHandler(func), m_counter(0)
{
	m_devices = std::vector<DeviceInfo>(m_numberOfDevices);
	m_audioDataFirst = std::vector<AudioItem*>(m_numberOfDevices, NULL);
	m_audioDataLast = std::vector<AudioItem*>(m_numberOfDevices, NULL);

	m_buffer = std::vector<std::vector<std::vector<TimeDelayEstimation::AudioDataItem>>>(m_numberOfDevices);
}

DataConsumer::~DataConsumer()
{
	Stop();
	FlushBuffer();
	FlushPackets();
}

void DataConsumer::Start()
{
	Worker();
}

void DataConsumer::Stop()
{
	if (WorkItem != nullptr) { WorkItem->Cancel(); }
}

HRESULT DataConsumer::Finish()
{
	Stop();
	return S_OK;
}

void DataConsumer::Worker()
{
	auto workItemDelegate = [this](Windows::Foundation::IAsyncAction^ action) 
	{ 
		while (1) {
			std::vector<size_t> packetCounts = std::vector<size_t>(m_numberOfDevices, 0);
			for (size_t i = 0; i < m_numberOfDevices; i++)
			{
				AudioItem *first, *last;
				size_t count;
				m_devices[i] = m_collector->RemoveData(i, &first, &last, &count);
				packetCounts[i] = count;
				if (m_audioDataLast[i] != NULL)
				{
					m_audioDataLast[i]->SetNext(first);
					if (last != NULL) m_audioDataLast[i] = last;
				}
				else
				{
					m_audioDataFirst[i] = first;
					m_audioDataLast[i] = last;
				}
			}
			int res = HandlePackets();

			switch (res)
			{
			case 3: FlushBuffer(); break;
			case 4: ProcessData(); break;
			}

			auto status = action->Status;
			if (status == Windows::Foundation::AsyncStatus::Canceled) break;
		}
	};

	auto completionDelegate = [this](Windows::Foundation::IAsyncAction^ action, Windows::Foundation::AsyncStatus status)
	{
		switch (action->Status)
		{
		case Windows::Foundation::AsyncStatus::Started:		break;
		case Windows::Foundation::AsyncStatus::Completed:	WorkItem = nullptr;	break;
		case Windows::Foundation::AsyncStatus::Canceled:	WorkItem = nullptr;	break;
		}
	};

	auto workItemHandler = ref new Windows::System::Threading::WorkItemHandler(workItemDelegate);
	auto completionHandler = ref new Windows::Foundation::AsyncActionCompletedHandler(completionDelegate, Platform::CallbackContext::Same);

	WorkItem = Windows::System::Threading::ThreadPool::RunAsync(workItemHandler, Windows::System::Threading::WorkItemPriority::Normal);
	WorkItem->Completed = completionHandler;
}

int DataConsumer::HandlePackets()
{
	for (size_t i = 0; i < m_numberOfDevices; i++)
	{
		// Only one data item
		if (m_audioDataFirst[i] == m_audioDataLast[i]) return 1;
	}
	UINT64* pos1 = new UINT64[m_numberOfDevices];
	UINT64* pos2 = new UINT64[m_numberOfDevices];
	UINT64 maxPos1 = 0;

	for (size_t i = 0; i < m_numberOfDevices; i++)
	{
		// Find latest starting point
		pos1[i] = m_audioDataFirst[i]->Position();
		pos2[i] = m_audioDataFirst[i]->Next()->Position();
		if (pos1[i] > maxPos1) maxPos1 = pos1[i];
	}

	bool removedData = false;

	for (size_t i = 0; i < m_numberOfDevices; i++)
	{
		// Remove data before latest starting point
		if (pos2[i] < maxPos1)
		{
			AudioItem* item = m_audioDataFirst[i];
			m_audioDataFirst[i] = item->Next();
			delete item;
			removedData = true;
		}
	}
	delete pos1;
	delete pos2;
	if (removedData) return 2;

	for (size_t i = 0; i < m_numberOfDevices; i++)
	{
		// Remove data before discontinuity
		if (m_audioDataFirst[i]->Next()->Discontinuity())
		{
			AudioItem* item = m_audioDataFirst[i];
			m_audioDataFirst[i] = item->Next();
			delete item;
			removedData = true;
		}
	}
	if (removedData) return 3;

	for (size_t i = 0; i < m_numberOfDevices; i++)
	{
		AudioItem* item = m_audioDataFirst[i];
		m_audioDataFirst[i] = item->Next();
		AddData(i, item->Bytes(), item->Data(), item->Position(), item->Next()->Position());
		delete item;
	}
	return 4;
}

void DataConsumer::ProcessData()
{
	size_t smallestBuffer = m_buffer[0][0].size();
	for (size_t i = 0; i < m_numberOfDevices; i++)
	{
		for (size_t j = 0; j < m_devices[i].GetChannels(); j++)
		{
			if (m_buffer[i][j].size() < smallestBuffer) smallestBuffer = m_buffer[i][j].size();
		}
	}

	if (smallestBuffer > BUFFER_SIZE)
	{
		m_collector->StoreData(false);
		UINT64 latestBegin = m_buffer[0][0][0].timestamp;

		for (size_t i = 0; i < m_numberOfDevices; i++)
		{
			for (size_t j = 0; j < m_devices[i].GetChannels(); j++) if (m_buffer[i][j][0].timestamp > latestBegin) latestBegin = m_buffer[i][j][0].timestamp;
		}

		bool sample = false;
		size_t pos = 0, posS = 0;
		INT16 threshold = MIN_AUDIO_THRESHOLD;

		while (1) 
		{ 
			if (m_buffer[0][0][pos].timestamp < latestBegin) pos++; 
			else break;	
			if (pos == m_buffer[0][0].size()) break;
		}

		while (pos < m_buffer[0][0].size())
		{
			if (abs(m_buffer[0][0][pos].value) > threshold)
			{
				posS = pos;
				sample = true;
				if (threshold == MAX_AUDIO_THRESHOLD) break;
				threshold *= 2;	
			}
			pos++;
		}
		if (sample) 
		{ 
			long align = CalculateTDE(posS);
			/*
			if (align != -99999)
			{
				Platform::String^ s1 = "SAMPLE.TXT";
				Platform::String^ s2 = "";

				for (size_t i = posS; i < posS + TDE_WINDOW; i++)
				{
					Platform::String^ s = align.ToString() + " " + m_buffer[0][0][i].timestamp.ToString() + " " + m_buffer[0][0][i].value.ToString() + " " + m_buffer[1][0][align+i].timestamp.ToString() + " " + m_buffer[1][0][align+i].value.ToString() + "\r\n";
					s2 = Platform::String::Concat(s2, s);				
				}
				StoreData(s1, s2);
			}
			*/
		}

		FlushBuffer();
		FlushCollector();		
		m_collector->StoreData(true);
	}
}

void DataConsumer::FlushPackets()
{
	for (size_t i = 0; i < m_numberOfDevices; i++)
	{
		while (m_audioDataFirst[i] != NULL) 
		{ 
			AudioItem* ptr = m_audioDataFirst[i];
			m_audioDataFirst[i] = ptr->Next();
			delete ptr; 
		}
		m_audioDataLast[i] = NULL;
	}
}

void DataConsumer::FlushBuffer()
{
	for (size_t i = 0; i < m_numberOfDevices; i++)
	{
		if (m_buffer[i].size() != 0)
		{
			for (size_t j = 0; j < m_devices[i].GetChannels(); j++)
			{
				m_buffer[i][j].clear();
			}
		}
	}
}

void DataConsumer::FlushCollector()
{
	for (size_t i = 0; i < m_numberOfDevices; i++)
	{
		AudioItem *first, *last;
		size_t count;
		m_devices[i] = m_collector->RemoveData(i, &first, &last, &count);
		while (first != NULL) 
		{	
			AudioItem* ptr = first;	
			first = ptr->Next(); 
			delete ptr; 
		}
	}
}

void DataConsumer::AddData(size_t device, DWORD cbBytes, const BYTE* pData, UINT64 pos1, UINT64 pos2)
{
	DWORD numPoints = cbBytes / (DWORD)(m_devices[device].GetChannels() * m_devices[device].GetBytesPerSample());
	INT16 *pi16 = (INT16*)pData;
	UINT64 delta = (pos2 - pos1) / numPoints;

	if (m_buffer[device].size() == 0)
	{
		for (size_t i = 0; i < m_devices[device].GetChannels(); i++)
		{
			m_buffer[device].push_back(std::vector<TimeDelayEstimation::AudioDataItem>());
		}
	}
	for (DWORD i = 0; i < numPoints; i++)
	{
		UINT64 time_delta = i * delta;
		for (size_t j = 0; j < m_devices[device].GetChannels(); j++)
		{
			TimeDelayEstimation::AudioDataItem item = { *pi16, pos1 + time_delta, i };
			m_buffer[device][j].push_back(item);
			pi16++;
		}
	}
}

long DataConsumer::CalculateTDE(size_t pos)
{
	TimeDelayEstimation::SignalData data = TimeDelayEstimation::SignalData(&m_buffer[0][0], &m_buffer[1][0], pos, pos + TDE_WINDOW, false);
	if (!data.Align((long)pos))
	{
		m_uiHandler(m_counter++, -999, -999, -999, 0, 0, 0, m_devices[0].GetSamplesPerSec());
		return -99999;
	}
	if (!data.Align((long)pos + TDE_WINDOW))
	{
		m_uiHandler(m_counter++, 999, 999, 999, 0, 0, 0, m_devices[0].GetSamplesPerSec());
		return -99999;
	}

	long align = data.Alignment();

	TimeDelayEstimation::SignalValue volume0 = TimeDelayEstimation::SignalZero;
	TimeDelayEstimation::SignalValue volume1 = TimeDelayEstimation::SignalZero;

	for (size_t i = pos; i < pos + TDE_WINDOW; i++)
	{
		TimeDelayEstimation::SignalValue vol = data.Channel0(i);
		volume0 += abs(vol)/10;

		vol = data.Channel1(i+align);
		volume1 += abs(vol) / 10;
	}

	TimeDelayEstimation::TDE tde(DELAY_WINDOW_SIZE);
	TimeDelayEstimation::TDEVector* v = tde.CC(data);
	TimeDelayEstimation::CalcType value = v->at(DELAY_WINDOW_SIZE).value;
	int delay1 = v->at(DELAY_WINDOW_SIZE).delay;

	for (int i = DELAY_WINDOW_BEGIN; i < DELAY_WINDOW_END; i++) {
		if (v->at(i).value > value) {
			value = v->at(i).value;
			delay1 = v->at(i).delay;
		}
	}
	delete v;	

	v = tde.ASDF(data);
	value = v->at(DELAY_WINDOW_SIZE).value;
	int delay2 = v->at(DELAY_WINDOW_SIZE).delay;

	for (int i = DELAY_WINDOW_BEGIN; i < DELAY_WINDOW_END; i++) {
		if (v->at(i).value < value) {
			value = v->at(i).value;
			delay2 = v->at(i).delay;
		}
	}
	delete v;

	v = tde.PHAT(data);
	value = v->at(DELAY_WINDOW_SIZE).value;
	int delay3 = v->at(DELAY_WINDOW_SIZE).delay;

	for (int i = DELAY_WINDOW_BEGIN; i < DELAY_WINDOW_END; i++) {
		if (v->at(i).value > value) {
			value = v->at(i).value;
			delay3 = v->at(i).delay;
		}
	}
	delete v;

	m_uiHandler(m_counter++, delay1, delay2, delay3, (int)data.Alignment(), (int)volume0, (int)volume1, m_devices[0].GetSamplesPerSec());
	return data.Alignment();
}

void DataConsumer::StoreData(String^ fileName, String^ data) 
{
	StorageFolder^ localFolder = ApplicationData::Current->LocalFolder;

	auto createFileTask = create_task(localFolder->CreateFileAsync(fileName, Windows::Storage::CreationCollisionOption::GenerateUniqueName));
	createFileTask.then([data](StorageFile^ newFile) {
		create_task(FileIO::AppendTextAsync(newFile, data));
	});	
}
