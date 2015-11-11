//
// MainPage.xaml.cpp
// Implementation of the MainPage class.
//

#include "pch.h"
#include "MainPage.xaml.h"
#include "DataCollector.h"
#include "DataConsumer.h"

using namespace SoundCapture;
using namespace Wasapi;

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

using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Activation;

// The Blank Page item template is documented at http://go.microsoft.com/fwlink/?LinkId=402352&clcid=0x409

MainPage::MainPage() : m_sampleCount(0), m_startCounter(10), m_bufferingCount(0)
{
	InitializeComponent();

	Application::Current->Resuming += ref new EventHandler<Platform::Object^>(this, &MainPage::App_Resuming);
	Application::Current->Suspending += ref new SuspendingEventHandler(this, &MainPage::App_Suspending);

	TimeSpan ts;
	ts.Duration = 10000000;
	m_timer = ref new DispatcherTimer();
	m_timer->Interval = ts;
	m_timer->Tick += ref new EventHandler<Platform::Object^>(this, &MainPage::Tick);
	m_timer->Start();
}

void SoundCapture::MainPage::Direction(double rate, double dist, int delay, int x, int length, SolidColorBrush^ color, int thickness) {
	double val = (((double)delay / rate) * 343.0) / dist;
	if (val > 1) val = 1;
	if (val < -1) val = -1;
	double ang = asin(val);
	::Shapes::Line^ line = ref new ::Shapes::Line();
	line->X1 = x;
	line->Y1 = 0;
	line->X2 = line->X1 + sin(ang) * length;
	line->Y2 = line->Y1 + cos(ang) * length;
	line->Stroke = color;
	line->StrokeThickness = thickness;
	canvas->Children->Append(line);
}

void SoundCapture::MainPage::Start()
{
	Wasapi::UIHandler^ uiHandler = ref new Wasapi::UIHandler([this](uint32 i0, int i1, int i2, int i3, int i4, int i5, UINT64 i6, UINT64 i7, uint32 i8)
	{
		auto uiDelegate = [this, i0, i1, i2, i3, i4, i5, i6, i7, i8]()
		{
			text1->Text = i0.ToString();
			switch ((HeartBeatType)i1)
			{
			case HeartBeatType::DATA:
				text2->Text = "DATA"; 
				m_bufferingCount = 0;
				break;
			case HeartBeatType::INVALID: 
				text2->Text = "INVALID"; 
				break;
			case HeartBeatType::SILENCE:
				text2->Text = "SILENCE"; 
				m_bufferingCount = 0;
				break;
			case HeartBeatType::BUFFERING: 
				text2->Text = "BUFFERING"; 
				m_bufferingCount++;
				break;
			case HeartBeatType::DEVICE_ERROR: 
				text2->Text = "ERROR"; 
				ResetEngine(10);
				break;
			case HeartBeatType::NODEVICE:
				text2->Text = "NO DEVICES";
				Reboot();
				break;
			}

			if (m_bufferingCount == 10) ResetEngine(10);

			text3->Text = i2.ToString();
			text4->Text = i3.ToString();
			text5->Text = i4.ToString();
			text6->Text = i5.ToString();
			text7->Text = i6.ToString();
			text8->Text = i7.ToString();

			if ((HeartBeatType)i1 == HeartBeatType::BUFFERING)
			{
				label1->Text = "TIME STAMP 0";
				label2->Text = "TIME STAMP 1";
			}
			else
			{
				label1->Text = "THRESHOLD";
				label2->Text = "VOLUME";
			}

			if ((HeartBeatType)i1 != HeartBeatType::BUFFERING)
			{
				canvas->Children->Clear();
			}

			if ((HeartBeatType)i1 == HeartBeatType::DATA)
			{
				UINT64 vol = i6 / 20;
				if (vol > 800) vol = 800;

				Direction(i8, 0.4, -1 * i2, 800, (int)vol, ref new SolidColorBrush(Windows::UI::Colors::Red), 4);
				Direction(i8, 0.4, -1 * i3, 800, (int)vol, ref new SolidColorBrush(Windows::UI::Colors::Green), 2);
				Direction(i8, 0.4, -1 * i4, 800, (int)vol, ref new SolidColorBrush(Windows::UI::Colors::Blue), 1);
				m_sampleCount++;
				text9->Text = m_sampleCount.ToString();
			}	
		};
		auto handler = ref new Windows::UI::Core::DispatchedHandler(uiDelegate);
		Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, handler);
	});
	m_wasapiEngine->InitializeAsync(uiHandler);
}

void SoundCapture::MainPage::App_Resuming(Object^ sender, Object^ e)
{
	ResetEngine(10);
}

void SoundCapture::MainPage::App_Suspending(Object^ sender, SuspendingEventArgs^ e)
{
	m_wasapiEngine->Finish();
	m_wasapiEngine = nullptr;
}

void SoundCapture::MainPage::Tick(Object^ sender, Object^ e)
{
	if (m_startCounter == 0)
	{
		text2->Text = "STARTED";
		m_timer->Stop();
		m_wasapiEngine = ref new WASAPIEngine();
		Start();
	}
	else
	{
		text2->Text = m_startCounter.ToString();
		m_startCounter--;
	}
}

void SoundCapture::MainPage::ResetEngine(uint16 counter)
{
	m_startCounter = 0;
	m_wasapiEngine->Finish();
	m_wasapiEngine = nullptr;

	text1->Text = "-";
	text3->Text = "-";
	text4->Text = "-";
	text5->Text = "-";
	text6->Text = "-";
	text7->Text = "-";
	text8->Text = "-";
	text9->Text = "-";
	text2->Text = "RESETTING";
	canvas->Children->Clear();
	m_startCounter = counter;
	m_timer->Start();
}

void SoundCapture::MainPage::Reboot()
{
	m_wasapiEngine->Finish();
	m_wasapiEngine = nullptr;

	text1->Text = "-";
	text3->Text = "-";
	text4->Text = "-";
	text5->Text = "-";
	text6->Text = "-";
	text7->Text = "-";
	text8->Text = "-";
	text9->Text = "-";
	text2->Text = "REBOOTING";
	canvas->Children->Clear();

	RuntimeComponent::HelperClass cl;
	cl.RebootComputer();
}
