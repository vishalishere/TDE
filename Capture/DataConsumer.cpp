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
#define MAX_AUDIO_THRESHOLD	6400
#define BUFFER_SIZE 44100
#define DELAY_WINDOW_SIZE 50
#define TDE_WINDOW 200
#define START_OFFSET -10
#define STORE_SAMPLE 0
#define SAMPLE_FILE "SAMPLE.TXT"

DataConsumer::DataConsumer(size_t nDevices, DataCollector^ collector, UIHandler^ func) :
	m_numberOfDevices(nDevices),
	m_collector(collector),
	m_uiHandler(func),
	m_counter(0),
	m_tick(0),
	m_packetCounter(0),
	m_discontinuityCounter(0),
	m_dataRemovalCounter(0),
	m_minAudioThreshold(MIN_AUDIO_THRESHOLD),
	m_maxAudioThreshold(MAX_AUDIO_THRESHOLD),
	m_delayWindow(DELAY_WINDOW_SIZE),
	m_bufferSize(BUFFER_SIZE),
	m_tdeWindow(TDE_WINDOW),
	m_storeSample(STORE_SAMPLE)
{
	m_devices = std::vector<DeviceInfo>(m_numberOfDevices);
	m_audioDataFirst = std::vector<AudioDataPacket*>(m_numberOfDevices, NULL);
	m_audioDataLast = std::vector<AudioDataPacket*>(m_numberOfDevices, NULL);

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
			for (size_t i = 0; i < m_numberOfDevices; i++)
			{
				AudioDataPacket *first, *last;
				size_t count;

				m_devices[i] = m_collector->RemoveData(i, &first, &last, &count);
				m_packetCounter += count;

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
			bool loop = true;
			while(loop)
			{ 
				switch (HandlePackets())
				{
				case Status::ONLY_ONE_SAMPLE:
					loop = false;
					break;
				case Status::DATA_REMOVED:
					m_dataRemovalCounter++;
					break;
				case Status::DISCONTINUITY:
					FlushBuffer();
					m_discontinuityCounter++;
					break;
				case Status::DATA_AVAILABLE:
					loop = ProcessData();
					break;
				}
			}
			HeartBeat(-3, 5000, m_packetCounter, m_discontinuityCounter, m_dataRemovalCounter, 0, m_devices[0].GetPosition(), m_devices[1].GetPosition());

			if (action->Status == Windows::Foundation::AsyncStatus::Canceled) break;
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

DataConsumer::Status DataConsumer::HandlePackets()
{
	for (size_t i = 0; i < m_numberOfDevices; i++)
	{
		// Only one data item
		if (m_audioDataFirst[i] == m_audioDataLast[i]) return Status::ONLY_ONE_SAMPLE;
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
			AudioDataPacket* packet = m_audioDataFirst[i];
			m_audioDataFirst[i] = packet->Next();
			delete packet;
			removedData = true;
		}
	}
	delete pos1;
	delete pos2;
	if (removedData) return Status::DATA_REMOVED;

	for (size_t i = 0; i < m_numberOfDevices; i++)
	{
		// Remove data before discontinuity
		if (m_audioDataFirst[i]->Discontinuity() || m_audioDataFirst[i]->Next()->Discontinuity())
		{
			AudioDataPacket* packet = m_audioDataFirst[i];
			m_audioDataFirst[i] = packet->Next();
			delete packet;
			removedData = true;
		}
	}
	if (removedData) return Status::DISCONTINUITY;

	for (size_t i = 0; i < m_numberOfDevices; i++)
	{
		AudioDataPacket* packet = m_audioDataFirst[i];
		m_audioDataFirst[i] = packet->Next();
		AddData(i, packet->Bytes(), packet->Data(), packet->Position(), packet->Next()->Position());
		delete packet;
	}
	return Status::DATA_AVAILABLE;
}

bool DataConsumer::ProcessData()
{
	size_t smallestBuffer = m_buffer[0][0].size();
	for (size_t i = 0; i < m_numberOfDevices; i++)
	{
		for (size_t j = 0; j < m_devices[i].GetChannels(); j++)
		{
			if (m_buffer[i][j].size() < smallestBuffer) smallestBuffer = m_buffer[i][j].size();
		}
	}

	if (smallestBuffer > m_bufferSize)
	{
		m_collector->StoreData(false);
		UINT64 latestBegin = m_buffer[0][0][0].timestamp;

		for (size_t i = 0; i < m_numberOfDevices; i++)
		{
			for (size_t j = 0; j < m_devices[i].GetChannels(); j++) if (m_buffer[i][j][0].timestamp > latestBegin) latestBegin = m_buffer[i][j][0].timestamp;
		}

		bool sample = false;
		size_t pos = 0, sample_pos = 0;
		uint32 threshold = m_minAudioThreshold;

		while (1) 
		{ 
			if (m_buffer[0][0][pos].timestamp < latestBegin) pos++; else break;	
			if (pos == m_buffer[0][0].size()) break;
		}

		while (pos < m_buffer[0][0].size())
		{
			if (abs(m_buffer[0][0][pos].value) > (int)threshold)
			{
				sample_pos = pos;
				sample = true;
				if (threshold >= m_maxAudioThreshold) break;
				threshold *= 2;	
			}
			pos++;
		}
		if (sample)
		{
			long align;
			if (CalculateTDE(sample_pos + START_OFFSET, &align))
			{
				if (m_storeSample)
				{
					Platform::String^ s1 = SAMPLE_FILE;
					Platform::String^ s2 = "";
					for (size_t i = sample_pos; i < sample_pos + m_tdeWindow; i++)
					{
						Platform::String^ s = align.ToString() + " " + m_buffer[0][0][i].timestamp.ToString() + " " + m_buffer[0][0][i].value.ToString() + " " +
							m_buffer[1][0][align + i].timestamp.ToString() + " " + m_buffer[1][0][align + i].value.ToString() + "\r\n";
						s2 = Platform::String::Concat(s2, s);
						if (i == m_buffer[0][0].size() - 1 || i == m_buffer[1][0].size() - 1) break;
					}
					StoreData(s1, s2);
				}
			}
			else HeartBeat(-1, 0, 0, 0, 0, 0, 0, 0);
		}
		else HeartBeat(-2, 1000, 0, 0, 0, 0, 0, 0);
			
		FlushBuffer();
		FlushCollector();		
		m_collector->StoreData(true);
		return false;
	}
	return true;
}

void DataConsumer::FlushPackets()
{
	for (size_t i = 0; i < m_numberOfDevices; i++)
	{
		while (m_audioDataFirst[i] != NULL) 
		{ 
			AudioDataPacket* ptr = m_audioDataFirst[i];
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
		AudioDataPacket *first, *last;
		size_t count;
		m_devices[i] = m_collector->RemoveData(i, &first, &last, &count);
		while (first != NULL) 
		{	
			AudioDataPacket* ptr = first;
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

bool DataConsumer::CalculateTDE(size_t pos, TimeDelayEstimation::DelayType* alignment)
{
	TimeDelayEstimation::SignalData data = TimeDelayEstimation::SignalData(&m_buffer[0][0], &m_buffer[1][0], pos, pos + m_tdeWindow, false);
	TimeDelayEstimation::DelayType align0, align1;

	if (!data.CalculateAlignment(pos, &align0, NULL)) return false;
	if (!data.CalculateAlignment(pos + m_tdeWindow, &align1, NULL)) return false;
	data.SetAlignment((align0 + align1) / 2);

	TimeDelayEstimation::DelayType align = data.Alignment();

	UINT64 volume0 = 0, volume1 = 0;

	for (size_t i = pos; i < pos + m_tdeWindow; i++)
	{
		volume0 += abs(data.Channel0(i));
		volume1 += abs(data.Channel1(i + align));
	}

	TimeDelayEstimation::TDE tde(m_delayWindow);

	int delay1 = tde.FindDelay(data, TimeDelayEstimation::Algoritm::CC);
	int delay2 = tde.FindDelay(data, TimeDelayEstimation::Algoritm::ASDF);
	//int delay3 = tde.FindDelay(data, TimeDelayEstimation::Algoritm::PHAT);

	m_uiHandler(m_counter++, 0, delay1, delay2, 0, (int)data.Alignment(), volume0, volume1, m_devices[0].GetSamplesPerSec());
	return true;
}

void DataConsumer::StoreData(String^ fileName, String^ data) 
{
	StorageFolder^ localFolder = ApplicationData::Current->LocalFolder;
	auto createFileTask = create_task(localFolder->CreateFileAsync(fileName, Windows::Storage::CreationCollisionOption::GenerateUniqueName));
	createFileTask.then([data](StorageFile^ newFile) 
	{
		create_task(FileIO::AppendTextAsync(newFile, data));
	});	
}

void DataConsumer::HeartBeat(int status, int delta, int msg0, int msg1, int msg2, int msg3, UINT64 msg4, UINT64 msg5)
{
	ULONGLONG tick = GetTickCount64();
	if (tick - m_tick > delta)
	{
		m_uiHandler(m_counter++, status, msg0, msg1, msg2, msg3, msg4, msg5, 44100);
		m_tick = tick;
		m_packetCounter = 0;
		m_discontinuityCounter = 0;
		m_dataRemovalCounter = 0;
	}
}
