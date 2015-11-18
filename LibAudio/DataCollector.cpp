#include "pch.h"
#include "Common.h"
#include "DataCollector.h"

using namespace Windows::Storage;
using namespace Windows::System::Threading;
using namespace Platform;
using namespace LibAudio;

DataCollector::DataCollector(size_t nDevices) : m_numberOfDevices(nDevices), m_store(true), m_error(false)
{
	if (!InitializeCriticalSectionEx(&m_CritSec, 0, 0))
	{
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}
	m_devices = std::vector<DeviceInfo>(m_numberOfDevices);
	m_audioDataFirst = std::vector<AudioDataPacket*>(m_numberOfDevices, NULL);
	m_audioDataLast = std::vector<AudioDataPacket*>(m_numberOfDevices, NULL);
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

void  DataCollector::DeviceError(size_t device)
{
	EnterCriticalSection(&m_CritSec);
	m_devices[device].SetError();
	LeaveCriticalSection(&m_CritSec);
}

void DataCollector::AddData(size_t device, BYTE* pData, DWORD cbBytes,  UINT64 u64QPCPosition, bool bDiscontinuity, bool bSilence)
{
	EnterCriticalSection(&m_CritSec);

	if (m_store)
	{
		m_packetCounts[device] = m_packetCounts[device] + 1;
		AudioDataPacket* item = new AudioDataPacket(pData, cbBytes, u64QPCPosition, bDiscontinuity, bSilence);
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

DeviceInfo DataCollector::RemoveData(size_t device, AudioDataPacket** first, AudioDataPacket** last, size_t *count, bool* error)
{
	EnterCriticalSection(&m_CritSec);
	*first = m_audioDataFirst[device];
	*last = m_audioDataLast[device];
	*count = m_packetCounts[device];
	*error = m_error;

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
			AudioDataPacket* ptr = m_audioDataFirst[i];
			m_audioDataFirst[i] = ptr->Next();
			delete ptr;
		}
		m_audioDataLast[i] = NULL;
	}
}
