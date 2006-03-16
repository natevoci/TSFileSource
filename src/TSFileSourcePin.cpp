/**
*  TSFileSourcePin.cpp
*  Copyright (C) 2003      bisswanger
*  Copyright (C) 2004-2005 bear
*  Copyright (C) 2005      nate
*
*  This file is part of TSFileSource, a directshow push source filter that
*  provides an MPEG transport stream output.
*
*  TSFileSource is free software; you can redistribute it and/or modify
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
*  along with TSFileSource; if not, write to the Free Software
*  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*  bisswanger can be reached at WinSTB@hotmail.com
*    Homepage: http://www.winstb.de
*
*  bear and nate can be reached on the forums at
*    http://forums.dvbowners.com/
*/

#include <streams.h>
#include "TSFileSource.h"
#include "TSFileSourceGuids.h"
#include "DvbFormats.h"
#include <math.h>

//#define USE_EVENT
#ifndef USE_EVENT
#include "Mmsystem.h"
#endif

CTSFileSourcePin::CTSFileSourcePin(LPUNKNOWN pUnk, CTSFileSourceFilter *pFilter, HRESULT *phr) :
	CSourceStream(NAME("MPEG2 Source Output"), phr, pFilter, L"Out"),
	CSourceSeeking(NAME("MPEG2 Source Output"), pUnk, phr, &m_SeekLock),
	m_pTSFileSourceFilter(pFilter),
	m_bRateControl(FALSE)
{
	m_dwSeekingCaps =
		
    AM_SEEKING_CanSeekAbsolute	|
	AM_SEEKING_CanSeekForwards	|
	AM_SEEKING_CanSeekBackwards	|
	AM_SEEKING_CanGetCurrentPos	|
	AM_SEEKING_CanGetStopPos	|
	AM_SEEKING_CanGetDuration	|
//	AM_SEEKING_CanPlayBackwards	|
//	AM_SEEKING_CanDoSegments	|
	AM_SEEKING_Source;
/*	
						AM_SEEKING_CanSeekForwards  |
						AM_SEEKING_CanGetStopPos    |
						AM_SEEKING_CanGetDuration   |
						AM_SEEKING_CanSeekAbsolute;
*/
	m_bSeeking = FALSE;

	m_rtLastSeekStart = 0;

	m_llBasePCR = -1;
	m_llNextPCR = -1;
	m_llPrevPCR = -1;
	m_lNextPCRByteOffset = 0;
	m_lPrevPCRByteOffset = 0;

	m_lTSPacketDeliverySize = m_pTSFileSourceFilter->m_pPidParser->get_PacketSize()*1000;

	m_DataRate = 10000000;
	m_DataRateTotal = 0;
	m_BitRateCycle = 0;
	for (int i = 0; i < 256; i++) { 
		m_BitRateStore[i] = 0;
	}

	m_pTSBuffer = new CTSBuffer(m_pTSFileSourceFilter->m_pClock);
	m_pPids = new PidInfo();

	debugcount = 0;

	m_rtLastCurrentTime = (REFERENCE_TIME)((REFERENCE_TIME)timeGetTime() * (REFERENCE_TIME)10000);
	m_LastFileSize = 0;
	m_LastStartSize = 0;
	m_DemuxLock = FALSE;
	m_IntLastStreamTime = 0;
	m_rtTimeShiftPosition = 0;
	m_LastMultiFileStart = 0;
	m_LastMultiFileEnd = 0;
	m_bGetAvailableMode = FALSE;
	m_IntBaseTimePCR = 0;
	m_IntStartTimePCR = 0;
	m_IntCurrentTimePCR = 0;
	m_IntEndTimePCR = 0;
	m_biMpegDemux = 0;
}

CTSFileSourcePin::~CTSFileSourcePin()
{
	delete m_pPids;
	delete m_pTSBuffer;
}

STDMETHODIMP CTSFileSourcePin::NonDelegatingQueryInterface( REFIID riid, void ** ppv )
{
	CAutoLock cAutoLock(m_pFilter->pStateLock());

	if (riid == IID_ITSFileSource)
	{
		return GetInterface((ITSFileSource*)m_pTSFileSourceFilter, ppv);
	}
	if (riid == IID_IFileSourceFilter)
	{
		return GetInterface((IFileSourceFilter*)m_pTSFileSourceFilter, ppv);
	}
    if (riid == IID_IAMFilterMiscFlags)
    {
		return GetInterface((IAMFilterMiscFlags*)m_pTSFileSourceFilter, ppv);
    }
	if (riid == IID_IAMStreamSelect && m_pTSFileSourceFilter->get_AutoMode())
	{
		 GetInterface((IAMStreamSelect*)m_pTSFileSourceFilter, ppv);
	}
	if (riid == IID_IAsyncReader)
	{
		if ((!m_pTSFileSourceFilter->m_pPidParser->pids.pcr
			&& !m_pTSFileSourceFilter->get_AutoMode()
			&& m_pTSFileSourceFilter->m_pPidParser->get_ProgPinMode())
			&& m_pTSFileSourceFilter->m_pPidParser->get_AsyncMode())
		{
			return GetInterface((IAsyncReader*)m_pTSFileSourceFilter, ppv);
		}
	}
	if (riid == IID_IMediaSeeking)
    {
        return CSourceSeeking::NonDelegatingQueryInterface( riid, ppv );
    }
	return CSourceStream::NonDelegatingQueryInterface(riid, ppv);
}

STDMETHODIMP CTSFileSourcePin::IsFormatSupported(const GUID * pFormat)
{
    CheckPointer(pFormat, E_POINTER);
	CAutoLock cAutoLock(m_pFilter->pStateLock());

	if (*pFormat == TIME_FORMAT_MEDIA_TIME)
		return S_OK;
	else
		return E_FAIL;
}

STDMETHODIMP CTSFileSourcePin::QueryPreferredFormat(GUID *pFormat)
{
    CheckPointer(pFormat, E_POINTER);
	CAutoLock cAutoLock(m_pFilter->pStateLock());
	*pFormat = TIME_FORMAT_MEDIA_TIME;
	return S_OK;
}

HRESULT CTSFileSourcePin::GetMediaType(CMediaType *pmt)
{
    CheckPointer(pmt, E_POINTER);
	CAutoLock cAutoLock(m_pFilter->pStateLock());

	pmt->InitMediaType();
	pmt->SetType      (& MEDIATYPE_Stream);
	pmt->SetSubtype   (& MEDIASUBTYPE_MPEG2_TRANSPORT);

	//Set for Program mode
	if (m_pTSFileSourceFilter->m_pPidParser->get_ProgPinMode())
		pmt->SetSubtype   (& MEDIASUBTYPE_MPEG2_PROGRAM);
	else if (!m_pTSFileSourceFilter->m_pPidParser->pids.pcr)
		pmt->SetSubtype   (& MEDIASUBTYPE_MPEG2_TRANSPORT);
//		pmt->SetSubtype   (& MEDIASUBTYPE_MPEG2_PROGRAM);

    return S_OK;
}

HRESULT CTSFileSourcePin::GetMediaType(int iPosition, CMediaType *pMediaType)
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
	if (m_pTSFileSourceFilter->m_pPidParser->get_ProgPinMode())
		pMediaType->SetSubtype   (& MEDIASUBTYPE_MPEG2_PROGRAM);
	else if (!m_pTSFileSourceFilter->m_pPidParser->pids.pcr)
		pMediaType->SetSubtype   (& MEDIASUBTYPE_MPEG2_TRANSPORT);
//		pMediaType->SetSubtype   (& MEDIASUBTYPE_MPEG2_PROGRAM);
	
    return S_OK;
}


HRESULT CTSFileSourcePin::CheckMediaType(const CMediaType* pType)
{
    CheckPointer(pType, E_POINTER);
	CAutoLock cAutoLock(m_pFilter->pStateLock());

	m_pTSFileSourceFilter->m_pDemux->AOnConnect();
	if(MEDIATYPE_Stream == pType->majortype)
	{
		//Are we in Transport mode
		if (MEDIASUBTYPE_MPEG2_TRANSPORT == pType->subtype && !m_pTSFileSourceFilter->m_pPidParser->get_ProgPinMode())
			return S_OK;

		//Are we in Program mode
		else if (MEDIASUBTYPE_MPEG2_PROGRAM == pType->subtype && m_pTSFileSourceFilter->m_pPidParser->get_ProgPinMode())
			return S_OK;

		else if (MEDIASUBTYPE_MPEG2_PROGRAM == pType->subtype && !m_pTSFileSourceFilter->m_pPidParser->pids.pcr)
			return S_OK;
	}

    return S_FALSE;
}

HRESULT CTSFileSourcePin::DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pRequest)
{
    CheckPointer(pAlloc, E_POINTER);
    CheckPointer(pRequest, E_POINTER);

    CAutoLock cAutoLock(m_pFilter->pStateLock());

    HRESULT hr;

    // Ensure a minimum number of buffers
    if (pRequest->cBuffers == 0)
    {
        pRequest->cBuffers = 2;
    }
    //pRequest->cbBuffer = 188*16;
	pRequest->cbBuffer = m_lTSPacketDeliverySize;

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

HRESULT CTSFileSourcePin::CheckConnect(IPin *pReceivePin)
{
	if(!pReceivePin)
		return E_INVALIDARG;

    CAutoLock cAutoLock(m_pFilter->pStateLock());
	HRESULT hr = CBaseOutputPin::CheckConnect(pReceivePin);
	if (SUCCEEDED(hr) && m_pTSFileSourceFilter->get_AutoMode())
	{
		PIN_INFO pInfo;
		if (SUCCEEDED(pReceivePin->QueryPinInfo(&pInfo)))
		{
			// Get an instance of the Demux control interface
			CComPtr<IMpeg2Demultiplexer> muxInterface;
			if(SUCCEEDED(pInfo.pFilter->QueryInterface (&muxInterface)))
			{
				pInfo.pFilter->Release();
				m_biMpegDemux = TRUE;
				return hr;
			}

			//Test for a filter with "MPEG-2" on input pin label
			if (_wcsicmp(pInfo.achName, L"MPEG-2") != 0)
			{
				pInfo.pFilter->Release();
				m_biMpegDemux = TRUE;
				return hr;
			}

			pInfo.pFilter->Release();

			FILTER_INFO pFilterInfo;
			if (SUCCEEDED(pInfo.pFilter->QueryFilterInfo(&pFilterInfo)))
			{
				pFilterInfo.pGraph->Release();

				//Test for an infinite tee filter
				if (_wcsicmp(pFilterInfo.achName, L"Tee") != 0 || _wcsicmp(pFilterInfo.achName, L"Flow") != 0)
				{
					m_biMpegDemux = TRUE;
					return hr;
				}
			}
		}
		m_biMpegDemux = FALSE;
		return E_FAIL;
	}
	return hr;
}

HRESULT CTSFileSourcePin::CompleteConnect(IPin *pReceivePin)
{
    CAutoLock cAutoLock(m_pFilter->pStateLock());

	HRESULT hr = CBaseOutputPin::CompleteConnect(pReceivePin);
	if (SUCCEEDED(hr))
	{
		m_pTSFileSourceFilter->OnConnect();
		m_rtDuration = m_pTSFileSourceFilter->m_pPidParser->pids.dur;
		m_rtStop = m_rtDuration;
		m_DataRate = m_pTSFileSourceFilter->m_pPidParser->pids.bitrate;
		m_IntBaseTimePCR = m_pTSFileSourceFilter->m_pPidParser->pids.start;
		m_IntStartTimePCR = m_pTSFileSourceFilter->m_pPidParser->pids.start;
		m_IntCurrentTimePCR = m_pTSFileSourceFilter->m_pPidParser->pids.start;
		m_IntEndTimePCR = m_pTSFileSourceFilter->m_pPidParser->pids.end;
	
		//Test if parser Locked
		if (!m_pTSFileSourceFilter->m_pPidParser->m_ParsingLock){
			//fix our pid values for this run
			m_pTSFileSourceFilter->m_pPidParser->pids.CopyTo(m_pPids); 
			m_bASyncModeSave = m_pTSFileSourceFilter->m_pPidParser->get_AsyncMode();
			m_PacketSave = m_pTSFileSourceFilter->m_pPidParser->get_PacketSize();
			m_TSIDSave = m_pTSFileSourceFilter->m_pPidParser->m_TStreamID;
			m_PinTypeSave  = m_pTSFileSourceFilter->m_pPidParser->get_ProgPinMode();
		}
	}
	return hr;
}

HRESULT CTSFileSourcePin::BreakConnect()
{
    CAutoLock cAutoLock(m_pFilter->pStateLock());

	HRESULT hr = CBaseOutputPin::BreakConnect();
	if (FAILED(hr))
		return hr;

	DisconnectDemux();

	m_pTSFileSourceFilter->m_pFileReader->CloseFile();
	m_pTSFileSourceFilter->m_pFileDuration->CloseFile();

	m_pTSBuffer->Clear();
	m_bSeeking = FALSE;
	m_rtLastSeekStart = 0;
	m_llBasePCR = -1;
	m_llNextPCR = -1;
	m_llPrevPCR = -1;
	m_lNextPCRByteOffset = 0;
	m_lPrevPCRByteOffset = 0;
	m_biMpegDemux = FALSE;

	m_DataRate = m_pTSFileSourceFilter->m_pPidParser->pids.bitrate;
	if (!m_DataRate)
		m_DataRate = 10000000;

	m_DataRateTotal = 0;
	m_BitRateCycle = 0;
	for (int i = 0; i < 256; i++) { 
		m_BitRateStore[i] = 0;
	}
	m_rtLastCurrentTime = (REFERENCE_TIME)((REFERENCE_TIME)timeGetTime() * (REFERENCE_TIME)10000);
	m_LastStartSize = 0;
	m_LastFileSize = 0;
	m_DemuxLock = FALSE;
	m_IntLastStreamTime = 0;
	m_rtTimeShiftPosition = 0;
	m_LastMultiFileStart = 0;
	m_LastMultiFileEnd = 0;
	return hr;
}

HRESULT CTSFileSourcePin::FillBuffer(IMediaSample *pSample)
{
	CheckPointer(pSample, E_POINTER);

	if (m_bSeeking)
	{
		return NOERROR;
	}

	//Test if parser Locked
	if (!m_pTSFileSourceFilter->m_pPidParser->m_ParsingLock){
		//fix our pid values for this run
		m_pTSFileSourceFilter->m_pPidParser->pids.CopyTo(m_pPids); 
		m_bASyncModeSave = m_pTSFileSourceFilter->m_pPidParser->get_AsyncMode();
		m_PacketSave = m_pTSFileSourceFilter->m_pPidParser->get_PacketSize();
		m_TSIDSave = m_pTSFileSourceFilter->m_pPidParser->m_TStreamID;
		m_PinTypeSave  = m_pTSFileSourceFilter->m_pPidParser->get_ProgPinMode();
	}


	CAutoLock lock(&m_FillLock);

	// Access the sample's data buffer
	PBYTE pData;
	LONG lDataLength;
	HRESULT hr = pSample->GetPointer(&pData);
	if (FAILED(hr))
	{
		return hr;
	}
	lDataLength = pSample->GetActualDataLength();

	m_pTSBuffer->SetFileReader(m_pTSFileSourceFilter->m_pFileReader);
	hr = m_pTSBuffer->Require(lDataLength);
	if (FAILED(hr))
	{
		return S_FALSE;
	}

//*************************************************************************************************
//Old Capture format Additions

	__int64 firstPass = m_llPrevPCR;

//	cold start
	if ((!m_pPids->pcr && m_bASyncModeSave) || !m_pTSFileSourceFilter->m_pPidParser->pidArray.Count()) {

		if (firstPass == -1) {

			m_DataRate = m_pPids->bitrate;
			m_DataRateTotal = 0;
			m_BitRateCycle = 0;
			for (int i = 0; i < 256; i++) { 
				m_BitRateStore[i] = 0;
			}
		}

		__int64 intTimeDelta = 0;
		CRefTime cTime;
		m_pTSFileSourceFilter->StreamTime(cTime);
		if (m_IntLastStreamTime > 20000000) {

			intTimeDelta = (__int64)(REFERENCE_TIME(cTime) - m_IntLastStreamTime);

			if (intTimeDelta > 0)
			{
				__int64 bitrate = ((__int64)lDataLength * (__int64)80000000) / intTimeDelta;
				AddBitRateForAverage(bitrate);
			}
				m_pPids->bitrate = m_DataRate;
		}

		{
			CAutoLock lock(&m_SeekLock);
			m_rtStart = REFERENCE_TIME(cTime) + m_rtLastSeekStart + intTimeDelta;
		}

		//Read from buffer
		m_pTSBuffer->DequeFromBuffer(pData, lDataLength);

		m_IntLastStreamTime = REFERENCE_TIME(cTime);
		m_llPrevPCR = REFERENCE_TIME(cTime);

		return NOERROR;
	}
//*************************************************************************************************

	if (m_llPrevPCR == -1)
	{
		Debug(TEXT("Finding the next two PCRs\n"));
		m_llBasePCR = -1;
		m_lPrevPCRByteOffset = 0;
		hr = FindNextPCR(&m_llPrevPCR, &m_lPrevPCRByteOffset, 1000000);
		if (FAILED(hr))
			Debug(TEXT("Failed to find PCR 1\n"));


		m_lNextPCRByteOffset = -1;
		if (m_lPrevPCRByteOffset < lDataLength)
		{
			m_lNextPCRByteOffset = lDataLength;
			hr = FindPrevPCR(&m_llNextPCR, &m_lNextPCRByteOffset);
			if (FAILED(hr) || (m_lNextPCRByteOffset == m_lPrevPCRByteOffset))
				m_lNextPCRByteOffset = -1;
		}

		if (m_lNextPCRByteOffset == -1)
		{
			m_lNextPCRByteOffset = m_lPrevPCRByteOffset + m_PacketSave;
			hr = FindNextPCR(&m_llNextPCR, &m_lNextPCRByteOffset, 1000000);
			if (FAILED(hr))
				Debug(TEXT("Failed to find PCR 2\n"));
		}

		m_llPCRDelta = m_llNextPCR - m_llPrevPCR;
		m_lByteDelta = m_lNextPCRByteOffset - m_lPrevPCRByteOffset;
	}

	if (m_lNextPCRByteOffset < 0)
	{

		__int64 llNextPCR = 0;
		long lNextPCRByteOffset = 0;

		lNextPCRByteOffset = lDataLength;
		hr = FindPrevPCR(&llNextPCR, &lNextPCRByteOffset);

		if (FAILED(hr))
		{
			lNextPCRByteOffset = 0;
			hr = FindNextPCR(&llNextPCR, &lNextPCRByteOffset, 1000000);
		}

		if (SUCCEEDED(hr))
		{
			m_lPrevPCRByteOffset = m_lNextPCRByteOffset;
			m_llPrevPCR = m_llNextPCR;

			m_llNextPCR = llNextPCR;
			m_lNextPCRByteOffset = lNextPCRByteOffset;

			m_llPCRDelta = m_llNextPCR - m_llPrevPCR;
			m_lByteDelta = m_lNextPCRByteOffset - m_lPrevPCRByteOffset;

			//8bits per byte and convert to sec divide by pcr duration then average it
			if ((__int64)ConvertPCRtoRT(m_llPCRDelta) > 0) 
			{
				__int64 bitrate = ((__int64)m_lByteDelta * (__int64)80000000) / (__int64)ConvertPCRtoRT(m_llPCRDelta);
				AddBitRateForAverage(bitrate);

				//TCHAR sz[60];
				//wsprintf(sz, TEXT("bitrate %i\n"), bitrate);
				//Debug(sz);
			}
		}
		else
		{
			Debug(TEXT("Failed to find next PCR\n"));
			Sleep(100); //delay to reduce cpu usage.
		}
	}

	//Calculate PCR
	__int64 pcrStart;
	if (m_lByteDelta > 0)
		pcrStart = m_llPrevPCR - (__int64)((__int64)(m_llPCRDelta * (__int64)m_lPrevPCRByteOffset) / (__int64)m_lByteDelta);
	else
	{
		Debug(TEXT("Invalid byte difference. Using previous PCR\n"));
		pcrStart = m_llPrevPCR;
	}

	//Read from buffer
	m_pTSBuffer->DequeFromBuffer(pData, lDataLength);
	m_lPrevPCRByteOffset -= lDataLength;
	m_lNextPCRByteOffset -= lDataLength;

	//Checking if basePCR is set
	if (m_llBasePCR == -1)
	{
		Debug(TEXT("Setting Base PCR\n"));
#ifdef USE_EVENT
		CComPtr<IReferenceClock> pReferenceClock;
		hr = Demux::GetReferenceClock(m_pTSFileSourceFilter, &pReferenceClock);
		if (pReferenceClock != NULL)
		{
			pReferenceClock->GetTime(&m_rtStartTime);
		}
		else
		{
			Debug(TEXT("Failed to find ReferenceClock. Sending sample now\n"));
			return S_OK;
		}
#else
		m_rtStartTime = (REFERENCE_TIME)((REFERENCE_TIME)timeGetTime() * 10000);
#endif
		m_llBasePCR = pcrStart;
	}

	// Calculate next event time
	//   rtStart is set relative to the time m_llBasePCR was last set.
	//     ie. on Run() and after each seek.
	//   m_rtStart is set relative to the begining of the file.
	//     this is so that IMediaSeeking can return the current position.
	pcrStart -= m_llBasePCR;
	CRefTime rtStart;
	rtStart = 0;
	if (pcrStart > -1)
	{
		rtStart = ConvertPCRtoRT(pcrStart);
		CAutoLock lock(&m_SeekLock);
		m_rtStart = rtStart + m_rtLastSeekStart;
	}

	//DEBUG: Trying to change playback rate to 10% slower. Didn't work.
	//rtStart = rtStart + (rtStart/10);

	REFERENCE_TIME rtNextTime = rtStart + m_rtStartTime - 391000;

	if (m_bRateControl)
	{
		//Wait if necessary
#ifdef USE_EVENT
		CComPtr<IReferenceClock> pReferenceClock;
		hr = Demux::GetReferenceClock(m_pTSFileSourceFilter, &pReferenceClock);
		if (pReferenceClock != NULL)
		{
			REFERENCE_TIME rtCurrTime;
			pReferenceClock->GetTime(&rtCurrTime);

			if (rtCurrTime < rtNextTime)
			{
				HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
				DWORD dwAdviseCookie = 0;
				pReferenceClock->AdviseTime(0, rtNextTime, (HEVENT)hEvent, &dwAdviseCookie);
				DWORD dwWaitResult = WaitForSingleObject(hEvent, INFINITE);
				CloseHandle(hEvent);
			}
			else
			{
				TCHAR sz[100];
				wsprintf(sz, TEXT("Bursting - late by %i (%i)\n"), rtCurrTime - rtNextTime, (pcrStart+m_llBasePCR) - m_llPrevPCR);
				Debug(sz);
			}
		}
#else
		REFERENCE_TIME rtCurrTime = (REFERENCE_TIME)((REFERENCE_TIME)timeGetTime() * 10000);

		__int64 refPCRdiff = (__int64)((__int64)rtNextTime - (__int64)rtCurrTime);

		//Loop until current time passes calculated current time
		while(rtCurrTime < rtNextTime)
		{
			refPCRdiff = (__int64)((__int64)rtNextTime - (__int64)rtCurrTime);
			refPCRdiff = refPCRdiff / 100000;	//sleep for a tenth of the time
			if (refPCRdiff == 0)	//break out if the sleep is really short
				break;

			Sleep((DWORD)(refPCRdiff)); // Delay it

			//Update current time
			rtCurrTime = (REFERENCE_TIME)((REFERENCE_TIME)timeGetTime() * 10000);
		}
#endif
	}

/*
#if DEBUG
	{
		CAutoLock lock(&m_SeekLock);
		TCHAR sz[100];
		long duration1 = m_pTSFileSourceFilter->m_pPidParser->pids.dur / (__int64)10000000;
		long duration2 = m_pTSFileSourceFilter->m_pPidParser->pids.dur % (__int64)10000000;
		long start1 = m_rtStart.m_time / (__int64)10000000;
		long start2 = m_rtStart.m_time % (__int64)10000000;
		long stop1 = m_rtStop.m_time / (__int64)10000000;
		long stop2 = m_rtStop.m_time % (__int64)10000000;
		wsprintf(sz, TEXT("\t\t\tduration %10i.%07i\t\tstart %10i.%07i\t\tstop %10i.%07i\n"), duration1, duration2, start1, start2, stop1, stop2);
		Debug(sz);
	}
#endif
*/
	//Set sample time
	//pSample->SetTime(&rtStart, &rtStart);
//PrintTime(TEXT("FillBuffer"), (__int64)m_rtLastCurrentTime, 10000);

//*************************************************************************************************
//Old Capture format Additions

	//If no TSID then we won't have a PAT so create one
	if (!m_TSIDSave && firstPass == -1 && !m_PinTypeSave)
	{
		ULONG pos = 0; 
		REFERENCE_TIME pcrPos = -1;

		//Get the first occurance of a timing packet
		hr = S_OK;
		hr = m_pTSFileSourceFilter->m_pPidParser->FindNextPCR(pData, lDataLength, m_pPids, &pcrPos, &pos, 1); //Get the PCR
		//If we have one then load our PAT, PMT & PCR Packets	
		if(pcrPos && hr == S_OK) {
			//Check if we can back up before timing packet	
			if (pos > m_PacketSave*2) {
			//back up before timing packet
				pos-= m_PacketSave*2;	//shift back to before pat & pmt packet
			}
		}
		else {
			// if no timing packet found then load the PAT at the start of the buffer
			pos=0;
		}

		LoadPATPacket(pData + pos, m_TSIDSave, m_pPids->sid, m_pPids->pmt);
		pos+= m_PacketSave;	//shift to the pmt packet

		//Also insert a pmt if we don't have one already
		if (!m_pPids->pmt){ 
			LoadPMTPacket(pData + pos,
				  m_pPids->pcr - m_pPids->opcr,
				  m_pPids->vid,
				  m_pPids->aud,
				  m_pPids->aud2,
				  m_pPids->ac3,
				  m_pPids->ac3_2,
				  m_pPids->txt);
		}
		else {
			//load another PAT instead
			LoadPATPacket(pData + pos, m_TSIDSave, m_pPids->sid, m_pPids->pmt);
		}

		pos+= m_PacketSave;	//shift to the pcr packet

		//Load in our own pcr if we need to
		if (m_pPids->opcr) {
			LoadPCRPacket(pData + pos, m_pPids->pcr - m_pPids->opcr, pcrPos);
		}

	}
/*
	//If we need to insert a new PCR packet
	if (m_pPids->opcr) {

		ULONG pos = 0, lastpos = 0;
		REFERENCE_TIME pcrPos = -1;
		hr = S_OK;
		while (hr == S_OK) {
			//Look for a timing packet
			hr = m_pTSFileSourceFilter->m_pPidParser->FindNextOPCR(pData, lDataLength, m_pPids, &pcrPos, &pos, 1); //Get the PCR
			if (pcrPos) {
				//Insert our new PCR Packet
				LoadPCRPacket(pData + pos, m_pPids->pcr - m_pPids->opcr, pcrPos);
//PrintTime(TEXT("Insert PCR packet"), (__int64) pcrPos, 90);
//				break;
			}
			pos += m_PacketSave;

			if (pos > lastpos + 10*packet && pos + m_PacketSave < lDataLength){
//PrintTime(TEXT("delta PCR packet"), (__int64) m_llPCRDelta, 90);
				__int64 offset = (__int64)(m_llPCRDelta *(__int64)(pos - lastpos) / (__int64)m_lByteDelta);
//PrintTime(TEXT("offset PCR packet"), (__int64) offset, 90);
				LoadPCRPacket(pData + pos, m_pPids->pcr - m_pPids->opcr, pcrPos + offset);
				pos += m_PacketSave;
				lastpos = pos;
			}
		};
	}
*/
//*************************************************************************************************
	m_IntCurrentTimePCR = m_llPrevPCR;
	m_pTSFileSourceFilter->m_pPidParser->pids.bitrate = m_DataRate;

	return NOERROR;
}

HRESULT CTSFileSourcePin::OnThreadStartPlay( )
{
	m_rtLastCurrentTime = (REFERENCE_TIME)((REFERENCE_TIME)timeGetTime() * (REFERENCE_TIME)10000);
	m_llPrevPCR = -1;
	m_pTSBuffer->Clear();
	m_DataRate = m_pTSFileSourceFilter->m_pPidParser->pids.bitrate;
	debugcount = 0;
	m_rtTimeShiftPosition = 0;
	m_LastMultiFileStart = 0;
	m_LastMultiFileEnd = 0;
	m_DataRate = m_pTSFileSourceFilter->m_pPidParser->pids.bitrate;

	CAutoLock lock(&m_SeekLock);
	//Test if parser Locked
	if (!m_pTSFileSourceFilter->m_pPidParser->m_ParsingLock){
		//fix our pid values for this run
		m_pTSFileSourceFilter->m_pPidParser->pids.CopyTo(m_pPids); 
		m_bASyncModeSave = m_pTSFileSourceFilter->m_pPidParser->get_AsyncMode();
		m_PacketSave = m_pTSFileSourceFilter->m_pPidParser->get_PacketSize();
		m_TSIDSave = m_pTSFileSourceFilter->m_pPidParser->m_TStreamID;
		m_PinTypeSave  = m_pTSFileSourceFilter->m_pPidParser->get_ProgPinMode();
	}

    DeliverNewSegment(m_rtStart, m_rtStop, 1.0 );
	m_rtLastSeekStart = REFERENCE_TIME(m_rtStart);
	m_rtLastCurrentTime = (REFERENCE_TIME)((REFERENCE_TIME)timeGetTime() * (REFERENCE_TIME)10000);

	return CSourceStream::OnThreadStartPlay( );
}

HRESULT CTSFileSourcePin::Run(REFERENCE_TIME tStart)
{
	CAutoLock fillLock(&m_FillLock);
	CAutoLock seekLock(&m_SeekLock);

	//Test if parser Locked
	if (!m_pTSFileSourceFilter->m_pPidParser->m_ParsingLock){
		//fix our pid values for this run
		m_pTSFileSourceFilter->m_pPidParser->pids.CopyTo(m_pPids); 
		m_bASyncModeSave = m_pTSFileSourceFilter->m_pPidParser->get_AsyncMode();
		m_PacketSave = m_pTSFileSourceFilter->m_pPidParser->get_PacketSize();
		m_TSIDSave = m_pTSFileSourceFilter->m_pPidParser->m_TStreamID;
		m_PinTypeSave  = m_pTSFileSourceFilter->m_pPidParser->get_ProgPinMode();
	}

	CBasePin::m_tStart = tStart;
	BOOL bTimeMode;
	BOOL bTimeShifting = IsTimeShifting(m_pTSFileSourceFilter->m_pFileReader, &bTimeMode);
	if (bTimeMode)
	{
		m_rtStart = CBasePin::m_tStart;//m_rtLastSeekStart;
		m_pTSFileSourceFilter->ResetStreamTime();
	}
	else
		m_rtLastSeekStart = REFERENCE_TIME(m_rtStart);

	m_rtTimeShiftPosition = 0;
	m_LastMultiFileStart = 0;
	m_LastMultiFileEnd = 0;

	if (!m_bSeeking && !m_DemuxLock)
	{	
		CComPtr<IReferenceClock> pClock;
		Demux::GetReferenceClock(m_pTSFileSourceFilter, &pClock);
		SetDemuxClock(pClock);
//		m_pTSFileSourceFilter->NotifyEvent(EC_CLOCK_UNSET, NULL, NULL);
	}

	return CBaseOutputPin::Run(tStart);

}

STDMETHODIMP CTSFileSourcePin::GetCurrentPosition(LONGLONG *pCurrent)
{
	if (pCurrent)
	{
		CAutoLock seekLock(&m_SeekLock);

		//Get the FileReader Type
		WORD bMultiMode;
		m_pTSFileSourceFilter->m_pFileReader->get_ReaderMode(&bMultiMode);
		//Do MultiFile timeshifting mode
		if (m_bGetAvailableMode && bMultiMode)
		{
			*pCurrent = max(0, (__int64)ConvertPCRtoRT(m_IntCurrentTimePCR - m_IntBaseTimePCR));
//			*pCurrent = max(0, (__int64)ConvertPCRtoRT(m_IntCurrentTimePCR));
			REFERENCE_TIME current, stop;
			return CSourceSeeking::GetPositions(&current, &stop);
		}

		REFERENCE_TIME stop;
		return GetPositions(pCurrent, &stop);
	}
	return CSourceSeeking::GetCurrentPosition(pCurrent);
}

STDMETHODIMP CTSFileSourcePin::GetPositions(LONGLONG *pCurrent, LONGLONG *pStop)
{
	if (pCurrent)
	{
		CAutoLock seekLock(&m_SeekLock);
		//Get the FileReader Type
		WORD bMultiMode;
		m_pTSFileSourceFilter->m_pFileReader->get_ReaderMode(&bMultiMode);

		//Do MultiFile timeshifting mode
		if(bMultiMode)
		{
			if (m_bGetAvailableMode)
			{
				*pCurrent = max(0, (__int64)ConvertPCRtoRT(m_IntCurrentTimePCR - m_IntBaseTimePCR));
				*pStop = max(0, (__int64)ConvertPCRtoRT(m_IntEndTimePCR - m_IntBaseTimePCR));
				REFERENCE_TIME current, stop;
				return CSourceSeeking::GetPositions(&current, &stop);
			}
			else
			{
				*pCurrent = max(0, (__int64)ConvertPCRtoRT(m_IntCurrentTimePCR - m_IntStartTimePCR));
				*pStop = max(0, (__int64)ConvertPCRtoRT(m_IntEndTimePCR - m_IntStartTimePCR));
				REFERENCE_TIME current, stop;
				return CSourceSeeking::GetPositions(&current, &stop);
			}
		}
		else
		{
			BOOL bTimeMode;
			BOOL bTimeShifting = IsTimeShifting(m_pTSFileSourceFilter->m_pFileReader, &bTimeMode);
			if (bTimeMode)
				*pCurrent = (REFERENCE_TIME)m_rtTimeShiftPosition;
			else
			{
				CRefTime cTime;
				m_pTSFileSourceFilter->StreamTime(cTime);
				*pCurrent = (REFERENCE_TIME)(m_rtLastSeekStart + REFERENCE_TIME(cTime));
			}
			REFERENCE_TIME current;
			return CSourceSeeking::GetPositions(&current, pStop);
		}
	}
	return CSourceSeeking::GetPositions(pCurrent, pStop);
}

STDMETHODIMP CTSFileSourcePin::SetPositions(LONGLONG *pCurrent, DWORD CurrentFlags
			     , LONGLONG *pStop, DWORD StopFlags)
{
	if(!m_rtDuration)
		return E_FAIL;

	BOOL bFileWasOpen = TRUE;
	if (m_pTSFileSourceFilter->m_pFileReader->IsFileInvalid())
	{
		HRESULT hr = m_pTSFileSourceFilter->m_pFileReader->OpenFile();
		if (FAILED(hr))
			return hr;

		bFileWasOpen = FALSE;
	}

	if (pCurrent)
	{
		//Get the FileReader Type
		WORD bMultiMode;
		m_pTSFileSourceFilter->m_pFileReader->get_ReaderMode(&bMultiMode);

		//Do MultiFile timeshifting mode
		if (m_bGetAvailableMode && bMultiMode)
		{
			REFERENCE_TIME rtStop = *pStop;
			REFERENCE_TIME rtCurrent = *pCurrent;
			if (CurrentFlags & AM_SEEKING_RelativePositioning)
			{
				CAutoLock lock(&m_SeekLock);
				rtCurrent += m_rtStart;
				CurrentFlags -= AM_SEEKING_RelativePositioning; //Remove relative flag
				CurrentFlags += AM_SEEKING_AbsolutePositioning; //Replace with absoulute flag
			}
			else
				rtCurrent = max(0, (__int64)((__int64)*pCurrent - max(0,(__int64)(ConvertPCRtoRT(m_IntStartTimePCR - m_IntBaseTimePCR)))));

			if (StopFlags & AM_SEEKING_RelativePositioning)
			{
				CAutoLock lock(&m_SeekLock);
				rtStop += m_rtStop;
				StopFlags -= AM_SEEKING_RelativePositioning; //Remove relative flag
				StopFlags += AM_SEEKING_AbsolutePositioning; //Replace with absoulute flag
			}
			else
				rtStop = max(0, (__int64)((__int64)*pStop - max(0,(__int64)(ConvertPCRtoRT(m_IntStartTimePCR - m_IntBaseTimePCR)))));

			HRESULT hr = setPositions(&rtCurrent, CurrentFlags, &rtStop, StopFlags);

			if (CurrentFlags & AM_SEEKING_ReturnTime)
				*pCurrent  = rtCurrent + max(0,(__int64)(ConvertPCRtoRT(m_IntStartTimePCR - m_IntBaseTimePCR)));

			if (StopFlags & AM_SEEKING_ReturnTime)
				*pStop  = rtStop + max(0,(__int64)(ConvertPCRtoRT(m_IntStartTimePCR - m_IntBaseTimePCR)));

			return hr;
		}
		return setPositions(pCurrent, CurrentFlags, pStop, StopFlags);
	}
	return CSourceSeeking::SetPositions(pCurrent, CurrentFlags, pStop, StopFlags);
}

HRESULT CTSFileSourcePin::setPositions(LONGLONG *pCurrent, DWORD CurrentFlags
			     , LONGLONG *pStop, DWORD StopFlags)
{
//PrintTime(TEXT("setPositions"), (__int64) *pCurrent, 10000);
	if(!m_rtDuration)
		return E_FAIL;

	if (pCurrent)
	{
		WORD readonly = 0;
		m_pTSFileSourceFilter->m_pFileReader->get_ReadOnly(&readonly);
		if (readonly) {
			//wait for the Length Changed Event to complete
			REFERENCE_TIME rtCurrentTime = (REFERENCE_TIME)((REFERENCE_TIME)timeGetTime() * (REFERENCE_TIME)10000);
			while ((REFERENCE_TIME)(m_rtLastCurrentTime + (REFERENCE_TIME)2000000) > rtCurrentTime) {
				rtCurrentTime = (REFERENCE_TIME)((REFERENCE_TIME)timeGetTime() * (REFERENCE_TIME)10000);
			}
		}

		REFERENCE_TIME rtCurrent = *pCurrent;
		if (CurrentFlags & AM_SEEKING_RelativePositioning)
		{
//PrintTime(TEXT("setPositions/RelativePositioning"), (__int64) *pCurrent, 10000);
			CAutoLock lock(&m_SeekLock);
			rtCurrent += m_rtStart;
			CurrentFlags -= AM_SEEKING_RelativePositioning; //Remove relative flag
			CurrentFlags += AM_SEEKING_AbsolutePositioning; //Replace with absoulute flag
		}

		if (CurrentFlags & AM_SEEKING_PositioningBitsMask)
		{
//PrintTime(TEXT("setPositions/PositioningBitsMask"), (__int64) *pCurrent, 10000);
			CAutoLock lock(&m_SeekLock);
			m_rtStart = rtCurrent;
		}

		if (!(CurrentFlags & AM_SEEKING_NoFlush) && (CurrentFlags & AM_SEEKING_PositioningBitsMask))
		{
//			if (ThreadExists())
//			{	
				m_bSeeking = TRUE;

				if(m_pTSFileSourceFilter->IsActive() && !m_DemuxLock)
					SetDemuxClock(NULL);

				CAutoLock fillLock(&m_FillLock);
				//Test if parser Locked
				if (!m_pTSFileSourceFilter->m_pPidParser->m_ParsingLock){
					//fix our pid values for this run
					m_pTSFileSourceFilter->m_pPidParser->pids.CopyTo(m_pPids); 
					m_bASyncModeSave = m_pTSFileSourceFilter->m_pPidParser->get_AsyncMode();
					m_PacketSave = m_pTSFileSourceFilter->m_pPidParser->get_PacketSize();
					m_TSIDSave = m_pTSFileSourceFilter->m_pPidParser->m_TStreamID;
					m_PinTypeSave  = m_pTSFileSourceFilter->m_pPidParser->get_ProgPinMode();
				}
				m_LastMultiFileStart = 0;
				m_LastMultiFileEnd = 0;

				DeliverBeginFlush();
				CSourceStream::Stop();
				m_DataRate = m_pTSFileSourceFilter->m_pPidParser->pids.bitrate;
				m_llPrevPCR = -1;

				m_pTSBuffer->SetFileReader(m_pTSFileSourceFilter->m_pFileReader);
				m_pTSBuffer->Clear();
				SetAccuratePos(rtCurrent);
				if (CurrentFlags & AM_SEEKING_PositioningBitsMask)
				{
					CAutoLock lock(&m_SeekLock);
					m_rtStart = rtCurrent;
				}
				m_rtLastSeekStart = rtCurrent;
				DeliverEndFlush();
				CSourceStream::Run();
				if (CurrentFlags & AM_SEEKING_ReturnTime)
					*pCurrent  = rtCurrent;

				CAutoLock lock(&m_SeekLock);
				return CSourceSeeking::SetPositions(&rtCurrent, CurrentFlags, pStop, StopFlags);
//			}
		}
		if (CurrentFlags & AM_SEEKING_ReturnTime)
			*pCurrent  = rtCurrent;

		return CSourceSeeking::SetPositions(&rtCurrent, CurrentFlags, pStop, StopFlags);
	}
	return CSourceSeeking::SetPositions(pCurrent, CurrentFlags, pStop, StopFlags);
}


STDMETHODIMP CTSFileSourcePin::GetAvailable(LONGLONG *pEarliest, LONGLONG *pLatest)
{
	m_bGetAvailableMode = TRUE;

	CAutoLock seekLock(&m_SeekLock);
	CheckPointer(pEarliest,E_POINTER);
	CheckPointer(pLatest,E_POINTER);

	if(!m_pTSFileSourceFilter->m_pFileReader)
		return CSourceSeeking::GetAvailable(pEarliest, pLatest);

	//Get the FileReader Type
	WORD bMultiMode;
	m_pTSFileSourceFilter->m_pFileReader->get_ReaderMode(&bMultiMode);

	//Do MultiFile timeshifting mode
	if(bMultiMode)
	{
		*pEarliest = max(0,(__int64)(ConvertPCRtoRT(m_IntStartTimePCR	- m_IntBaseTimePCR)));
		*pLatest = max(0,(__int64)(ConvertPCRtoRT(m_IntEndTimePCR - m_IntBaseTimePCR)));;
		return S_OK;
	}
	return CSourceSeeking::GetAvailable(pEarliest, pLatest);
}

HRESULT CTSFileSourcePin::ChangeStart()
{
//	UpdateFromSeek(TRUE);
	m_bSeeking = FALSE;
    return S_OK;
}

HRESULT CTSFileSourcePin::ChangeStop()
{
//	UpdateFromSeek();
	m_bSeeking = FALSE;
    return S_OK;
}

HRESULT CTSFileSourcePin::ChangeRate()
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

void CTSFileSourcePin::ClearBuffer(void)
{
	m_pTSBuffer->Clear();
}

void CTSFileSourcePin::UpdateFromSeek(BOOL updateStartPosition)
{
	if (ThreadExists())
	{	
		m_bSeeking = TRUE;
		CAutoLock fillLock(&m_FillLock);
		CAutoLock seekLock(&m_SeekLock);
		if(m_pTSFileSourceFilter->IsActive() && !m_DemuxLock)
			SetDemuxClock(NULL);
		DeliverBeginFlush();
		m_llPrevPCR = -1;
		if (updateStartPosition == TRUE)
		{
			m_pTSBuffer->Clear();
			SetAccuratePos(m_rtStart);
			//m_pTSFileSourceFilter->FileSeek(m_rtStart);
			m_rtLastSeekStart = REFERENCE_TIME(m_rtStart);
		}
		DeliverEndFlush();
	}
	m_bSeeking = FALSE;
}


HRESULT CTSFileSourcePin::SetAccuratePos(REFERENCE_TIME seektime)
{
PrintTime(TEXT("seekin"), (__int64) seektime, 10000);
	HRESULT hr;

	//Return as quick as possible if cold starting
	__int64 fileStart, filelength = 0;
	m_pTSFileSourceFilter->m_pFileReader->GetFileSize(&fileStart, &filelength);
	if(filelength < 2000000)
	{
		__int64 nFileIndex = (__int64)min((__int64)-filelength, (__int64)(300000 - filelength));
		m_pTSFileSourceFilter->m_pFileReader->setFilePointer(nFileIndex, FILE_END);

		return S_OK;
	}

	ULONG pos;
	__int64 pcrEndPos;
	ULONG ulBytesRead = 0;
	long lDataLength = 1000000;
	__int64 nFileIndex = 0;
	m_pTSFileSourceFilter->ResetStreamTime();
	m_rtStart = seektime;
	m_rtLastSeekStart = REFERENCE_TIME(m_rtStart);
	m_rtTimeShiftPosition = seektime;

	WORD bMultiMode;
	m_pTSFileSourceFilter->m_pFileReader->get_ReaderMode(&bMultiMode);

	//Do MultiFile timeshifting mode
	if(bMultiMode)
	{
		if (m_bGetAvailableMode && ((__int64)(seektime + (__int64)20000000) > m_pTSFileSourceFilter->m_pPidParser->pids.dur))
		{
			seektime = max(0, m_pTSFileSourceFilter->m_pPidParser->pids.dur -(__int64)20000000);
			m_rtStart = seektime;
			m_rtLastSeekStart = REFERENCE_TIME(m_rtStart);
			m_rtTimeShiftPosition = seektime;
		}
	}
	else
	{
		BOOL bTimeMode;
		BOOL timeShifting = IsTimeShifting(m_pTSFileSourceFilter->m_pFileReader, &bTimeMode);

		//Prevent the filtergraph clock from approaching the end time
		if (bTimeMode && ((__int64)(seektime + (__int64)40000000) > m_pTSFileSourceFilter->m_pPidParser->pids.dur))
			seektime = max(0, m_pTSFileSourceFilter->m_pPidParser->pids.dur -(__int64)40000000);
	}

//***********************************************************************************************
//Old Capture format Additions

	if (!m_pTSFileSourceFilter->m_pPidParser->pids.pcr && m_pTSFileSourceFilter->m_pPidParser->get_AsyncMode()) {
//	if (!m_pPidParser->pids.pcr && !m_pPidParser->get_ProgPinMode()) {
		// Revert to old method
		// shifting right by 14 rounds the seek and duration time down to the
		// nearest multiple 16.384 ms. More than accurate enough for our seeks.
		nFileIndex = 0;

		if (m_pTSFileSourceFilter->m_pPidParser->pids.dur>>14)
			nFileIndex = filelength * (__int64)(seektime>>14) / (__int64)(m_pTSFileSourceFilter->m_pPidParser->pids.dur>>14);

		nFileIndex = (__int64)max((__int64)300000, (__int64)nFileIndex);
		m_pTSFileSourceFilter->m_pFileReader->setFilePointer((__int64)(nFileIndex - filelength), FILE_END);

		return S_OK;
	}
//***********************************************************************************************

	PBYTE pData = new BYTE[4000000];
	__int64 pcrByteRate = (__int64)((__int64)m_DataRate / (__int64)720);
//PrintTime(TEXT("our pcr Byte Rate"), (__int64)pcrByteRate, 90);

////Multireader fixed 	__int64	pcrDuration = (__int64)((__int64)((__int64)m_pTSFileSourceFilter->m_pPidParser->pids.dur * (__int64)9) / (__int64)1000);
		__int64	pcrDuration = (__int64)max(0, (__int64)((__int64)m_pTSFileSourceFilter->m_pPidParser->pids.end - (__int64)m_pTSFileSourceFilter->m_pPidParser->pids.start));
//PrintTime(TEXT("our pcr pid.start time for reference"), (__int64)m_pTSFileSourceFilter->m_pPidParser->pids.start, 90);
//PrintTime(TEXT("our pcr pid.end time for reference"), (__int64)m_pTSFileSourceFilter->m_pPidParser->pids.end, 90);
PrintTime(TEXT("our pcr duration"), (__int64)pcrDuration, 90);

	//Get estimated time of seek as pcr 
	__int64	pcrDeltaSeekTime = (__int64)((__int64)((__int64)seektime * (__int64)9) / (__int64)1000);
PrintTime(TEXT("our pcr Delta SeekTime"), (__int64)pcrDeltaSeekTime, 90);

//This is where we create a pcr time relative to the current stream position
//
	//Set Pointer to end of file to get end pcr
	m_pTSFileSourceFilter->m_pFileReader->setFilePointer((__int64) - ((__int64)lDataLength), FILE_END);
	m_pTSFileSourceFilter->m_pFileReader->Read(pData, lDataLength, &ulBytesRead);
	if (ulBytesRead != lDataLength)
	{
PrintTime(TEXT("File Read Call failed"), (__int64)ulBytesRead, 90);

		delete[] pData;
		return S_FALSE;
	}


	pos = ulBytesRead - m_pTSFileSourceFilter->m_pPidParser->get_PacketSize();
	hr = S_OK;
	hr = m_pTSFileSourceFilter->m_pPidParser->FindNextPCR(pData, ulBytesRead, &m_pTSFileSourceFilter->m_pPidParser->pids, &pcrEndPos, &pos, -1); //Get the PCR
	__int64	pcrSeekTime = pcrDeltaSeekTime + (__int64)(pcrEndPos - pcrDuration);
PrintTime(TEXT("our pcr end time for reference"), (__int64)pcrEndPos, 90);

	//Test if we have a pcr or if the pcr is less than rollover time
	if (FAILED(hr) || pcrEndPos == 0 || pcrSeekTime < 0) {
PrintTime(TEXT("get lastpcr failed now using first pcr"), (__int64)m_IntStartTimePCR, 90);
	
		//Set seektime to position relative to first pcr
		pcrSeekTime = m_IntStartTimePCR + pcrDeltaSeekTime;

		//test if pcr time is now larger than file size
		if (pcrSeekTime > m_IntEndTimePCR || pcrEndPos == 0) {
//		if (pcrSeekTime > pcrDuration) {
PrintTime(TEXT("get first pcr failed as well SEEK ERROR AT START"), (__int64) pcrSeekTime, 90);

			// Revert to old method
			// shifting right by 14 rounds the seek and duration time down to the
			// nearest multiple 16.384 ms. More than accurate enough for our seeks.
			nFileIndex = 0;

			if (m_pTSFileSourceFilter->m_pPidParser->pids.dur>>14)
				nFileIndex = filelength * (__int64)(seektime>>14) / (__int64)(m_pTSFileSourceFilter->m_pPidParser->pids.dur>>14);

			nFileIndex = (__int64)max((__int64)300000, (__int64)nFileIndex);
			m_pTSFileSourceFilter->m_pFileReader->setFilePointer((__int64)(nFileIndex - filelength), FILE_END);

			delete[] pData;
			return S_OK;
		}
	}

PrintTime(TEXT("our predicted pcr position for seek"), (__int64)pcrSeekTime, 90);

	//create our predicted file pointer position
	nFileIndex = (pcrDeltaSeekTime / (__int64)1000) * pcrByteRate;

	// set back so we can get last batch of data if at end of file
	if ((__int64)(nFileIndex + (__int64)lDataLength) > filelength)
		nFileIndex = (__int64)(filelength - (__int64)lDataLength);


	//Set Pointer to the predicted file position to get end pcr
	nFileIndex = max(100000, nFileIndex);
	m_pTSFileSourceFilter->m_pFileReader->setFilePointer((__int64)(nFileIndex - filelength), FILE_END);
	m_pTSFileSourceFilter->m_pFileReader->Read(pData, lDataLength, &ulBytesRead);
	__int64 pcrPos;
	pos = 0;

	hr = S_OK;
	hr = m_pTSFileSourceFilter->m_pPidParser->FindNextPCR(pData, ulBytesRead, &m_pTSFileSourceFilter->m_pPidParser->pids, &pcrPos, &pos, 1);
	nFileIndex += (__int64)pos;

	//compare our predicted file position to our predicted seektime and adjust file pointer
	if (pcrPos > pcrSeekTime) {
		nFileIndex -= (__int64)((__int64)((__int64)(pcrPos - pcrSeekTime) / (__int64)1000) * (__int64)pcrByteRate);
PrintTime(TEXT("seek---------"), (__int64) pcrPos, 90);
	}
	else if (pcrSeekTime > pcrPos) {
			nFileIndex += (__int64)((__int64)((__int64)(pcrSeekTime - pcrPos) / (__int64)1000) * (__int64)pcrByteRate);
PrintTime(TEXT("seek+++++++++++++"), (__int64) pcrPos, 90);
	}

	lDataLength = 4000000;

	//Now we are close so setup the a +/- 2meg buffer
	nFileIndex -= (__int64)(lDataLength / 2); //Centre buffer

	// set buffer start back from EOF so we can get last batch of data
	if ((nFileIndex + lDataLength) > filelength)
		nFileIndex = (__int64)(filelength - (__int64)lDataLength);

	nFileIndex = max(100000, nFileIndex);
	m_pTSFileSourceFilter->m_pFileReader->setFilePointer((__int64)(nFileIndex - filelength), FILE_END);
	ulBytesRead = 0;
	m_pTSFileSourceFilter->m_pFileReader->Read(pData, lDataLength, &ulBytesRead);

	pcrPos = 0;
	pos = ulBytesRead / 2;//buffer the centre search

	hr = S_OK;		
	while (pcrSeekTime > pcrPos && hr == S_OK) {
		//Seek forwards
		pos += m_pTSFileSourceFilter->m_pPidParser->get_PacketSize();
		hr = m_pTSFileSourceFilter->m_pPidParser->FindNextPCR(pData, ulBytesRead, &m_pTSFileSourceFilter->m_pPidParser->pids, &pcrPos, &pos, 1); //Get the PCR
PrintTime(TEXT("seekfwdloop"), (__int64) pcrPos, 90);
	}
PrintTime(TEXT("seekfwd"), (__int64) pcrPos, 90);

	//Store this pos for later use
	__int64 posSave = 0;
	if (SUCCEEDED(hr))
		posSave = pos; //Save this position if where past seek value
	else
		pos = ulBytesRead;//set buffer to end for backward search

	pos -= m_pTSFileSourceFilter->m_pPidParser->get_PacketSize();
	pcrPos -= 1; 

	hr = S_OK;		
	while (pcrPos > pcrSeekTime && hr == S_OK) {
		//Seek backwards
		hr = m_pTSFileSourceFilter->m_pPidParser->FindNextPCR(pData, ulBytesRead, &m_pTSFileSourceFilter->m_pPidParser->pids, &pcrPos, &pos, -1); //Get the PCR
		pos -= m_pTSFileSourceFilter->m_pPidParser->get_PacketSize();
PrintTime(TEXT("seekbackloop"), (__int64) pcrPos, 90);
	}
PrintTime(TEXT("seekback"), (__int64) pcrPos, 90);

	// if we have backed up to correct pcr
	if (SUCCEEDED(hr)) {
		//Get mid position between pcr's only if in TS pin mode
		if (posSave) {
			//set mid way
			posSave -= (__int64)pos;
			pos += (ULONG)((__int64)posSave /(__int64)2);
		}

		//Set pointer to locale
		nFileIndex += (__int64)pos;

PrintTime(TEXT("seekend"), (__int64) pcrPos, 90);
	}
	else
	{
		// Revert to old method
		// shifting right by 14 rounds the seek and duration time down to the
		// nearest multiple 16.384 ms. More than accurate enough for our seeks.
		nFileIndex = 0;

		if (m_pTSFileSourceFilter->m_pPidParser->pids.dur>>14)
			nFileIndex = filelength * (__int64)(seektime>>14) / (__int64)(m_pTSFileSourceFilter->m_pPidParser->pids.dur>>14);

PrintTime(TEXT("SEEK ERROR AT END"), (__int64)pcrPos, 90);
	}
		nFileIndex = max(300000, nFileIndex);
		m_pTSFileSourceFilter->m_pFileReader->setFilePointer((__int64)(nFileIndex - filelength), FILE_END);

	delete[] pData;

	return S_OK;
}

HRESULT CTSFileSourcePin::UpdateDuration(FileReader *pFileReader)
{
	HRESULT hr = E_FAIL;

//***********************************************************************************************
//Old Capture format Additions

	if(!m_pTSFileSourceFilter->m_pPidParser->pids.pcr && !m_pTSFileSourceFilter->m_pPidParser->get_ProgPinMode())
	{
		hr = S_FALSE;

		if (m_bSeeking)
			return hr;

		REFERENCE_TIME rtCurrentTime = (REFERENCE_TIME)((REFERENCE_TIME)timeGetTime() * (REFERENCE_TIME)10000);
		REFERENCE_TIME rtStop = 0;

		if (m_BitRateCycle < 50)
			m_DataRateSave = m_DataRate;

		//Calculate our time increase
		__int64 fileStart;
		__int64	fileSize = 0;
		pFileReader->GetFileSize(&fileStart, &fileSize);

		__int64 calcDuration = 0;
		if ((__int64)((__int64)m_DataRateSave / (__int64)8000) > 0)
		{
			calcDuration = (__int64)(fileSize / (__int64)((__int64)m_DataRateSave / (__int64)8000));
			calcDuration = (__int64)(calcDuration * (__int64)10000);
		}

		if ((__int64)m_pTSFileSourceFilter->m_pPidParser->pids.dur)
		{
			if (!m_bSeeking)
			{
				m_pTSFileSourceFilter->m_pPidParser->pids.dur = (REFERENCE_TIME)calcDuration;
				for (int i = 0; i < m_pTSFileSourceFilter->m_pPidParser->pidArray.Count(); i++)
				{
					m_pTSFileSourceFilter->m_pPidParser->pidArray[i].dur = m_pTSFileSourceFilter->m_pPidParser->pids.dur;
				}
				m_rtDuration = m_pTSFileSourceFilter->m_pPidParser->pids.dur;
				m_rtStop = m_pTSFileSourceFilter->m_pPidParser->pids.dur;
			}

			if ((REFERENCE_TIME)(m_rtLastCurrentTime + (REFERENCE_TIME)10000000) < rtCurrentTime) {
				//Get CSourceSeeking current time.
				CSourceSeeking::GetPositions(&rtCurrentTime, &rtStop);
				//Test if we had been seeking recently and wait 2sec if so.
				if ((REFERENCE_TIME)(m_rtLastSeekStart + (REFERENCE_TIME)20000000) < rtCurrentTime) {

					//Send event to update filtergraph clock.
					if (!m_bSeeking)
					{
						m_rtLastCurrentTime = (REFERENCE_TIME)((REFERENCE_TIME)timeGetTime() * (REFERENCE_TIME)10000);
						m_pTSFileSourceFilter->NotifyEvent(EC_LENGTH_CHANGED, NULL, NULL);
						hr = S_OK;
					}
				}
			}
		}
		return hr;
	}

//***********************************************************************************************

	hr = S_FALSE;

	if (m_bSeeking)
		return hr;

	WORD readonly = 0;
	pFileReader->get_ReadOnly(&readonly);

	REFERENCE_TIME rtCurrentTime = (REFERENCE_TIME)((REFERENCE_TIME)timeGetTime() * (REFERENCE_TIME)10000);
	REFERENCE_TIME rtStop = 0;

	//check for duration every second of size change
	if (readonly)
	{
		//Get the FileReader Type
		WORD bMultiMode;
		pFileReader->get_ReaderMode(&bMultiMode);
		if(bMultiMode
			&& (REFERENCE_TIME)(m_rtLastCurrentTime + (REFERENCE_TIME)10000000) < rtCurrentTime)	//Do MultiFile timeshifting mode
		{
			//Check if Cold Start
			if(!m_IntBaseTimePCR && !m_IntStartTimePCR && !m_IntEndTimePCR)
			{
				m_LastMultiFileStart = -1;
				m_LastMultiFileEnd = -1;
			}

			//Get CSourceSeeking current time.
			CSourceSeeking::GetPositions(&rtCurrentTime, &rtStop);
			//Test if we had been seeking recently and wait 2sec if so.
			if ((REFERENCE_TIME)(m_rtLastSeekStart + (REFERENCE_TIME)20000000) > rtCurrentTime)
			{
				if(m_rtLastSeekStart || rtCurrentTime)
//				if (m_IntEndTimePCR != -1) //cold start
					return hr;
			}

			BOOL bLengthChanged = FALSE;
			BOOL bStartChanged = FALSE;
			ULONG ulBytesRead;
			LONG lDataLength = 188000;
			__int64 pcrPos;
			ULONG pos;

			// We'll use fileEnd instead of fileLength since fileLength /could/ be the same
			// even though fileStart and fileEnd have moved.
			__int64 fileStart, fileEnd, filelength;
			pFileReader->GetFileSize(&fileStart, &fileEnd);
			filelength = fileEnd;
			fileEnd += fileStart;
			if (fileStart != m_LastMultiFileStart)
			{
				ulBytesRead = 0;
				pcrPos = -1;

				//Set Pointer to start of file to get end pcr
				pFileReader->setFilePointer(300000, FILE_BEGIN);
				PBYTE pData = new BYTE[lDataLength];
				pFileReader->Read(pData, lDataLength, &ulBytesRead);
				pos = 0;

				hr = S_OK;
				hr = m_pTSFileSourceFilter->m_pPidParser->FindNextPCR(pData, ulBytesRead, &m_pTSFileSourceFilter->m_pPidParser->pids, &pcrPos, &pos, 1); //Get the PCR
				delete[] pData;
				//park the Pointer to end of file 
//				pFileReader->setFilePointer( (__int64)max(0, (__int64)(m_pTSFileSourceFilter->m_pFileReader->getFilePointer() -(__int64)100000)), FILE_BEGIN);

				//Cold Start
				if (m_LastMultiFileStart == -1)
				{
					m_IntStartTimePCR = pcrPos;
					m_IntBaseTimePCR = m_IntStartTimePCR;
					m_LastMultiFileStart = fileStart;
				}

				__int64	pcrDeltaTime = (__int64)(pcrPos - m_IntStartTimePCR);
				//Test if we have a pcr or if the pcr is less than rollover time
				if (FAILED(hr) || pcrDeltaTime < 0)
				{
					Debug(TEXT("Negative PCR Delta. This should only happen if there's a pcr rollover.\n"));
					PrintTime(TEXT("Prev Start PCR"), m_IntStartTimePCR, 90);
					PrintTime(TEXT("Start PCR"), pcrPos, 90);
					if(pcrPos)
						m_IntStartTimePCR = pcrPos;

					return hr;
				}
				else
				{

					m_IntStartTimePCR = pcrPos;

					//update the times in the array
					for (int i = 0; i < m_pTSFileSourceFilter->m_pPidParser->pidArray.Count(); i++)
						m_pTSFileSourceFilter->m_pPidParser->pidArray[i].start += pcrDeltaTime;

					m_pTSFileSourceFilter->m_pPidParser->pids.start += pcrDeltaTime;

					m_LastMultiFileStart = fileStart;
					bStartChanged = TRUE;
					m_rtLastSeekStart = (__int64)max(0, (__int64)ConvertPCRtoRT(m_IntCurrentTimePCR - m_IntStartTimePCR));
					m_rtStart = m_rtLastSeekStart;
					m_pTSFileSourceFilter->ResetStreamTime();
				}
			};

			if (fileEnd != m_LastMultiFileEnd)
			{
				ulBytesRead = 0;
				pcrPos = -1;

				//Set Pointer to end of file to get end pcr
				pFileReader->setFilePointer((__int64)-lDataLength, FILE_END);
				PBYTE pData = new BYTE[lDataLength];
				if FAILED(hr = pFileReader->Read(pData, lDataLength, &ulBytesRead))
				{
					Debug(TEXT("Failed to read from end of file"));
				}

				if (ulBytesRead < lDataLength)
				{
					Debug(TEXT("Didn't read as much as it should have"));
				}

				pos = ulBytesRead - m_pTSFileSourceFilter->m_pPidParser->get_PacketSize();

				hr = S_OK;
				hr = m_pTSFileSourceFilter->m_pPidParser->FindNextPCR(pData, ulBytesRead, &m_pTSFileSourceFilter->m_pPidParser->pids, &pcrPos, &pos, -1); //Get the PCR
				delete[] pData;
				//park the Pointer to end of file 
//				pFileReader->setFilePointer( (__int64)max(0, (__int64)(m_pTSFileSourceFilter->m_pFileReader->getFilePointer() -(__int64)100000)), FILE_BEGIN);

				//Cold Start
				if (m_LastMultiFileEnd == -1 && pcrPos)
				{
					m_IntEndTimePCR = pcrPos;
					m_pTSFileSourceFilter->m_pPidParser->pids.dur = (__int64)ConvertPCRtoRT(m_IntEndTimePCR - m_IntStartTimePCR); 
					m_LastMultiFileEnd = fileEnd;
					return hr;
				}

				__int64	pcrDeltaTime = (__int64)(pcrPos - m_IntEndTimePCR);
				//Test if we have a pcr or if the pcr is less than rollover time
				if (FAILED(hr) || pcrDeltaTime < 0)
				{
					Debug(TEXT("Negative PCR Delta. This should only happen if there's a pcr rollover.\n"));
					PrintTime(TEXT("Prev End PCR"), m_IntEndTimePCR, 90);
					PrintTime(TEXT("End PCR"), pcrPos, 90);
					if(pcrPos)
						m_IntEndTimePCR = pcrPos;
					return hr;
				}
				else
					bLengthChanged = TRUE;

				m_IntEndTimePCR = pcrPos;
				//update the times in the array
				for (int i = 0; i < m_pTSFileSourceFilter->m_pPidParser->pidArray.Count(); i++)
				{
					m_pTSFileSourceFilter->m_pPidParser->pidArray[i].end += pcrDeltaTime;
				}

				m_pTSFileSourceFilter->m_pPidParser->pids.end += pcrDeltaTime;
				m_LastMultiFileEnd = fileEnd;
			}

			if ((bLengthChanged | bStartChanged) && !m_bSeeking)
			{	
				__int64	pcrDeltaTime;
				if(m_bGetAvailableMode)
					//Use this code to cause the end time to be relative to the base time.
					pcrDeltaTime = (__int64)(m_IntEndTimePCR - m_IntBaseTimePCR);
				else
					//Use this code to cause the end time to be relative to the start time.
					pcrDeltaTime = (__int64)(m_IntEndTimePCR - m_IntStartTimePCR);

				m_pTSFileSourceFilter->m_pPidParser->pids.dur = (__int64)ConvertPCRtoRT(pcrDeltaTime);

				// update pid arrays
				for (int i = 0; i < m_pTSFileSourceFilter->m_pPidParser->pidArray.Count(); i++)
					m_pTSFileSourceFilter->m_pPidParser->pidArray[i].dur = m_pTSFileSourceFilter->m_pPidParser->pids.dur;

				m_rtDuration = m_pTSFileSourceFilter->m_pPidParser->pids.dur;

				if(m_bGetAvailableMode)
					//Use this code to cause the end time to be relative to the base time.
					m_rtStop = m_pTSFileSourceFilter->m_pPidParser->pids.dur;
				else
				{
					//Use this code to cause the end time to be relative to the start time.
					__int64 offset = max(0, (__int64)((__int64)m_rtStart - (__int64)m_rtDuration));
					if (offset)
						m_rtStop = ConvertPCRtoRT(m_IntCurrentTimePCR - m_IntStartTimePCR);
					else
						m_rtStop = m_pTSFileSourceFilter->m_pPidParser->pids.dur;
				}

//PrintTime(TEXT("UpdateDuration: m_IntBaseTimePCR"), (__int64)m_IntBaseTimePCR, 90);
//PrintTime(TEXT("UpdateDuration: m_IntStartTimePCR"), (__int64)m_IntStartTimePCR, 90);
//PrintTime(TEXT("UpdateDuration: m_IntEndTimePCR"), (__int64)m_IntEndTimePCR, 90);
//PrintTime(TEXT("UpdateDuration: pids.start"), (__int64)m_pTSFileSourceFilter->m_pPidParser->pids.start, 90);
//PrintTime(TEXT("UpdateDuration: pids.end"), (__int64)m_pTSFileSourceFilter->m_pPidParser->pids.end, 90);
//PrintTime(TEXT("UpdateDuration: pids.dur"), (__int64)m_pTSFileSourceFilter->m_pPidParser->pids.dur, 10000);

				if (!m_bSeeking)
				{
					m_rtLastCurrentTime = (REFERENCE_TIME)((REFERENCE_TIME)timeGetTime() * (REFERENCE_TIME)10000);
					m_pTSFileSourceFilter->NotifyEvent(EC_LENGTH_CHANGED, NULL, NULL);
					return S_OK;
				}
			}
			return S_FALSE;
		}
		else // FileReader Mode
		{
			//check for duration every second of size change
			BOOL bTimeMode;
			BOOL bTimeShifting = IsTimeShifting(pFileReader, &bTimeMode);

			BOOL bLengthChanged = FALSE;

			//check for valid values
			if ((m_pTSFileSourceFilter->m_pPidParser->pids.pcr | m_pTSFileSourceFilter->m_pPidParser->get_ProgPinMode())
				&& m_IntEndTimePCR
				&& 1 == 1){
				//check for duration every second of size change
				if(((REFERENCE_TIME)(m_rtLastCurrentTime + (REFERENCE_TIME)10000000) < rtCurrentTime))
				{
					ULONG pos;
					__int64 pcrPos;
					ULONG ulBytesRead = 0;
					long lDataLength = 188000;

					//Do a quick parse of duration if not seeking
					if (m_bSeeking)
						return hr;

					//Set Pointer to end of file to get end pcr
					pFileReader->setFilePointer((__int64)-lDataLength, FILE_END);
					PBYTE pData = new BYTE[lDataLength];
					pFileReader->Read(pData, lDataLength, &ulBytesRead);
					pos = ulBytesRead - m_pTSFileSourceFilter->m_pPidParser->get_PacketSize();

					hr = S_OK;
					hr = m_pTSFileSourceFilter->m_pPidParser->FindNextPCR(pData, ulBytesRead, &m_pTSFileSourceFilter->m_pPidParser->pids, &pcrPos, &pos, -1); //Get the PCR
					delete[] pData;
	
					__int64	pcrDeltaTime = (__int64)(pcrPos - m_IntEndTimePCR);
					//Test if we have a pcr or if the pcr is less than rollover time
					if (FAILED(hr) || pcrDeltaTime < (__int64)0) {
						if(pcrPos)
							m_IntEndTimePCR = pcrPos;
						return hr;
					}

					m_IntEndTimePCR = pcrPos;
					m_IntStartTimePCR += pcrDeltaTime;
					//if not time shifting update the duration
					if (!bTimeMode)
						m_pTSFileSourceFilter->m_pPidParser->pids.dur += (__int64)ConvertPCRtoRT(pcrDeltaTime);

					//update the times in the array
					for (int i = 0; i < m_pTSFileSourceFilter->m_pPidParser->pidArray.Count(); i++)
					{
						m_pTSFileSourceFilter->m_pPidParser->pidArray[i].end += pcrDeltaTime;
						// update the start time if shifting else update the duration
						if (bTimeMode)
							m_pTSFileSourceFilter->m_pPidParser->pidArray[i].start += pcrDeltaTime;
						else
							m_pTSFileSourceFilter->m_pPidParser->pidArray[i].dur = m_pTSFileSourceFilter->m_pPidParser->pids.dur;
					}
					m_pTSFileSourceFilter->m_pPidParser->pids.end += pcrDeltaTime;
					bLengthChanged = TRUE;
				}
			}
			else if ((REFERENCE_TIME)(m_rtLastCurrentTime + (REFERENCE_TIME)10000000) < rtCurrentTime
				&& 1 == 1)
			{
				//update all of the pid array times from a file parse.
				if (!m_bSeeking)
					m_pTSFileSourceFilter->m_pPidParser->RefreshDuration(TRUE, pFileReader);

				bLengthChanged = TRUE;
			}

			if (bLengthChanged)
			{
				if (!m_bSeeking)
				{
					//Set the filtergraph clock time to stationary position
					if (bTimeMode)
					{
//						__int64 current = (__int64)ConvertPCRtoRT(max(0, (__int64)(m_IntEndTimePCR - m_IntCurrentTimePCR)));
//						current = max(0,(__int64)(m_pTSFileSourceFilter->m_pPidParser->pids.dur - current - (__int64)10000000)); 
						CRefTime cTime;
						m_pTSFileSourceFilter->StreamTime(cTime);
						REFERENCE_TIME current = (REFERENCE_TIME)(m_rtLastSeekStart + REFERENCE_TIME(cTime));
						//Set the position of the filtergraph clock if first time shift pass
						if (!m_rtTimeShiftPosition)
							m_rtTimeShiftPosition = (REFERENCE_TIME)min(current, m_pTSFileSourceFilter->m_pPidParser->pids.dur - (__int64)10000000);
						//  set clock to stop or update time if not first pass
//						m_rtStop = max(0, (__int64)(m_pTSFileSourceFilter->m_pPidParser->pids.dur - (__int64)ConvertPCRtoRT((__int64)(m_IntEndTimePCR - m_IntCurrentTimePCR))));
						m_rtStop = max(m_rtTimeShiftPosition, m_rtLastSeekStart);
						if (!m_bSeeking)
						{
							m_rtLastCurrentTime = (REFERENCE_TIME)((REFERENCE_TIME)timeGetTime() * (REFERENCE_TIME)10000);
							m_pTSFileSourceFilter->NotifyEvent(EC_LENGTH_CHANGED, NULL, NULL);
							hr = S_OK;
						}
					}
					else
					{
						// if was time shifting but not anymore such as filewriter pause or stop
						if(m_rtTimeShiftPosition){
							//reset the stream clock and last seek save value
							m_rtStart = m_rtTimeShiftPosition;
							m_rtStop = m_rtTimeShiftPosition;
							m_rtLastSeekStart = m_rtTimeShiftPosition;
							m_pTSFileSourceFilter->ResetStreamTime();
							m_rtTimeShiftPosition = 0;
						}
						else
						{
							if(bTimeShifting)
							{
								m_rtTimeShiftPosition = 0;
//								CRefTime cTime;
//								m_pTSFileSourceFilter->StreamTime(cTime);
//								REFERENCE_TIME rtCurrent = (REFERENCE_TIME)(m_rtLastSeekStart + REFERENCE_TIME(cTime));
								__int64 current = (__int64)max(0, (__int64)ConvertPCRtoRT(max(0,(__int64)(m_IntEndTimePCR - m_IntCurrentTimePCR))));
								REFERENCE_TIME rtCurrent = (REFERENCE_TIME)(m_pTSFileSourceFilter->m_pPidParser->pids.dur - current); 
								m_rtDuration = m_pTSFileSourceFilter->m_pPidParser->pids.dur;
								m_rtStop = rtCurrent;
							}
							else
							{
								m_rtTimeShiftPosition = 0;
								m_rtDuration = m_pTSFileSourceFilter->m_pPidParser->pids.dur;
								m_rtStop = m_pTSFileSourceFilter->m_pPidParser->pids.dur;
							}

						}
												

					}

					//Send event to update filtergraph clock
					if (!m_bSeeking)
					{
						m_rtLastCurrentTime = (REFERENCE_TIME)((REFERENCE_TIME)timeGetTime() * (REFERENCE_TIME)10000);
						m_pTSFileSourceFilter->NotifyEvent(EC_LENGTH_CHANGED, NULL, NULL);
						return S_OK;;
					}

				}

				if ((REFERENCE_TIME)(m_rtLastCurrentTime + (REFERENCE_TIME)10000000) < rtCurrentTime) {
					//Get CSourceSeeking current time.
					CSourceSeeking::GetPositions(&rtCurrentTime, &rtStop);
					//Test if we had been seeking recently and wait 2sec if so.
					if ((REFERENCE_TIME)(m_rtLastSeekStart + (REFERENCE_TIME)20000000) < rtCurrentTime) {

						//Send event to update filtergraph clock.
						if (!m_bSeeking)
						{
							m_rtLastCurrentTime = (REFERENCE_TIME)((REFERENCE_TIME)timeGetTime() * (REFERENCE_TIME)10000);
							m_pTSFileSourceFilter->NotifyEvent(EC_LENGTH_CHANGED, NULL, NULL);
							return S_OK;;
						}
					}
				}

			}
			else
				return S_FALSE;
		}
	}
	return S_OK;
}

void CTSFileSourcePin::WaitPinLock(void)
{
	CAutoLock fillLock(&m_FillLock);
	CAutoLock lock(&m_SeekLock);
	{
	}
}

HRESULT CTSFileSourcePin::SetDuration(REFERENCE_TIME duration)
{
	CAutoLock fillLock(&m_FillLock);
	CAutoLock lock(&m_SeekLock);

	m_rtDuration = duration;
	m_rtStop = m_rtDuration;

    return S_OK;
}

REFERENCE_TIME CTSFileSourcePin::getPCRPosition(void)
{
	return ConvertPCRtoRT(m_IntCurrentTimePCR);
}

BOOL CTSFileSourcePin::IsTimeShifting(FileReader *pFileReader, BOOL *timeMode)
{
	WORD readonly = 0;
	pFileReader->get_ReadOnly(&readonly);
	__int64	fileStart, fileSize = 0;
	pFileReader->GetFileSize(&fileStart, &fileSize);
	*timeMode = (m_LastFileSize == fileSize) & (fileStart ? TRUE : FALSE) & (readonly ? TRUE : FALSE) & (m_LastStartSize != fileStart);
	m_LastFileSize = fileSize;
	m_LastStartSize = fileStart;
	return (fileStart ? TRUE : FALSE) & (readonly ? TRUE : FALSE);
}

BOOL CTSFileSourcePin::get_RateControl()
{
	return m_bRateControl;
}

void CTSFileSourcePin::set_RateControl(BOOL bRateControl)
{
	m_bRateControl = bRateControl;
}

__int64 CTSFileSourcePin::ConvertPCRtoRT(__int64 pcrtime)
{
	return (__int64)(pcrtime / 9) * 1000;
}

HRESULT CTSFileSourcePin::FindNextPCR(__int64 *pcrtime, long *byteOffset, long maxOffset)
{
	HRESULT hr = E_FAIL;

	long bytesToRead = m_lTSPacketDeliverySize + m_PacketSave;	//Read an extra packet to make sure we don't miss a PCR that spans a gap.
	BYTE *pData = new BYTE[bytesToRead];

	while (*byteOffset < maxOffset)
	{
		bytesToRead = min(bytesToRead, maxOffset-*byteOffset);

		hr = m_pTSBuffer->ReadFromBuffer(pData, bytesToRead, *byteOffset);
		if (FAILED(hr))
			break;

		ULONG pos = 0;
		hr = m_pTSFileSourceFilter->m_pPidParser->FindFirstPCR(pData, bytesToRead, m_pPids, pcrtime, &pos);
		if (SUCCEEDED(hr))
		{
			*byteOffset += pos;
			break;
		}

		*byteOffset += m_lTSPacketDeliverySize;
	};

	delete[] pData;
	return hr;
}


HRESULT CTSFileSourcePin::FindPrevPCR(__int64 *pcrtime, long *byteOffset)
{
	HRESULT hr = E_FAIL;

	long bytesToRead = m_lTSPacketDeliverySize + m_PacketSave; //Read an extra packet to make sure we don't miss a PCR that spans a gap.
	BYTE *pData = new BYTE[bytesToRead];

	while (*byteOffset > 0)
	{
		bytesToRead = min(m_lTSPacketDeliverySize, *byteOffset);
		*byteOffset -= bytesToRead;

		bytesToRead += m_PacketSave;

		hr = m_pTSBuffer->ReadFromBuffer(pData, bytesToRead, *byteOffset);
		if (FAILED(hr))
			break;

		ULONG pos = 0;
		hr = m_pTSFileSourceFilter->m_pPidParser->FindLastPCR(pData, bytesToRead, m_pPids, pcrtime, &pos);
		if (SUCCEEDED(hr))
		{
			*byteOffset += pos;
			break;
		}

		//*byteOffset -= m_lTSPacketDeliverySize;
	};

	delete[] pData;
	return hr;
}

long CTSFileSourcePin::get_BitRate()
{
    return m_DataRate;
}

void CTSFileSourcePin::set_BitRate(long rate)
{
    m_DataRate = rate;
}

void  CTSFileSourcePin::AddBitRateForAverage(__int64 bitratesample)
{
	if (bitratesample < (__int64)1)
		return;

	//Replace the old value with the new value
	m_DataRateTotal += bitratesample - m_BitRateStore[m_BitRateCycle];
	
	//If the previous value is not set then the total not made up from 256 values yet.
	if (m_BitRateStore[m_BitRateCycle] == 0)
		m_DataRate = (long)(m_DataRateTotal / (__int64)(m_BitRateCycle+1));
	else
		m_DataRate = (long)(m_DataRateTotal / (__int64)256);
		
	//Store the new value
	m_BitRateStore[m_BitRateCycle] = bitratesample;

	//Rotate array
	m_BitRateCycle++;
	if (m_BitRateCycle > 255)
		m_BitRateCycle = 0;

}

void CTSFileSourcePin::Debug(LPCTSTR lpOutputString)
{
	TCHAR sz[200];
	wsprintf(sz, TEXT("%05i - %s"), debugcount, lpOutputString);
	::OutputDebugString(sz);
	debugcount++;
}

void CTSFileSourcePin::PrintTime(LPCTSTR lstring, __int64 value, __int64 divider)
{

	TCHAR sz[100];
	long ms = (long)(value / divider);
	long secs = ms / 1000;
	long mins = secs / 60;
	long hours = mins / 60;
	ms = ms % 1000;
	secs = secs % 60;
	mins = mins % 60;
	wsprintf(sz, TEXT("%05i - %s %02i:%02i:%02i.%03i\n"), debugcount, lstring, hours, mins, secs, ms);
	::OutputDebugString(sz);
	debugcount++;
//MessageBox(NULL, sz,lstring, NULL);
}

void CTSFileSourcePin::PrintLongLong(LPCTSTR lstring, __int64 value)
{
	TCHAR sz[100];
	double dVal = value;
	double len = log10(dVal);
	int pos = len;
	sz[pos+1] = '\0';
	while (pos >= 0)
	{
		int val = value % 10;
		sz[pos] = '0' + val;
		value /= 10;
		pos--;
	}
	TCHAR szout[100];
	wsprintf(szout, TEXT("%05i - %s %s\n"), debugcount, lstring, sz);
	::OutputDebugString(szout);
	debugcount++;
}

HRESULT CTSFileSourcePin::DisconnectDemux()
{
	// Parse only the existing Mpeg2 Demultiplexer Filter
	// in the filter graph, we do this by looking for filters
	// that implement the IMpeg2Demultiplexer interface while
	// the count is still active.
	CFilterList FList(NAME("MyList"));  // List to hold the downstream peers.
	if (SUCCEEDED(Demux::GetPeerFilters(m_pTSFileSourceFilter, PINDIR_OUTPUT, FList)) && FList.GetHeadPosition())
	{
		IBaseFilter* pFilter = NULL;
		POSITION pos = FList.GetHeadPosition();
		pFilter = FList.Get(pos);
		while (SUCCEEDED(Demux::GetPeerFilters(pFilter, PINDIR_OUTPUT, FList)) && pos)
		{
			pFilter = FList.GetNext(pos);
		}

		pos = FList.GetHeadPosition();

		while (pos)
		{
			pFilter = FList.GetNext(pos);
			if(pFilter != NULL)
			{
				// Get an instance of the Demux control interface
				CComPtr<IMpeg2Demultiplexer> muxInterface;
				if(SUCCEEDED(pFilter->QueryInterface (&muxInterface)))
				{
					DisconnectOutputPins(pFilter);
					muxInterface.Release();
				}

				pFilter->Release();
				pFilter = NULL;
			}
		}
	}

	//Clear the filter list;
	POSITION pos = FList.GetHeadPosition();
	while (pos){

		FList.Remove(pos);
		pos = FList.GetHeadPosition();
	}

	return S_OK;
}

HRESULT CTSFileSourcePin::DisconnectOutputPins(IBaseFilter *pFilter)
{
	CComPtr<IPin> pOPin;
	PIN_DIRECTION  direction;
	// Enumerate the Demux pins
	CComPtr<IEnumPins> pIEnumPins;
	if (SUCCEEDED(pFilter->EnumPins(&pIEnumPins)))
	{

		ULONG pinfetch(0);
		while(pIEnumPins->Next(1, &pOPin, &pinfetch) == S_OK)
		{

			pOPin->QueryDirection(&direction);
			if(direction == PINDIR_OUTPUT)
			{
				// Get an instance of the Demux control interface
				CComPtr<IMpeg2Demultiplexer> muxInterface;
				if(SUCCEEDED(pFilter->QueryInterface (&muxInterface)))
				{
					LPWSTR pinName = L"";
					pOPin->QueryId(&pinName);
					muxInterface->DeleteOutputPin(pinName);
					muxInterface.Release();
				}
				else
				{
					IPin *pIPin = NULL;
					pOPin->ConnectedTo(&pIPin);
					if (pIPin)
					{
						pOPin->Disconnect();
						pIPin->Disconnect();
						pIPin->Release();
					}
				}

			}
			pOPin.Release();
			pOPin = NULL;
		}
	}
	return S_OK;
}

HRESULT CTSFileSourcePin::DisconnectInputPins(IBaseFilter *pFilter)
{
	CComPtr<IPin> pIPin;
	PIN_DIRECTION  direction;
	// Enumerate the Demux pins
	CComPtr<IEnumPins> pIEnumPins;
	if (SUCCEEDED(pFilter->EnumPins(&pIEnumPins)))
	{

		ULONG pinfetch(0);
		while(pIEnumPins->Next(1, &pIPin, &pinfetch) == S_OK)
		{

			pIPin->QueryDirection(&direction);
			if(direction == PINDIR_INPUT)
			{
				IPin *pOPin = NULL;
				pIPin->ConnectedTo(&pOPin);
				if (pOPin)
				{
					pOPin->Disconnect();
					pIPin->Disconnect();
					pOPin->Release();
				}
			}
			pIPin.Release();
			pIPin = NULL;
		}
	}
	return S_OK;
}

HRESULT CTSFileSourcePin::SetDemuxClock(IReferenceClock *pClock)
{
	// Parse only the existing Mpeg2 Demultiplexer Filter
	// in the filter graph, we do this by looking for filters
	// that implement the IMpeg2Demultiplexer interface while
	// the count is still active.
	CFilterList FList(NAME("MyList"));  // List to hold the downstream peers.
	if (SUCCEEDED(Demux::GetPeerFilters(m_pTSFileSourceFilter, PINDIR_OUTPUT, FList)) && FList.GetHeadPosition())
	{
		IBaseFilter* pFilter = NULL;
		POSITION pos = FList.GetHeadPosition();
		pFilter = FList.Get(pos);
		while (SUCCEEDED(Demux::GetPeerFilters(pFilter, PINDIR_OUTPUT, FList)) && pos)
		{
			pFilter = FList.GetNext(pos);
		}

		pos = FList.GetHeadPosition();

		while (pos)
		{
			pFilter = FList.GetNext(pos);
			if(pFilter != NULL)
			{
				// Get an instance of the Demux control interface
				IMpeg2Demultiplexer* muxInterface = NULL;
				if(SUCCEEDED(pFilter->QueryInterface (&muxInterface)))
				{
//***********************************************************************************************
//Old Capture format Additions
//					if (m_pPidParser->pids.pcr && m_pTSFileSourceFilter->get_AutoMode()) // && !m_pPidParser->pids.opcr) 
//***********************************************************************************************
					if (m_biMpegDemux && m_pTSFileSourceFilter->get_AutoMode())
						pFilter->SetSyncSource(pClock);
					muxInterface->Release();
				}
				pFilter->Release();
				pFilter = NULL;
			}
		}
	}

	//Clear the filter list;
	POSITION pos = FList.GetHeadPosition();
	while (pos){

		FList.Remove(pos);
		pos = FList.GetHeadPosition();
	}

	return S_OK;
}

HRESULT CTSFileSourcePin::ReNewDemux()
{
	// Parse only the existing Mpeg2 Demultiplexer Filter
	// in the filter graph, we do this by looking for filters
	// that implement the IMpeg2Demultiplexer interface while
	// the count is still active.
	CFilterList FList(NAME("MyList"));  // List to hold the downstream peers.
	if (SUCCEEDED(Demux::GetPeerFilters(m_pTSFileSourceFilter, PINDIR_OUTPUT, FList)) && FList.GetHeadPosition())
	{
		IBaseFilter* pFilter = NULL;
		POSITION pos = FList.GetHeadPosition();
		pFilter = FList.Get(pos);
		while (SUCCEEDED(Demux::GetPeerFilters(pFilter, PINDIR_OUTPUT, FList)) && pos)
		{
			pFilter = FList.GetNext(pos);
		}

		pos = FList.GetHeadPosition();

		//Reconnect the Tee filter
		PIN_INFO PinInfo;
		PinInfo.pFilter = NULL;
		IPin *pTPin = NULL;
		if (SUCCEEDED((IPin*)this->ConnectedTo(&pTPin)))
		{
			if (SUCCEEDED(pTPin->QueryPinInfo(&PinInfo)))
			{
				IMpeg2Demultiplexer* muxInterface = NULL;
				if(SUCCEEDED(PinInfo.pFilter->QueryInterface (&muxInterface)))
				{
					muxInterface->Release();
					PinInfo.pFilter->Release();
					PinInfo.pFilter = NULL;
				}
				else
				{
					DisconnectInputPins(PinInfo.pFilter);
					DisconnectOutputPins(PinInfo.pFilter);
					//Reconnect the Tee Filter pins
					IGraphBuilder *pGraphBuilder;
					if(SUCCEEDED((IBaseFilter*)m_pTSFileSourceFilter->GetFilterGraph()->QueryInterface(IID_IGraphBuilder, (void **) &pGraphBuilder)))
					{
						pGraphBuilder->Connect((IPin*)this, pTPin);
						pGraphBuilder->Release();
					}
				}
			}
			pTPin->Release();
		}


		while (pos)
		{
			pFilter = FList.GetNext(pos);
			if(pFilter != NULL)
			{
				// Get an instance of the Demux control interface
				IMpeg2Demultiplexer* muxInterface = NULL;
				if(SUCCEEDED(pFilter->QueryInterface (&muxInterface)))
				{
					muxInterface->Release();

					//Get the Demux Filter Info
					CLSID ClsID;
					pFilter->GetClassID(&ClsID);
					LPWSTR pName = new WCHAR[128];
					FILTER_INFO FilterInfo;
					if (SUCCEEDED(pFilter->QueryFilterInfo(&FilterInfo)))
					{
						memcpy(pName, FilterInfo.achName, 128);
						pFilter->SetSyncSource(NULL);
						IPin *pIPin = NULL;
						IPin *pOPin = NULL;

						if (!PinInfo.pFilter)
						{
							GetPinConnection(pFilter, &pIPin, &pOPin);
							if (pIPin) pIPin->Release();
						}

						FilterInfo.pGraph->RemoveFilter(pFilter);
						FilterInfo.pGraph->Release();
						pFilter->Release();

						//Replace the Demux Filter
						pFilter = NULL;
						if (SUCCEEDED(CoCreateInstance(ClsID, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, reinterpret_cast<void**>(&pFilter))))
						{
							if (SUCCEEDED(FilterInfo.pGraph->AddFilter(pFilter, pName)))
							{
								IMpeg2Demultiplexer* muxInterface = NULL;
								if(SUCCEEDED(pFilter->QueryInterface (&muxInterface)))
									muxInterface->Release();

								if (PinInfo.pFilter)
								{
									RenderOutputPin(PinInfo.pFilter);
									m_pTSFileSourceFilter->OnConnect();
									RenderOutputPins(pFilter);
								}
								else
								{
									pIPin = NULL;
									IPin *pNPin = NULL;
									GetPinConnection(pFilter, &pIPin, &pNPin);
									if (pNPin) pNPin->Release();

									IGraphBuilder *pGraphBuilder;
									if(SUCCEEDED(FilterInfo.pGraph->QueryInterface(IID_IGraphBuilder, (void **) &pGraphBuilder)))
									{
										pGraphBuilder->Connect(pOPin, pIPin);
										m_pTSFileSourceFilter->OnConnect();
										RenderOutputPins(pFilter);
										pGraphBuilder->Release();
										if (pOPin) pOPin->Release();
										if (pIPin) pIPin->Release();
									}
								}
							}
						}
					}
					delete[] pName;
				}
				pFilter->Release();
				pFilter = NULL;
			}
		}
		if (PinInfo.pFilter) PinInfo.pFilter->Release();
	}

	//Clear the filter list;
	POSITION pos = FList.GetHeadPosition();
	while (pos){

		FList.Remove(pos);
		pos = FList.GetHeadPosition();
	}

	return S_OK;
}

HRESULT CTSFileSourcePin::GetPinConnection(IBaseFilter *pFilter, IPin **ppIPin, IPin **ppOPin)
{
	HRESULT hr = E_FAIL;

	if (!pFilter || !ppOPin || !ppIPin)
		return hr;

	IPin *pOPin = NULL;
	IPin *pIPin = NULL;

	FILTER_INFO FilterInfo;
	if (SUCCEEDED(pFilter->QueryFilterInfo(&FilterInfo)))
	{
		PIN_DIRECTION  direction;
		// Enumerate the Filter pins
		CComPtr<IEnumPins> pIEnumPins;
		if (SUCCEEDED(pFilter->EnumPins(&pIEnumPins))){

			ULONG pinfetch(0);
			while(pIEnumPins->Next(1, &pIPin, &pinfetch) == S_OK){

				pIPin->QueryDirection(&direction);
				if(direction == PINDIR_INPUT){

					*ppIPin = pIPin;
					hr = pIPin->ConnectedTo(&pOPin);
					*ppOPin = pOPin;
					FilterInfo.pGraph->Release();
					return hr;
				}
			}
			pIPin->Release();
			pIPin = NULL;
		}
		FilterInfo.pGraph->Release();
	}
	return hr;
}

HRESULT CTSFileSourcePin::RenderOutputPins(IBaseFilter *pFilter)
{
	HRESULT hr = E_FAIL;

	if (!pFilter)
		return hr;

	IPin *pOPin = NULL;
	FILTER_INFO FilterInfo;
	if (SUCCEEDED(pFilter->QueryFilterInfo(&FilterInfo)))
	{
		PIN_DIRECTION  direction;
		// Enumerate the Filter pins
		CComPtr<IEnumPins> pIEnumPins;
		if (SUCCEEDED(pFilter->EnumPins(&pIEnumPins))){

			ULONG pinfetch(0);
			while(pIEnumPins->Next(1, &pOPin, &pinfetch) == S_OK){

				pOPin->QueryDirection(&direction);
				if(direction == PINDIR_OUTPUT)
				{
					IPin *pIPin = NULL;
					pOPin->ConnectedTo(&pIPin);
					if (!pIPin)
					{
						//Render this Filter Output
						IGraphBuilder *pGraphBuilder;
						if(SUCCEEDED(FilterInfo.pGraph->QueryInterface(IID_IGraphBuilder, (void **) &pGraphBuilder)))
						{
							hr = pGraphBuilder->Render(pOPin);
							pGraphBuilder->Release();
						}
					}
					else
						pIPin->Release();
				}
				pOPin->Release();
				pOPin = NULL;
			};
		}
		FilterInfo.pGraph->Release();
	}
	return hr;
}

HRESULT CTSFileSourcePin::RenderOutputPin(IBaseFilter *pFilter)
{
	HRESULT hr = E_FAIL;

	if (!pFilter)
		return hr;

	IPin *pOPin = NULL;
	FILTER_INFO FilterInfo;
	if (SUCCEEDED(pFilter->QueryFilterInfo(&FilterInfo)))
	{
		PIN_DIRECTION  direction;
		// Enumerate the Filter pins
		CComPtr<IEnumPins> pIEnumPins;
		if (SUCCEEDED(pFilter->EnumPins(&pIEnumPins))){

			ULONG pinfetch(0);
			while(pIEnumPins->Next(1, &pOPin, &pinfetch) == S_OK)
			{
				pOPin->QueryDirection(&direction);
				if(direction == PINDIR_OUTPUT)
				{
					IPin *pIPin = NULL;
					pOPin->ConnectedTo(&pIPin);
					if (!pIPin)
					{
						//Render this Filter Output
						IGraphBuilder *pGraphBuilder;
						if(SUCCEEDED(FilterInfo.pGraph->QueryInterface(IID_IGraphBuilder, (void **) &pGraphBuilder)))
						{
							pGraphBuilder->Render(pOPin);
							pGraphBuilder->Release();
							pOPin->Release();
							break;
						}
					}
					else
						pIPin->Release();
				}
				pOPin->Release();
				pOPin = NULL;
			};
		}
		FilterInfo.pGraph->Release();
	}
	return hr;
}