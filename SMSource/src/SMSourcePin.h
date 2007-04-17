/**
*  SMSourcePin.h
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







#ifndef SMSOURCEPIN_H
#define SMSOURCEPIN_H

#include "SMSource.h"

/**********************************************
 *
 *  CSMStreamPin Class
 *  
 *
 **********************************************/


class CSMStreamPin : public CSourceSeeking,
					 public CSourceStream
{
protected:

    CSMSourceFilter  * m_pSMSourceFilter;             // Main renderer object    
    CCritSec   m_pReceiveLock;          // Protects our internal state
	BOOL m_bSeeking;
    CCritSec m_ReceiveLock;         // Sublock for received samples

public:

	CSMStreamPin(LPUNKNOWN pUnk,
				 CSMSourceFilter *pFilter,
//				 CCritSec *pLock,
//				 CCritSec *pReceiveLock,
				 HRESULT *phr);
    ~CSMStreamPin();

    // Override the version that offers exactly one media type

//    HRESULT GetMediaType(CMediaType *pMediaType);
	STDMETHODIMP IsFormatSupported(const GUID * pFormat);
	STDMETHODIMP QueryPreferredFormat(GUID *pFormat);
	STDMETHODIMP GetDuration(LONGLONG *pDuration);
	STDMETHODIMP GetStopPosition(LONGLONG *pStop);
	STDMETHODIMP GetCurrentPosition(LONGLONG *pCurrent);
	STDMETHODIMP GetCapabilities( DWORD * pCapabilities );
	STDMETHODIMP CheckCapabilities( DWORD * pCapabilities );
	STDMETHODIMP ConvertTimeFormat( LONGLONG * pTarget, const GUID * pTargetFormat,
                           LONGLONG    Source, const GUID * pSourceFormat );
	STDMETHODIMP SetPositions( LONGLONG * pCurrent,  DWORD CurrentFlags
                      , LONGLONG * pStop,  DWORD StopFlags );
	STDMETHODIMP GetPositions( LONGLONG * pCurrent, LONGLONG * pStop );
	STDMETHODIMP GetAvailable( LONGLONG * pEarliest, LONGLONG * pLatest );
	STDMETHODIMP SetRate( double dRate);
	STDMETHODIMP GetRate( double * pdRate);
	STDMETHODIMP GetPreroll(LONGLONG *pPreroll);












	HRESULT CheckConnect(IPin *pReceivePin);
	HRESULT GetMediaType(CMediaType *pmt);
	HRESULT GetMediaType(int iPosition, CMediaType *pMediaType);
	HRESULT CheckMediaType(const CMediaType* pType);
	HRESULT CompleteConnect(IPin *pReceivePin);
	HRESULT BreakConnect();
    HRESULT DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pRequest);
    HRESULT FillBuffer(IMediaSample *pSample);
	HRESULT OnThreadStartPlay( );
	HRESULT SetDuration(REFERENCE_TIME duration);
    REFERENCE_TIME GetStartTime();
	HRESULT SetStartTime(REFERENCE_TIME start);

	void UpdateFromSeek();
    
    // Quality control
	// Not implemented because we aren't going in real time.
	// If the file-writing filter slows the graph down, we just do nothing, which means
	// wait until we're unblocked. No frames are ever dropped.

	   // CSourceSeeking

	STDMETHODIMP NonDelegatingQueryInterface( REFIID riid, void ** ppv );
    //
    HRESULT ChangeStart( );
    HRESULT ChangeStop( );
    HRESULT ChangeRate( );
   
    CCritSec  m_SeekLock;
	BOOL m_biMpegDemux;
    STDMETHODIMP Notify(IBaseFilter *pSelf, Quality q)
    {
        return E_FAIL;
    }

};


#endif

