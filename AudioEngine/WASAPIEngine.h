#pragma once
#include "WASAPICapture.h"
#include "DeviceState.h"
#include "WASAPIDevice.h"
#include "DataCollector.h"
#include "DataConsumer.h"

using namespace AudioEngine;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Media::Devices;
using namespace Windows::Devices::Enumeration;
using namespace Platform::Collections;

namespace AudioEngine
{
	// Custom properties defined in mmdeviceapi.h in the format "{GUID} PID"
	static Platform::String^ PKEY_AudioEndpoint_Supports_EventDriven_Mode = "{1da5d803-d492-4edd-8c23-e0c0ffee7f0e} 7";

	public ref class WASAPIEngine sealed
	{
	public:
		WASAPIEngine();
		virtual ~WASAPIEngine();
		IAsyncAction^ InitializeAsync(UIDelegate^ func);
		void Finish();
		
	private:
		std::vector<WASAPIDevice^> m_deviceList;
		DataCollector^	m_collector;
		DataConsumer^	m_consumer;
	};
}
