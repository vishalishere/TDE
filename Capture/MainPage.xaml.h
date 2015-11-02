//
// MainPage.xaml.h
// Declaration of the MainPage class.
//

#pragma once

#include "MainPage.g.h"
#include "WASAPIEngine.h"

using namespace Wasapi;
using namespace Windows::UI::Xaml::Media;

using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Activation;

namespace SoundCapture
{
	/// <summary>
	/// An empty page that can be used on its own or navigated to within a Frame.
	/// </summary>
	public ref class MainPage sealed
	{
	public:
		MainPage();

	private:
		WASAPIEngine^ m_wasapiEngine;

		void Start();
		void Direction(double rate, double dist, int delay, int x, int length, SolidColorBrush^ color);
		
		void App_Resuming(Object^ sender, Object^ e);
		void App_Suspending(Object^ sender, SuspendingEventArgs^ e);
	};
}
