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

#include "pch.h"
#include "WASAPICapture.h"

using namespace Windows::Storage;
using namespace Windows::System::Threading;
using namespace Wasapi;
using namespace Platform;

#define BITS_PER_BYTE 8

//
//  WASAPICapture()
//
WASAPICapture::WASAPICapture() :
    m_BufferFrames( 0 ),
    m_cbDataSize( 0 ),
    m_dwQueueID( 0 ),
	m_counter(0),
    m_DeviceStateChanged( nullptr ),
    m_AudioClient( nullptr ),
    m_AudioCaptureClient( nullptr ),
    m_SampleReadyAsyncResult( nullptr ),
    m_fWriting( false )
{
    // Create events for sample ready or user stop
    m_SampleReadyEvent = CreateEventEx( nullptr, nullptr, 0, EVENT_ALL_ACCESS );
    if (nullptr == m_SampleReadyEvent)
    {
        ThrowIfFailed( HRESULT_FROM_WIN32( GetLastError() ) );
    }

    if (!InitializeCriticalSectionEx( &m_CritSec, 0, 0 ))
    {
        ThrowIfFailed( HRESULT_FROM_WIN32( GetLastError() ) );
    }

    m_DeviceStateChanged = ref new DeviceStateChangedEvent();
    if (nullptr == m_DeviceStateChanged)
    {
        ThrowIfFailed( E_OUTOFMEMORY );
    }

	HRESULT hr = S_OK;
	DWORD dwTaskID = 0;
	
	hr = MFLockSharedWorkQueue(L"Capture", 0, &dwTaskID, &m_dwQueueID);

	if (FAILED(hr))
	{
		ThrowIfFailed(hr);
	}

	// Set the capture event work queue to use the MMCSS queue
	m_xSampleReady.SetQueueID(m_dwQueueID);
}

//
//  ~WASAPICapture()
//
WASAPICapture::~WASAPICapture()
{
    SAFE_RELEASE( m_AudioClient );
    SAFE_RELEASE( m_AudioCaptureClient );
    SAFE_RELEASE( m_SampleReadyAsyncResult );

    if (INVALID_HANDLE_VALUE != m_SampleReadyEvent)
    {
        CloseHandle( m_SampleReadyEvent );
        m_SampleReadyEvent = INVALID_HANDLE_VALUE;
    }

    MFUnlockWorkQueue( m_dwQueueID );

    m_DeviceStateChanged = nullptr;

    DeleteCriticalSection( &m_CritSec );
}

//
//  InitializeAudioDeviceAsync()
//
//  Activates the default audio capture on a asynchronous callback thread.  This needs
//  to be called from the main UI thread.
//
HRESULT WASAPICapture::InitializeAudioDeviceAsync(String^ id, size_t i, DataCollector^ streams)
{
	m_CaptureDeviceID = i;
	m_streams = streams;

	//IntPtr ptr = System::Runtime::InteropServices::Marshal::StringToHGlobalAnsi(id);

	// Register MMCSS work queue
	HRESULT hr = S_OK;

    ComPtr<IActivateAudioInterfaceAsyncOperation> asyncOp;
    
    // Get a string representing the Default Audio Capture Device
	m_DeviceIdString = id;

    // This call must be made on the main UI thread.  Async operation will call back to 
    // IActivateAudioInterfaceCompletionHandler::ActivateCompleted, which must be an agile interface implementation
    hr = ActivateAudioInterfaceAsync( m_DeviceIdString->Data(), __uuidof(IAudioClient3), nullptr, this, &asyncOp );
    if (FAILED( hr ))
    {
        m_DeviceStateChanged->SetState( DeviceState::DeviceStateInError, hr, true );
    }
    return hr;
}

//
//  ActivateCompleted()
//
//  Callback implementation of ActivateAudioInterfaceAsync function.  This will be called on MTA thread
//  when results of the activation are available.
//
HRESULT WASAPICapture::ActivateCompleted(IActivateAudioInterfaceAsyncOperation *operation)
{
    HRESULT hr = S_OK;
    HRESULT hrActivateResult = S_OK;
    ComPtr<IUnknown> punkAudioInterface;

    // Check for a successful activation result
    hr = operation->GetActivateResult( &hrActivateResult, &punkAudioInterface );
    if (FAILED( hr ))
    {
        goto exit;
    }

    hr = hrActivateResult;
    if (FAILED( hr ))
    {
        goto exit;
    }

    // Get the pointer for the Audio Client
    punkAudioInterface.CopyTo( &m_AudioClient );
    if (nullptr == m_AudioClient)
    {
        hr = E_NOINTERFACE;
        goto exit;
    }

    hr = m_AudioClient->GetMixFormat( &m_MixFormat );
    if (FAILED( hr ))
    {
        goto exit;
    }

    // convert from Float to 16-bit PCM
    switch ( m_MixFormat->wFormatTag )
    {
    case WAVE_FORMAT_PCM:
        // nothing to do
        break;

    case WAVE_FORMAT_IEEE_FLOAT:
        m_MixFormat->wFormatTag = WAVE_FORMAT_PCM;
        m_MixFormat->wBitsPerSample = 16;
        m_MixFormat->nBlockAlign = m_MixFormat->nChannels * m_MixFormat->wBitsPerSample / BITS_PER_BYTE;
        m_MixFormat->nAvgBytesPerSec = m_MixFormat->nSamplesPerSec * m_MixFormat->nBlockAlign;
        break;

    case WAVE_FORMAT_EXTENSIBLE:
        {
            WAVEFORMATEXTENSIBLE *pWaveFormatExtensible = reinterpret_cast<WAVEFORMATEXTENSIBLE *>(m_MixFormat);
            if ( pWaveFormatExtensible->SubFormat == KSDATAFORMAT_SUBTYPE_PCM )
            {
                // nothing to do
            }
            else if ( pWaveFormatExtensible->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT )
            {
                pWaveFormatExtensible->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
                pWaveFormatExtensible->Format.wBitsPerSample = 16;
                pWaveFormatExtensible->Format.nBlockAlign =
                    pWaveFormatExtensible->Format.nChannels *
                    pWaveFormatExtensible->Format.wBitsPerSample /
                    BITS_PER_BYTE;
                pWaveFormatExtensible->Format.nAvgBytesPerSec =
                    pWaveFormatExtensible->Format.nSamplesPerSec *
                    pWaveFormatExtensible->Format.nBlockAlign;
                pWaveFormatExtensible->Samples.wValidBitsPerSample =
                    pWaveFormatExtensible->Format.wBitsPerSample;

                // leave the channel mask as-is
            }
            else
            {
                // we can only handle float or PCM
                hr = HRESULT_FROM_WIN32( ERROR_NOT_FOUND );
            }
            break;
        }

    default:
        // we can only handle float or PCM
        hr = HRESULT_FROM_WIN32( ERROR_NOT_FOUND );
        break;
    }

    if (FAILED( hr ))
    {
        goto exit;
    }

    // The wfx parameter below is optional (Its needed only for MATCH_FORMAT clients). Otherwise, wfx will be assumed 
    // to be the current engine format based on the processing mode for this stream
    hr = m_AudioClient->GetSharedModeEnginePeriod(m_MixFormat, &m_DefaultPeriodInFrames, &m_FundamentalPeriodInFrames, &m_MinPeriodInFrames, &m_MaxPeriodInFrames);
    if (FAILED(hr))
    {
        return hr;
    }

    hr = m_AudioClient->InitializeSharedAudioStream(
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        m_MinPeriodInFrames,
        m_MixFormat,
        nullptr);

    if (FAILED( hr ))
    {
        goto exit;
    }

    // Get the maximum size of the AudioClient Buffer
    hr = m_AudioClient->GetBufferSize( &m_BufferFrames );
    if (FAILED( hr ))
    {
        goto exit;
    }

    // Get the capture client
    hr = m_AudioClient->GetService( __uuidof(IAudioCaptureClient), (void**) &m_AudioCaptureClient );
    if (FAILED( hr ))
    {
        goto exit;
    }

    // Create Async callback for sample events
    hr = MFCreateAsyncResult( nullptr, &m_xSampleReady, nullptr, &m_SampleReadyAsyncResult );
    if (FAILED( hr ))
    {
        goto exit;
    }

    // Sets the event handle that the system signals when an audio buffer is ready to be processed by the client
    hr = m_AudioClient->SetEventHandle( m_SampleReadyEvent );
    if (FAILED( hr ))
    {
        goto exit;
    }

	m_DeviceStateChanged->SetState(DeviceState::DeviceStateInitialized, S_OK, true);

	m_streams->Initialize(m_CaptureDeviceID, m_MixFormat->nChannels, m_MixFormat->nSamplesPerSec, m_MixFormat->wBitsPerSample, m_MixFormat);

exit:
    if (FAILED( hr ))
    {
        m_DeviceStateChanged->SetState( DeviceState::DeviceStateInError, hr, true );
        SAFE_RELEASE( m_AudioClient );
        SAFE_RELEASE( m_AudioCaptureClient );
        SAFE_RELEASE( m_SampleReadyAsyncResult );
    }
    
    // Need to return S_OK
    return S_OK;
}


//
//  StartCaptureAsync()
//
//  Starts asynchronous capture on a separate thread via MF Work Item
//
HRESULT WASAPICapture::StartCaptureAsync()
{
    HRESULT hr = S_OK;

    // We should be in the initialzied state if this is the first time through getting ready to capture.
    if (m_DeviceStateChanged->GetState() == DeviceState::DeviceStateInitialized)
    {
        m_DeviceStateChanged->SetState( DeviceState::DeviceStateStarting, S_OK, true );
        return MFPutWorkItem2( MFASYNC_CALLBACK_QUEUE_MULTITHREADED, 0, &m_xStartCapture, nullptr );
    }

    // We are in the wrong state
    return E_NOT_VALID_STATE;
}

//
//  OnStartCapture()
//
//  Callback method to start capture
//
HRESULT WASAPICapture::OnStartCapture(IMFAsyncResult* pResult)
{
    HRESULT hr = S_OK;
	
    // Start the capture
    hr = m_AudioClient->Start();
    if (SUCCEEDED( hr ))
    {
        m_DeviceStateChanged->SetState( DeviceState::DeviceStateCapturing, S_OK, true );
        MFPutWaitingWorkItem( m_SampleReadyEvent, 0, m_SampleReadyAsyncResult, &m_SampleReadyKey );
    }
    else
    {
        m_DeviceStateChanged->SetState( DeviceState::DeviceStateInError, hr, true );
    }

    return S_OK;
}

//
//  StopCaptureAsync()
//
//  Stop capture asynchronously via MF Work Item
//
HRESULT WASAPICapture::StopCaptureAsync()
{
    if ( (m_DeviceStateChanged->GetState() != DeviceState::DeviceStateCapturing) &&
         (m_DeviceStateChanged->GetState() != DeviceState::DeviceStateInError) )
    {
        return E_NOT_VALID_STATE;
    }

    m_DeviceStateChanged->SetState( DeviceState::DeviceStateStopping, S_OK, true );

    return MFPutWorkItem2( MFASYNC_CALLBACK_QUEUE_MULTITHREADED, 0, &m_xStopCapture, nullptr );
}

//
//  OnStopCapture()
//
//  Callback method to stop capture
//
HRESULT WASAPICapture::OnStopCapture(IMFAsyncResult* pResult)
{
    // Stop capture by cancelling Work Item
    // Cancel the queued work item (if any)
    if (0 != m_SampleReadyKey)
    {
        MFCancelWorkItem( m_SampleReadyKey );
        m_SampleReadyKey = 0;
    }

    m_AudioClient->Stop();
    SAFE_RELEASE( m_SampleReadyAsyncResult );

    return S_OK;
}

//
//  FinishCaptureAsync()
//
//  Finalizes WAV file on a separate thread via MF Work Item
//
HRESULT WASAPICapture::FinishCaptureAsync()
{
    // We should be flushing when this is called
    if (m_DeviceStateChanged->GetState() == DeviceState::DeviceStateFlushing)
    {
        return MFPutWorkItem2( MFASYNC_CALLBACK_QUEUE_MULTITHREADED, 0, &m_xFinishCapture, nullptr ); 
    }

    // We are in the wrong state
    return E_NOT_VALID_STATE;
}

//
//  OnFinishCapture()
//
//  Because of the asynchronous nature of the MF Work Queues and the DataWriter, there could still be
//  a sample processing.  So this will get called to finalize the WAV header.
//
HRESULT WASAPICapture::OnFinishCapture(IMFAsyncResult* pResult)
{
    // FixWAVHeader will set the DeviceStateStopped when all async tasks are complete
	return S_OK;
}

//
//  OnSampleReady()
//
//  Callback method when ready to fill sample buffer
//
HRESULT WASAPICapture::OnSampleReady(IMFAsyncResult* pResult)
{
    HRESULT hr = S_OK;

    hr = OnAudioSampleRequested( false );

    if (SUCCEEDED( hr ))
    {
        // Re-queue work item for next sample
        if (m_DeviceStateChanged->GetState() ==  DeviceState::DeviceStateCapturing)
        {
            hr = MFPutWaitingWorkItem( m_SampleReadyEvent, 0, m_SampleReadyAsyncResult, &m_SampleReadyKey );
        }
    }
    else
    {
        m_DeviceStateChanged->SetState( DeviceState::DeviceStateInError, hr, true );
    }
    
    return hr;
}

//
//  OnAudioSampleRequested()
//
//  Called when audio device fires m_SampleReadyEvent
//
HRESULT WASAPICapture::OnAudioSampleRequested(Platform::Boolean IsSilence)
{
    HRESULT hr = S_OK;
    UINT32 FramesAvailable = 0;
    BYTE *Data = nullptr;
    DWORD dwCaptureFlags;
    UINT64 u64DevicePosition = 0;
    UINT64 u64QPCPosition = 0;
    DWORD cbBytesToCapture = 0;

    EnterCriticalSection( &m_CritSec );

	if (m_DeviceStateChanged->GetState() == DeviceState::DeviceStateInError)
	{
		m_streams->DeviceError(m_CaptureDeviceID);
		goto exit;
	}

    // If this flag is set, we have already queued up the async call to finialize the WAV header
    // So we don't want to grab or write any more data that would possibly give us an invalid size
    if ( (m_DeviceStateChanged->GetState() == DeviceState::DeviceStateStopping) ||
         (m_DeviceStateChanged->GetState() == DeviceState::DeviceStateFlushing) )
    {
        goto exit;
    }

    // A word on why we have a loop here;
    // Suppose it has been 10 milliseconds or so since the last time
    // this routine was invoked, and that we're capturing 48000 samples per second.
    //
    // The audio engine can be reasonably expected to have accumulated about that much
    // audio data - that is, about 480 samples.
    //
    // However, the audio engine is free to accumulate this in various ways:
    // a. as a single packet of 480 samples, OR
    // b. as a packet of 80 samples plus a packet of 400 samples, OR
    // c. as 48 packets of 10 samples each.
    //
    // In particular, there is no guarantee that this routine will be
    // run once for each packet.
    //
    // So every time this routine runs, we need to read ALL the packets
    // that are now available;
    //
    // We do this by calling IAudioCaptureClient::GetNextPacketSize
    // over and over again until it indicates there are no more packets remaining.
    for
    (
        hr = m_AudioCaptureClient->GetNextPacketSize(&FramesAvailable);
        SUCCEEDED(hr) && FramesAvailable > 0;
        hr = m_AudioCaptureClient->GetNextPacketSize(&FramesAvailable)
    )
    {
        cbBytesToCapture = FramesAvailable * m_MixFormat->nBlockAlign;
        
        // WAV files have a 4GB (0xFFFFFFFF) size limit, so likely we have hit that limit when we
        // overflow here.  Time to stop the capture
        if ( (m_cbDataSize + cbBytesToCapture) < m_cbDataSize )
        {
            StopCaptureAsync();
            goto exit;
        }

        // Get sample buffer
        hr = m_AudioCaptureClient->GetBuffer( &Data, &FramesAvailable, &dwCaptureFlags, &u64DevicePosition, &u64QPCPosition );
        if (FAILED( hr ))
        {
            goto exit;
        }
		/*
        if (dwCaptureFlags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)
        {
            // Pass down a discontinuity flag in case the app is interested and reset back to capturing
            m_DeviceStateChanged->SetState( DeviceState::DeviceStateDiscontinuity, S_OK, true );
            m_DeviceStateChanged->SetState( DeviceState::DeviceStateCapturing, S_OK, false );
        }
		*/
		if (dwCaptureFlags & AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR)
		{
			String^ str = String::Concat(m_DeviceIdString, "-TIMESTAMP_ERROR\n");
			OutputDebugString(str->Data());
		}
		
        // Zero out sample if silence
        if ( (dwCaptureFlags & AUDCLNT_BUFFERFLAGS_SILENT) || IsSilence )
        {
            memset( Data, 0, FramesAvailable * m_MixFormat->nBlockAlign );
        }

		// HANDLE AUDIO DATA
		m_streams->AddData(m_CaptureDeviceID, Data, cbBytesToCapture, u64QPCPosition, dwCaptureFlags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY, dwCaptureFlags & AUDCLNT_BUFFERFLAGS_SILENT);

        // Release buffer back
        m_AudioCaptureClient->ReleaseBuffer( FramesAvailable );

        // Increase the size of our 'data' chunk and flush counter.  m_cbDataSize needs to be accurate
        // Its OK if m_cbFlushCounter is an approximation
        m_cbDataSize += cbBytesToCapture;
		m_counter += cbBytesToCapture;
    }

exit:
    LeaveCriticalSection( &m_CritSec );

    return hr;
}
