#include "pch.h"
#include "Parser.h"

using namespace DataParser;

using namespace Platform;
using namespace concurrency;
using namespace Windows::Storage;
using namespace Windows::Foundation;

const double SCALE = 32000.0;
CRITICAL_SECTION CriticalSection;

Parser::Parser() : iData(NULL), iCorrupted(false) {
	BOOL res = InitializeCriticalSectionAndSpinCount(&CriticalSection, 0x00000400);
	if (!res) throw ref new Exception(0, "CriticalSection");
}

Parser::~Parser()
{
	Flush();
	DeleteCriticalSection(&CriticalSection);
}

void Parser::Flush() {
	if (iData != NULL) {
		for (size_t i = 0; i < iData->size(); i++) {
			if (iData->at(i) != NULL) {
				delete iData->at(i);
				iData->at(i) = NULL;
			}
		}
		delete iData;
		iData = NULL;
	}
	iCorrupted = false;
}

void Parser::LoadSingle(StorageFolder^ folder, String^ fileName, LoadHandler^ func) {
	Flush();
	create_task(folder->GetFileAsync(fileName)).then([this, func](task<StorageFile^> getFileTask)
	{
		try
		{
			Windows::Storage::StorageFile^ file = getFileTask.get();
			if (file != nullptr)
			{
				create_task(FileIO::ReadTextAsync(file)).then([this, func](task<String^> task)
				{
					try
					{
						String^ fileContent = task.get();
						ParseSingle(fileContent, func);
					}
					catch (COMException^ ex) {
						iCorrupted = true;
						func(ex->Message, 0, 0);
					}
				});
			}
		}
		catch (Exception^ e) {
			iCorrupted = true;
			func(e->Message, 0, 0);
		}
	});
}

void Parser::LoadMultiple(StorageFolder^ folder, const Array<String^>^ fileNames, LoadHandler^ func) {
	Flush();
	size_t size = fileNames->Length;
	iData = new std::vector<std::vector<TimeDelayEstimation::AudioDataItem>*>(size);
	for (size_t i = 0; i < size; i++) iData->at(i) = NULL;

	for (size_t i = 0; i < fileNames->Length; ++i) {
		create_task(folder->GetFileAsync(fileNames->get((unsigned int)i))).then([this, i, func](task<StorageFile^> getFileTask)
		{
			try
			{
				Windows::Storage::StorageFile^ file = getFileTask.get();
				if (file != nullptr)
				{
					create_task(FileIO::ReadTextAsync(file)).then([this, i, func](task<String^> task)
					{
						try
						{
							String^ fileContent = task.get();
							ParseMultiple(i, fileContent, func);
						}
						catch (COMException^ ex) {
							iCorrupted = true;
							func(ex->Message, 0, 0);
						}	
					});
				}
			}
			catch (Exception^ e) {
				iCorrupted = true;
				func(e->Message, 0, 0);
			}
		});
	}
}

void Parser::ParseSingle(String^ data, LoadHandler^ func) {
	std::vector<wchar_t*>* lines = new std::vector<wchar_t*>();
	wchar_t* buffer = new wchar_t[data->Length() + 1];
	wcscpy_s(buffer, data->Length() + 1, data->Data());

	wchar_t *token = NULL, *next_token = NULL;
	wchar_t strDelimit1[] = L"\r\n", strDelimit2[] = L";";

	token = wcstok_s(buffer, strDelimit1, &next_token);
	while (token != NULL) {
		lines->push_back(token);
		token = wcstok_s(NULL, strDelimit1, &next_token);
	}
	iData = new std::vector<std::vector<TimeDelayEstimation::AudioDataItem>*>(lines->size());
	next_token = NULL;
	for (size_t i = 0; i < lines->size(); i++) {
		iData->at(i) = new std::vector<TimeDelayEstimation::AudioDataItem>;
		token = wcstok_s(lines->at(i), strDelimit2, &next_token);
		while (token != NULL) {
			int d = _wtoi(token);
			iData->at(i)->push_back({ (INT16)d, 0, false });
			token = wcstok_s(NULL, strDelimit2, &next_token);
		}
	}
	delete[] buffer;
	delete lines;
	func("Loaded " + (iData->at(0)->size()).ToString() + " datapoints", iData->at(0)->size() - 1, 1);
}

void Parser::ParseMultiple(size_t index, String^ data, LoadHandler^ func) {
	EnterCriticalSection(&CriticalSection);

	iData->at(index) = new std::vector<TimeDelayEstimation::AudioDataItem>;

	wchar_t* buffer = new wchar_t[data->Length()+1];
	wcscpy_s(buffer, data->Length()+1, data->Data());

	wchar_t *token = NULL;
	wchar_t *next_token = NULL;

	wchar_t strDelimit[] = L"\r\n";
	token = wcstok_s(buffer, strDelimit, &next_token);

	while (token != NULL) {
		double d = _wtof(token);
		iData->at(index)->push_back({ (INT16)(d*SCALE), 0, false });
		token = wcstok_s(NULL, strDelimit, &next_token);
	}
	delete[] buffer;
	LeaveCriticalSection(&CriticalSection);

	for (size_t i = 0; i < iData->size(); i++) {
		if (iData->at(i) == NULL) return;
	}
	func("Loaded " + (iData->at(0)->size()).ToString() + " datapoints", iData->at(0)->size() - 1, 1);
}

void Parser::Graph(const TimeDelayEstimation::TDEVector* v, GraphHandler^ func) {
	int64 max = 0;
	for (unsigned int i = 2; i < v->size() - 2; i++) {
		if (abs(v->at(i).value) > max) max = abs(v->at(i).value);
	}
	double scale = 100.0 / (double)max;
	for (unsigned int i = 2; i < v->size() - 3; i++) {
		double y1 = 100.0 - (v->at(i).value * scale);
		double y2 = 100.0 - (v->at(i + 1).value * scale);
		func(i, (int)y1, (int)y2);
	}
}

void Parser::TDE(int type, int size, int min, int max, int ref, int sig, GraphHandler^ func, MsgHandler^ msg, IAsyncAction^ action)
{
	if (iData == NULL || iCorrupted) {
		msg("No data");
		return;
	}
	timeunion beginTime, endTime;
	GetSystemTimeAsFileTime(&beginTime.fileTime);

	TimeDelayEstimation::SignalData data(iData->at(ref), iData->at(sig), min, max, false);

	TimeDelayEstimation::TDE tmp(size);
	TimeDelayEstimation::CalculationStep* step = NULL;

	while (step == NULL || !step->done) {
		switch (type) {
			case 0: step = tmp.CC_Step(step, data); break;
			case 1: step = tmp.ASDF_Step(step, data); break;
			default: step = tmp.PHAT_Step(step, data); break;
		}
		
		auto status = action->Status;
		if (status == AsyncStatus::Canceled)
		{
			msg("CANCELED");
			delete step;
			return;
		}
	}
	Graph(step->data, func);
	delete step;

	GetSystemTimeAsFileTime(&endTime.fileTime);
	int64 delta = (endTime.ul.QuadPart - beginTime.ul.QuadPart) / 10000;
	msg(delta.ToString() + " ms");
}

void Parser::Sound(int min, int max, int ref, int sig, int width, int height, SoundHandler^ func, MsgHandler^ msg, IAsyncAction^ action)
{
	if (iData == NULL || iCorrupted) {
		msg("No data");
		return;
	}
	timeunion beginTime, endTime;
	GetSystemTimeAsFileTime(&beginTime.fileTime);

	TimeDelayEstimation::SignalData data(iData->at(ref), iData->at(sig), min, max, false);

	int maxLevel = 0;
	for (int i = min; i < max; i++) {
		if (abs(data.Channel0(i)) > maxLevel) maxLevel = abs(data.Channel0(i));
		if (abs(data.Channel1(i, 0)) > maxLevel) maxLevel = abs(data.Channel1(i, 0));
	}
	maxLevel /= height;

	for (int i = 0; i < (max - min); i += max(1, (max - min) / width)) {
		int min1 = data.Channel0(i + min) / maxLevel;
		int max1 = data.Channel0(i + min) / maxLevel;
		int min2 = (data.Channel1(i + min, 0)) / maxLevel;
		int max2 = (data.Channel1(i + min, 0)) / maxLevel;
		for (int j = 1; j < (max - min) / width; j++) {
			int val1 = data.Channel0(i + j + min) / maxLevel;
			int val2 = data.Channel1(i + j + min, 0) / maxLevel;
			if (min1 > val1) min1 = val1;
			if (max1 < val1) max1 = val1;
			if (min2 > val2) min2 = val2;
			if (max2 < val2) max2 = val2;
		}
		double x = (double)i / (double)(max - min);
		func(x, min1, max1, min2, max2);

		auto status = action->Status;
		if (status == AsyncStatus::Canceled)
		{
			msg("CANCELED");
			return;
		}
	}
	GetSystemTimeAsFileTime(&endTime.fileTime);
	int64 delta = (endTime.ul.QuadPart - beginTime.ul.QuadPart) / 10000;
	msg(delta.ToString() + " ms");
}
