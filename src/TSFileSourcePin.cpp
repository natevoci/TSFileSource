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

#define USE_EVENT
#ifndef USE_EVENT
#include "Mmsystem.h"
#endif

CTSFileSourcePin::CTSFileSourcePin(LPUNKNOWN pUnk, CTSFileSourceFilter *pFilter, FileReader *pFileReader, PidParser *pPidParser, HRESULT *phr) :
	CSourceStream(NAME("MPEG2 Source Output"), phr, pFilter, L"Out"),
	CSourceSeeking(NAME("MPEG2 Source Output"), pUnk, phr, &m_SeekLock ),
	m_pTSFileSourceFilter(pFilter),
	m_pFileReader(pFileReader),
	m_bRateControl(FALSE),
	m_pPidParser(pPidParser)
{
	m_dwSeekingCaps =	
						AM_SEEKING_CanSeekForwards  |
						AM_SEEKING_CanGetStopPos    |
						AM_SEEKING_CanGetDuration   |
						AM_SEEKING_CanSeekAbsolute;

	m_bSeeking = FALSE;

	m_rtLastSeekStart = 0;

	m_llBasePCR = -1;
	m_llNextPCR = -1;
	m_llPrevPCR = -1;
	m_lNextPCRByteOffset = 0;
	m_lPrevPCRByteOffset = 0;

	m_lTSPacketDeliverySize = 188*1000;

	m_DataRate = 0;
	m_DataRateTotal = 0;
	m_BitRateCycle = 0;

	m_pTSBuffer = new CTSBuffer(m_pFileReader, &m_pPidParser->pids, &m_pPidParser->pidArray);

	debugcount = 0;
}

CTSFileSourcePin::~CTSFileSourcePin()
{
	delete m_pTSBuffer;
}

STDMETHODIMP CTSFileSourcePin::NonDelegatingQueryInterface( REFIID riid, void ** ppv )
{
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
			m_lNextPCRByteOffset = m_lPrevPCRByteOffset+188;
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
			__int64 bitrate = ((__int64)m_lByteDelta * (__int64)80000000) / (__int64)ConvertPCRtoRT(m_llPCRDelta);
			AddBitRateForAverage(bitrate);

			TCHAR sz[60];
			wsprintf(sz, TEXT("bitrate %i\n"), bitrate);
			Debug(sz);
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
		pcrStart = m_llPrevPCR - (m_llPCRDelta * m_lPrevPCRByteOffset / m_lByteDelta);
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
	CRefTime rtStart = ConvertPCRtoRT(pcrStart);

	{
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

	WORD readonly = 0;
	m_pFileReader->get_ReadOnly(&readonly);
	if (readonly)
	{
		m_pPidParser->RefreshDuration(TRUE, m_pFileReader);
		SetDuration(m_pPidParser->pids.dur);
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

	return NOERROR;
}

HRESULT CTSFileSourcePin::OnThreadStartPlay( )
{
	m_llPrevPCR = -1;
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

	if (m_pPidParser->pidArray.Count() >= 2)
	{
//		m_pTSFileSourceFilter->ShowFilterProperties();
	}

	CAutoLock lock(&m_SeekLock);

    DeliverNewSegment(m_rtStart, m_rtStop, 1.0 );

	return CSourceStream::OnThreadStartPlay( );
}

HRESULT CTSFileSourcePin::Run(REFERENCE_TIME tStart)
{
	return CBaseOutputPin::Run(tStart);
}


HRESULT CTSFileSourcePin::ChangeStart()
{
	UpdateFromSeek(TRUE);
    return S_OK;
}

HRESULT CTSFileSourcePin::ChangeStop()
{
    UpdateFromSeek();
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
		DeliverBeginFlush();
		m_llPrevPCR = -1;
		if (updateStartPosition == TRUE)
		{
			m_pTSBuffer->Clear();
			m_pTSFileSourceFilter->FileSeek(m_rtStart);
			m_rtLastSeekStart = m_rtStart;
		}
		DeliverEndFlush();
	}
	m_bSeeking = FALSE;
}

HRESULT CTSFileSourcePin::SetDuration(REFERENCE_TIME duration)
{
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

	FILTER_INFO		filterInfo;
	hr = m_pFilter->QueryFilterInfo(&filterInfo);

	if (filterInfo.pGraph)
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

	long bytesToRead = m_lTSPacketDeliverySize+188;	//Read an extra packet to make sure we don't miss a PCR that spans a gap.
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

