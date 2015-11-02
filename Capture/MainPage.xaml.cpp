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

MainPage::MainPage()
{
	InitializeComponent();

	Application::Current->Resuming += ref new EventHandler<Platform::Object^>(this, &MainPage::App_Resuming);
	Application::Current->Suspending += ref new SuspendingEventHandler(this, &MainPage::App_Suspending);

	m_wasapiEngine = ref new WASAPIEngine();
	Start();
}

void SoundCapture::MainPage::Direction(double rate, double dist, int delay, int x, int length, SolidColorBrush^ color) {
	double val = (((double)(2*delay) / (2.0*rate)) * 343.0) / dist;
	if (val > 1) val = 1;
	if (val < -1) val = -1;
	double ang = asin(val);
	::Shapes::Line^ line = ref new ::Shapes::Line();
	line->X1 = x;
	line->Y1 = 0;
	line->X2 = line->X1 + sin(ang) * length;
	line->Y2 = line->Y1 + cos(ang) * length;
	line->Stroke = color;
	line->StrokeThickness = 3;
	canvas->Children->Append(line);
}

void SoundCapture::MainPage::Start()
{
	Wasapi::UIHandler^ uiHandler = ref new Wasapi::UIHandler([this](int i0, int i1, int i2, int i3, int i4, int i5, int i6, int i7)
	{
		auto uiDelegate = [this, i0, i1, i2, i3, i4, i5, i6, i7]()
		{
			text1->Text = i0.ToString();
			text2->Text = i1.ToString();
			text3->Text = i2.ToString();
			text4->Text = i3.ToString();
			text5->Text = i4.ToString();
			text6->Text = i5.ToString();
			text7->Text = i6.ToString();
	
			canvas->Children->Clear();

			int vol = i5/50;
			if (vol > 600) vol = 600;

			Direction(i7, 0.35, -1 * i1, 600, vol, ref new SolidColorBrush(Windows::UI::Colors::Red));
			Direction(i7, 0.35, -1 * i2, 650, vol, ref new SolidColorBrush(Windows::UI::Colors::Blue));
			Direction(i7, 0.35, -1 * i3, 700, vol, ref new SolidColorBrush(Windows::UI::Colors::Green));
		};
		auto handler = ref new Windows::UI::Core::DispatchedHandler(uiDelegate);
		Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, handler);
	});
	m_wasapiEngine->InitializeAsync(uiHandler);
}

void SoundCapture::MainPage::App_Resuming(Object^ sender, Object^ e)
{
	//Start();
}

void SoundCapture::MainPage::App_Suspending(Object^ sender, SuspendingEventArgs^ e)
{
	m_wasapiEngine->Finish();
}
