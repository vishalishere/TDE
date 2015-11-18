#include "pch.h"
#include "WASAPIDevice.h"

using namespace LibAudio;

WASAPIDevice::WASAPIDevice()
{
}

void WASAPIDevice::InitCaptureDevice(size_t id, DataCollector^ collector)
{
	Number = id;
	Capture = Make<WASAPICapture>();

	StateChangedEvent = Capture->GetDeviceStateEvent();
	DeviceStateChangeToken = StateChangedEvent->StateChangedEvent += ref new DeviceStateChangedHandler(this, &WASAPIDevice::OnDeviceStateChange);

	Capture->InitializeAudioDeviceAsync(ID, Number, collector);
}

void WASAPIDevice::OnDeviceStateChange(Object^ sender, DeviceStateChangedEventArgs^ e)
{
	// Get the current time for messages
	auto t = Windows::Globalization::DateTimeFormatting::DateTimeFormatter::LongTime;
	Windows::Globalization::Calendar^ calendar = ref new Windows::Globalization::Calendar();
	calendar->SetToNow();

	// Handle state specific messages
	switch (e->State)
	{
		case DeviceState::DeviceStateInitialized:
		{
			String^ str = String::Concat(ID, "-DeviceStateInitialized\n");
			OutputDebugString(str->Data());
			Capture->StartCaptureAsync();
			break;
		}
		case DeviceState::DeviceStateCapturing:
		{
			String^ str = String::Concat(ID, "-DeviceStateCapturing\n");
			OutputDebugString(str->Data());
			break;
		}
		case DeviceState::DeviceStateDiscontinuity:
		{
			String^ str = String::Concat(ID, "-DeviceStateDiscontinuity\n");
			OutputDebugString(str->Data());
			break;
		}
		case DeviceState::DeviceStateInError:
		{
			String^ str = String::Concat(ID, "-DeviceStateInError\n");
			OutputDebugString(str->Data());
			break;
		}
	}
}
