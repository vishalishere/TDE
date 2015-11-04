#include "pch.h"
#include "TimeDelayEstimation.h"

using namespace TimeDelayEstimation;

CalculationStep::CalculationStep() : data(NULL), inbuf(NULL), outbuf(NULL), done(false), step(0) {}

CalculationStep::~CalculationStep() {
	if (data != NULL) delete data;
	if (inbuf != NULL) delete inbuf;
	if (outbuf != NULL) delete outbuf;
}

TDE::TDE(size_t aMaxDelay, const SignalData& aData)
{
	if (aMaxDelay < 5)
	{
		m_maxDelay = 5;
		m_windowStart = 1;
		m_windowEnd = 10;
	}
	else if (aMaxDelay < 10)
	{
		m_maxDelay = (DelayType)aMaxDelay;
		m_windowStart = 2;
		m_windowEnd = 2 * (DelayType)aMaxDelay - 1;
	}
	else if (aMaxDelay < 20)
	{
		m_maxDelay = (DelayType)aMaxDelay;
		m_windowStart = 2;
		m_windowEnd = 2 * (DelayType)aMaxDelay - 1;
	}
	else
	{
		m_maxDelay = (DelayType)aMaxDelay;
		m_windowStart = 5;
		m_windowEnd = 2 * (DelayType)aMaxDelay - 4;
	}
	m_dataLength = aData.Length();
	size_t first = aData.First();

	m_channel0 = new SignalValue[m_dataLength];
	m_channel1 = new SignalValue[m_dataLength + 2 * m_maxDelay];

	for (size_t i = 0; i < m_maxDelay; i++)
	{
		m_channel1[i] = aData.Channel1(first - m_maxDelay + i, 0);
	}
	for (size_t i = 0; i < m_dataLength; i++)
	{ 
		m_channel0[i] = aData.Channel0(first + i);
		m_channel1[i + m_maxDelay] = aData.Channel1(first + i, 0);
	}
	for (size_t i = 0; i < m_maxDelay; i++)
	{
		m_channel1[i + m_dataLength + m_maxDelay] = aData.Channel1(first + m_dataLength + i, 0);
	}
}

TDE::~TDE()
{
	delete[] m_channel0;
	delete[] m_channel1;
}

DelayType TDE::FindDelay(Algoritm a)
{
	TDEVector* v;
	switch (a)
	{
	case Algoritm::CC: v = CrossCorrelation(); break;
	case Algoritm::ASDF: v = AverageSquareDifference(); break;
	case Algoritm::PHAT: v = PhaseTransform(); break;
	default: return FindPeak();
	}

	CalcType value = v->at(m_maxDelay).value;
	DelayType delay = v->at(m_maxDelay).delay;

	for (DelayType i = m_windowStart; i < m_windowEnd; i++)
	{
		if (a == Algoritm::ASDF ? v->at(i).value < value : v->at(i).value > value)
		{
			value = v->at(i).value;
			delay = v->at(i).delay;
		}
	}
	delete v;
	return delay;
}

DelayType TDE::FindPeak()
{
	CalcType value0Max = CalcZero;
	CalcType value1Max = CalcZero;

	size_t index0 = 0;
	size_t index1 = 0;

	for (size_t position = 0; position < m_dataLength; position++)
	{
		CalcType val0 = abs(m_channel0[position]);
		CalcType val1 = abs(m_channel1[position + m_maxDelay]); 
		if (val0 > value0Max)
		{
			value0Max = val0;
			index0 = position;
		}
		if (val1 > value1Max)
		{
			value1Max = val1;
			index1 = position;
		}
	}
	return index1 - index0;
}

TDEVector* TDE::CrossCorrelation() 
{
	TDEVector* result = new TDEVector(2 * m_maxDelay + 1, { 0, CalcZero });
	for (DelayType delay = -m_maxDelay; delay <= m_maxDelay; delay++) 
	{
		CalcType sum = 0;
		for (size_t position = 0; position < m_dataLength; position++)
		{
			sum += (CalcType)m_channel0[position] * (CalcType)m_channel1[position + delay + m_maxDelay];
		}
		result->at(delay + m_maxDelay).delay = delay;
		result->at(delay + m_maxDelay).value = sum;
	}
	return result;
}

TDEVector* TDE::PhaseTransform() 
{
	TDEVector* result = CrossCorrelation();
	
	int nfft = (int)result->size();
	typedef kissfft<double> FFT;
	typedef std::complex<double> cpx_type;

	FFT fft(nfft, false);
	FFT ifft(nfft, true);

	std::vector<cpx_type> inbuf(nfft);
	std::vector<cpx_type> outbuf(nfft);

	for (int k = 0; k < nfft; ++k) inbuf[k] = cpx_type((double)result->at(k).value, 0);
	fft.transform(&inbuf[0], &outbuf[0]);

	for (int k = 0; k < nfft; ++k) inbuf[k] = outbuf[k] / std::abs(outbuf[k]);
	ifft.transform(&inbuf[0], &outbuf[0]);

	for (int k = 0; k < nfft; ++k) result->at(k).value = (int)std::abs(outbuf[k]);
	return result;
}

TDEVector* TDE::AverageSquareDifference() 
{
	TDEVector* result = new TDEVector(2 * m_maxDelay + 1, { 0, CalcZero });
	for (DelayType delay = -m_maxDelay; delay <= m_maxDelay; delay++) 
	{
		CalcType sum = 0;
		for (size_t position = 0; position < m_dataLength; position++) 
		{
			CalcType diff = m_channel0[position] - m_channel1[position + delay + m_maxDelay];
			sum += diff * diff;
		}
		result->at(delay + m_maxDelay).delay = delay;
		result->at(delay + m_maxDelay).value = sum / m_dataLength;
	}
	return result;
}

CalculationStep* TDE::CrossCorrelation_Step(CalculationStep* aStep) 
{
	CalculationStep* step = NULL;
	if (aStep == NULL) 
	{
		step = new CalculationStep;
		step->data = new TDEVector(2 * m_maxDelay + 1, { 0, CalcZero });
		step->delay = -m_maxDelay;
	}
	else 
	{
		step = aStep;
		step->delay++;
	}
	CalcType sum = 0;
	for (size_t position = 0; position < m_dataLength; position++) 
	{
		sum += (CalcType)m_channel0[position] * (CalcType)m_channel1[position + m_maxDelay + step->delay];
	}
	step->data->at(step->delay + m_maxDelay).delay = step->delay;
	step->data->at(step->delay + m_maxDelay).value = sum;

	if (step->delay == m_maxDelay) 
	{
		step->done = true;
	}
	return step;
}

CalculationStep* TDE::PhaseTransform_Step(CalculationStep* aStep) 
{
	typedef kissfft<double> FFT;
	if (aStep == NULL) 
	{
		return CrossCorrelation_Step(aStep);
	}
	else if (aStep->step==0)
	{
		CalculationStep* step = CrossCorrelation_Step(aStep);
		if (step->done) step->done = false;
		else return step;
	}
	else if (aStep->step == 1) 
	{
		aStep->nfft = (int)aStep->data->size();
		aStep->inbuf = new std::vector<cpx_type>(aStep->nfft);
		aStep->outbuf = new std::vector<cpx_type>(aStep->nfft);
	}
	else if (aStep->step == 2)
	{
		for (int k = 0; k < aStep->nfft; ++k) (*aStep->inbuf)[k] = cpx_type((double)aStep->data->at(k).value, 0);
	}
	else if (aStep->step == 3) 
	{
		FFT fft(aStep->nfft, false);
		fft.transform(&(*aStep->inbuf)[0], &(*aStep->outbuf)[0]);
	}
	else if (aStep->step == 4) 
	{
		for (int k = 0; k < aStep->nfft; ++k) (*aStep->inbuf)[k] = (*aStep->outbuf)[k] / std::abs((*aStep->outbuf)[k]);
	}
	else if (aStep->step == 5) 
	{
		FFT ifft(aStep->nfft, true);
		ifft.transform(&(*aStep->inbuf)[0], &(*aStep->outbuf)[0]);
	}
	else 
	{
		for (int k = 0; k < aStep->nfft; ++k) aStep->data->at(k).value = (int)std::abs((*aStep->outbuf)[k]);
		aStep->done = true;
		return aStep;
	}
	aStep->step++;
	return aStep;
}

CalculationStep* TDE::AverageSquareDifference_Step(CalculationStep* aStep) 
{
	CalculationStep* step = NULL;
	if (aStep == NULL) 
	{
		step = new CalculationStep;
		step->data = new TDEVector(2 * m_maxDelay + 1, { 0, CalcZero });
		step->delay = -m_maxDelay;
	}
	else 
	{
		step = aStep;
		step->delay++;
	}
	CalcType sum = 0;
	for (size_t position = 0; position < m_dataLength; position++) 
	{
		CalcType diff = m_channel0[position] - m_channel1[position + m_maxDelay + step->delay];
		sum += diff * diff;
	}
	step->data->at(step->delay + m_maxDelay).delay = step->delay;
	step->data->at(step->delay + m_maxDelay).value = sum / m_dataLength;

	if (step->delay == m_maxDelay) 
	{
		step->done = true;
	}
	return step;
}

void TDE::Normalize(TDEVector* vec, CalcType level) 
{
	if (vec == nullptr) return;
	CalcType max = abs(vec->at(0).value);
	for (size_t i = 1; i < vec->size(); i++) 
	{
		if (abs(vec->at(i).value) > max) 
		{
			max = abs(vec->at(i).value);
		}
	}
	if (max == CalcZero) return;
	for (size_t i = 0; i < vec->size(); i++) 
	{
		vec->at(i).value = (vec->at(i).value * level) / max;
	}
}
