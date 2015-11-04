#pragma once
#include <vector>
#include "AudioDataItem.h"

namespace TimeDelayEstimation {

	typedef long DelayType;
	typedef AudioDataItem SignalType;
	typedef int SignalValue;
	typedef LONG64 CalcType;

	const SignalValue SignalZero = 0;
	const CalcType CalcZero = 0;

	class SignalData
	{
	public:

		SignalData(std::vector<SignalType>* channel0, std::vector<SignalType>* channel1, bool copyData = true);
		SignalData(std::vector<SignalType>* channel0, std::vector<SignalType>* channel1, size_t minPos, size_t maxPos, bool copyData = true);

		~SignalData();

		size_t First() const { return m_min; }
		size_t Last() const { return m_max - 1; }
		size_t Length() const { return m_max-m_min; }

		bool Align(size_t position);
		bool CalculateAlignment(size_t position, DelayType* alignment, UINT64* delta);
		long Alignment() const { return m_alignment; }
		void SetAlignment(DelayType alignment) { m_alignment = alignment; }

		SignalValue Channel0(size_t position) const;
		SignalValue Channel1(size_t position, DelayType delay) const;

		UINT64 TimeStamp0(size_t position) const;
		UINT64 TimeStamp1(size_t position) const;

		const AudioDataItem& DataItem(size_t position) const;

	private:

		SignalValue Channel1(size_t position) const;

		bool m_copy;

		std::size_t m_min;
		std::size_t m_max;
		DelayType m_alignment;

		std::vector<SignalType>* m_channel0;
		std::vector<SignalType>* m_channel1;
	};
}
