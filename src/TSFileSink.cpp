/**
*  TSFileSink.cpp
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

CUnknown * WINAPI CTSFileSink::CreateInstance(LPUNKNOWN punk, HRESULT *phr)
{
	ASSERT(phr);
	CTSFileSink *pNewObject = new CTSFileSink(punk, phr);

	if (pNewObject == NULL) {
		if (phr)
			*phr = E_OUTOFMEMORY;
	}

	return pNewObject;
}

//////////////////////////////////////////////////////////////////////
// CTSFileSink
//////////////////////////////////////////////////////////////////////
CTSFileSink::CTSFileSink(IUnknown *pUnk, HRESULT *phr) :
	CUnknown(NAME("CTSFileSink"), NULL),
	m_pFilter(NULL),
	m_pPin(NULL),
	m_pPosition(NULL),
	m_pRegStore(NULL),
	m_pFileName(0),
	m_pFileWriter(NULL)
{
	ASSERT(phr);

	m_pFileWriter = new MultiFileWriter();

	m_pFilter = new CTSFileSinkFilter(this, GetOwner(), &m_Lock, phr);
    if (m_pFilter == NULL)
	{
        if (phr)
            *phr = E_OUTOFMEMORY;
        return;
    }

	m_pPin = new CTSFileSinkPin(this, GetOwner(), m_pFilter, &m_Lock, phr);
	if (m_pPin == NULL) {
		if (phr)
			*phr = E_OUTOFMEMORY;
		return;
	}

	m_pRegStore = new CRegStore();
	// TODO: Load Registry Settings data
}

CTSFileSink::~CTSFileSink()
{
	CloseFile();

	if (m_pFileWriter) delete m_pFileWriter;
	if (m_pPin) delete m_pPin;
	if (m_pFilter) delete m_pFilter;
	if (m_pPosition) delete m_pPosition;
	if (m_pFileName) delete m_pFileName;
	if (m_pRegStore) delete m_pRegStore;
}

STDMETHODIMP CTSFileSink::NonDelegatingQueryInterface(REFIID riid, void ** ppv)
{
    CheckPointer(ppv,E_POINTER);
    CAutoLock lock(&m_Lock);

    if (riid == IID_IFileSinkFilter)
	{
        return GetInterface((IFileSinkFilter *) this, ppv);
    } 
    else if (riid == IID_IBaseFilter || riid == IID_IMediaFilter || riid == IID_IPersist)
	{
        return m_pFilter->NonDelegatingQueryInterface(riid, ppv);
    } 
    else if (riid == IID_IMediaPosition || riid == IID_IMediaSeeking)
	{
        if (m_pPosition == NULL) 
        {
            HRESULT hr = S_OK;
            m_pPosition = new CPosPassThru(NAME("TSFileSink Pass Through"), (IUnknown *) GetOwner(), (HRESULT *) &hr, m_pPin);
            if (m_pPosition == NULL) 
                return E_OUTOFMEMORY;

            if (FAILED(hr)) 
            {
                delete m_pPosition;
                m_pPosition = NULL;
                return hr;
            }
        }

        return m_pPosition->NonDelegatingQueryInterface(riid, ppv);
    } 

    return CUnknown::NonDelegatingQueryInterface(riid, ppv);
}


HRESULT CTSFileSink::OpenFile()
{
    CAutoLock lock(&m_Lock);

    // Has a filename been set yet
    if (m_pFileName == NULL)
	{
        return ERROR_INVALID_NAME;
    }

	HRESULT hr = m_pFileWriter->OpenFile(m_pFileName);

    return S_OK;
}

HRESULT CTSFileSink::CloseFile()
{
    // Must lock this section to prevent problems related to
    // closing the file while still receiving data in Receive()
    CAutoLock lock(&m_Lock);

	m_pFileWriter->CloseFile();

    return NOERROR;
}

HRESULT CTSFileSink::Write(PBYTE pbData, LONG lDataLength)
{
    CAutoLock lock(&m_Lock);

	HRESULT hr = m_pFileWriter->Write(pbData, lDataLength);

    return S_OK;
}

STDMETHODIMP CTSFileSink::GetBufferSize(long *size)
{
	CheckPointer(size, E_POINTER);
	*size = 0;
	return S_OK;
}

STDMETHODIMP CTSFileSink::SetFileName(LPCWSTR pszFileName, const AM_MEDIA_TYPE *pmt)
{
    CheckPointer(pszFileName,E_POINTER);

    if(wcslen(pszFileName) > MAX_PATH)
        return ERROR_FILENAME_EXCED_RANGE;

	long length = lstrlenW(pszFileName);

	// Check that the filename ends with .tsbuffer. If it doesn't we'll add it
	if ((length < 9) || (_wcsicmp(pszFileName+length-9, L".tsbuffer") != 0))
	{
		m_pFileName = new wchar_t[1+length+9];
		if (m_pFileName == 0)
			return E_OUTOFMEMORY;

		swprintf(m_pFileName, L"%s.tsbuffer", pszFileName);
	}
	else
	{
	    m_pFileName = new WCHAR[1+length];
		if (m_pFileName == 0)
			return E_OUTOFMEMORY;

		wcscpy(m_pFileName, pszFileName);
	}
	HRESULT hr = S_OK;

    return hr;
}

STDMETHODIMP CTSFileSink::GetCurFile(LPOLESTR * ppszFileName, AM_MEDIA_TYPE *pmt)
{
    CheckPointer(ppszFileName, E_POINTER);
    *ppszFileName = NULL;

    if (m_pFileName != NULL) 
    {
		//QzTask = CoTask
        *ppszFileName = (LPOLESTR)CoTaskMemAlloc(sizeof(WCHAR) * (1+lstrlenW(m_pFileName)));

        if (*ppszFileName != NULL)
        {
            lstrcpyW(*ppszFileName, m_pFileName);
        }
    }

    if(pmt)
    {
        ZeroMemory(pmt, sizeof(*pmt));
        pmt->majortype = MEDIATYPE_NULL;
        pmt->subtype = MEDIASUBTYPE_NULL;
    }

    return S_OK;

}

