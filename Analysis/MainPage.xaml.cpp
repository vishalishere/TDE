//
// MainPage.xaml.cpp
// Implementation of the MainPage class.
//

#include "pch.h"
#include "MainPage.xaml.h"

#include <ppltasks.h>
#include <sstream>

using namespace TDEAnalysis;

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;
using namespace Windows::System::Threading;

using namespace concurrency;
using namespace Windows::Storage;

// The Blank Page item template is documented at http://go.microsoft.com/fwlink/?LinkId=402352&clcid=0x409

TDEAnalysis::MainPage::MainPage()
{
	InitializeComponent();
	iMultiple = true;
}

void TDEAnalysis::MainPage::Button1_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	Load();
}

void TDEAnalysis::MainPage::Button2_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	TDE();
}

void TDEAnalysis::MainPage::Button3_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	if (WorkItem != nullptr)
	{
		WorkItem->Cancel();
	}	
}

void TDEAnalysis::MainPage::Button4_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	canvas1->Children->Clear();
	text->Text = "---";
}

void TDEAnalysis::MainPage::Button5_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	Sound();
}

void TDEAnalysis::MainPage::Button6_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	double d = 0.0;
	switch (rt->SelectedIndex) {
		case 1:  d = 44100; break;
		case 2:  d = 48000; break;
		default: d = 16000; break; 
	}
	DrawMics(d, -1 * _wtoi(delay1->Text->Data()), -1 * _wtoi(delay2->Text->Data()), -1 * _wtoi(delay3->Text->Data()));
}

void TDEAnalysis::MainPage::multiple_Checked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	iMultiple = !iMultiple;
}

void TDEAnalysis::MainPage::Msg(String^ message) {
	text->Text = message;
}

void TDEAnalysis::MainPage::DrawMic(double rate, double dist, int delay, int x, SolidColorBrush^ color) {
	for (int i = (2 * delay) - 1; i < 2 * (delay + 1); i++) {
		double val = (((double)i / (2.0*rate)) * 343.0) / dist;
		if (val > 1 || val < -1) continue;
		double ang = asin(val);
		::Shapes::Line^ line = ref new ::Shapes::Line();
		line->X1 = x;
		line->Y1 = 0;
		line->X2 = line->X1 + sin(ang) * 200;
		line->Y2 = line->Y1 + cos(ang) * 200;
		line->Stroke = color;
		canvas2->Children->Append(line);
	}
}

void TDEAnalysis::MainPage::DrawMics(double rate, int delay1, int delay2, int delay3)
{
	canvas2->Children->Clear();
	DrawMic(rate, 0.03573, delay1, 200, ref new SolidColorBrush(Windows::UI::Colors::Blue));
	DrawMic(rate, 0.07146, delay2, 250, ref new SolidColorBrush(Windows::UI::Colors::Green));
	DrawMic(rate, 0.21438, delay3, 500, ref new SolidColorBrush(Windows::UI::Colors::Red));
}

void TDEAnalysis::MainPage::DrawGrid(int x, int count) {
	::Shapes::Line^ line1 = ref new ::Shapes::Line();
	line1->X1 = 10;
	line1->X2 = 10 + count * x;
	line1->Y1 = 100;
	line1->Y2 = 100;
	line1->Stroke = ref new SolidColorBrush(Windows::UI::Colors::DarkGray);
	line1->StrokeThickness = 4;
	canvas1->Children->Append(line1);

	for (int i = 0; i < count; i++) {
		::Shapes::Line^ line = ref new ::Shapes::Line();
		line->X1 = 10 + i * x;
		line->X2 = 10 + i * x;
		line->Y1 = 0;
		line->Y2 = 200;
		if (i == (count - 1) / 2) {
			line->Stroke = ref new SolidColorBrush(Windows::UI::Colors::DarkGray);
			line->StrokeThickness = 4;
		}
		else {
			line->Stroke = ref new SolidColorBrush(Windows::UI::Colors::LightGray);
			if ((((count - 1) / 2) - i) % 10 == 0 ) {
				line->StrokeThickness = 3;
			}
			else if ((((count - 1) / 2) - i) % 5 == 0) {
				line->StrokeThickness = 2;
			}
			else {
				line->StrokeThickness = 1;
			}
		}
		canvas1->Children->Append(line);
	}
}

void TDEAnalysis::MainPage::DrawGraph(int ind, int y1, int y2)
{
	::Shapes::Line^ line = ref new ::Shapes::Line();
	line->X1 = 10 + ind * 10;
	line->X2 = 20 + ind * 10;
	line->Y1 = y1;
	line->Y2 = y2;

	switch (clr->SelectedIndex) {
		case 1:  line->Stroke = ref new SolidColorBrush(Windows::UI::Colors::Green); break;
		case 2:  line->Stroke = ref new SolidColorBrush(Windows::UI::Colors::Blue); break;
		case 3:  line->Stroke = ref new SolidColorBrush(Windows::UI::Colors::Gray); break;
		case 4:  line->Stroke = ref new SolidColorBrush(Windows::UI::Colors::Yellow); break;
		default: line->Stroke = ref new SolidColorBrush(Windows::UI::Colors::Red); break;
	}
	canvas1->Children->Append(line);
}

void TDEAnalysis::MainPage::DrawSound(double x, int y1, int y2, int y3, int y4)
{
	::Shapes::Line^ line1 = ref new ::Shapes::Line();
	line1->X1 = 10 + 2 * (int)(600 * x);
	line1->X2 = 10 + 2 * (int)(600 * x);
	line1->Y1 = min(y1,0) + 150;
	line1->Y2 = max(y2,0) + 150;
	line1->Stroke = ref new SolidColorBrush(Windows::UI::Colors::Blue);
	canvas2->Children->Append(line1);

	::Shapes::Line^ line2 = ref new ::Shapes::Line();
	line2->X1 = 10 + 2 * (int)(600 * x) + 1;
	line2->X2 = 10 + 2 * (int)(600 * x) + 1;
	line2->Y1 = min(y3,0) + 150;
	line2->Y2 = max(y4,0) + 150;
	line2->Stroke = ref new SolidColorBrush(Windows::UI::Colors::Red);
	canvas2->Children->Append(line2);
}

void TDEAnalysis::MainPage::TDE()
{
	if (WorkItem != nullptr) {
		text->Text = "Already running";
		return;
	}
	int tp = calcType->SelectedIndex;
	int sz = _wtoi(calcSize->Text->Data());
	int mn = _wtoi(calcMin->Text->Data());
	int mx = _wtoi(calcMax->Text->Data());
	int sg = sig->SelectedIndex;
	int rf = ref->SelectedIndex;

	DrawGrid(10, 2 * sz + 1);

	DataParser::GraphHandler^ uiDelegate1 = ref new DataParser::GraphHandler([this](int ind, int y1, int y2)
	{
		auto uiDelegate = [this, ind, y1, y2]()
		{
			DrawGraph(ind, y1, y2);
		};
		auto handler = ref new Windows::UI::Core::DispatchedHandler(uiDelegate);
		Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, handler);
	});

	DataParser::MsgHandler^ uiDelegate2 = ref new DataParser::MsgHandler([this](String^ message)
	{
		auto uiDelegate = [this, message]()
		{
			Msg(message);
		};
		auto handler = ref new Windows::UI::Core::DispatchedHandler(uiDelegate);
		Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, handler);
	});

	auto workItemDelegate = [this, tp, sz, mn, mx, rf, sg, uiDelegate1, uiDelegate2](IAsyncAction^ workItem)
	{
		parser.TDE(tp, sz, mn, mx, rf, sg, uiDelegate1, uiDelegate2, workItem);
	};

	auto completionDelegate = [this](IAsyncAction^ action, AsyncStatus status)
	{
		switch (action->Status)
		{
		case AsyncStatus::Started:
			break;
		case AsyncStatus::Completed:
			WorkItem = nullptr;
			break;
		case AsyncStatus::Canceled:
			WorkItem = nullptr;
			break;
		}
	};

	auto workItemHandler = ref new Windows::System::Threading::WorkItemHandler(workItemDelegate);
	auto completionHandler = ref new Windows::Foundation::AsyncActionCompletedHandler(completionDelegate, Platform::CallbackContext::Same);

	WorkItem = Windows::System::Threading::ThreadPool::RunAsync(workItemHandler, Windows::System::Threading::WorkItemPriority::Normal);
	WorkItem->Completed = completionHandler;
}

void TDEAnalysis::MainPage::Sound()
{
	if (WorkItem != nullptr) {
		text->Text = "Already running";
		return;
	}

	int mn = _wtoi(calcMin->Text->Data());
	int mx = _wtoi(calcMax->Text->Data());

	int sg = sig->SelectedIndex;
	int rf = ref->SelectedIndex;

	canvas2->Children->Clear();

	DataParser::SoundHandler^ uiDelegate1 = ref new DataParser::SoundHandler([this](double x, int y1, int y2, int y3, int y4)
	{
		auto uiDelegate = [this, x, y1, y2, y3, y4]()
		{
			DrawSound(x, y1, y2, y3, y4);
		};
		auto handler = ref new Windows::UI::Core::DispatchedHandler(uiDelegate);
		Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, handler);
	});

	DataParser::MsgHandler^ uiDelegate2 = ref new DataParser::MsgHandler([this](String^ message)
	{
		auto uiDelegate = [this, message]()
		{
			Msg(message);
		};
		auto handler = ref new Windows::UI::Core::DispatchedHandler(uiDelegate);
		Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, handler);
	});

	auto workItemDelegate = [this, mn, mx, rf, sg, uiDelegate1, uiDelegate2](IAsyncAction^ workItem)
	{
		parser.Sound(mn, mx, rf, sg, 600, 150, uiDelegate1, uiDelegate2, workItem);
	};

	auto completionDelegate = [this](IAsyncAction^ action, AsyncStatus status)
	{
		switch (action->Status)
		{
		case AsyncStatus::Started:
			break;
		case AsyncStatus::Completed:
			WorkItem = nullptr;
			break;
		case AsyncStatus::Canceled:
			WorkItem = nullptr;
			break;
		}
	};

	auto workItemHandler = ref new Windows::System::Threading::WorkItemHandler(workItemDelegate);
	auto completionHandler = ref new Windows::Foundation::AsyncActionCompletedHandler(completionDelegate, Platform::CallbackContext::Same);

	WorkItem = Windows::System::Threading::ThreadPool::RunAsync(workItemHandler, Windows::System::Threading::WorkItemPriority::Normal);
	WorkItem->Completed = completionHandler;
}

void TDEAnalysis::MainPage::Load()
{
	DataParser::LoadHandler^ uiDelegate = ref new DataParser::LoadHandler([this](String^ message, int count, int result)
	{
		text->Text = message;
		if (result) {
			calcMin->Text = "0";
			calcMax->Text = count.ToString();
		}
	});

	Windows::ApplicationModel::Package^ package = Windows::ApplicationModel::Package::Current;
	Windows::Storage::StorageFolder^ installedLocation = package->InstalledLocation;

	if (iMultiple) {
		Array<String^>^ fileNames = ref new Array<String^>(4);
		fileNames->set(0, fileName->Text + "0.csv");
		fileNames->set(1, fileName->Text + "1.csv");
		fileNames->set(2, fileName->Text + "2.csv");
		fileNames->set(3, fileName->Text + "3.csv");
		parser.LoadMultiple(installedLocation, fileNames, uiDelegate);
	}
	else {
		parser.LoadSingle(installedLocation, fileName->Text + ".csv", uiDelegate);
	}
}
