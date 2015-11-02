#include "pch.h"
#include "SignalData.h"

using namespace TimeDelayEstimation;

SignalData::SignalData(std::vector<SignalType>* channel0, std::vector<SignalType>* channel1, bool copy)
	: m_copy(copy), m_min(0), m_max(min(channel0->size(),channel1->size())), m_alignment(0)
{
	if (copy)
	{
		m_channel0 = new std::vector<SignalType>(*channel0);
		m_channel1 = new std::vector<SignalType>(*channel1);
	}
	else
	{
		m_channel0 = channel0;
		m_channel1 = channel1;
	}
}

SignalData::SignalData(std::vector<SignalType>* channel0, std::vector<SignalType>* channel1, std::size_t minPos, std::size_t maxPos, bool copy)
	: m_copy(copy), m_min(minPos), m_max(maxPos), m_alignment(0)
{
	if (copy)
	{
		m_channel0 = new std::vector<SignalType>(*channel0);
		m_channel1 = new std::vector<SignalType>(*channel1);
	}
	else
	{
		m_channel0 = channel0;
		m_channel1 = channel1;
	}
	if (minPos >= channel0->size() || minPos >= channel1->size()) m_min = min(channel0->size() - 1, channel1->size() - 1);
	if (maxPos >= channel0->size() || maxPos >= channel1->size()) m_max = min(channel0->size() - 1, channel1->size() - 1);
}

SignalData::~SignalData() {
	if (m_copy) {
		delete m_channel0;
		delete m_channel1;
	}
}

SignalValue SignalData::Channel0(size_t position) const {
	if (position < 0 || position >= m_channel0->size()) return SignalZero;
	return m_channel0->at(position).value;
}

SignalValue SignalData::Channel1(size_t position) const {
	if (position < 0 || position >= m_channel1->size()) return SignalZero;
	return m_channel1->at(position).value;
}

SignalValue SignalData::Channel1(size_t position, DelayType delay) const {
	return Channel1(position + delay + m_alignment);
}

bool SignalData::Align(long position) {
	long p = position;
	
	if (position >= m_channel0->size()) return false;
	if (position >= m_channel1->size()) return false;

	if (m_channel0->at(position).timestamp > m_channel1->at(position).timestamp)
	{
		while (m_channel1->at(p).timestamp < m_channel0->at(position).timestamp) {
			p++;
			if (p >= m_channel1->size()) return false;
		}
		if (p > 0 && m_channel0->at(position).timestamp - m_channel1->at(p-1).timestamp > m_channel1->at(p).timestamp - m_channel0->at(position).timestamp)
		{
			p--;
		}
	}
	else if (m_channel0->at(position).timestamp < m_channel1->at(position).timestamp)
	{
		while (m_channel1->at(p).timestamp > m_channel0->at(position).timestamp) {
			p--;
			if (p < 0) return false;
		}
		if (p < m_channel1->size()-1 && m_channel0->at(position).timestamp - m_channel1->at(p).timestamp > m_channel1->at(p + 1).timestamp - m_channel0->at(position).timestamp)
		{
			p++;
		}
	}
	m_alignment = p - position;
	return true;
}
