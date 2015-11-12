#pragma once
#include <algorithm>
#include <Windows.h>

namespace AudioEngine
{
	class AudioDataPacket
	{
	public:
		AudioDataPacket(const BYTE* pData, DWORD cbBytes, UINT64 u64QPCPosition, bool bDiscontinuity, bool bSilence) :
			m_pData(NULL), 
			m_cbBytes(0),
			m_u64QPCPosition(u64QPCPosition),
			m_bDiscontinuity(bDiscontinuity),
			m_bSilence(bSilence),
			m_next(NULL)
		{
			if (!m_bDiscontinuity && cbBytes > 0)
			{
				m_cbBytes = cbBytes;
				m_pData = new BYTE[cbBytes];

				BYTE* d = m_pData;
				const BYTE* const dend = m_pData + cbBytes;
				while (d != dend)
					*d++ = *pData++;
			}
		}

		~AudioDataPacket()
		{
			if (m_pData != NULL) delete[] m_pData;
		}

		const BYTE* Data() const { return m_pData; }
		DWORD Bytes() const { return m_cbBytes; }

		UINT64 Position() const { return m_u64QPCPosition; }

		bool Discontinuity() const { return m_bDiscontinuity; }
		bool Silence() const { return m_bSilence;  }

		void SetNext(AudioDataPacket* next) { m_next = next; }
		AudioDataPacket* Next() const { return m_next; }

	private:
		BYTE* m_pData;
		DWORD m_cbBytes;

		UINT64 m_u64QPCPosition;

		bool m_bDiscontinuity;
		bool m_bSilence;

		AudioDataPacket* m_next;
	};
}
