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

#include "PidParser.h"

CTSFileSourcePin::CTSFileSourcePin(LPUNKNOWN pUnk, CTSFileSourceFilter *pFilter, HRESULT *phr) :
	CSourceStream(NAME("MPEG2 Source Output"), phr, pFilter, L"Out"),
	CSourceSeeking(NAME("MPEG2 Source Output"), pUnk, phr, &m_SeekLock ),
	m_pTSFileSourceFilter(pFilter)
{
	m_dwSeekingCaps =	AM_SEEKING_CanSeekForwards  |
						AM_SEEKING_CanGetStopPos    |
						AM_SEEKING_CanGetDuration   |
						AM_SEEKING_CanSeekAbsolute;

	m_llBasePCR = -1;
	m_llNextPCR = -1;
	m_llPrevPCR = -1;
	m_lNextPCRByteOffset = 0;
	m_lPrevPCRByteOffset = 0;

//**********************************************************************************************
//bitrate Additions

	m_WaitForPCR = 0;
	m_lByteDelta = 0;

	m_BitRateCycle = 0;
	m_BitRateCount = 0;

	m_lTSPacketDeliverySize = 188*16;
	m_bRateControl = FALSE;

//Removed	m_bRateControl = TRUE;
//Removed	m_lTSPacketDeliverySize = 188*1000;

//**********************************************************************************************

	m_pTSBuffer = new CTSBuffer(m_pTSFileSourceFilter->get_FileReader(), &m_pTSFileSourceFilter->get_Pids()->pids);

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
		m_rtDuration = m_pTSFileSourceFilter->get_Pids()->pids.dur;
		m_rtStop = m_rtDuration;
		//Set inital Data rate from pid store
		m_DataRate = m_pTSFileSourceFilter->get_Pids()->pids.rate;

	}
	return hr;
}

HRESULT CTSFileSourcePin::FillBuffer(IMediaSample *pSample)
{
	CheckPointer(pSample, E_POINTER);
	if (m_pTSFileSourceFilter->get_FileReader()->IsFileInvalid())
	{
		return NOERROR;
	}

	// Access the sample's data buffer
	PBYTE pData;
	LONG lDataLength;
	HRESULT hr = pSample->GetPointer(&pData);
	if (FAILED(hr))
	{
		return hr;
	}
	lDataLength = pSample->GetActualDataLength();

	//FillBufferSyncTS(pSample);

//**********************************************************************************************
//Bitrate Additions

#define USE_EVENT
#ifdef USE_EVENT
#else

//**********************************************************************************************

	m_pTSBuffer->Require(lDataLength);

//**********************************************************************************************
//Bitrate Additions

#endif

//**********************************************************************************************

	//Test if constant Bitrate disabled
	if (!m_bRateControl)
	{

//**********************************************************************************************
//Bitrate Additions

#ifdef USE_EVENT

		m_pTSBuffer->Require(lDataLength);
#endif
//**********************************************************************************************

		m_pTSBuffer->DequeFromBuffer(pData, lDataLength);

		//Reset base PCR time and exit normaly if not constant Bitrate
		m_llPrevPCR = -1;
		return S_OK;
	}
	else
	{

//**********************************************************************************************
//Bitrate Additions

#ifdef USE_EVENT

		hr = m_pTSFileSourceFilter->get_FileReader()->Read(pData, lDataLength);


		//Skip if first PCR has already been found else find it
		if (m_llBasePCR == -1)
		{
			m_llPrevPCR = -1; //reset last PCR
			m_WaitForPCR = 0;
			m_lByteDelta = 0;
			m_BitRateCycle = 0;
			m_BitRateCount = 0;

			long a;
			m_llBasePCR = FindFirstPCR(&a, pData, lDataLength); //returns -1 if not found
			if (m_llBasePCR != (__int64) -1) //If PCR found then set the start time reference
			{
				IReferenceClock* pReferenceClock = NULL;
				hr = GetReferenceClock(&pReferenceClock);

				if (pReferenceClock != NULL)
				{
					pReferenceClock->GetTime(&m_rtStartTime);
					pReferenceClock->Release();
				}
			}
		}

		long a;
		//Get the last PCR anyway and save it if found
		m_llNextPCR = FindLastPCR(&a, pData, lDataLength);
		if (m_llNextPCR != (__int64)-1)
		{
			if (m_llPrevPCR != (__int64)-1)
			{
				m_llPCRDelta = (REFERENCE_TIME)((ConvertPCRtoRT(m_llNextPCR - m_llPrevPCR))); 
				//8bits per byte and convert to sec divide by pcr duration then average it
				__int64 dataRate = ((((__int64)m_lByteDelta + (__int64)a)*(__int64)80000000) / m_llPCRDelta);
				GetBitRateAverage(dataRate);
				//Reset data Gap Register to PCR position in buffer.
				m_lByteDelta =  - (__int64)a;// - 94; //sample length is added later to get true pos
			}

			m_llPrevPCR = m_llNextPCR;
		}
		m_lByteDelta = m_lByteDelta + lDataLength;

		//Skip if last PCR has not been found
		if (m_llPrevPCR != (__int64)-1)
		{
			REFERENCE_TIME rtStart = (REFERENCE_TIME)(m_llPrevPCR - m_llBasePCR);
			rtStart = ConvertPCRtoRT(rtStart);

			REFERENCE_TIME rtNextTime = m_rtStartTime + rtStart;//m_llPCRDelta;

			IReferenceClock* pReferenceClock = NULL;
			hr = GetReferenceClock(&pReferenceClock);

			if (pReferenceClock != NULL)
			{
				REFERENCE_TIME rtCurrTime;
				pReferenceClock->GetTime(&rtCurrTime);

				if ((__int64)rtCurrTime < (__int64)rtNextTime)
				{
					HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
					DWORD dwAdviseCookie = 0;
					pReferenceClock->AdviseTime(0, rtNextTime, (HEVENT)hEvent, &dwAdviseCookie);
					DWORD dwWaitResult = WaitForSingleObject(hEvent, INFINITE);
					CloseHandle(hEvent);
					m_WaitForPCR = 0;
				}
				else
				{
					//Get number of buffer cycles in a second from bit data rate
					__int64 cyclesinPCR = m_DataRate / lDataLength;

					//make sure cycle is not in error
					if ((__int64)cyclesinPCR < (__int64)1 || (__int64)cyclesinPCR > (__int64)100000)
					{
						cyclesinPCR = (__int64)lDataLength;
					}
					//add calculated average wait to the wait durations
					m_WaitForPCR = m_WaitForPCR + (REFERENCE_TIME)(80000000 / cyclesinPCR);
					rtNextTime = rtNextTime + m_WaitForPCR;
					pReferenceClock->GetTime(&rtCurrTime);

					if ((__int64)rtCurrTime < (__int64)rtNextTime)
					{
						HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
						DWORD dwAdviseCookie = 0;
						pReferenceClock->AdviseTime(0, rtNextTime, (HEVENT)hEvent, &dwAdviseCookie);
						DWORD dwWaitResult = WaitForSingleObject(hEvent, INFINITE);
						CloseHandle(hEvent);
						m_WaitForPCR = 0;
					}

				}
				pReferenceClock->Release();
			}
		}
	}
	return hr;
}

#else

		// nates code

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


				TCHAR sz[60];
				__int64 bitrate = (__int64)m_lByteDelta * (__int64)90000 / m_llPCRDelta;

//**********************************************************************************************
//bitrate Additions

				//8bits per byte and convert to sec divide by pcr duration then average it
				GetBitRateAverage(((__int64)m_lByteDelta * (__int64)80000000) / (__int64)ConvertPCRtoRT(m_llPCRDelta));

//**********************************************************************************************

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
			IReferenceClock* pReferenceClock = NULL;
			hr = GetReferenceClock(&pReferenceClock);
			if (pReferenceClock != NULL)
			{
				m_llBasePCR = pcrStart;
				pReferenceClock->GetTime(&m_rtStartTime);
				pReferenceClock->Release();
			}
			else
			{
				Debug(TEXT("Failed to find ReferenceClock. Sending sample now\n"));
				return S_OK;
			}
		}

		//Calculate next event time
		pcrStart -= m_llBasePCR;
		REFERENCE_TIME rtNextTime = ConvertPCRtoRT(pcrStart);
		rtNextTime += m_rtStartTime - 391000;

		//Wait if necessary
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

		//Set sample time
		//pSample->SetTime(&rtStart, &rtStart);

		return hr;
	}
}
#endif
//**********************************************************************************************


HRESULT CTSFileSourcePin::OnThreadStartPlay( )
{
	m_llPrevPCR = -1;
	debugcount = 0;

    DeliverNewSegment(m_rtStart, m_rtStop, 1.0 );
    return CSourceStream::OnThreadStartPlay( );
}

HRESULT CTSFileSourcePin::Run(REFERENCE_TIME tStart)
{
	//m_rtStartTime = tStart;
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
	m_llPrevPCR = -1;
	if (ThreadExists())
	{
		DeliverBeginFlush();
		Stop();
		if (updateStartPosition == TRUE)
		{
			m_pTSBuffer->Clear();
			m_pTSFileSourceFilter->FileSeek(m_rtStart);
		}
		DeliverEndFlush();
		CSourceStream::Run();
	}
}

HRESULT CTSFileSourcePin::SetDuration(REFERENCE_TIME duration)
{
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

/*
//This was an early attempt to see if vlc handled it better if we were sync
//byte aligned, but it didn't seem to make any difference.
//If this is be reimplemented it will be different now because CTSBuffer
//has been introduced since.

HRESULT CTSFileSourcePin::FillBufferSyncTS(IMediaSample *pSample)
{
	PBYTE pData;
	LONG lDataLength;

	HRESULT hr = pSample->GetPointer(&pData);
	if (FAILED(hr))
	{
		return hr;
	}
	lDataLength = pSample->GetActualDataLength();

	PBYTE pTempData = new BYTE[lDataLength];
	hr = m_pTSFileSourceFilter->get_FileReader()->Read(pTempData, lDataLength);
	if (FAILED(hr))
		return hr;

	if (lDataLength % 188 == 0)
	{
		//Attempt to find a sync byte and start the buffer from there.
		//We shouldn't need to worry about this, but it can't hurt
		long lOffset = 0;
		hr = PidParser::FindSyncByte(pTempData, lDataLength, &lOffset, 1);

		if (SUCCEEDED(hr))
		{
			memcpy(pData, pTempData+lOffset, lDataLength-lOffset);
			if (lOffset > 0)
			{
				hr = m_pTSFileSourceFilter->get_FileReader()->Read(pTempData+lDataLength-lOffset, lOffset);
				if (FAILED(hr))
					return hr;
			}
			return S_OK;
		}
	}
	//We didn't sync we just return the data as we read it.
	memcpy(pData, pTempData, lDataLength);

	return S_OK;
}
*/

//**********************************************************************************************

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

		long pos = 0;
		hr = PidParser::FindFirstPCR(pData, bytesToRead, &m_pTSFileSourceFilter->get_Pids()->pids, pcrtime, &pos);
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

		long pos = 0;
		hr = PidParser::FindLastPCR(pData, bytesToRead, &m_pTSFileSourceFilter->get_Pids()->pids, pcrtime, &pos);
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

void CTSFileSourcePin::Debug(LPCTSTR lpOutputString)
{
	TCHAR sz[200];
	wsprintf(sz, TEXT("%05i - %s"), debugcount, lpOutputString);
	::OutputDebugString(sz);
	debugcount++;
}

//*********************************************************************************************
//Bitrate addition

__int64 CTSFileSourcePin::FindFirstPCR(long* a, PBYTE pbData, long lbufflen)
{
	*a = 0; 
	return PinGetNextPCR(pbData, lbufflen, a, 1);
}

__int64 CTSFileSourcePin::FindLastPCR(long* a, PBYTE pbData, long lbufflen)
{
	*a = lbufflen - 188; 
	return PinGetNextPCR(pbData, lbufflen, a, -1);
}


__int64 CTSFileSourcePin::PinGetNextPCR(PBYTE pbData, long lbufflen, long* a, int step)
{
	REFERENCE_TIME pcrtime;
	HRESULT hr = S_OK;

	pcrtime = 0;

	while( pcrtime == 0 && hr == S_OK)
    {	
		hr = PinSyncBuffer(pbData,lbufflen, a, step);
		if (S_FALSE == PinCheckForPCR(pbData, *a, &pcrtime)) {
			*a = *a + (step * 188);
		}
		else 
		{
			return pcrtime;
		}
	}
		return -1;
}

HRESULT CTSFileSourcePin::PinSyncBuffer(PBYTE pbData, long lbuflen, long* a, int step)
{
	while( ((pbData[*a] != 0x47) || (pbData[*a+188] != 0x47) || (pbData[*a+376] != 0x47))
			&& *a < lbuflen && *a > -1 )
	{ 
		*a = *a + step;
	};


	if (*a < lbuflen && *a > -1) {

		return S_OK;
	} else {
		return S_FALSE;
	}	
}

HRESULT CTSFileSourcePin::PinCheckForPCR(PBYTE pbData, long pos, REFERENCE_TIME* pcrtime)
{    

	if ((WORD)((0x1F & pbData[pos+1])<<8 | (0xFF & pbData[pos+2])) == m_pTSFileSourceFilter->get_Pids()->pids.pcr) {

		WORD pcrbit = 0x30;
		if (m_pTSFileSourceFilter->get_Pids()->pids.pcr != m_pTSFileSourceFilter->get_Pids()->pids.vid){pcrbit = 0x20;};

		if (((pbData[pos+3] & 0x30) == pcrbit)

			&& (pbData[pos+4] != 0x00)
			&& ((pbData[pos+5] & 0x10) != 0x00)) {

			*pcrtime = (REFERENCE_TIME) 
							((REFERENCE_TIME)(0xFF & pbData[pos+6])<<25) | 
					        ((REFERENCE_TIME)(0xFF & pbData[pos+7])<<17) | 
							((REFERENCE_TIME)(0xFF & pbData[pos+8])<<9)  | 
							((REFERENCE_TIME)(0xFF & pbData[pos+9])<<1)  |
							((REFERENCE_TIME)(0x80 & pbData[pos+10])>>7);

			return S_OK;
		};
	}
	return S_FALSE;
}

long CTSFileSourcePin::get_BitRate()
{
    return m_DataRate;
}

void CTSFileSourcePin::set_BitRate(long rate)
{
    m_DataRate = rate;
}

void  CTSFileSourcePin::GetBitRateAverage(__int64 bitratesample)
{

	if (bitratesample < (__int64)1) return;

	//Setup for first sample and inc through array
	m_BitRateCycle++;

	//Set Number of counts to average
	if (m_BitRateCount < 257)
		m_BitRateCount = m_BitRateCycle;

	//Rotate array
	if (m_BitRateCycle > 255)
		m_BitRateCycle = 0;

	//Store the sample
	m_BitRateStore[m_BitRateCycle] = bitratesample;

	__int64 datarate = 0;

	//Add all Bitrate Smaples
	for (int i = 0; i < m_BitRateCount; i++)
	{
		datarate = datarate + m_BitRateStore[i];
	}
	m_DataRate = (long)(datarate / (__int64)m_BitRateCount);
	return;
}
//*********************************************************************************************

