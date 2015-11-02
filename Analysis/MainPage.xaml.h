//
// MainPage.xaml.h
// Declaration of the MainPage class.
//

#pragma once

#include "MainPage.g.h"

namespace TDEAnalysis {

	static Windows::Foundation::IAsyncAction^ WorkItem = nullptr;

	/// <summary>
	/// An empty page that can be used on its own or navigated to within a Frame.
	/// </summary>
	public ref class MainPage sealed
	{
	public:
		MainPage();

	private:

		void Button1_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void Button2_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void Button3_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void Button4_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void Button5_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void Button6_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);

		void multiple_Checked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);

		void Msg(Platform::String^ message);

		void DrawGrid(int x, int count);
		void DrawGraph(int ind, int y1, int y2);
		void DrawMic(double rate, double dist, int delay, int x, Windows::UI::Xaml::Media::SolidColorBrush^ color);
		void DrawMics(double rate, int delay1, int delay2, int delay3);
		void DrawSound(double x, int y1, int y2, int y3, int y4);

		void Load();
		void TDE();
		void Sound();

		bool iMultiple;
		DataParser::Parser parser;	
	};
}
