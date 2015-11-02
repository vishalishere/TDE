#pragma once

#include "..\LibFFT\kiss_fft130\kissfft.hh"
#include "SignalData.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

namespace TimeDelayEstimation {

	typedef std::complex<double> cpx_type;

	struct DataPoint {
		int delay;
		CalcType value;
	};

	typedef std::vector<DataPoint> TDEVector;

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
		TDE(int aMaxDelay) : iMaxDelay(aMaxDelay) {}
		~TDE() {}

		TDEVector* CC(const SignalData& aData) { return CrossCorrelation(aData); }
		TDEVector* PHAT(const SignalData& aData) { return PhaseTransform(aData); }
		TDEVector* ASDF(const SignalData& aData) { return AverageSquareDifference(aData); }

		CalculationStep* CC_Step(CalculationStep* aStep, const SignalData& aData) { return CrossCorrelation_Step(aStep, aData); }
		CalculationStep* PHAT_Step(CalculationStep* aStep, const SignalData& aData) { return PhaseTransform_Step(aStep, aData); }
		CalculationStep* ASDF_Step(CalculationStep* aStep, const SignalData& aData) { return AverageSquareDifference_Step(aStep, aData); }

		TDEVector* CrossCorrelation(const SignalData& aData);
		TDEVector* PhaseTransform(const SignalData& aData);
		TDEVector* AverageSquareDifference(const SignalData& aData);

		CalculationStep* CrossCorrelation_Step(CalculationStep* aStep, const SignalData& aData);
		CalculationStep* PhaseTransform_Step(CalculationStep* aStep, const SignalData& aData);
		CalculationStep* AverageSquareDifference_Step(CalculationStep* aStep, const SignalData& aData);

		void Normalize(TDEVector* vec, CalcType level);

	private:

		int iMaxDelay;

	};
}
