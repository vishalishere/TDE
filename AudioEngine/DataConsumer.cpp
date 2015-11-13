#include "pch.h"
#include "DataConsumer.h"

#include <ppltasks.h>
#include <sstream>

using namespace Windows::Storage;
using namespace Windows::System::Threading;
using namespace Platform;
using namespace AudioEngine;
using namespace concurrency;

DataConsumer::DataConsumer(size_t nDevices, DataCollector^ collector, UIDelegate^ func, TDEParameters^ parameters) :
	m_numberOfDevices(nDevices),
	m_collector(collector),
	m_uiHandler(func),
	m_counter(0),
	m_tick(0),
	m_packetCounter(0),
	m_discontinuityCounter(0),
	m_dataRemovalCounter(0),
	m_params(parameters)
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
			bool error;
			for (size_t i = 0; i < m_numberOfDevices; i++)
			{
				AudioDataPacket *first, *last;
				size_t count;
			
				m_devices[i] = m_collector->RemoveData(i, &first, &last, &count, &error);
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
			if (error) HeartBeat(HeartBeatType::DEVICE_ERROR);
			bool loop = true;
			while(loop)
			{ 
				switch (HandlePackets())
				{
				case Status::ONLY_ONE_SAMPLE:
					loop = false;
					break;
				case Status::EXCESS_DATA:
				case Status::SILENCE:
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
			HeartBeat(HeartBeatType::BUFFERING, 10000, (int)m_packetCounter, (int)m_discontinuityCounter, (int)m_dataRemovalCounter, 
				0, m_devices[m_params->Device0()].GetPosition(), m_devices[m_params->Device1()].GetPosition());

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
	if (removedData) return Status::EXCESS_DATA;

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
		// Remove silent data
		if (m_audioDataFirst[i]->Silence())
		{
			AudioDataPacket* packet = m_audioDataFirst[i];
			m_audioDataFirst[i] = packet->Next();
			delete packet;
			removedData = true;
		}
	}
	if (removedData) return Status::SILENCE;

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
	size_t smallestBuffer = m_buffer[m_params->Device0()][m_params->Channel0()].size();
	for (size_t i = 0; i < m_numberOfDevices; i++)
	{
		for (size_t j = 0; j < m_devices[i].GetChannels(); j++)
		{
			if (m_buffer[i][j].size() < smallestBuffer) smallestBuffer = m_buffer[i][j].size();
		}
	}

	if (smallestBuffer > m_params->BufferSize())
	{
		m_collector->StoreData(false);
		UINT64 latestBegin = m_buffer[m_params->Device0()][m_params->Channel0()][0].timestamp;

		for (size_t i = 0; i < m_numberOfDevices; i++)
		{
			for (size_t j = 0; j < m_devices[i].GetChannels(); j++)
			{
				if (m_buffer[i][j][0].timestamp > latestBegin) latestBegin = m_buffer[i][j][0].timestamp;
			}
		}

		bool sample = false;
		size_t pos = 0, sample_pos = 0;
		uint32 threshold = m_params->MinAudioThreshold();

		while (1) 
		{ 
			if (m_buffer[m_params->Device0()][m_params->Channel0()][pos].timestamp < latestBegin) pos++; else break;
			if (pos == m_buffer[m_params->Device0()][m_params->Channel0()].size()) break;
		}

		while (pos < m_buffer[m_params->Device0()][m_params->Channel0()].size())
		{
			if (abs(m_buffer[m_params->Device0()][m_params->Channel0()][pos].value) > (int)threshold)
			{
				sample_pos = pos;
				sample = true;
				if (threshold >= m_params->MaxAudioThreshold()) break;
				threshold *= 2;	
			}
			pos++;
		}
		if (sample)
		{
			if (!CalculateTDE(min(smallestBuffer-(m_params->DelayWindow()+m_params->TDEWindow()),max(m_params->DelayWindow(),sample_pos + m_params->StartOffset())), threshold))
			{
				HeartBeat(HeartBeatType::INVALID);
			}
		}
		else HeartBeat(HeartBeatType::SILENCE, 2000);
			
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
		bool error;
		m_devices[i] = m_collector->RemoveData(i, &first, &last, &count, &error);
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
	UINT64 delta = pos2 - pos1;
	double d = (double)delta / numPoints;

	if (m_buffer[device].size() == 0)
	{
		for (size_t i = 0; i < m_devices[device].GetChannels(); i++)
		{
			m_buffer[device].push_back(std::vector<TimeDelayEstimation::AudioDataItem>());
		}
	}
	for (DWORD i = 0; i < numPoints; i++)
	{
		UINT64 time_delta = UINT64((double)i * d);
		for (size_t j = 0; j < m_devices[device].GetChannels(); j++)
		{
			TimeDelayEstimation::AudioDataItem item = { *pi16, pos1 + time_delta, i };
			m_buffer[device][j].push_back(item);
			pi16++;
		}
	}
}

bool DataConsumer::CalculateTDE(size_t pos, uint32 threshold)
{
	TimeDelayEstimation::SignalData data = TimeDelayEstimation::SignalData(&m_buffer[m_params->Device0()][m_params->Channel0()], 
																		   &m_buffer[m_params->Device1()][m_params->Channel1()], 
		pos, pos + m_params->TDEWindow(), false);
	TimeDelayEstimation::DelayType align0, align1;

	if (!data.CalculateAlignment(data.First(), &align0, NULL)) return false;
	if (!data.CalculateAlignment(data.Last(), &align1, NULL)) return false;

	UINT64 volume = 0;
	for (size_t i = data.First(); i <= data.Last(); i++)
	{
		volume += abs(data.Value(i));
	}

	TimeDelayEstimation::DelayType align = (align0 + align1) / 2;
	data.SetAlignment(align);

	TimeDelayEstimation::TDE tde(m_params->DelayWindow(), data);

	int delay1 = tde.FindDelay(TimeDelayEstimation::Algoritm::CC);
	int delay2 = tde.FindDelay(TimeDelayEstimation::Algoritm::ASDF);
	int delay3 = tde.FindDelay(TimeDelayEstimation::Algoritm::PEAK);

	m_uiHandler(m_counter++, (int)HeartBeatType::DATA, delay1, delay2, delay3, (int)align, threshold, volume, m_devices[m_params->Device0()].GetSamplesPerSec());
	
	if (m_params->StoreSample())
	{
		Platform::String^ str = "POS: " + pos.ToString() + " THRESHOLD: " + threshold.ToString() + " ALIGN: " + align.ToString() + 
			" CC: " + delay1.ToString() + " ASDF: " + delay2.ToString() + " PEAK: " + delay3.ToString() + "\r\n";
		for (size_t i = data.First() - m_params->DelayWindow(); i <= data.Last() + m_params->DelayWindow(); i++)
		{
			TimeDelayEstimation::AudioDataItem item0, item1;
			data.DataItem0(i, &item0);
			data.DataItem1(i + align, &item1, item0.timestamp);

			Platform::String^ s = item0.timestamp.ToString() + "\t" + item0.value.ToString() + "\t" +
								  item1.timestamp.ToString() + "\t" + item1.value.ToString() + "\r\n";
			str = Platform::String::Concat(str, s);
		}
		StoreData(str);
	}
	
	return true;
}

void DataConsumer::StoreData(String^ data) 
{
	StorageFolder^ localFolder = ApplicationData::Current->LocalFolder;
	auto createFileTask = create_task(localFolder->CreateFileAsync(m_params->SampleFile(), Windows::Storage::CreationCollisionOption::GenerateUniqueName));
	createFileTask.then([data](StorageFile^ newFile) 
	{
		create_task(FileIO::AppendTextAsync(newFile, data));
	});	
}

void DataConsumer::HeartBeat(HeartBeatType status, int delta, int msg0, int msg1, int msg2, long msg3, UINT64 msg4, UINT64 msg5)
{
	ULONGLONG tick = GetTickCount64();
	if (tick - m_tick > delta)
	{
		m_uiHandler(m_counter++, (int)status, msg0, msg1, msg2, msg3, msg4, msg5, 44100);
		m_tick = tick;
		m_packetCounter = 0;
		m_discontinuityCounter = 0;
		m_dataRemovalCounter = 0;
	}
}
