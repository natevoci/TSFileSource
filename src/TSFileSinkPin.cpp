/**
*  TSFileSinkPin.cpp
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
*  authors can be reached on the forums at
*    http://forums.dvbowners.com/
*/

#include <streams.h>
#include "TSFileSink.h"
#include "TSFileSinkGuids.h"

//////////////////////////////////////////////////////////////////////
// CTSFileSinkPin
//////////////////////////////////////////////////////////////////////
CTSFileSinkPin::CTSFileSinkPin(CTSFileSink *pTSFileSink, LPUNKNOWN pUnk, CBaseFilter *pFilter, CCritSec *pLock, HRESULT *phr) :
	CRenderedInputPin(NAME("Input Pin"), pFilter, pLock, phr, L"In"),
	m_pTSFileSink(pTSFileSink),
    m_tLast(0)
{
}

CTSFileSinkPin::~CTSFileSinkPin()
{
}

HRESULT CTSFileSinkPin::CheckMediaType(const CMediaType *)
{
    return S_OK;
}

HRESULT CTSFileSinkPin::BreakConnect()
{
    if (m_pTSFileSink->m_pPosition != NULL) {
        m_pTSFileSink->m_pPosition->ForceRefresh();
    }

    return CRenderedInputPin::BreakConnect();
}

STDMETHODIMP CTSFileSinkPin::ReceiveCanBlock()
{
    return S_FALSE;
}

STDMETHODIMP CTSFileSinkPin::Receive(IMediaSample *pSample)
{
    CheckPointer(pSample,E_POINTER);

    CAutoLock lock(&m_ReceiveLock);
    PBYTE pbData;

    REFERENCE_TIME tStart, tStop;
    pSample->GetTime(&tStart, &tStop);

    DbgLog((LOG_TRACE, 1, TEXT("tStart(%s), tStop(%s), Diff(%d ms), Bytes(%d)"),
           (LPCTSTR) CDisp(tStart),
           (LPCTSTR) CDisp(tStop),
           (LONG)((tStart - m_tLast) / 10000),
           pSample->GetActualDataLength()));

    m_tLast = tStart;

    HRESULT hr = pSample->GetPointer(&pbData);
    if (FAILED(hr)) {
        return hr;
    }

    return m_pTSFileSink->Write(pbData, pSample->GetActualDataLength());
}

STDMETHODIMP CTSFileSinkPin::EndOfStream(void)
{
    CAutoLock lock(&m_ReceiveLock);
    return CRenderedInputPin::EndOfStream();

}

STDMETHODIMP CTSFileSinkPin::NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
    m_tLast = 0;
    return S_OK;

}
