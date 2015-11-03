#pragma once
#include <Windows.h>

namespace Wasapi
{
	class DeviceInfo
	{
	public:
		DeviceInfo() : m_initialized(false), m_bytesPerSample(0), m_channels(0), m_lastuQPCPosition(0) {}

		size_t GetChannels() const { return m_channels; }
		int GetBytesPerSample()	const { return m_bytesPerSample; }
		int GetSamplesPerSec() const { return m_nSamplesPerSec;  }
		UINT64 GetPosition() { return m_lastuQPCPosition; }

		bool Initialized() const { return m_initialized; }
		bool DeviceError() const { return !m_working;  }

		void SetError() { m_working = false; }

		void SetConfiguration(size_t channels, int bytesPerSample, int nSamplesPerSec)
		{ 
			m_channels = channels;
			m_bytesPerSample = bytesPerSample;
			m_nSamplesPerSec = nSamplesPerSec;
			m_initialized = true;
			m_working = true;
		}

		void SetPosition(UINT64 lastuQPCPosition) { m_lastuQPCPosition = lastuQPCPosition; }

	private:
		bool m_working;
		bool m_initialized;
		size_t m_channels;
		int m_bytesPerSample;
		int m_nSamplesPerSec;
		UINT64 m_lastuQPCPosition;
	};
}
