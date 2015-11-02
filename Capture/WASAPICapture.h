//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

//
// WASAPICapture.h
//

#include <Windows.h>
#include <wrl\implements.h>
#include <mfapi.h>
#include <AudioClient.h>
#include <mmdeviceapi.h>
#include "DeviceState.h"
#include "DataCollector.h"

using namespace Platform;
using namespace Microsoft::WRL;
using namespace Windows::Media::Devices;
using namespace Windows::Storage::Streams;

#pragma once
namespace Wasapi
{
    // Primary WASAPI Capture Class
    class WASAPICapture :
        public RuntimeClass< RuntimeClassFlags< ClassicCom >, FtmBase, IActivateAudioInterfaceCompletionHandler > 
    {
    public:
        WASAPICapture();

        HRESULT InitializeAudioDeviceAsync(String^ id, size_t i, DataCollector^ streams);
        HRESULT StartCaptureAsync();
        HRESULT StopCaptureAsync();
        HRESULT FinishCaptureAsync();

        DeviceStateChangedEvent^ GetDeviceStateEvent() { return m_DeviceStateChanged; };

        METHODASYNCCALLBACK( WASAPICapture, StartCapture, OnStartCapture );
        METHODASYNCCALLBACK( WASAPICapture, StopCapture, OnStopCapture );
        METHODASYNCCALLBACK( WASAPICapture, SampleReady, OnSampleReady );
        METHODASYNCCALLBACK( WASAPICapture, FinishCapture, OnFinishCapture );

        // IActivateAudioInterfaceCompletionHandler
        STDMETHOD(ActivateCompleted)( IActivateAudioInterfaceAsyncOperation *operation );

    private:
        ~WASAPICapture();

        HRESULT OnStartCapture( IMFAsyncResult* pResult );
        HRESULT OnStopCapture( IMFAsyncResult* pResult );
        HRESULT OnFinishCapture( IMFAsyncResult* pResult );
        HRESULT OnSampleReady( IMFAsyncResult* pResult );
        HRESULT OnSendScopeData( IMFAsyncResult* pResult );

        HRESULT OnAudioSampleRequested( Platform::Boolean IsSilence = false );
        
    private:
        Platform::String^			m_DeviceIdString;
        UINT32						m_BufferFrames;
        HANDLE						m_SampleReadyEvent;
        MFWORKITEM_KEY				m_SampleReadyKey;
		CRITICAL_SECTION			m_CritSec;
        DWORD						m_dwQueueID;

        DWORD						m_cbDataSize;
        BOOL						m_fWriting;
		DWORD						m_counter;
		WAVEFORMATEX				*m_MixFormat;

        IAudioClient3				*m_AudioClient;
        UINT32						m_DefaultPeriodInFrames;
        UINT32						m_FundamentalPeriodInFrames;
        UINT32						m_MaxPeriodInFrames;
        UINT32						m_MinPeriodInFrames;
        IAudioCaptureClient			*m_AudioCaptureClient;
        IMFAsyncResult				*m_SampleReadyAsyncResult;

        DeviceStateChangedEvent^	m_DeviceStateChanged;

		int StartCounter = 0;
		size_t m_CaptureDeviceID;
		DataCollector^ m_streams;
    };
}
