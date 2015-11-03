#pragma once
#include <algorithm>
#include <Windows.h>

namespace Wasapi
{
	class AudioDataPacket
	{
	public:
		AudioDataPacket(BYTE* pData, DWORD cbBytes, UINT64 u64QPCPosition, bool bDiscontinuity) :
			m_pData(NULL), 
			m_cbBytes(0),
			m_u64QPCPosition(u64QPCPosition),
			m_bDiscontinuity(bDiscontinuity),
			m_next(NULL)
		{
			if (!m_bDiscontinuity && cbBytes > 0)
			{
				m_cbBytes = cbBytes;
				m_pData = new BYTE[cbBytes];
				std::copy(pData,pData+cbBytes,m_pData);
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

		void SetNext(AudioDataPacket* next) { m_next = next; }
		AudioDataPacket* Next() const { return m_next; }

	private:
		BYTE* m_pData;
		DWORD m_cbBytes;
		UINT64 m_u64QPCPosition;
		bool m_bDiscontinuity;
		AudioDataPacket* m_next;
	};
}
