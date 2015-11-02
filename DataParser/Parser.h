#pragma once
#include <vector>
#include "..\LibTDE\TimeDelayEstimation.h"

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Storage;

namespace DataParser
{
	public delegate void LoadHandler(String^, size_t, int);
	public delegate void GraphHandler(int, int, int);
	public delegate void SoundHandler(double, int, int, int, int);
	public delegate void MsgHandler(String^);

    public ref class Parser sealed
    {
    public:
        Parser();
		
		void LoadSingle(StorageFolder^ folder, String^ fileName, LoadHandler^ func);
		void LoadMultiple(StorageFolder^ folder, const Array<String^>^ fileNames, LoadHandler^ func);

		void TDE(int type, int size, int min, int max, int ref, int sig, GraphHandler^ func, MsgHandler^ msg, IAsyncAction^ action);
		void Sound(int min, int max, int ref, int sig, int width, int height, SoundHandler^ func, MsgHandler^ msg, IAsyncAction^ action);
		
	private:
		
		~Parser();
		void Flush();

		void ParseSingle(String^ data, LoadHandler^ func);
		void ParseMultiple(size_t index, String^ data, LoadHandler^ func);
		void Graph(const TimeDelayEstimation::TDEVector* v, GraphHandler^ func);

		bool iCorrupted;
		std::vector<std::vector<TimeDelayEstimation::SignalType>*>* iData;

		union timeunion {
			FILETIME fileTime;
			ULARGE_INTEGER ul;
		};
    };
}
