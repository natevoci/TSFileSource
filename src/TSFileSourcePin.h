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

// Define a typedef for a list of filters.
typedef CGenericList<IBaseFilter> CFilterList;

#include "TSFileSource.h"
#include "PidParser.h"
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
	STDMETHODIMP IsFormatSupported(const GUID * pFormat);
	STDMETHODIMP QueryPreferredFormat(GUID *pFormat);
	HRESULT GetMediaType(CMediaType *pMediaType);
	HRESULT GetMediaType(int iPosition, CMediaType *pMediaType);
	HRESULT CheckMediaType(const CMediaType* pType);
	HRESULT DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pRequest);
	HRESULT CheckConnect(IPin *pReceivePin);
	HRESULT CompleteConnect(IPin *pReceivePin);
	HRESULT BreakConnect();
	HRESULT FillBuffer(IMediaSample *pSample);
	HRESULT OnThreadStartPlay();
	HRESULT Run(REFERENCE_TIME tStart);

	// CSourceSeeking
	STDMETHODIMP GetCurrentPosition(LONGLONG *pCurrent);
	STDMETHODIMP GetPositions(LONGLONG *pCurrent, LONGLONG *pStop);
	STDMETHODIMP SetPositions(LONGLONG *pCurrent, DWORD CurrentFlags
			     , LONGLONG *pStop, DWORD StopFlags);
	STDMETHODIMP GetAvailable(LONGLONG *pEarliest, LONGLONG *pLatest);

	// CSourcePosition
	STDMETHODIMP get_CurrentPosition(REFTIME * pllTime);
	HRESULT ChangeStart();
	HRESULT ChangeStop();
	HRESULT ChangeRate();
	void UpdateFromSeek(BOOL updateStartPosition = FALSE);

	HRESULT SetAccuratePos(REFERENCE_TIME seektime);
	HRESULT UpdateDuration(FileReader *pFileReader);
	HRESULT SetDuration(REFERENCE_TIME duration);
	BOOL IsTimeShifting(FileReader *pFileReader, BOOL *timeMode);

	HRESULT CTSFileSourcePin::DisconnectDemuxPins();
	HRESULT SetDemuxClock(IReferenceClock *pClock);

	BOOL get_RateControl();
	void set_RateControl(BOOL bRateControl);

	long get_BitRate();
	void set_BitRate(long rate);
	virtual void PrintTime(LPCTSTR lstring, __int64 value, __int64 divider);
	virtual void PrintLongLong(LPCTSTR lstring, __int64 value);

protected:

	HRESULT FindNextPCR(__int64 *pcrtime, long *byteOffset, long maxOffset);
	HRESULT FindPrevPCR(__int64 *pcrtime, long *byteOffset);
	__int64 ConvertPCRtoRT(REFERENCE_TIME pcrtime);
	void AddBitRateForAverage(__int64 bitratesample);
	void Debug(LPCTSTR lpOutputString);

protected:
	CTSFileSourceFilter * const m_pTSFileSourceFilter;
	CTSBuffer *m_pTSBuffer;
	
	CCritSec  m_FillLock;
	CCritSec  m_SeekLock;
	BOOL      m_bSeeking;

	REFERENCE_TIME m_rtStartTime;
	REFERENCE_TIME m_rtPrevTime;
	REFERENCE_TIME m_rtLastSeekStart;

	__int64 m_llBasePCR;
	__int64 m_llNextPCR;
	__int64 m_llPrevPCR;
	__int64 m_llPCRDelta;

	int debugcount;
	BOOL   m_bRateControl;
	long m_lNextPCRByteOffset;
	long m_lPrevPCRByteOffset;
	long m_lByteDelta;
	long m_lTSPacketDeliverySize;
	long m_DataRate;
	long m_BitRateCycle;
	__int64 m_DataRateTotal;
	__int64 m_BitRateStore[256];

	REFERENCE_TIME m_rtLastCurrentTime;
	REFERENCE_TIME m_rtTimeShiftPosition;
	__int64 m_LastFileSize;
	__int64 m_LastStartSize;

	__int64 m_IntLastStreamTime;
	__int64 m_DataRateSave;
	__int64 m_LastMultiFileStart;
	__int64 m_LastMultiFileEnd;

public:
	BOOL	m_DemuxLock;

};

#endif
