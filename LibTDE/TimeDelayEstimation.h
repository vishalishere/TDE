#pragma once

#include "..\LibFFT\kiss_fft130\kissfft.hh"
#include "SignalData.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

namespace TimeDelayEstimation {

	typedef std::complex<double> cpx_type;

	struct DataPoint {
		DelayType delay;
		CalcType value;
	};

	typedef std::vector<DataPoint> TDEVector;

	enum class Algoritm
	{
		CC,
		ASDF,
		PHAT,
		PEAK
	};

	struct CalculationStep {
		bool done;
		int step;

		DelayType delay;
		TDEVector *data;

		int nfft;

		std::vector<cpx_type> *inbuf;
		std::vector<cpx_type> *outbuf;

		CalculationStep();
		~CalculationStep();
	};

	class TDE
	{
	public:
		TDE(size_t aMaxDelay, const SignalData& aData);
		~TDE();

		DelayType FindDelay(Algoritm a);
		DelayType FindPeak();
		
		TDEVector* CC() { return CrossCorrelation(); }
		TDEVector* PHAT() { return PhaseTransform(); }
		TDEVector* ASDF() { return AverageSquareDifference(); }
	
		CalculationStep* CC_Step(CalculationStep* aStep) { return CrossCorrelation_Step(aStep); }
		CalculationStep* PHAT_Step(CalculationStep* aStep) { return PhaseTransform_Step(aStep); }
		CalculationStep* ASDF_Step(CalculationStep* aStep) { return AverageSquareDifference_Step(aStep); }

		TDEVector* CrossCorrelation();
		TDEVector* PhaseTransform();
		TDEVector* AverageSquareDifference();

		CalculationStep* CrossCorrelation_Step(CalculationStep* aStep);
		CalculationStep* PhaseTransform_Step(CalculationStep* aStep);
		CalculationStep* AverageSquareDifference_Step(CalculationStep* aStep);

		void Normalize(TDEVector* vec, CalcType level);

	private:

		DelayType m_maxDelay;
		DelayType m_windowStart;
		DelayType m_windowEnd;

		SignalValue* m_channel0;
		SignalValue* m_channel1;
		size_t m_dataLength;
	};
}
