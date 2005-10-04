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

#define USE_EVENT
#ifndef USE_EVENT
#include "Mmsystem.h"
#endif

CTSFileSourcePin::CTSFileSourcePin(LPUNKNOWN pUnk, CTSFileSourceFilter *pFilter, FileReader *pFileReader, PidParser *pPidParser, HRESULT *phr) :
	CSourceStream(NAME("MPEG2 Source Output"), phr, pFilter, L"Out"),
	CSourceSeeking(NAME("MPEG2 Source Output"), pUnk, phr, &m_SeekLock),
	m_pTSFileSourceFilter(pFilter),
	m_pFileReader(pFileReader),
	m_bRateControl(FALSE),
	m_pPidParser(pPidParser)
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

	m_lTSPacketDeliverySize = 188*1000;

	m_DataRate = 10000000;
	m_DataRateTotal = 0;
	m_BitRateCycle = 0;
	for (int i = 0; i < 256; i++) { 
		m_BitRateStore[i] = 0;
	}

	m_pTSBuffer = new CTSBuffer(m_pFileReader, &m_pPidParser->pids, &m_pPidParser->pidArray);

	debugcount = 0;

	m_rtLastCurrentTime = 0;
	m_LastFileSize = 0;

	m_DemuxLock = FALSE;
	m_IntLastStreamTime = 0;
}

CTSFileSourcePin::~CTSFileSourcePin()
{
	delete m_pTSBuffer;
}

STDMETHODIMP CTSFileSourcePin::NonDelegatingQueryInterface( REFIID riid, void ** ppv )
{
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
	if (riid == IID_IMediaSeeking)
    {
        return CSourceSeeking::NonDelegatingQueryInterface( riid, ppv );
    }
	return CSourceStream::NonDelegatingQueryInterface(riid, ppv);
}

HRESULT CTSFileSourcePin::GetMediaType(CMediaType *pmt)
{
	CAutoLock cAutoLock(m_pFilter->pStateLock());
    CheckPointer(pmt, E_POINTER);

	pmt->InitMediaType();
	pmt->SetType      (& MEDIATYPE_Stream);
	pmt->SetSubtype   (& MEDIASUBTYPE_MPEG2_TRANSPORT);
    return S_OK;
}

HRESULT CTSFileSourcePin::GetMediaType(int iPosition, CMediaType *pMediaType)
{
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
    return S_OK;
}


HRESULT CTSFileSourcePin::CheckMediaType(const CMediaType* pType)
{
	CAutoLock cAutoLock(m_pFilter->pStateLock());

	if((MEDIATYPE_Stream == pType->majortype) &&
       (MEDIASUBTYPE_MPEG2_TRANSPORT == pType->subtype))
    {
        return S_OK;
    }

    return S_FALSE;
}

HRESULT CTSFileSourcePin::DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pRequest)
{
    HRESULT hr;
    CAutoLock cAutoLock(m_pFilter->pStateLock());

    CheckPointer(pAlloc, E_POINTER);
    CheckPointer(pRequest, E_POINTER);

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
	HRESULT hr = CBaseOutputPin::CheckConnect(pReceivePin);
	if (SUCCEEDED(hr) && m_pTSFileSourceFilter->get_AutoMode())// && m_pPidParser->m_TStreamID)
	{
		PIN_INFO pInfo;
		if (SUCCEEDED(pReceivePin->QueryPinInfo(&pInfo)))
		{
			TCHAR name[128];
			sprintf(name, "%S", pInfo.achName);

			//Test for a filter with "MPEG-2" on input pin label
			if (strstr(name, "MPEG-2") != NULL)
			{
				pInfo.pFilter->Release();
				return hr;
			}

			// Get an instance of the Demux control interface
			IMpeg2Demultiplexer* muxInterface = NULL;
			if(SUCCEEDED(pInfo.pFilter->QueryInterface (&muxInterface)))
			{
				muxInterface->Release();
				pInfo.pFilter->Release();
				return hr;
			}


			FILTER_INFO pFilterInfo;
			if (SUCCEEDED(pInfo.pFilter->QueryFilterInfo(&pFilterInfo)))
			{
				TCHAR name[128];
				sprintf(name, "%S", pFilterInfo.achName);

				pFilterInfo.pGraph->Release();
				pInfo.pFilter->Release();

				//Test for an infinite tee filter
				if (strstr(name, "Tee") != NULL)
					return hr;
				else if (strstr(name, "Flow") != NULL)
					return hr;
			}

		}
		return E_FAIL;
	}
	return hr;
}

HRESULT CTSFileSourcePin::CompleteConnect(IPin *pReceivePin)
{
	HRESULT hr = CBaseOutputPin::CompleteConnect(pReceivePin);
	if (SUCCEEDED(hr))
	{
		m_pTSFileSourceFilter->OnConnect();
		m_rtDuration = m_pPidParser->pids.dur;
		m_rtStop = m_rtDuration;
		m_DataRate = m_pPidParser->pids.bitrate;
	}

	return hr;
}

HRESULT CTSFileSourcePin::BreakConnect()
{
	return CBaseOutputPin::BreakConnect();
}

HRESULT CTSFileSourcePin::FillBuffer(IMediaSample *pSample)
{
	CheckPointer(pSample, E_POINTER);

	if (m_pFileReader->IsFileInvalid())
	{
		int count = 0;
		__int64	fileSize = 0;
		m_pFileReader->GetFileSize(&fileSize);
		//If this a file start then return null.
		while(fileSize < 500000 && count < 10)
		{
			Sleep(100);
			m_pFileReader->GetFileSize(&fileSize);
			count++;
		}

		CheckPointer(pSample, E_POINTER);
		if (m_pFileReader->IsFileInvalid())
			return NOERROR;
	}

	if (m_bSeeking)
	{
		return NOERROR;
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

	hr = m_pTSBuffer->Require(lDataLength);
	if (FAILED(hr))
	{
		return S_FALSE;
	}


//*************************************************************************************************
//Old Capture format Additions

	__int64 firstPass = m_llPrevPCR;

	if (!m_pPidParser->pids.pcr) {

		__int64 intTimeDelta = 0;
		CRefTime cTime;
		m_pTSFileSourceFilter->StreamTime(cTime);
		if (m_IntLastStreamTime > 20000000) {

			intTimeDelta = (__int64)(REFERENCE_TIME(cTime) - m_IntLastStreamTime);
			__int64 bitrate = ((__int64)lDataLength * (__int64)80000000) / intTimeDelta;
			AddBitRateForAverage(bitrate);
			m_pPidParser->pids.bitrate = m_DataRate;
		}

		{
			CAutoLock lock(&m_SeekLock);
			m_rtStart = REFERENCE_TIME(cTime) + m_rtLastSeekStart + intTimeDelta;
		}

		//Read from buffer
		m_pTSBuffer->DequeFromBuffer(pData, lDataLength);

		if (!m_IntLastStreamTime)
		{
			ULONG pos = 0;
			LoadPATPacket(pData+pos, m_pPidParser->m_TStreamID, m_pPidParser->pids.sid, m_pPidParser->pids.pmt);
			pos += 188;

			if (!m_pPidParser->pids.pmt) 
				LoadPMTPacket(pData+pos,
							  m_pPidParser->pids.pcr,
							  m_pPidParser->pids.vid,
							  m_pPidParser->pids.aud,
							  m_pPidParser->pids.aud2,
							  m_pPidParser->pids.ac3,
							  m_pPidParser->pids.ac3_2,
							  m_pPidParser->pids.txt);
		}


		m_IntLastStreamTime = REFERENCE_TIME(cTime);
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
			m_lNextPCRByteOffset = m_lPrevPCRByteOffset + 188;
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

				TCHAR sz[60];
				wsprintf(sz, TEXT("bitrate %i\n"), bitrate);
				Debug(sz);
			}
		}
		else
		{
			Debug(TEXT("Failed to find next PCR\n"));
		}
	}

	//Calculate PCR
	__int64 pcrStart;
	if (m_lByteDelta > 0)
	{
		pcrStart = m_llPrevPCR - (__int64)((__int64)(m_llPCRDelta * (__int64)m_lPrevPCRByteOffset) / (__int64)m_lByteDelta);
	}
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
		IReferenceClock* pReferenceClock = NULL;
		hr = GetReferenceClock(&pReferenceClock);
		if (pReferenceClock != NULL)
		{
			pReferenceClock->GetTime(&m_rtStartTime);
			pReferenceClock->Release();
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
		IReferenceClock* pReferenceClock = NULL;
		hr = GetReferenceClock(&pReferenceClock);
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
			pReferenceClock->Release();
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
		long duration1 = m_pPidParser->pids.dur / (__int64)10000000;
		long duration2 = m_pPidParser->pids.dur % (__int64)10000000;
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
//PrintTime("FillBuffer", (__int64)m_rtLastCurrentTime, 10000);

//*************************************************************************************************
//Old Capture format Additions

	//If no TSID then we won't have a PAT so create one
	if (!m_pPidParser->m_TStreamID && firstPass == -1)
	{
		ULONG pos = 0; 
		REFERENCE_TIME pcrPos = -1;

		//Get the first occurance of a timing packet
		hr = S_OK;
		hr = m_pPidParser->FindNextPCR(pData, lDataLength, &m_pPidParser->pids, &pcrPos, &pos, 1); //Get the PCR
		//If we have one then load our PAT, PMT & PCR Packets	
		if(pcrPos && hr == S_OK) {
			//Check if we can back up before timing packet	
			if (pos > 188*2) {
			//back up before timing packet
				pos-= 188*2;	//shift back to before pat & pmt packet
			}
		}
		else {
			// if no timing packet found then load the PAT at the start of the buffer
			pos=0;
		}

		LoadPATPacket(pData + pos, m_pPidParser->m_TStreamID, m_pPidParser->pids.sid, m_pPidParser->pids.pmt);
		pos+= 188;	//shift to the pmt packet

		//Also insert a pmt if we don't have one already
		if (!m_pPidParser->pids.pmt){ 
			LoadPMTPacket(pData + pos,
				  m_pPidParser->pids.pcr - m_pPidParser->pids.opcr,
				  m_pPidParser->pids.vid,
				  m_pPidParser->pids.aud,
				  m_pPidParser->pids.aud2,
				  m_pPidParser->pids.ac3,
				  m_pPidParser->pids.ac3_2,
				  m_pPidParser->pids.txt);
		}
		else {
			//load another PAT instead
			LoadPATPacket(pData + pos, m_pPidParser->m_TStreamID, m_pPidParser->pids.sid, m_pPidParser->pids.pmt);
		}

		pos+= 188;	//shift to the pcr packet

		//Load in our own pcr if we need to
		if (m_pPidParser->pids.opcr) {
			LoadPCRPacket(pData + pos, m_pPidParser->pids.pcr - m_pPidParser->pids.opcr, pcrPos);
		}

	}
/*
	//If we need to insert a new PCR packet
	if (m_pPidParser->pids.opcr) {

		ULONG pos = 0, lastpos = 0;
		REFERENCE_TIME pcrPos = -1;
		hr = S_OK;
		while (hr == S_OK) {
			//Look for a timing packet
			hr = m_pPidParser->FindNextOPCR(pData, lDataLength, &m_pPidParser->pids, &pcrPos, &pos, 1); //Get the PCR
			if (pcrPos) {
				//Insert our new PCR Packet
				LoadPCRPacket(pData + pos, m_pPidParser->pids.pcr - m_pPidParser->pids.opcr, pcrPos);
//PrintTime("Insert PCR packet", (__int64) pcrPos, 90);
//				break;
			}
			pos += 188;

			if (pos > lastpos + 10*188 && pos + 188 < lDataLength){
//PrintTime("delta PCR packet", (__int64) m_llPCRDelta, 90);
				__int64 offset = (__int64)(m_llPCRDelta *(__int64)(pos - lastpos) / (__int64)m_lByteDelta);
//PrintTime("offset PCR packet", (__int64) offset, 90);
				LoadPCRPacket(pData + pos, m_pPidParser->pids.pcr - m_pPidParser->pids.opcr, pcrPos + offset);
				pos += 188;
				lastpos = pos;
			}
		};
	}
*/
//*************************************************************************************************

	return NOERROR;
}

HRESULT CTSFileSourcePin::OnThreadStartPlay( )
{
	m_rtLastCurrentTime = (REFERENCE_TIME)((REFERENCE_TIME)timeGetTime() * (REFERENCE_TIME)10000);
	m_llPrevPCR = -1;
	m_DataRate = m_pPidParser->pids.bitrate;
	debugcount = 0;

	//Check if file is being recorded
	if(m_pFileReader->get_FileSize() < 2001000)
	{
		if (m_pTSFileSourceFilter->RefreshPids() == S_OK)
		{
			m_pTSFileSourceFilter->LoadPgmReg();
			DeliverBeginFlush();
			DeliverEndFlush();
			SetDuration(m_pPidParser->pids.dur);
		}
	}

	CAutoLock lock(&m_SeekLock);

    DeliverNewSegment(m_rtStart, m_rtStop, 1.0 );
	m_rtLastSeekStart = REFERENCE_TIME(m_rtStart);
	m_rtLastCurrentTime = (REFERENCE_TIME)((REFERENCE_TIME)timeGetTime() * (REFERENCE_TIME)10000);

	return CSourceStream::OnThreadStartPlay( );
}

HRESULT CTSFileSourcePin::Run(REFERENCE_TIME tStart)
{
	CAutoLock fillLock(&m_FillLock);
	CAutoLock seekLock(&m_SeekLock);

	CBasePin::m_tStart = tStart;
	m_rtLastSeekStart = REFERENCE_TIME(m_rtStart);
	m_rtLastCurrentTime = (REFERENCE_TIME)((REFERENCE_TIME)timeGetTime() * (REFERENCE_TIME)10000);

	if (!m_bSeeking && !m_DemuxLock)
	{	
		IReferenceClock *pClock = NULL;
		GetReferenceClock(&pClock);
		SetDemuxClock(pClock);
		m_pTSFileSourceFilter->NotifyEvent(EC_CLOCK_UNSET, NULL, NULL);
	}

	return CBaseOutputPin::Run(tStart);

}

STDMETHODIMP CTSFileSourcePin::GetCurrentPosition(LONGLONG *pCurrent)
{
	if (pCurrent)
	{
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
		CRefTime cTime;
		m_pTSFileSourceFilter->StreamTime(cTime);
		*pCurrent = (REFERENCE_TIME)(m_rtLastSeekStart + REFERENCE_TIME(cTime));
		REFERENCE_TIME current;
		return CSourceSeeking::GetPositions(&current, pStop);
	}
	return CSourceSeeking::GetPositions(pCurrent, pStop);
}

STDMETHODIMP CTSFileSourcePin::SetPositions(LONGLONG *pCurrent, DWORD CurrentFlags
			     , LONGLONG *pStop, DWORD StopFlags)
{
	if (pCurrent)
	{
		WORD readonly = 0;
		m_pFileReader->get_ReadOnly(&readonly);
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
			CAutoLock lock(&m_SeekLock);
			rtCurrent += m_rtStart;
			CurrentFlags -= AM_SEEKING_RelativePositioning; //Remove relative flag
			CurrentFlags += AM_SEEKING_AbsolutePositioning; //Replace with absoulute flag
		}

		if (CurrentFlags & AM_SEEKING_PositioningBitsMask)
		{
			CAutoLock lock(&m_SeekLock);
			m_rtStart = rtCurrent;
		}

		if (!(CurrentFlags & AM_SEEKING_NoFlush) && (CurrentFlags & AM_SEEKING_PositioningBitsMask))
		{
			if (ThreadExists())
			{	
				m_bSeeking = TRUE;
				if(m_pTSFileSourceFilter->IsActive() && !m_DemuxLock)
					SetDemuxClock(NULL);

				CAutoLock fillLock(&m_FillLock);
				DeliverBeginFlush();
				CSourceStream::Stop();
				m_DataRate = m_pPidParser->pids.bitrate;
				m_llPrevPCR = -1;
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
				CAutoLock lock(&m_SeekLock);
				return CSourceSeeking::SetPositions(&rtCurrent, CurrentFlags, pStop, StopFlags);
			}
		}
		return CSourceSeeking::SetPositions(&rtCurrent, CurrentFlags, pStop, StopFlags);
	}

	return CSourceSeeking::SetPositions(pCurrent, CurrentFlags, pStop, StopFlags);
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

void CTSFileSourcePin::UpdateFromSeek(BOOL updateStartPosition)
{
	if (ThreadExists())
	{	
		m_bSeeking = TRUE;
		CAutoLock fillLock(&m_FillLock);
		CAutoLock seekLock(&m_SeekLock);
		SetDemuxClock(NULL);
		DeliverBeginFlush();
		m_llPrevPCR = -1;
		if (updateStartPosition == TRUE)
		{
			m_pTSBuffer->Clear();
			m_pTSFileSourceFilter->FileSeek(m_rtStart);
			m_rtLastSeekStart = REFERENCE_TIME(m_rtStart);
		}
		DeliverEndFlush();
	}
	m_bSeeking = FALSE;
}


HRESULT CTSFileSourcePin::SetAccuratePos(REFERENCE_TIME seektime)
{

//***********************************************************************************************
//Old Capture format Additions

	if (!m_pPidParser->pids.pcr) {
			// Revert to old method
			// shifting right by 14 rounds the seek and duration time down to the
			// nearest multiple 16.384 ms. More than accurate enough for our seeks.
		__int64 filelength = 0;
		m_pFileReader->GetFileSize(&filelength);
		__int64 nFileIndex = filelength * (__int64)(seektime>>14) / (__int64)(m_pPidParser->pids.dur>>14);

		if (nFileIndex < 300000)
			nFileIndex = 300000; //Skip head of file
		m_pFileReader->SetFilePointer(nFileIndex, FILE_BEGIN);
		return S_OK;
	}
//***********************************************************************************************

//PrintTime("seekin", (__int64) seektime, 10000);

	HRESULT hr;
	ULONG pos;
	__int64 pcrEndPos;
	ULONG ulBytesRead = 0;
	long lDataLength = 1000000;
	PBYTE pData = new BYTE[4000000];
	__int64 nFileIndex = 0;
	__int64 filelength = 0;
	m_pFileReader->GetFileSize(&filelength);

	__int64 pcrByteRate = (__int64)((__int64)m_DataRate / (__int64)720);
//PrintTime("our pcr Byte Rate", (__int64)pcrByteRate, 90);

	__int64	pcrDuration = (__int64)((__int64)((__int64)m_pPidParser->pids.dur * (__int64)9) / (__int64)1000);
//PrintTime("our pcr duration", (__int64)pcrDuration, 90);

	//Get estimated time of seek as pcr 
	__int64	pcrDeltaSeekTime = (__int64)((__int64)((__int64)seektime * (__int64)9) / (__int64)1000);
//PrintTime("our pcr Delta SeekTime", (__int64)pcrDeltaSeekTime, 90);

//This is where we create a pcr time relative to the current stream position
//
	//Set Pointer to end of file to get end pcr
	m_pFileReader->SetFilePointer((__int64) - lDataLength, FILE_END);
	m_pFileReader->Read(pData, lDataLength, &ulBytesRead);
	pos = ulBytesRead - 188;
	hr = S_OK;
	hr = PidParser::FindNextPCR(pData, ulBytesRead, &m_pPidParser->pids, &pcrEndPos, &pos, -1); //Get the PCR
	__int64	pcrSeekTime = pcrDeltaSeekTime + (__int64)(pcrEndPos - pcrDuration);
//PrintTime("our pcr end time for reference", (__int64)pcrEndPos, 90);

	//Test if we have a pcr or if the pcr is less than rollover time
	if (FAILED(hr) || pcrSeekTime < 0) {
//PrintTime("get lastpcr failed now using first pcr", (__int64)m_pPidParser->pids.start, 90);
	
		//Set seektime to position relative to first pcr
		pcrSeekTime = m_pPidParser->pids.start + pcrDeltaSeekTime;

		//test if pcr time is now larger than file size
		if (pcrSeekTime > pcrDuration) {
//PrintTime("get first pcr failed as well SEEK ERROR AT START", (__int64) pcrSeekTime, 90);

			// Revert to old method
			// shifting right by 14 rounds the seek and duration time down to the
			// nearest multiple 16.384 ms. More than accurate enough for our seeks.
			nFileIndex = filelength * (__int64)(seektime>>14) / (__int64)(m_pPidParser->pids.dur>>14);

			if (nFileIndex < 300000)
				nFileIndex = 300000; //Skip head of file
			m_pFileReader->SetFilePointer(nFileIndex, FILE_BEGIN);
			delete[] pData;
			return S_OK;
		}
	}

//PrintTime("our predicted pcr position for seek", (__int64)pcrSeekTime, 90);

	//create our predicted file pointer position
	nFileIndex = (pcrDeltaSeekTime / (__int64)1000) * pcrByteRate;

	// set back so we can get last batch of data if at end of file
	if ((__int64)(nFileIndex + (__int64)lDataLength) > filelength)
		nFileIndex = (__int64)(filelength - (__int64)lDataLength);

	if (nFileIndex < 0)
		nFileIndex = 0;

	//Set Pointer to the predicted file position to get end pcr
	m_pFileReader->SetFilePointer(nFileIndex, FILE_BEGIN);
	m_pFileReader->Read(pData, lDataLength, &ulBytesRead);
	__int64 pcrPos;
	pos = 0;

	hr = S_OK;
	hr = PidParser::FindNextPCR(pData, ulBytesRead, &m_pPidParser->pids, &pcrPos, &pos, 1);
	nFileIndex += (__int64)pos;

	//compare our predicted file position to our predicted seektime and adjust file pointer
	if (pcrPos > pcrSeekTime) {
		nFileIndex -= (__int64)((__int64)((__int64)(pcrPos - pcrSeekTime) / (__int64)1000) * (__int64)pcrByteRate);
//PrintTime("seek---------", (__int64) pcrPos, 90);
	}
	else if (pcrSeekTime > pcrPos) {
			nFileIndex += (__int64)((__int64)((__int64)(pcrSeekTime - pcrPos) / (__int64)1000) * (__int64)pcrByteRate);
//PrintTime("seek+++++++++++++", (__int64) pcrPos, 90);
	}

	lDataLength = 4000000;

	//Now we are close so setup the a +/- 2meg buffer
	nFileIndex -= (__int64)(lDataLength / 2); //Centre buffer

	// set buffer start back from EOF so we can get last batch of data
	if ((nFileIndex + lDataLength) > filelength)
		nFileIndex = (__int64)(filelength - (__int64)lDataLength);

	//shift buffer start to beginning of file if needed
	if (nFileIndex < 0)
		nFileIndex = 0;

	m_pFileReader->SetFilePointer(nFileIndex, FILE_BEGIN);
	ulBytesRead = 0;
	m_pFileReader->Read(pData, lDataLength, &ulBytesRead);

	pcrPos = 0;
	pos = ulBytesRead / 2;//buffer the centre search

	hr = S_OK;		
	while (pcrSeekTime > pcrPos && hr == S_OK) {
		//Seek forwards
		pos += 188;
		hr = m_pPidParser->FindNextPCR(pData, ulBytesRead, &m_pPidParser->pids, &pcrPos, &pos, 1); //Get the PCR
//PrintTime("seekfwdloop", (__int64) pcrPos, 90);
	}
//PrintTime("seekfwd", (__int64) pcrPos, 90);

	//Store this pos for later use
	__int64 posSave = 0;
	if (SUCCEEDED(hr))
		posSave = pos; //Save this position if where past seek value
	else
		pos = ulBytesRead;//set buffer to end for backward search

	pos -= 188;
	pcrPos -= 1; 

	hr = S_OK;		
	while (pcrPos > pcrSeekTime && hr == S_OK) {
		//Seek backwards
		hr = m_pPidParser->FindNextPCR(pData, ulBytesRead, &m_pPidParser->pids, &pcrPos, &pos, -1); //Get the PCR
		pos -= 188;
//PrintTime("seekbackloop", (__int64) pcrPos, 90);
	}
//PrintTime("seekback", (__int64) pcrPos, 90);

	// if we have backed up to correct pcr
	if (SUCCEEDED(hr)) {
		//Get mid position between pcr's
		if (posSave) {
			//set mid way
			posSave -= (__int64)pos;
			pos += (ULONG)((__int64)posSave /(__int64)2);
		}

		//Set pointer to locale
		nFileIndex += (__int64)pos;

//PrintTime("seekend", (__int64) pcrPos, 90);
		if (nFileIndex < 300000)
			nFileIndex = 300000;
		else if (nFileIndex > filelength)
			nFileIndex = filelength - 100000;
	}
	else
	{
		// Revert to old method
		// shifting right by 14 rounds the seek and duration time down to the
		// nearest multiple 16.384 ms. More than accurate enough for our seeks.
		nFileIndex = filelength * (__int64)(seektime>>14) / (__int64)(m_pPidParser->pids.dur>>14);

//PrintTime("SEEK ERROR AT END", (__int64) pcrSeekTime, 90);
		if (nFileIndex < 300000)
			nFileIndex = 300000;
		else if (nFileIndex > filelength)
			nFileIndex = filelength - 100000;
	}

	m_pFileReader->SetFilePointer(nFileIndex, FILE_BEGIN);

	delete[] pData;

	return S_OK;
	
}

HRESULT CTSFileSourcePin::UpdateDuration(FileReader *pFileReader)
{
	HRESULT hr = E_FAIL;

#define EC_DVB_DURATIONCHANGE  0x41E


//***********************************************************************************************
//Old Capture format Additions

	if(!m_pPidParser->pids.pcr)
	{
		hr = S_FALSE;

		if (m_bSeeking)
			return hr;

		REFERENCE_TIME rtCurrentTime = (REFERENCE_TIME)((REFERENCE_TIME)timeGetTime() * (REFERENCE_TIME)10000);
		REFERENCE_TIME rtStop = 0;

		__int64	fileSize = 0;
		pFileReader->GetFileSize(&fileSize);
		 __int64 calcDuration = (__int64)(fileSize / (__int64)((__int64)m_DataRate / (__int64)8000));
		calcDuration = (__int64)(calcDuration * (__int64)10000);
		if ((__int64)m_pPidParser->pids.dur < calcDuration && m_BitRateCycle < 50)
		{
			if (!m_bSeeking)
			{
				m_pPidParser->pids.dur = (REFERENCE_TIME)calcDuration;
				for (int i = 0; i < m_pPidParser->pidArray.Count(); i++)
				{
					m_pPidParser->pidArray[i].dur = m_pPidParser->pids.dur;
				}
				m_rtDuration = m_pPidParser->pids.dur;
				m_rtStop = m_pPidParser->pids.dur;
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
			//Send a Custom Duration update for applications. 
			if (!m_bSeeking)
				m_pTSFileSourceFilter->NotifyEvent(EC_DVB_DURATIONCHANGE, NULL, NULL);

			Sleep(1000);
		}
		return hr;
	}

//***********************************************************************************************


	WORD readonly = 0;
	pFileReader->get_ReadOnly(&readonly);
	if (readonly)
	{
		hr = S_FALSE;

		if (m_bSeeking)
			return hr;

		REFERENCE_TIME rtCurrentTime = (REFERENCE_TIME)((REFERENCE_TIME)timeGetTime() * (REFERENCE_TIME)10000);
		REFERENCE_TIME rtStop = 0;

		bool secondDelay = false;
		//Do a quick parse of duration if seeking
		if (m_DataRate > 0) {

			__int64	fileSize = 0;
			pFileReader->GetFileSize(&fileSize);
			//check for duration every second of size change
			if((m_LastFileSize + (__int64)((__int64)m_DataRate / (__int64)8)) < fileSize) {

				m_LastFileSize = fileSize;
				 __int64 calcDuration = (REFERENCE_TIME)(fileSize / (__int64)((__int64)m_DataRate / (__int64)8000));
				calcDuration = (REFERENCE_TIME)(calcDuration * (__int64)10000);
				 __int64 deltaDuration = calcDuration - m_pPidParser->pids.dur;
				m_pPidParser->pids.dur = calcDuration;
				m_pPidParser->pids.end += (__int64)((__int64)((__int64)deltaDuration * (__int64)9) / (__int64)1000);
				for (int i = 0; i < m_pPidParser->pidArray.Count(); i++)
				{
					m_pPidParser->pidArray[i].dur = m_pPidParser->pids.dur;
					m_pPidParser->pidArray[i].end += (__int64)((__int64)((__int64)deltaDuration * (__int64)9) / (__int64)1000);
				}
				secondDelay = true;
			}
		}
		else if ((REFERENCE_TIME)(m_rtLastCurrentTime + (REFERENCE_TIME)10000000) < rtCurrentTime)
		{
			if (!m_bSeeking)
				m_pPidParser->RefreshDuration(TRUE, pFileReader);

			secondDelay = true;
		}

		if (secondDelay)
		{
			if (!m_bSeeking)
			{
				m_rtDuration = m_pPidParser->pids.dur;
				m_rtStop = m_pPidParser->pids.dur;
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
			//Send a Custom Duration update for applications. 
			if (!m_bSeeking)
				m_pTSFileSourceFilter->NotifyEvent(EC_DVB_DURATIONCHANGE, NULL, NULL);
		}

	}
	else
		return S_FALSE;

	return S_OK;
}

HRESULT CTSFileSourcePin::SetDuration(REFERENCE_TIME duration)
{
	CAutoLock fillLock(&m_FillLock);
	CAutoLock lock(&m_SeekLock);

	m_rtDuration = duration;
	m_rtStop = m_rtDuration;

    return S_OK;
}

BOOL CTSFileSourcePin::get_RateControl()
{
	return m_bRateControl;
}

void CTSFileSourcePin::set_RateControl(BOOL bRateControl)
{
	m_bRateControl = bRateControl;
}


HRESULT CTSFileSourcePin::GetReferenceClock(IReferenceClock **pClock)
{
	HRESULT hr;

	FILTER_INFO	filterInfo;
	if (SUCCEEDED(m_pTSFileSourceFilter->QueryFilterInfo(&filterInfo)) && filterInfo.pGraph != NULL)
	{
		// Get IMediaFilter interface
		IMediaFilter* pMediaFilter = NULL;
		hr = filterInfo.pGraph->QueryInterface(IID_IMediaFilter, (void**)&pMediaFilter);
		filterInfo.pGraph->Release();

		if (pMediaFilter)
		{
			// Get IReferenceClock interface
			hr = pMediaFilter->GetSyncSource(pClock);
			pMediaFilter->Release();
			return S_OK;
		}
	}
	return E_FAIL;
}

__int64 CTSFileSourcePin::ConvertPCRtoRT(__int64 pcrtime)
{
	return (__int64)(pcrtime / 9) * 1000;
}

HRESULT CTSFileSourcePin::FindNextPCR(__int64 *pcrtime, long *byteOffset, long maxOffset)
{
	HRESULT hr = E_FAIL;

	long bytesToRead = m_lTSPacketDeliverySize + 188;	//Read an extra packet to make sure we don't miss a PCR that spans a gap.
	BYTE *pData = new BYTE[bytesToRead];

	while (*byteOffset < maxOffset)
	{
		bytesToRead = min(bytesToRead, maxOffset-*byteOffset);

		hr = m_pTSBuffer->ReadFromBuffer(pData, bytesToRead, *byteOffset);
		if (FAILED(hr))
			break;

		ULONG pos = 0;
		hr = PidParser::FindFirstPCR(pData, bytesToRead, &m_pPidParser->pids, pcrtime, &pos);
		if (SUCCEEDED(hr))
		{
			*byteOffset += pos;
			break;
		}

		*byteOffset += m_lTSPacketDeliverySize;
	}

	delete[] pData;
	return hr;
}


HRESULT CTSFileSourcePin::FindPrevPCR(__int64 *pcrtime, long *byteOffset)
{
	HRESULT hr = E_FAIL;

	long bytesToRead = m_lTSPacketDeliverySize + 188; //Read an extra packet to make sure we don't miss a PCR that spans a gap.
	BYTE *pData = new BYTE[bytesToRead];

	while (*byteOffset > 0)
	{
		bytesToRead = min(m_lTSPacketDeliverySize, *byteOffset);
		*byteOffset -= bytesToRead;

		bytesToRead += 188;

		hr = m_pTSBuffer->ReadFromBuffer(pData, bytesToRead, *byteOffset);
		if (FAILED(hr))
			break;

		ULONG pos = 0;
		hr = PidParser::FindLastPCR(pData, bytesToRead, &m_pPidParser->pids, pcrtime, &pos);
		if (SUCCEEDED(hr))
		{
			*byteOffset += pos;
			break;
		}

		//*byteOffset -= m_lTSPacketDeliverySize;
	}

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

void CTSFileSourcePin::PrintTime(const char* lstring, __int64 value, __int64 divider)
{

	TCHAR sz[100];
	long ms = (long)(value / divider); 
	long secs = ms / 1000;
	long mins = secs / 60;
	long hours = mins / 60;
	ms -= (secs*(__int64)1000);
	secs -= (mins*(__int64)60);
	mins -= (hours*(__int64)60);
	sprintf(sz, TEXT("Time Position %02i hrs %02i mins %02i.%03i secs"), hours, mins, secs, ms);
	MessageBox(NULL, sz, lstring, NULL);
}


HRESULT CTSFileSourcePin::SetDemuxClock(IReferenceClock *pClock)
{
	// Parse only the existing Mpeg2 Demultiplexer Filter
	// in the filter graph, we do this by looking for filters
	// that implement the IMpeg2Demultiplexer interface while
	// the count is still active.
	CFilterList FList(NAME("MyList"));  // List to hold the downstream peers.
	if (SUCCEEDED(GetPeerFilters(m_pTSFileSourceFilter, PINDIR_OUTPUT, FList)) && FList.GetHeadPosition())
	{
		IBaseFilter* pFilter = NULL;
		POSITION pos = FList.GetHeadPosition();
		pFilter = FList.Get(pos);
		while (SUCCEEDED(GetPeerFilters(pFilter, PINDIR_OUTPUT, FList)) && pos)
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
					if (m_pPidParser->pids.pcr && m_pTSFileSourceFilter->get_AutoMode()) // && !m_pPidParser->pids.opcr) 
//***********************************************************************************************
						pFilter->SetSyncSource(pClock);
					muxInterface->Release();
				}
				pFilter->Release();
				pFilter = NULL;
			}
		}
	}
	return S_OK;
}

// Find all the immediate upstream or downstream peers of a filter.
HRESULT CTSFileSourcePin::GetPeerFilters(
    IBaseFilter *pFilter, // Pointer to the starting filter
    PIN_DIRECTION Dir,    // Direction to search (upstream or downstream)
    CFilterList &FilterList)  // Collect the results in this list.
{
    if (!pFilter) return E_POINTER;

    IEnumPins *pEnum = 0;
    IPin *pPin = 0;
    HRESULT hr = pFilter->EnumPins(&pEnum);
    if (FAILED(hr)) return hr;
    while (S_OK == pEnum->Next(1, &pPin, 0))
    {
        // See if this pin matches the specified direction.
        PIN_DIRECTION thisPinDir;
        hr = pPin->QueryDirection(&thisPinDir);
        if (FAILED(hr))
        {
            // Something strange happened.
            hr = E_UNEXPECTED;
            pPin->Release();
            break;
        }
        if (thisPinDir == Dir)
        {
            // Check if the pin is connected to another pin.
            IPin *pPinNext = 0;
            hr = pPin->ConnectedTo(&pPinNext);
            if (SUCCEEDED(hr))
            {
                // Get the filter that owns that pin.
                PIN_INFO PinInfo;
                hr = pPinNext->QueryPinInfo(&PinInfo);
                pPinNext->Release();
                if (FAILED(hr) || (PinInfo.pFilter == NULL))
                {
                    // Something strange happened.
                    pPin->Release();
                    pEnum->Release();
                    return E_UNEXPECTED;
                }
                // Insert the filter into the list.
                AddFilterUnique(FilterList, PinInfo.pFilter);
                PinInfo.pFilter->Release();
            }
        }
        pPin->Release();
    }
    pEnum->Release();
    return S_OK;
}

void CTSFileSourcePin::AddFilterUnique(CFilterList &FilterList, IBaseFilter *pNew)
{
    if (pNew == NULL) return;

    POSITION pos = FilterList.GetHeadPosition();
    while (pos)
    {
        IBaseFilter *pF = FilterList.GetNext(pos);
        if (IsEqualObject(pF, pNew))
        {
            return;
        }
    }
    pNew->AddRef();  // The caller must release everything in the list.
    FilterList.AddTail(pNew);
}

// Get the first upstream or downstream filter
HRESULT CTSFileSourcePin::GetNextFilter(
    IBaseFilter *pFilter, // Pointer to the starting filter
    PIN_DIRECTION Dir,    // Direction to search (upstream or downstream)
    IBaseFilter **ppNext) // Receives a pointer to the next filter.
{
    if (!pFilter || !ppNext) return E_POINTER;

    IEnumPins *pEnum = 0;
    IPin *pPin = 0;
    HRESULT hr = pFilter->EnumPins(&pEnum);
    if (FAILED(hr)) return hr;
    while (S_OK == pEnum->Next(1, &pPin, 0))
    {
        // See if this pin matches the specified direction.
        PIN_DIRECTION thisPinDir;
        hr = pPin->QueryDirection(&thisPinDir);
        if (FAILED(hr))
        {
            // Something strange happened.
            hr = E_UNEXPECTED;
            pPin->Release();
            break;
        }
        if (thisPinDir == Dir)
        {
            // Check if the pin is connected to another pin.
            IPin *pPinNext = 0;
            hr = pPin->ConnectedTo(&pPinNext);
            if (SUCCEEDED(hr))
            {
                // Get the filter that owns that pin.
                PIN_INFO PinInfo;
                hr = pPinNext->QueryPinInfo(&PinInfo);
                pPinNext->Release();
                pPin->Release();
                pEnum->Release();
                if (FAILED(hr) || (PinInfo.pFilter == NULL))
                {
                    // Something strange happened.
                    return E_UNEXPECTED;
                }
                // This is the filter we're looking for.
                *ppNext = PinInfo.pFilter; // Client must release.
                return S_OK;
            }
        }
        pPin->Release();
    }
    pEnum->Release();
    // Did not find a matching filter.
    return E_FAIL;
}


