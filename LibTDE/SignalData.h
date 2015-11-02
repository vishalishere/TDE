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

		SignalData(std::vector<SignalType>* channel0, std::vector<SignalType>* channel1, bool copy = true);
		SignalData(std::vector<SignalType>* channel0, std::vector<SignalType>* channel1, std::size_t minPos, std::size_t maxPos, bool copy = true);

		~SignalData();

		std::size_t First() const { return m_min; }
		std::size_t Last() const { return m_max - 1; }
		std::size_t Length() const { return m_max-m_min; }

		bool Align(long position);
		long Alignment() const { return m_alignment; }

		SignalValue Channel0(std::size_t position) const;
		SignalValue Channel1(std::size_t position) const;
		SignalValue Channel1(std::size_t position, DelayType delay) const;

	private:
		bool m_copy;

		std::size_t m_min;
		std::size_t m_max;
		long m_alignment;

		std::vector<SignalType>* m_channel0;
		std::vector<SignalType>* m_channel1;
	};
}
