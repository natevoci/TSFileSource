/**
*  SMSourcePin.ccp
*  Copyright (C) 2004-2006 bear
*
*  This file is part of SMSource, a directshow Shared Memory push source filter that
*  provides an MPEG transport stream output.
*
*  SMSource is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.
*
*  TSFileSource is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with SMSource; if not, write to the Free Software
*  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*  bear can be reached on the forums at
*    http://forums.dvbowners.com/
*/




#include "SMSourcePin.h"
#include <atlbase.h>




CSMStreamPin::CSMStreamPin(LPUNKNOWN pUnk,
						   CSMSourceFilter *pFilter,
//						   CCritSec *pLock,
//						   CCritSec *pReceiveLock,
						   HRESULT *phr) :
        CSourceStream(NAME("MPEG2 Source Output"), phr, pFilter, L"Out"),
 		CSourceSeeking(NAME("MPEG2 Source Output"), pUnk, phr, &m_pReceiveLock),
       m_pSMSourceFilter(pFilter),
		m_biMpegDemux(FALSE)//,
//	    m_pReceiveLock(pReceiveLock)
			
{
	   // m_rtDuration is defined as the length of the source clip.
       // we default to the maximum amount of time.
       //
       //m_rtDuration = DEFAULT_DURATION;
       //m_rtStop = m_rtDuration;

        // no seeking to absolute pos's and no seeking backwards!
        //
		// m_rtDuration = m_pSMSourceFilter->m_pFileReader->GetFileSize (m_pSMSource->m_hFile, NULL) / 188;

        m_dwSeekingCaps = AM_SEEKING_Source;
//          AM_SEEKING_CanSeekForwards  |
//		  AM_SEEKING_CanGetStopPos    |
//          AM_SEEKING_CanGetDuration   |
//          AM_SEEKING_CanSeekAbsolute;

		m_bSeeking = FALSE;
}


CSMStreamPin::~CSMStreamPin()
{
}

STDMETHODIMP CSMStreamPin::NonDelegatingQueryInterface( REFIID riid, void ** ppv )
{
    CAutoLock cAutoLock(m_pFilter->pStateLock());
	if (riid == IID_ISMSource)
	{
		return GetInterface((ISMSource*)m_pSMSourceFilter, ppv);
	}
	if (riid == IID_IFileSourceFilter)
	{
		return GetInterface((IFileSourceFilter*)m_pSMSourceFilter, ppv);
	}
	if (riid == IID_ISpecifyPropertyPages)
	{
		return GetInterface((ISpecifyPropertyPages*)m_pSMSourceFilter, ppv);
	}
//    if( riid == IID_IMediaSeeking ) 
//	{
//		return CSourceSeeking::NonDelegatingQueryInterface( riid, ppv );
//    }
    return CSourceStream::NonDelegatingQueryInterface(riid, ppv);
}

// GetMediaType: This method tells the downstream pin what types we support.

// Here is how CSourceStream deals with media types:
//
// If you support exactly one type, override GetMediaType(CMediaType*). It will then be
// called when (a) our filter proposes a media type, (b) the other filter proposes a
// type and we have to check that type.
//
// If you support > 1 type, override GetMediaType(int,CMediaType*) AND CheckMediaType.
//
// In this case we support only one type, which we obtain from the bitmap file.

/*
HRESULT CSMStreamPin::GetMediaType(CMediaType *pmt)
{
	CAutoLock cAutoLock(m_pFilter->pStateLock());

    CheckPointer(pmt, E_POINTER);

     pmt -> InitMediaType () ;
     pmt -> SetType      (& MEDIATYPE_Stream) ;
     pmt -> SetSubtype   (& MEDIASUBTYPE_MPEG2_TRANSPORT) ;

    return S_OK;
}
*/
HRESULT CSMStreamPin::CheckConnect(IPin *pReceivePin)
{
	if(!pReceivePin)
		return E_INVALIDARG;

	m_biMpegDemux = FALSE;
    CAutoLock cAutoLock(m_pFilter->pStateLock());
	HRESULT hr = CBaseOutputPin::CheckConnect(pReceivePin);
	if (SUCCEEDED(hr))
	{
		PIN_INFO pInfo;
		if (SUCCEEDED(pReceivePin->QueryPinInfo(&pInfo)))
		{
			// Get an instance of the Demux control interface
			CComPtr<IMpeg2Demultiplexer> muxInterface;
			if(SUCCEEDED(pInfo.pFilter->QueryInterface (&muxInterface)))
			{
				// Create out new pin to make sure the interface is real
				CComPtr<IPin>pIPin = NULL;
				AM_MEDIA_TYPE pintype;
				ZeroMemory(&pintype, sizeof(AM_MEDIA_TYPE));
				hr = muxInterface->CreateOutputPin(&pintype, L"MS" ,&pIPin);
				if (SUCCEEDED(hr) && pIPin != NULL)
				{
					pInfo.pFilter->Release();
					hr = muxInterface->DeleteOutputPin(L"MS");
					if SUCCEEDED(hr)
						m_biMpegDemux = TRUE;
//					else if (!m_pSMSourceFilter->get_AutoMode())
//						hr = S_OK;

					return hr;
				}
			}

			//Test for a filter with "MPEG-2" on input pin label
			if (wcsstr(pInfo.achName, L"MPEG-2") != NULL)
			{
				pInfo.pFilter->Release();
				m_biMpegDemux = TRUE;
				return S_OK;
			}

			FILTER_INFO pFilterInfo;
			if (SUCCEEDED(pInfo.pFilter->QueryFilterInfo(&pFilterInfo)))
			{
				pInfo.pFilter->Release();
				pFilterInfo.pGraph->Release();

				//Test for an infinite tee filter
				if (wcsstr(pFilterInfo.achName, L"Tee") != NULL || wcsstr(pFilterInfo.achName, L"Flow") != NULL)
				{
					m_biMpegDemux = TRUE;
					return S_OK;
				}
				hr = E_FAIL;
			}
			else
				pInfo.pFilter->Release();

		}
//		if(!m_pTSFileSourceFilter->get_AutoMode())
//			return S_OK;
//		else
			return E_FAIL;
	}
	return hr;
}

STDMETHODIMP CSMStreamPin::IsFormatSupported(const GUID * pFormat)
{
    CheckPointer(pFormat, E_POINTER);
	CAutoLock cAutoLock(m_pFilter->pStateLock());

	if (*pFormat == TIME_FORMAT_MEDIA_TIME)
		return S_OK;
	else
		return E_FAIL;
}

STDMETHODIMP CSMStreamPin::QueryPreferredFormat(GUID *pFormat)
{
    CheckPointer(pFormat, E_POINTER);
	CAutoLock cAutoLock(m_pFilter->pStateLock());
	*pFormat = TIME_FORMAT_MEDIA_TIME;
	return S_OK;
}

HRESULT CSMStreamPin::GetDuration(LONGLONG *pDuration)
{
    CheckPointer(pDuration, E_POINTER);
//	CAutoLock cAutoLock(m_pFilter->pStateLock());
    CAutoLock lock(&m_pReceiveLock);
    *pDuration = m_rtDuration;
    return S_OK;
}

HRESULT CSMStreamPin::GetStopPosition(LONGLONG *pStop)
{
    CheckPointer(pStop, E_POINTER);
//	CAutoLock cAutoLock(m_pFilter->pStateLock());
    CAutoLock lock(&m_pReceiveLock);
    *pStop = m_rtStop;
    return S_OK;
}

HRESULT CSMStreamPin::GetCurrentPosition(LONGLONG *pCurrent)
{
    // GetCurrentPosition is typically supported only in renderers and
    // not in source filters.
    return E_NOTIMPL;
}

HRESULT CSMStreamPin::GetCapabilities( DWORD * pCapabilities )
{
    CheckPointer(pCapabilities, E_POINTER);
    *pCapabilities = m_dwSeekingCaps;
    return S_OK;
}

HRESULT CSMStreamPin::CheckCapabilities( DWORD * pCapabilities )
{
    CheckPointer(pCapabilities, E_POINTER);

    // make sure all requested capabilities are in our mask
    return (~m_dwSeekingCaps & *pCapabilities) ? S_FALSE : S_OK;
}

HRESULT CSMStreamPin::ConvertTimeFormat( LONGLONG * pTarget, const GUID * pTargetFormat,
                           LONGLONG    Source, const GUID * pSourceFormat )
{
    CheckPointer(pTarget, E_POINTER);
    // format guids can be null to indicate current format

    // since we only support TIME_FORMAT_MEDIA_TIME, we don't really
    // offer any conversions.
    if(pTargetFormat == 0 || *pTargetFormat == TIME_FORMAT_MEDIA_TIME)
    {
        if(pSourceFormat == 0 || *pSourceFormat == TIME_FORMAT_MEDIA_TIME)
        {
            *pTarget = Source;
            return S_OK;
        }
    }

    return E_INVALIDARG;
}


HRESULT CSMStreamPin::SetPositions( LONGLONG * pCurrent,  DWORD CurrentFlags
                      , LONGLONG * pStop,  DWORD StopFlags )
{
   DWORD StopPosBits = StopFlags & AM_SEEKING_PositioningBitsMask;
    DWORD StartPosBits = CurrentFlags & AM_SEEKING_PositioningBitsMask;

    if(StopFlags) {
        CheckPointer(pStop, E_POINTER);

        // accept only relative, incremental, or absolute positioning
        if(StopPosBits != StopFlags) {
            return E_INVALIDARG;
        }
    }

    if(CurrentFlags) {
        CheckPointer(pCurrent, E_POINTER);
        if(StartPosBits != AM_SEEKING_AbsolutePositioning &&
           StartPosBits != AM_SEEKING_RelativePositioning) {
            return E_INVALIDARG;
        }
    }


    // scope for autolock
    {
//		CAutoLock cAutoLock(m_pFilter->pStateLock());
    CAutoLock lock(&m_pReceiveLock);

        // set start position
        if(StartPosBits == AM_SEEKING_AbsolutePositioning)
        {
            m_rtStart = *pCurrent;
        }
        else if(StartPosBits == AM_SEEKING_RelativePositioning)
        {
            m_rtStart += *pCurrent;
        }

        // set stop position
        if(StopPosBits == AM_SEEKING_AbsolutePositioning)
        {
            m_rtStop = *pStop;
        }
        else if(StopPosBits == AM_SEEKING_IncrementalPositioning)
        {
            m_rtStop = m_rtStart + *pStop;
        }
        else if(StopPosBits == AM_SEEKING_RelativePositioning)
        {
            m_rtStop = m_rtStop + *pStop;
        }
    }


    HRESULT hr = S_OK;
    if(SUCCEEDED(hr) && StopPosBits) {
        hr = ChangeStop();
    }
    if(StartPosBits) {
        hr = ChangeStart();
    }

    return hr;
}


HRESULT CSMStreamPin::GetPositions( LONGLONG * pCurrent, LONGLONG * pStop )
{
    if(pCurrent) {
        *pCurrent = m_rtStart;
    }
    if(pStop) {
        *pStop = m_rtStop;
    }

    return S_OK;;
}


HRESULT CSMStreamPin::GetAvailable( LONGLONG * pEarliest, LONGLONG * pLatest )
{
    if(pEarliest) {
        *pEarliest = 0;
    }
    if(pLatest) {
		CAutoLock cAutoLock(m_pFilter->pStateLock());
        *pLatest = m_rtDuration;
    }
    return S_OK;
}

HRESULT CSMStreamPin::SetRate( double dRate)
{
    {
//		CAutoLock cAutoLock(m_pFilter->pStateLock());
    CAutoLock lock(&m_pReceiveLock);
        m_dRateSeeking = dRate;
    }
    return ChangeRate();
}

HRESULT CSMStreamPin::GetRate( double * pdRate)
{
    CheckPointer(pdRate, E_POINTER);
//	CAutoLock cAutoLock(m_pFilter->pStateLock());
    CAutoLock lock(&m_pReceiveLock);
    *pdRate = m_dRateSeeking;
    return S_OK;
}

HRESULT CSMStreamPin::GetPreroll(LONGLONG *pPreroll)
{
    CheckPointer(pPreroll, E_POINTER);
    *pPreroll = 0;
    return S_OK;
}



HRESULT CSMStreamPin::GetMediaType(CMediaType *pmt)
{
    CheckPointer(pmt, E_POINTER);
	CAutoLock cAutoLock(m_pFilter->pStateLock());

	pmt->InitMediaType();
	pmt->SetType      (& MEDIATYPE_Stream);
	pmt->SetSubtype   (& MEDIASUBTYPE_MPEG2_TRANSPORT);

	//Set for Program mode
//	if (m_pTSFileSourceFilter->m_pPidParser->get_ProgPinMode())
		pmt->SetSubtype   (& MEDIASUBTYPE_MPEG2_PROGRAM);
//	else if (!m_pTSFileSourceFilter->m_pPidParser->pids.pcr)
		pmt->SetSubtype   (& MEDIASUBTYPE_MPEG2_TRANSPORT);
//		pmt->SetSubtype   (& MEDIASUBTYPE_MPEG2_PROGRAM);

    return S_OK;
}

HRESULT CSMStreamPin::GetMediaType(int iPosition, CMediaType *pMediaType)
{
	CAutoLock cAutoLock(m_pFilter->pStateLock());
    
	if(iPosition < 0)
    {
        return E_INVALIDARG;
    }
    if(iPosition > 0)
    {
        return VFW_S_NO_MORE_ITEMS;
    }

    CheckPointer(pMediaType,E_POINTER);
	
	pMediaType->InitMediaType();
	pMediaType->SetType      (& MEDIATYPE_Stream);
	pMediaType->SetSubtype   (& MEDIASUBTYPE_MPEG2_TRANSPORT);

	//Set for Program mode
//	if (m_pTSFileSourceFilter->m_pPidParser->get_ProgPinMode())
//		pMediaType->SetSubtype   (& MEDIASUBTYPE_MPEG2_PROGRAM);
//	else if (!m_pTSFileSourceFilter->m_pPidParser->pids.pcr)
		pMediaType->SetSubtype   (& MEDIASUBTYPE_MPEG2_TRANSPORT);
//		pMediaType->SetSubtype   (& MEDIASUBTYPE_MPEG2_PROGRAM);
	
    return S_OK;
}


HRESULT CSMStreamPin::CheckMediaType(const CMediaType* pType)
{
    CheckPointer(pType, E_POINTER);
	CAutoLock cAutoLock(m_pFilter->pStateLock());

//	m_pTSFileSourceFilter->m_pDemux->AOnConnect();
	if(MEDIATYPE_Stream == pType->majortype)
	{
		//Are we in Transport mode
		if (MEDIASUBTYPE_MPEG2_TRANSPORT == pType->subtype)// && !m_pTSFileSourceFilter->m_pPidParser->get_ProgPinMode())
			return S_OK;

		//Are we in Program mode
		else if (MEDIASUBTYPE_MPEG2_PROGRAM == pType->subtype)// && m_pTSFileSourceFilter->m_pPidParser->get_ProgPinMode())
			return S_OK;

		else if (MEDIASUBTYPE_MPEG2_PROGRAM == pType->subtype)// && !m_pTSFileSourceFilter->m_pPidParser->pids.pcr)
			return S_OK;
	}

   return S_FALSE;
}



HRESULT CSMStreamPin::CompleteConnect(IPin *pReceivePin)
{
    CAutoLock cAutoLock(m_pFilter->pStateLock());

	HRESULT hr = CBaseOutputPin::CompleteConnect(pReceivePin);
	if (SUCCEEDED(hr))
	{
//		m_pTSFileSourceFilter->OnConnect();
	}
	return hr;
}

HRESULT CSMStreamPin::BreakConnect()
{
    CAutoLock cAutoLock(m_pFilter->pStateLock());

	HRESULT hr = CBaseOutputPin::BreakConnect();
	if (FAILED(hr))
		return hr;

	m_pSMSourceFilter->m_pFileReader->CloseFile();

//	m_bSeeking = FALSE;
	return hr;
}

HRESULT CSMStreamPin::DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pRequest)
{
    HRESULT hr;
    CAutoLock cAutoLock(m_pFilter->pStateLock());

    CheckPointer(pAlloc, E_POINTER);
    CheckPointer(pRequest, E_POINTER);

    // If the bitmap file was not loaded, just fail here.
    
    // Ensure a minimum number of buffers
    if (pRequest->cBuffers == 0)
    {
        pRequest->cBuffers = 2;
    }
    pRequest->cbBuffer = 65536/4; //188*16;

    ALLOCATOR_PROPERTIES Actual;
    hr = pAlloc->SetProperties(pRequest, &Actual);
    if (FAILED(hr)) 
    {
        return hr;
    }

    // Is this allocator unsuitable?
    if (Actual.cbBuffer < pRequest->cbBuffer) 
    {
        return E_FAIL;
    }

    return S_OK;
}


// This is where we insert the bits into the video stream.
// FillBuffer is called once for every sample in the stream.

HRESULT CSMStreamPin::FillBuffer(IMediaSample *pSample)
{
	if (m_bSeeking)
	{
		Sleep(1);
		return NOERROR;
	}

    PBYTE pData;
    CheckPointer(pSample, E_POINTER);

    if (m_pSMSourceFilter->m_pFileReader->IsFileInvalid()) {
		Sleep(1);
        return NOERROR;
	}

    CAutoLock lock(&m_FillLock);

    // Access the sample's data buffer
    HRESULT hr = pSample->GetPointer(&pData);
	if (FAILED(hr)) {
		Sleep(1);
		return hr;
	}

	int m_loopCount = 20;
	ULONG ulBytesRead = 0;

	__int64 llStartPosition, llfilelength = 0;
	if (FAILED(m_pSMSourceFilter->m_pFileReader->GetFileSize(&llStartPosition, &llfilelength)))
	{ 
		m_loopCount = 20;
		return E_FAIL;
    }

	__int64 currPosition = m_pSMSourceFilter->m_pFileReader->getFilePointer();
	DWORD dwErr = GetLastError();
	if ((DWORD)currPosition == (DWORD)0xFFFFFFFF && dwErr)
	{
		m_loopCount = 20;
		return E_FAIL;
	}
//	currPosition += llStartPosition;

	hr = m_pSMSourceFilter->m_pFileReader->Read(pData, pSample->GetActualDataLength(), &ulBytesRead);
	if (FAILED(hr)){

		m_loopCount = 20;
		Sleep(1);
		return hr;
	}
/*
	while (ulBytesRead < (ULONG)pSample->GetActualDataLength()) 
	{
				m_pSMSourceFilter->m_pFileReader->SetFilePointer(currPosition, FILE_BEGIN);
				Sleep(100);
				ulBytesRead = 0;				
				currPosition = m_pSMSourceFilter->m_pFileReader->GetFilePointer();
				HRESULT hr = m_pSMSourceFilter->m_pFileReader->Read(pData, (ULONG)pSample->GetActualDataLength(), &ulBytesRead);
				if (FAILED(hr)){

					Sleep(1);
					return E_FAIL;
				}
	}
	return S_OK;

*/








	if (ulBytesRead < (ULONG)pSample->GetActualDataLength()) 
	{

		WORD wReadOnly = 0;
		m_pSMSourceFilter->m_pFileReader->get_ReadOnly(&wReadOnly);
		if (wReadOnly)
		{

			::OutputDebugString(TEXT("TSBuffer::Require() Waiting for file to grow. \n"));
			m_loopCount = min(20, m_loopCount);
			while (ulBytesRead < (ULONG)pSample->GetActualDataLength() && m_loopCount) 
			{
				m_loopCount = max(2, m_loopCount);
				m_loopCount--;

				WORD bDelay = 0;
//				m_pSMSourceFilter->m_pFileReader->get_DelayMode(&bDelay);
				if (bDelay > 0)
				{
					Sleep(2000);
					m_loopCount = 0;
				}
				else
				{
					if (!m_loopCount)
					{
						m_loopCount = 20;
						return E_FAIL;
					}
//					m_pSMSourceFilter->m_pFileReader->SetFilePointer(0, FILE_END);
					Sleep(100);
				}

				__int64 llStartPosition2 = 0;
				if (FAILED(m_pSMSourceFilter->m_pFileReader->GetFileSize(&llStartPosition2, &llfilelength)))
				{ 
					m_loopCount = 20;
					return E_FAIL;
				}

				llStartPosition2 = llStartPosition - llStartPosition2;

				hr = m_pSMSourceFilter->m_pFileReader->SetFilePointer(currPosition + llStartPosition2, FILE_BEGIN);
				dwErr = GetLastError();
				if ((DWORD)hr == (DWORD)0xFFFFFFFF && dwErr)
				{
					m_loopCount = 20;
					return E_FAIL;
				}

				ULONG ulNextBytesRead = 0;		
				hr = m_pSMSourceFilter->m_pFileReader->Read(pData, (ULONG)pSample->GetActualDataLength(), &ulNextBytesRead);
				if (FAILED(hr) && !m_loopCount){

					m_loopCount = 20;
					return E_FAIL;
				}

				if (((ulNextBytesRead == 0) | (ulNextBytesRead == ulBytesRead)) && !m_loopCount){

					m_loopCount = 20;
					return E_FAIL;
				}

				ulBytesRead = ulNextBytesRead;
			}
		}
		else
		{
			return E_FAIL;
		}
	}
	m_loopCount = 20;
	return S_OK;
}




HRESULT CSMStreamPin::OnThreadStartPlay( )
{
 	CAutoLock fillLock(&m_FillLock);
 	CAutoLock lock(&m_pReceiveLock);
  DeliverNewSegment(m_rtStart, m_rtStop, 1.0 );
    return CSourceStream::OnThreadStartPlay( );
}

HRESULT CSMStreamPin::Run(REFERENCE_TIME tStart)
{
	CAutoLock fillLock(&m_FillLock);
	CAutoLock lock(&m_pReceiveLock);

//	CBasePin::m_tStart = tStart;
	return CBaseOutputPin::Run(tStart);
}

void CSMStreamPin::UpdateFromSeek()
{
	if (ThreadExists())
	{
		DeliverBeginFlush();
		CSourceStream::Stop();
		DeliverEndFlush();
		CSourceStream::Run();
	}
}


HRESULT CSMStreamPin::SetDuration(REFERENCE_TIME duration)
{  
	m_rtDuration = duration;
	m_rtStop = m_rtDuration;

    return S_OK;

}

REFERENCE_TIME CSMStreamPin::GetStartTime()
{
	return m_rtStart;
}


HRESULT CSMStreamPin::SetStartTime(REFERENCE_TIME start)
{
	m_rtStart = start;
	return S_OK;
}


HRESULT CSMStreamPin::ChangeStart( )
{
	CAutoLock cAutoLockSeeking(CSourceSeeking::m_pLock);

	if (ThreadExists())
	{
		m_bSeeking = TRUE;
		DeliverBeginFlush();
		CSourceStream::Stop();
		m_pSMSourceFilter->FileSeek(m_rtStart);
		DeliverEndFlush();
		m_bSeeking = FALSE;
		CSourceStream::Run();
	}
    return S_OK;
}

HRESULT CSMStreamPin::ChangeStop( )
{
	m_bSeeking = TRUE;
    UpdateFromSeek();
	m_bSeeking = FALSE;
    return S_OK;
}


HRESULT CSMStreamPin::ChangeRate( )
{
    {   // Scope for critical section lock.
        CAutoLock cAutoLockSeeking(CSourceSeeking::m_pLock);
        if( m_dRateSeeking <= 0 ) {
            m_dRateSeeking = 1.0;  // Reset to a reasonable value.
            return E_FAIL;
        }
    }
    UpdateFromSeek();
    return S_OK;
}