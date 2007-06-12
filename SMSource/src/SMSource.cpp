/**
*  SMSource.ccp
*  Copyright (C) 2003      bisswanger
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
*  bisswanger can be reached at WinSTB@hotmail.com
*    Homepage: http://www.winstb.de
*
*  bear can be reached on the forums at
*    http://forums.dvbowners.com/
*/

//------------------------------------------------------------------------------
// File: SMSource.cpp
//
// Desc: DirectShow Shared Memory push mode source filter
//       Provides a MPEG - transport output stream.
//
//------------------------------------------------------------------------------

#include "SMSource.h"
#include "SMSourceGuids.h"
#include "resource.h"
//#include <objbase.h>
//#include <strmif.h>
//#include <objbase.h>
#include <streams.h>
#include "RegStore.h"

CUnknown * WINAPI CSMSourceFilter::CreateInstance(LPUNKNOWN punk, HRESULT *phr)
{
	ASSERT(phr);
	CSMSourceFilter *pNewObject = new CSMSourceFilter(punk, phr);

	if (pNewObject == NULL) {
		if (phr)
			*phr = E_OUTOFMEMORY;
	}

	return pNewObject;
}

/// Constructor
CSMSourceFilter::CSMSourceFilter(IUnknown *pUnk, HRESULT *phr) :
	CSource(NAME("CSMSourceFilter"), pUnk, CLSID_SMSource, phr),
    m_pPin(NULL),
    m_pPosition(NULL),
	m_buflen(94000),
    m_pFileName(0)
{
    ASSERT(phr);

	m_pClock = new CSMSourceClock( NAME(""), GetOwner(), phr );
	if (m_pClock == NULL)
	{
		if (phr)
			*phr = E_OUTOFMEMORY;
		return;
	}

    m_pPin = new CSMStreamPin(GetOwner(),
                              this,
//                              &m_Lock,
//                              &m_ReceiveLock,
                              phr);

    if (m_pPin == NULL) {
        if (phr)
            *phr = E_OUTOFMEMORY;
        return;
    }

	// get the overrides from the register
	CRegStore *store = new CRegStore("SOFTWARE\\SMSinkFilter");
	__int64 sharedMemorySize = max(store->getInt("numBlocks", 8), 2);
	sharedMemorySize *= max(store->getInt("blockSize", 2), 2);
	sharedMemorySize = min(sharedMemorySize, 64);
	sharedMemorySize *= 1048576;
	delete store;

	m_pSharedMemory = new SharedMemory(sharedMemorySize);
	m_pFileReader = new FileReader(m_pSharedMemory);
}

CSMSourceFilter::~CSMSourceFilter()
{
	CloseFile();
    if (m_pPin) delete m_pPin;
    if (m_pPosition) delete m_pPosition;
    if (m_pFileName) delete m_pFileName;
    if (m_pFileReader) delete m_pFileReader;
    if (m_pSharedMemory) delete m_pSharedMemory;
}


// NonDelegatingQueryInterface
//
// Override this to say what interfaces we support where
//
STDMETHODIMP CSMSourceFilter::NonDelegatingQueryInterface(REFIID riid, void ** ppv)
{
    CheckPointer(ppv,E_POINTER);
    CAutoLock lock(&m_Lock);

	// Do we have this interface
	if (riid == IID_ISMSource)
	{
		return GetInterface((ISMSource*)this, ppv);
	}
	if (riid == IID_IFileSourceFilter)
	{
		return GetInterface((IFileSourceFilter*)this, ppv);
	}
	if (riid == IID_ISpecifyPropertyPages)
	{
		return GetInterface((ISpecifyPropertyPages*)this, ppv);
	}
	if (riid == IID_IMediaPosition || riid == IID_IMediaSeeking)
	{
		return m_pPin->NonDelegatingQueryInterface(riid, ppv);
	}
	if (riid == IID_IReferenceClock)
	{
		return GetInterface((IReferenceClock*)m_pClock, ppv);
	}
//    if (riid == IID_IAMFilterMiscFlags)
//    {
//		return GetInterface((IAMFilterMiscFlags*)this, ppv);
//    }
//    if (riid == IID_IAMPushSource)
//    {
//		return GetInterface((IAMPushSource*)this, ppv);
//    }
	return CSource::NonDelegatingQueryInterface(riid, ppv);

} // NonDelegatingQueryInterface


CBasePin * CSMSourceFilter::GetPin(int n)
{
	CAutoLock lock(&m_Lock);
	if (n == 0) {
		ASSERT(m_pPin);
		return m_pPin;
	} else {
		return NULL;
	}
}

//
// GetPinCount
//
int CSMSourceFilter::GetPinCount()
{
	CAutoLock lock(&m_Lock);
	if (m_pPin)
		return 1;
	else
		return 0;
}

STDMETHODIMP CSMSourceFilter::FindPin(LPCWSTR Id, IPin ** ppPin)
{
    CheckPointer(ppPin,E_POINTER);
    ValidateReadWritePtr(ppPin,sizeof(IPin *));

	CAutoLock lock(&m_Lock);
	if (!wcscmp(Id, m_pPin->CBasePin::Name())) {

		*ppPin = m_pPin;
		if (*ppPin!=NULL){
			(*ppPin)->AddRef();
			return NOERROR;
		}
	}

	return CSource::FindPin(Id, ppPin);
}

//
// Stop
//
// Overriden to close the dump file
//
STDMETHODIMP CSMSourceFilter::Stop()
{
	CAutoLock lock(&m_Lock);
	CAutoLock cObjectLock(m_pLock);

	HRESULT hr = CBaseFilter::Stop();

	hr = CloseFile();

	if (m_pSharedMemory) m_pSharedMemory->Destroy();  

	return hr;
}


//
// Pause
//
STDMETHODIMP CSMSourceFilter::Pause()
{
	CAutoLock cObjectLock(m_pLock);

	if(!((m_State == State_Paused) || (m_State == State_Running)))
	{
		if (m_pFileReader->IsFileInvalid())
		{
			HRESULT hr = m_pFileReader->OpenFile();
			if (FAILED(hr))
				return hr;
		}
		m_pFileReader->SetFilePointer(0, FILE_END);
	}

	return CBaseFilter::Pause();
}


//
// Run
//
// Overriden to open the dump file
//
STDMETHODIMP CSMSourceFilter::Run(REFERENCE_TIME tStart)
{
    // TCHAR sz[100];
	CAutoLock cObjectLock(m_pLock);

	if(!((m_State == State_Paused) || (m_State == State_Running)))
	{
		if (m_pFileReader->IsFileInvalid())
		{
			HRESULT hr = m_pFileReader->OpenFile();
			if (FAILED(hr))
				return hr;
		}
		m_pFileReader->SetFilePointer(0, FILE_END);
	}

	return CBaseFilter::Run(tStart);
}



STDMETHODIMP CSMSourceFilter::GetPages(CAUUID *pPages)
{
	if (pPages == NULL) return E_POINTER;
	pPages->cElems = 1;
	pPages->pElems = (GUID*)CoTaskMemAlloc(sizeof(GUID));
	if (pPages->pElems == NULL) 
	{
		return E_OUTOFMEMORY;
	}
	pPages->pElems[0] = CLSID_SMSourceProp;
	 return S_OK;
}


HRESULT CSMSourceFilter::FileSeek(REFERENCE_TIME seektime)
{

//	DOUBLE fileindex;
	// TCHAR sz[100];

	if (m_pFileReader->IsFileInvalid()) 
    {
        return S_FALSE;
    }
	
	__int64 llStartPosition;

	if (FAILED(m_pFileReader->GetFileSize(&llStartPosition, &m_filelength)))
	{ 
		return S_FALSE;
    }

	m_pFileReader->SetFilePointer(0, FILE_END);
    return S_OK;
/*
	if (seektime > m_Duration) {
		return S_FALSE;
	}

	if (m_Duration > 10) {

	fileindex = ((m_filelength / (double)m_Duration) * (double)seektime) / 188;


		// NewPCR = (REFERENCE_TIME) (m_StartPCR + (REFERENCE_TIME)(perc * (double)(m_EndPCR - m_StartPCR)));
	//	wsprintf(sz, TEXT("%u"), ((DWORD)fileindex * 188)); // 1f f0

	m_pFileReader->SetFilePointer(((DWORD)fileindex * 188), FILE_BEGIN);

	}
	
	//SetAccuratePos(seektime);
*/
    return S_OK;

}


HRESULT CSMSourceFilter::SetAccuratePos(REFERENCE_TIME tStart)
{
	REFERENCE_TIME actualpos = 0;
	int a;

	tStart = m_StartPCR + ((m_pPin->GetStartTime() * 9) / 1000);

	ULONG dwBytesRead = 0;
	if (tStart > m_StartPCR) {
		m_pFileReader->Read(m_pData, m_buflen, &dwBytesRead);
		a = 0;
		actualpos = GetNextPCR(&a, 1);

		while (actualpos > tStart) {
			m_pFileReader->SetFilePointer(-3 * m_buflen, FILE_CURRENT);
			dwBytesRead = 0;
			m_pFileReader->Read(m_pData, m_buflen, &dwBytesRead);

			a = 0;
			actualpos = GetNextPCR(&a, 1);
		}
		
		while (actualpos < tStart) {
			actualpos = GetNextPCR(&a, 1);
			if (actualpos == 0) {
				a = 0;
				actualpos = GetNextPCR(&a, 1);
			}
		}
	}
	return S_OK;
}

STDMETHODIMP CSMSourceFilter::GetCurFile(LPOLESTR * ppszFileName,AM_MEDIA_TYPE *pmt)
{

 	CAutoLock lock(&m_Lock);
   CheckPointer(ppszFileName, E_POINTER);
    *ppszFileName = NULL;

    if (m_pFileName != NULL) 
    {
        *ppszFileName = (LPOLESTR)
        QzTaskMemAlloc(sizeof(WCHAR) * (1+lstrlenW(m_pFileName)));

        if (*ppszFileName != NULL) 
        {
            lstrcpyW(*ppszFileName, m_pFileName);
        }
    }

    if(pmt) 
    {
        ZeroMemory(pmt, sizeof(*pmt));
        pmt->majortype = MEDIATYPE_Video;
        pmt->subtype = MEDIASUBTYPE_MPEG2_TRANSPORT;
    }

    return S_OK;

} // GetCurFile


//
// OpenFile
//
// Opens the file ready for streaming
//

HRESULT CSMSourceFilter::OpenFile(REFERENCE_TIME tStart)
{
//	REFERENCE_TIME start, stop;

	HRESULT hr;

	hr = m_pFileReader->SetFileName(m_pFileName);
	if (FAILED(hr))
	{
		if(m_pFileName)
		{
			delete[] m_pFileName;
			m_pFileName = NULL;
		}

		return hr;
	}

	m_pSharedMemory->SetShareMode(TRUE);

	hr = m_pFileReader->OpenFile();
	if (FAILED(hr))
	{
		if (!m_pSharedMemory->GetShareMode())
			m_pSharedMemory->SetShareMode(TRUE);
		else
			m_pSharedMemory->SetShareMode(FALSE);

		hr = m_pFileReader->OpenFile();
		if (FAILED(hr))
		{
			if(m_pFileName)
			{
				m_pSharedMemory->SetShareMode(TRUE);
//				delete[] m_pFileName;
//				m_pFileName = NULL;
			}

			return VFW_E_INVALIDMEDIATYPE;
		}
	}

	m_pFileReader->SetFilePointer(0, FILE_BEGIN);
//	m_pPin->GetPositions(&start, &stop);
//	if (start > 0)
//	{
//		FileSeek(start);
//	}

    return S_OK;

} // Open


//
// CloseFile
//
// Closes any dump file we have opened
//
HRESULT CSMSourceFilter::CloseFile()
{
    // Must lock this section to prevent problems related to
    // closing the file while still receiving data in Receive()

    CAutoLock lock(&m_Lock);

    if (m_pFileReader->IsFileInvalid()) {

        return S_OK;
    }
    m_pFileReader->CloseFile();

    return NOERROR;

} // Open

HRESULT CSMSourceFilter::GetPids()
{

    int a;
	DWORD filepoint;
	HRESULT hr = S_OK;

    if (m_pFileReader->IsFileInvalid()) {
        return NOERROR;
	}

    CAutoLock lock(&m_Lock);

    // Access the sample's data buffer

	m_vpid = 0x00;
	m_apid = 0x00;
	m_sidpid = 0x00;
	m_ac3pid = 0x00;
	m_a2pid = 0x00;
	m_pcrpid = 0x00;
	m_pmtpid = 0x00;

	m_pFileReader->SetFilePointer(0, FILE_BEGIN);
	ULONG dwBytesRead = 0;
	m_pFileReader->Read(m_pData, m_buflen, &dwBytesRead);

	a = 0;

	while (m_pmtpid == 0x00 && hr == S_OK) {
		hr = SyncBuffer(&a, 1);
		if (hr == S_OK) {
			CheckForPMT(a);
			a = a + 188;
		}
	}

	a = 0;
	CheckVAPids(0);

	m_StartPCR = GetNextPCR(&a, 1);
	
	for (int b1 = 1; b1 < 4; b1++) {

    m_pFileReader->SetFilePointer(m_buflen * b1, FILE_BEGIN);
	dwBytesRead = 0;
	m_pFileReader->Read(m_pData, m_buflen, &dwBytesRead);
	CheckVAPids(b1);
	}

	filepoint = m_pFileReader->SetFilePointer(-m_buflen, FILE_END);
	dwBytesRead = 0;
	m_pFileReader->Read(m_pData, m_buflen, &dwBytesRead);

	a = m_buflen - 188;
	m_EndPCR = GetNextPCR(&a, -1);

	m_Duration = (m_EndPCR - m_StartPCR)/9 * 1000;
	m_pPin->SetDuration(m_Duration);

	CheckEPG();

	if (m_vpid != 0 && m_apid != 0) {
		return S_OK;
	} else {
		return S_FALSE;
	}
}


HRESULT CSMSourceFilter::SyncBuffer(int* a, int step)
{
	while( ((m_pData[*a] != 0x47) || (m_pData[*a+188] != 0x47) || (m_pData[*a+376] != 0x47))
			&& *a < m_buflen && *a > -1 )
	{ 
		*a = *a + step;
	};


	if (*a < m_buflen && *a > -1) {

		return S_OK;
	} else {
		return S_FALSE;
	}	
}


HRESULT CSMSourceFilter::CheckVAPids(int offset)
{
	int a;
	WORD pid;
	HRESULT hr;
	int addlength, addfield;
	WORD error, start;
	DWORD psiID, pesID;
	// TCHAR sz[100];

	a = 1;
	hr = SyncBuffer(&a, 1);
   
		if (hr == S_OK) {
		for (int b = a; b < m_buflen - 188; b=b+188)
		{   // MessageBox(NULL, TEXT("start check"), TEXT("jJ"), MB_OK);
			error = 0x80&m_pData[b+1];
			start = 0x40&m_pData[b+1];
			addfield = (0x30 & m_pData[b+3])>>4;
			if (addfield > 1) {
				addlength = (0xff & m_pData[b+4]) + 1;
			} else {
				addlength = 0;
			}
            //       wsprintf(sz, TEXT("%d"), addlength); // 1f f0
		    //       MessageBox(NULL, sz, TEXT("kk"), MB_OK);
			if (start == 0x40) {
				pesID = ( (255&m_pData[b+4+addlength])<<24 
					    | (255&m_pData[b+5+addlength])<<16
						| (255&m_pData[b+6+addlength])<<8
						| (255&m_pData[b+7+addlength]) );
				psiID = pesID>>16;
				
				pid = ((0x1F & m_pData[b+1])<<8 | (0xFF & m_pData[b+2]));

				if ((pesID == 0x1e0) && (m_vpid == 0)) {
					m_vpid = pid;
					
				};
				if (pesID == 0x1c0) {
					if (m_apid == 0) {
						m_apid = pid;
					} else {
						if (m_apid != pid) {
							m_a2pid = pid;
						}
					}
				};
			}
		}
	}
	return S_OK;
}


HRESULT CSMSourceFilter::CheckForPCR(int pos, REFERENCE_TIME* pcrtime)
{    
	if (!pcrtime)
		return E_POINTER;

	if (FALSE)
	{
		// Get PTS
		if((0xC0&m_pData[pos+4]) == 0x40) {

			*pcrtime =	((REFERENCE_TIME)(0x38 & m_pData[pos+4])<<27) |
						((REFERENCE_TIME)(0x03 & m_pData[pos+4])<<28) |
						((REFERENCE_TIME)(0xFF & m_pData[pos+5])<<20) |
						((REFERENCE_TIME)(0xF8 & m_pData[pos+6])<<12) |
						((REFERENCE_TIME)(0x03 & m_pData[pos+6])<<13) |
						((REFERENCE_TIME)(0xFF & m_pData[pos+7])<<5)  |
						((REFERENCE_TIME)(0xF8 & m_pData[pos+8])>>3);
			return S_OK;
		}
		return S_FALSE;
	}

	if ((WORD)((0x1F & m_pData[pos+1])<<8 | (0xFF & m_pData[pos+2])) == m_pcrpid)
	{
		WORD pcrmask = 0x10;
		if ((m_pData[pos+3] & 0x30) == 30)
			pcrmask = 0xff;

		if (((m_pData[pos+3] & 0x30) >= 0x20)
			&& ((m_pData[pos+4] & 0x11) != 0x00)
			&& ((m_pData[pos+5] & pcrmask) == 0x10))
		{
				*pcrtime =	((REFERENCE_TIME)(0xFF & m_pData[pos+6])<<25) |
							((REFERENCE_TIME)(0xFF & m_pData[pos+7])<<17) |
							((REFERENCE_TIME)(0xFF & m_pData[pos+8])<<9)  |
							((REFERENCE_TIME)(0xFF & m_pData[pos+9])<<1)  |
							((REFERENCE_TIME)(0x80 & m_pData[pos+10])>>7);

				//A true PCR has been found so drop the other OPCR search
				m_pcrpid = 0;

				return S_OK;
		};
	}

//*********************************************************************************************
//Old Capture format Additions


	if ((WORD)((0x1F & m_pData[pos+1])<<8 | (0xFF & m_pData[pos+2])) == m_pcrpid &&
		(WORD)(m_pData[pos+1]&0xF0) == 0x40)
	{
		if (((m_pData[pos+3] & 0x10) == 0x10)
			&& ((m_pData[pos+4]) == 0x00)
			&& ((m_pData[pos+5]) == 0x00)
			&& ((m_pData[pos+6]) == 0x01)
			&& ((m_pData[pos+7]) == 0xEA)
			&& ((m_pData[pos+8] | m_pData[pos+9]) == 0x00)
			&& ((m_pData[pos+10] & 0xC0) == 0x80)
			&& ((m_pData[pos+11] & 0xC0) == 0x80 || (m_pData[pos+11] & 0xC0) == 0xC0)
			&& (m_pData[pos+12] >= 0x05)
			)
		{
			// Get PTS
			if((0xF0 & m_pData[pos+13]) == 0x10 || (0xF0 & m_pData[pos+13]) == 0x30) {

				*pcrtime =	((REFERENCE_TIME)(0x0C & m_pData[pos+13])<<29) |
							((REFERENCE_TIME)(0xFF & m_pData[pos+14])<<22) |
							((REFERENCE_TIME)(0xFE & m_pData[pos+15])<<14) |
							((REFERENCE_TIME)(0xFF & m_pData[pos+16])<<7)  |
							((REFERENCE_TIME)(0xFE & m_pData[pos+17])>>1);
				return S_OK;
			}
			// Get DTS
			if((0xF0 & m_pData[pos+18]) == 0x10) {

				*pcrtime =	((REFERENCE_TIME)(0x0C & m_pData[pos+18])<<29) |
							((REFERENCE_TIME)(0xFF & m_pData[pos+19])<<22) |
							((REFERENCE_TIME)(0xFE & m_pData[pos+20])<<14) |
							((REFERENCE_TIME)(0xFF & m_pData[pos+21])<<7)  |
							((REFERENCE_TIME)(0xFE & m_pData[pos+22])>>1);
				return S_OK;
			}
		};
	}

//*********************************************************************************************

	return S_FALSE;
/*
	if ((((0x1F & m_pData[pos+1])<<8) | (0xFF & m_pData[pos+2])) == m_pcrpid) {

		if (((m_pData[pos+3] & 0x30) == 0x30) 
			&& (m_pData[pos+4] != 0x00)
			&& ((m_pData[pos+5] & 0x10) != 0x00)) {
			*pcrtime = (REFERENCE_TIME) 
							((0xFF & m_pData[pos+6])<<25) | 
					        ((0xFF & m_pData[pos+7])<<17) | 
							((0xFF & m_pData[pos+8])<<9)  | 
							((0xFF & m_pData[pos+9])<<1)  |
							((0x80 & m_pData[pos+10])>>7);

			return S_OK;
		};
	}
	return S_FALSE;*/
}


bool CSMSourceFilter::CheckForEPG(int pos)
{    

	WORD sidpid;

	if (   ((0xFF & m_pData[pos+1]) == 0x40)
		&& ((0xFF & m_pData[pos+2]) == 0x12) 
		&& ((0xFF & m_pData[pos+5]) == 0x4e) 
		&& ((0x01 & m_pData[pos+11]) == 0x00)    // should be 0x01
		) {

		sidpid = (WORD)(((0xFF & m_pData[pos+8])<<8) | (0xFF & m_pData[pos+9]));

		if (sidpid == m_sidpid) {

			return true;
		}

	};

	return false;
}


HRESULT CSMSourceFilter::CheckEPG()
{
	int pos;
	int len;
	int len1;
	int DummyPos;
	int iterations;
    bool epgfound;

	HRESULT hr;
	
	pos = 0;
	iterations = 0;
	epgfound = false;
	m_shortdescr[0] = 0xFF;
	m_extenddescr[0] = 0xFF;

	m_pFileReader->SetFilePointer(188000, FILE_BEGIN);
	ULONG dwBytesRead = 0;
	m_pFileReader->Read(m_pData, m_buflen, &dwBytesRead);

	hr = SyncBuffer(&pos, 1);

	while ((pos < m_buflen) && (hr == S_OK) && (epgfound == false)) {
		
		epgfound = CheckForEPG(pos);

		if ((iterations > 20) && (epgfound == false)) { 
			hr = S_FALSE; 
		}
		else 
		{
			if (epgfound == false) {
				pos = pos + 188;
				if (pos >= m_buflen) {
					dwBytesRead = 0;
					hr = m_pFileReader->Read(m_pData, m_buflen, &dwBytesRead);
					iterations++;
					pos = 0;
					if (hr == S_OK) {hr = SyncBuffer(&pos, 1);};
				}
			}
		}
	};

	if (hr == S_FALSE) { return S_FALSE; };

	len = ((0x0F & m_pData[pos+29])<<8 | (0xFF & m_pData[pos+30]));

	if (len > 1000) { return S_FALSE;};

	DummyPos = min(len, 157);

	CpyRegion(m_pData, pos+31, m_pDummy, 0, DummyPos);
	
	while (pos < m_buflen && DummyPos < len) {
		pos = pos + 188;
		if (   ((0xFF & m_pData[pos+1]) == 0x00)
			&& ((0xFF & m_pData[pos+2]) == 0x12) ) {
			
			len1 = min(len-DummyPos, 183);
           
			CpyRegion(m_pData, pos+4, m_pDummy, DummyPos, len1);
			DummyPos += len1;
		};
	};

	ParseEISection(DummyPos);

	return S_OK;
}



bool CSMSourceFilter::ParseEsDescriptor(int pos, int lastpos)
{
	bool AC3Stream = false;
	WORD DescTag;

    while (pos < lastpos) {
		DescTag = (0xFF & m_pData[pos]);
		if (DescTag == 0x6a) { AC3Stream = true; };
		if (DescTag == 0x0a) {};
		if (DescTag == 0x52) {};
		pos = pos + (int)(0xFF & m_pData[pos+1]) + 2;
	};

	return AC3Stream;
}


HRESULT CSMSourceFilter::ParseEISection (int lDataLength)
{
	int pos = 0;
	WORD DescTag;
	int DescLen;

	while (pos < lDataLength) {

		DescTag = (0xFF & m_pDummy[pos]);
		DescLen = (int)(0xFF & m_pDummy[pos+1]);

		if (DescTag == 0x4d) {
			// short event descriptor
			ParseShortEvent(pos, DescLen+2);
		} else {
		if (DescTag == 0x4e) {
			// extended event descriptor
			ParseExtendedEvent(pos, DescLen+2);
		}
		};
		
		pos = pos + DescLen + 2;
	};
	return S_OK;
}


HRESULT CSMSourceFilter::ParseShortEvent(int start, int lDataLength) 
{

	CpyRegion(m_pDummy, start, m_shortdescr, 0, lDataLength);

	return S_OK;
}


HRESULT CSMSourceFilter::ParseExtendedEvent(int start, int lDataLength)
{

	CpyRegion(m_pDummy, start, m_extenddescr, 0, lDataLength);

	return S_OK;

}


void CSMSourceFilter::CpyRegion(PBYTE buffer1, int start1, PBYTE buffer2, int start2, int len)
{
	for (int b = 0; b < len; b++) {
		buffer2[start2+b] = buffer1[start1 + b];
	}
}


HRESULT CSMSourceFilter::CheckForPMT(int pos)
{
	WORD pid;
	WORD channeloffset;
	WORD EsDescLen;
	WORD StreamType;
	bool AC3Stream = false;

	if ( (0x30&m_pData[pos+3])==0x10 && m_pData[pos+4]==0 && 
		  m_pData[pos+5]==2 && (0xf0&m_pData[pos+6])==0xb0 ) { 
				
		m_pmtpid = (WORD)(0x1F & m_pData[pos+1])<<8 | (0xFF & m_pData[pos+2]);
		m_pcrpid = (WORD)(0x1F & m_pData[pos+13])<<8 | (0xFF & m_pData[pos+14]);

		m_sidpid = (WORD)(0xFF & m_pData[pos+8])<<8 | (0xFF & m_pData[pos+9]);

		channeloffset = (WORD)(0x0F & m_pData[pos+15])<<8 | (0xFF & m_pData[pos+16]);
		         
		for (int b=17+channeloffset+pos; b<pos+182; b=b+5) {

			if ( (0xe0&m_pData[b+1]) == 0xe0 ) {

				pid = (0x1F & m_pData[b+1])<<8 | (0xFF & m_pData[b+2]);
				EsDescLen = (0x0F&m_pData[b+3]<<8 | 0xFF&m_pData[b+4]);

				StreamType = ( 0xFF&m_pData[b] );
				AC3Stream = ParseEsDescriptor(b + 5, b + 5 + EsDescLen);

				if (StreamType == 0x02) { m_vpid = pid; };

				if ((StreamType == 0x03) || (StreamType == 0x04)) {
					if (m_apid != 0) {
						m_a2pid = pid;
					}
					else {
							m_apid = pid;
					}
				};

				if (StreamType == 0x06) { 

					if (AC3Stream) {	m_ac3pid = pid; };
				};

				b+=EsDescLen;
			}
		}
		return S_OK;
	}
	else
	{
		return S_FALSE;
	}
}


REFERENCE_TIME CSMSourceFilter::GetNextPCR(int* a, int step)
{
	REFERENCE_TIME pcrtime;
	HRESULT hr = S_OK;

	pcrtime = 0;

	while( pcrtime == 0 && hr == S_OK)
    {	
		hr = SyncBuffer(a, step);
		if (S_FALSE == CheckForPCR(*a, &pcrtime)) {
			*a = *a + (step * 188);
		} else {
			hr = S_FALSE;
		}
	}

	return pcrtime;
}




STDMETHODIMP CSMSourceFilter::Load(LPCOLESTR pszFileName,const AM_MEDIA_TYPE *pmt)
{
    // Is this a valid filename supplied
	CAutoLock lock(&m_Lock);


    CheckPointer(pszFileName,E_POINTER);
	HRESULT hr = S_OK;

    if(wcslen(pszFileName) > MAX_PATH)
        return ERROR_FILENAME_EXCED_RANGE;

    // Take a copy of the filename

    m_pFileName = new WCHAR[1+lstrlenW(pszFileName)];
    if (m_pFileName == 0)
        return E_OUTOFMEMORY;

	lstrcpyW(m_pFileName,pszFileName);
	
	hr = OpenFile(0);
	if (FAILED(hr)) 
    {
        return VFW_E_INVALIDMEDIATYPE;
    }

//	hr = GetPids();
//	if (FAILED(hr)) 
//    {
//        return VFW_E_INVALID_MEDIA_TYPE;
//    }
	
//	__int64 llStartPosition;

//	if (FAILED(hr = m_pFileReader->GetFileSize(&llStartPosition, &m_filelength)))
//	{ 
//		return S_FALSE;
//    }

	CloseFile();

	if(GetFilterGraph())
		GetFilterGraph()->SetDefaultSyncSource();

    return hr;

}

STDMETHODIMP CSMSourceFilter::GetVideoPid(WORD *pVPid)
{
    if(!pVPid)
        return E_INVALIDARG;

    CAutoLock lock(&m_Lock);
    
    *pVPid = m_vpid;

    return NOERROR;

}


STDMETHODIMP CSMSourceFilter::GetAudioPid(WORD *pAPid)
{
    if(!pAPid)
        return E_INVALIDARG;

    CAutoLock lock(&m_Lock);

    *pAPid = m_apid;

    return NOERROR;

}

STDMETHODIMP CSMSourceFilter::GetAC3Pid(WORD *pAC3Pid)
{
    if(!pAC3Pid)
        return E_INVALIDARG;

    CAutoLock lock(&m_Lock);

    *pAC3Pid = m_ac3pid;

    return NOERROR;

}


STDMETHODIMP CSMSourceFilter::GetAudio2Pid(WORD *pA2Pid)
{
    if(!pA2Pid)
        return E_INVALIDARG;

    CAutoLock lock(&m_Lock);

    *pA2Pid = m_a2pid;

    return NOERROR;

}

STDMETHODIMP CSMSourceFilter::GetSIDPid(WORD *pSIDPid)
{
    if(!pSIDPid)
        return E_INVALIDARG;

    CAutoLock lock(&m_Lock);

    *pSIDPid = m_sidpid;

    return NOERROR;

}


STDMETHODIMP CSMSourceFilter::GetPMTPid(WORD *pPMTPid)
{
    if(!pPMTPid)
        return E_INVALIDARG;

    CAutoLock lock(&m_Lock);

    *pPMTPid = m_pmtpid;

    return NOERROR;

}


STDMETHODIMP CSMSourceFilter::GetPCRPid(WORD *pPCRPid)
{
    if(!pPCRPid)
        return E_INVALIDARG;

    CAutoLock lock(&m_Lock);

    *pPCRPid = m_pcrpid;

    return NOERROR;

}


STDMETHODIMP CSMSourceFilter::GetDur(REFERENCE_TIME *dur)
{
    if(!dur)
        return E_INVALIDARG;

    CAutoLock lock(&m_Lock);

    *dur = m_Duration;

    return NOERROR;

}


STDMETHODIMP CSMSourceFilter::GetShortDescr (BYTE *pointer)
{
	if (!pointer)
		  return E_INVALIDARG;

	CAutoLock lock(&m_Lock);

	if ((int)m_shortdescr[1] < 126) {

		CpyRegion(m_shortdescr, 0, pointer, 0, (int)m_shortdescr[1]+2);
	};

	return NOERROR;
}


STDMETHODIMP CSMSourceFilter::GetExtendedDescr (BYTE *pointer)
{
	if (!pointer)
		  return E_INVALIDARG;

	CAutoLock lock(&m_Lock);

	CpyRegion(m_extenddescr, 0, pointer, 0, 600);

	return NOERROR;
}

