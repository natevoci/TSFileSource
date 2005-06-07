/**
*  TSFileSourcePin.h
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

#ifndef TSFILESOURCEPIN_H
#define TSFILESOURCEPIN_H

#include "TSBuffer.h"

/**********************************************
 *
 *  CTSFileSourcePin Class
 *
 **********************************************/

class CTSFileSourcePin : public CSourceStream,
						 public CSourceSeeking
{
public:
	CTSFileSourcePin(LPUNKNOWN pUnk, CTSFileSourceFilter *pFilter, HRESULT *phr);
	~CTSFileSourcePin();

	STDMETHODIMP NonDelegatingQueryInterface( REFIID riid, void ** ppv );

	//CSourceStream
	HRESULT GetMediaType(CMediaType *pMediaType);
	HRESULT DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pRequest);
	HRESULT CompleteConnect(IPin *pReceivePin);
	HRESULT FillBuffer(IMediaSample *pSample);
	HRESULT OnThreadStartPlay();
	HRESULT Run(REFERENCE_TIME tStart);

	// CSourceSeeking
	HRESULT ChangeStart();
	HRESULT ChangeStop();
	HRESULT ChangeRate();
	void UpdateFromSeek(BOOL updateStartPosition = FALSE);

	HRESULT SetDuration(REFERENCE_TIME duration);

	BOOL get_RateControl();
	void set_RateControl(BOOL bRateControl);

//*********************************************************************************************
//Bitrate addition

	long get_BitRate();
	void set_BitRate(long rate);

	void GetBitRateAverage(__int64 bitratesample);
	__int64 FindFirstPCR(long* a, PBYTE pbData, long lbufflen);
	__int64 FindLastPCR(long* a, PBYTE pbData, long lbufflen);
	__int64 PinGetNextPCR(PBYTE pbData, long lbufflen, long* a, int step);
	HRESULT PinSyncBuffer(PBYTE pbData, long lbuflen, long* a, int step);
	HRESULT PinCheckForPCR(PBYTE pbData, long pos, REFERENCE_TIME* pcrtime);

//*********************************************************************************************
protected:
	HRESULT GetReferenceClock(IReferenceClock **pClock);
	//HRESULT FillBufferSyncTS(IMediaSample *pSample);

	__int64 ConvertPCRtoRT(REFERENCE_TIME pcrtime);

	HRESULT FindNextPCR(__int64 *pcrtime, long *byteOffset, long maxOffset);
	HRESULT FindPrevPCR(__int64 *pcrtime, long *byteOffset);

	void Debug(LPCTSTR lpOutputString);

protected:
	CTSFileSourceFilter * const m_pTSFileSourceFilter;
	CTSBuffer *m_pTSBuffer;

	CCritSec  m_SeekLock;

	REFERENCE_TIME m_rtStartTime;
	REFERENCE_TIME m_rtPrevTime;

	__int64 m_llBasePCR;
	__int64 m_llNextPCR;
	__int64 m_llPrevPCR;
	__int64 m_llPCRDelta;

	long m_lNextPCRByteOffset;
	long m_lPrevPCRByteOffset;
	long m_lByteDelta;

	long m_lTSPacketDeliverySize;

	BOOL   m_bRateControl;

	int debugcount;

//*********************************************************************************************
//Bitrate addition

	REFERENCE_TIME m_WaitForPCR;
	long m_DataRate;
	int m_BitRateCycle;
	int m_BitRateCount;
	__int64 m_BitRateStore[256];



//*********************************************************************************************
};

#endif
