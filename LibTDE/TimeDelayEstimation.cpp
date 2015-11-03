#include "pch.h"
#include "TimeDelayEstimation.h"

using namespace TimeDelayEstimation;

CalculationStep::CalculationStep() : data(NULL), inbuf(NULL), outbuf(NULL), done(false), step(0) {}

TDE::TDE(size_t aMaxDelay)
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
}

CalculationStep::~CalculationStep() {
	if (data != NULL) delete data;
	if (inbuf != NULL) delete inbuf;
	if (outbuf != NULL) delete outbuf;
}

DelayType TDE::FindDelay(const SignalData& aData, Algoritm a)
{
	TDEVector* v;
	switch (a)
	{
	case Algoritm::ASDF: v = AverageSquareDifference(aData); break;
	case Algoritm::PHAT: v = PhaseTransform(aData); break;
	default: v = CrossCorrelation(aData); break;
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

TDEVector* TDE::CrossCorrelation(const SignalData& aData) 
{
	TDEVector* result = new TDEVector(2 * m_maxDelay + 1, { 0, CalcZero });
	for (DelayType delay = -m_maxDelay; delay <= m_maxDelay; delay++) 
	{
		CalcType sum = 0;
		for (size_t position = aData.First(); position < aData.Last() + 1; position++) 
		{
			sum += (CalcType)aData.Channel0(position) * (CalcType)aData.Channel1(position, delay);
		}
		result->at(delay + m_maxDelay).delay = delay;
		result->at(delay + m_maxDelay).value = sum;
	}
	return result;
}

TDEVector* TDE::PhaseTransform(const SignalData& aData) 
{
	TDEVector* result = CrossCorrelation(aData);
	
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

TDEVector* TDE::AverageSquareDifference(const SignalData& aData) 
{
	TDEVector* result = new TDEVector(2 * m_maxDelay + 1, { 0, CalcZero });
	for (DelayType delay = -m_maxDelay; delay <= m_maxDelay; delay++) 
	{
		CalcType sum = 0;
		for (size_t position = aData.First(); position < aData.Last() + 1; position++) 
		{
			CalcType diff = aData.Channel0(position) - aData.Channel1(position, delay);
			sum += diff * diff;
		}
		result->at(delay + m_maxDelay).delay = delay;
		result->at(delay + m_maxDelay).value = sum / aData.Length();
	}
	return result;
}

CalculationStep* TDE::CrossCorrelation_Step(CalculationStep* aStep, const SignalData& aData) 
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
	for (size_t position = aData.First(); position < aData.Last() + 1; position++) 
	{
		sum += (CalcType)aData.Channel0(position) * (CalcType)aData.Channel1(position, step->delay);
	}
	step->data->at(step->delay + m_maxDelay).delay = step->delay;
	step->data->at(step->delay + m_maxDelay).value = sum;

	if (step->delay == m_maxDelay) 
	{
		step->done = true;
	}
	return step;
}

CalculationStep* TDE::PhaseTransform_Step(CalculationStep* aStep, const SignalData& aData) 
{
	typedef kissfft<double> FFT;
	if (aStep == NULL) 
	{
		return CrossCorrelation_Step(aStep, aData);
	}
	else if (aStep->step==0)
	{
		CalculationStep* step = CrossCorrelation_Step(aStep, aData);
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

CalculationStep* TDE::AverageSquareDifference_Step(CalculationStep* aStep, const SignalData& aData) 
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
	for (size_t position = aData.First(); position <= aData.Last(); position++) 
	{
		CalcType diff = aData.Channel0(position) - aData.Channel1(position, step->delay);
		sum += diff * diff;
	}
	step->data->at(step->delay + m_maxDelay).delay = step->delay;
	step->data->at(step->delay + m_maxDelay).value = sum / aData.Length();

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
