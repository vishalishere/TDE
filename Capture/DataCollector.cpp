#include "pch.h"
#include "DataCollector.h"

using namespace Windows::Storage;
using namespace Windows::System::Threading;
using namespace Platform;
using namespace Wasapi;

DataCollector::DataCollector(size_t nDevices) : m_numberOfDevices(nDevices), m_store(true)
{
	if (!InitializeCriticalSectionEx(&m_CritSec, 0, 0))
	{
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}
	m_devices = std::vector<DeviceInfo>(m_numberOfDevices);
	m_audioDataFirst = std::vector<AudioItem*>(m_numberOfDevices, NULL);
	m_audioDataLast = std::vector<AudioItem*>(m_numberOfDevices, NULL);
	m_packetCounts = std::vector<size_t>(m_numberOfDevices, 0);
}

DataCollector::~DataCollector()
{
	FlushPackets();
	DeleteCriticalSection(&m_CritSec);
}

HRESULT DataCollector::Initialize(size_t device, int channels, int nSamplesPerSec, int bitsPerSample, WAVEFORMATEX *mixFormat)
{
	EnterCriticalSection(&m_CritSec);
	HRESULT hr = S_OK;
	m_devices[device].SetConfiguration(channels, bitsPerSample / 8, nSamplesPerSec);
	LeaveCriticalSection(&m_CritSec);
	return hr;
}

void DataCollector::AddData(size_t device, BYTE* pData, DWORD cbBytes,  UINT64 u64QPCPosition, bool bDiscontinuity)
{
	EnterCriticalSection(&m_CritSec);

	if (m_store)
	{
		m_packetCounts[device] = m_packetCounts[device] + 1;
		AudioItem* item = new AudioItem(pData, cbBytes, u64QPCPosition, bDiscontinuity);
		if (m_audioDataFirst[device] == NULL)
		{
			m_audioDataFirst[device] = item;
			m_audioDataLast[device] = item;
		}
		else
		{
			m_audioDataLast[device]->SetNext(item);
			m_audioDataLast[device] = item;
		}
	}
	m_devices[device].SetPosition(u64QPCPosition);

	LeaveCriticalSection(&m_CritSec);
}

DeviceInfo DataCollector::RemoveData(size_t device, AudioItem** first, AudioItem** last, size_t *count)
{
	EnterCriticalSection(&m_CritSec);
	*first = m_audioDataFirst[device];
	*last = m_audioDataLast[device];
	*count = m_packetCounts[device];

	m_audioDataFirst[device] = NULL;
	m_audioDataLast[device] = NULL;
	m_packetCounts[device] = 0;

	DeviceInfo info = m_devices[device];
	LeaveCriticalSection(&m_CritSec);

	return info;
}

void DataCollector::StoreData(bool store)
{
	EnterCriticalSection(&m_CritSec);
	m_store = store;
	LeaveCriticalSection(&m_CritSec);
}

HRESULT DataCollector::Finish()
{
	return S_OK;
}

void DataCollector::FlushPackets()
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