#include "pch.h"
#include "WASAPIEngine.h"

using namespace concurrency;

WASAPIEngine::WASAPIEngine()
{
	m_deviceList = std::vector<WASAPIDevice^>();
	m_collector = nullptr;
	m_consumer = nullptr;
	HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
}

WASAPIEngine::~WASAPIEngine()
{
}

IAsyncAction^ WASAPIEngine::InitializeAsync(UIHandler^ func)
{
	return create_async([this ,func]
	{
		// Get the string identifier of the audio renderer
		String^ AudioSelector = MediaDevice::GetAudioCaptureSelector();

		// Add custom properties to the query
		auto PropertyList = ref new Platform::Collections::Vector<String^>();
		PropertyList->Append(PKEY_AudioEndpoint_Supports_EventDriven_Mode);

		// Setup the asynchronous callback
		Concurrency::task<DeviceInformationCollection^> enumOperation(DeviceInformation::FindAllAsync(AudioSelector, PropertyList));
		enumOperation.then([this](DeviceInformationCollection^ DeviceInfoCollection)
		{
			if ((DeviceInfoCollection == nullptr) || (DeviceInfoCollection->Size == 0))
			{
				OutputDebugString(L"No Devices Found.\n");
			}
			else
			{
				try
				{
					// Enumerate through the devices and the custom properties
					for (unsigned int i = 0; i < DeviceInfoCollection->Size; i++)
					{
						DeviceInformation^ deviceInfo = DeviceInfoCollection->GetAt(i);
						String^ DeviceInfoString = deviceInfo->Name;
						String^ DeviceIdString = deviceInfo->Id;

						if (deviceInfo->Properties->Size > 0)
						{
							auto device = ref new WASAPIDevice();
							device->Name = DeviceInfoString;
							device->ID = DeviceIdString;
							m_deviceList.push_back(device);
							OutputDebugString(device->Name->Data());
							OutputDebugString(device->ID->Data());
							OutputDebugString(L"\n");
						}
					}
				}
				catch (Platform::Exception^ e)
				{

				}
			}
		})
		.then([this, func]()
		{
			if (m_deviceList.size() > 0)
			{
				m_collector = ref new DataCollector(m_deviceList.size());
				for (size_t i = 0; i < m_deviceList.size(); i++)
				{
					m_deviceList[i]->InitCaptureDevice(i, m_collector);
				}
				m_consumer = ref new DataConsumer(m_deviceList.size(), m_collector, func);
				m_consumer->Start();
			}
		});
	});
}

void WASAPIEngine::Finish()
{
	if (m_deviceList.size() > 0)
	{
		for (size_t i = 0; i < m_deviceList.size(); i++)
		{
			m_deviceList[i]->Capture->FinishCaptureAsync();
		}
		m_consumer->Finish();
		m_collector->Finish();
	}
	return;
}
