/**
*  SMSink.cpp
*  Copyright (C) 2005      nate
*  Copyright (C) 2006      bear
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
*  nate can be reached on the forums at
*    http://forums.dvbowners.com/
*/


#include <streams.h>
#include "SMSinkGuids.h"
#include "SMSink.h"
#include "RegStore.h"



// Constructor

CDumpFilter::CDumpFilter(CDump *pDump,
                         LPUNKNOWN pUnk,
                         CCritSec *pLock,
                         HRESULT *phr) :
    CBaseFilter(NAME("SharedMemory Sink Filter"), pUnk, pLock, CLSID_SMSink),
    m_pDump(pDump)
{
}


//
// GetPin
//
CBasePin * CDumpFilter::GetPin(int n)
{
    if (n == 0) {
        return m_pDump->m_pPin;
    } else {
        return NULL;
    }
}


//
// GetPinCount
//
int CDumpFilter::GetPinCount()
{
    return 1;
}


//
// Stop
//
// Overriden to close the dump file
//
STDMETHODIMP CDumpFilter::Stop()
{
    CAutoLock cObjectLock(m_pLock);

    if (m_pDump)
        m_pDump->CloseFile();
    
    return CBaseFilter::Stop();
}


//
// Pause
//
// Overriden to open the dump file
//
STDMETHODIMP CDumpFilter::Pause()
{
    CAutoLock cObjectLock(m_pLock);

    if (m_pDump)
    {
        // GraphEdit calls Pause() before calling Stop() for this filter.
        // If we have encountered a write error (such as disk full),
        // then stopping the graph could cause our log to be deleted
        // (because the current log file handle would be invalid).
        // 
        // To preserve the log, don't open/create the log file on pause
        // if we have previously encountered an error.  The write error
        // flag gets cleared when setting a new log file name or
        // when restarting the graph with Run().
        if (!m_pDump->m_fWriteError)
        {
            m_pDump->OpenFile();
        }
    }

    return CBaseFilter::Pause();
}


//
// Run
//
// Overriden to open the dump file
//
STDMETHODIMP CDumpFilter::Run(REFERENCE_TIME tStart)
{
    CAutoLock cObjectLock(m_pLock);

    // Clear the global 'write error' flag that would be set
    // if we had encountered a problem writing the previous dump file.
    // (eg. running out of disk space).
    //
    // Since we are restarting the graph, a new file will be created.
    m_pDump->m_fWriteError = FALSE;

    if (m_pDump)
        m_pDump->OpenFile();

    return CBaseFilter::Run(tStart);
}


//
//  Definition of CDumpInputPin
//
CDumpInputPin::CDumpInputPin(CDump *pDump,
                             LPUNKNOWN pUnk,
                             CBaseFilter *pFilter,
                             CCritSec *pLock,
                             CCritSec *pReceiveLock,
                             HRESULT *phr) :

    CRenderedInputPin(NAME("CDumpInputPin"),
                  pFilter,                   // Filter
                  pLock,                     // Locking
                  phr,                       // Return code
                  L"Input"),                 // Pin name
    m_pReceiveLock(pReceiveLock),
    m_pDump(pDump),
    m_tLast(0)
{
}


//
// CheckMediaType
//
// Check if the pin can support this specific proposed type and format
//
HRESULT CDumpInputPin::CheckMediaType(const CMediaType *)
{
    return S_OK;
}


//
// BreakConnect
//
// Break a connection
//
HRESULT CDumpInputPin::BreakConnect()
{
    if (m_pDump->m_pPosition != NULL) {
        m_pDump->m_pPosition->ForceRefresh();
    }

    return CRenderedInputPin::BreakConnect();
}


//
// ReceiveCanBlock
//
// We don't hold up source threads on Receive
//
STDMETHODIMP CDumpInputPin::ReceiveCanBlock()
{
    return S_FALSE;
}


//
// Receive
//
// Do something with this media sample
//
STDMETHODIMP CDumpInputPin::Receive(IMediaSample *pSample)
{
    CheckPointer(pSample,E_POINTER);

    CAutoLock lock(m_pReceiveLock);
    PBYTE pbData;

    // Has the filter been stopped yet?
    if (m_pDump->m_hFile == INVALID_HANDLE_VALUE) {
        return NOERROR;
    }

    REFERENCE_TIME tStart, tStop;
    pSample->GetTime(&tStart, &tStop);

    DbgLog((LOG_TRACE, 1, TEXT("tStart(%s), tStop(%s), Diff(%d ms), Bytes(%d)"),
           (LPCTSTR) CDisp(tStart),
           (LPCTSTR) CDisp(tStop),
           (LONG)((tStart - m_tLast) / 10000),
           pSample->GetActualDataLength()));

    m_tLast = tStart;

    // Copy the data to the file

    HRESULT hr = pSample->GetPointer(&pbData);
    if (FAILED(hr)) {
        return hr;
    }

    return m_pDump->Write(pbData, pSample->GetActualDataLength());
}

//
// EndOfStream
//
STDMETHODIMP CDumpInputPin::EndOfStream(void)
{
    CAutoLock lock(m_pReceiveLock);
    return CRenderedInputPin::EndOfStream();

} // EndOfStream


//
// NewSegment
//
// Called when we are seeked
//
STDMETHODIMP CDumpInputPin::NewSegment(REFERENCE_TIME tStart,
                                       REFERENCE_TIME tStop,
                                       double dRate)
{
    m_tLast = 0;
    return S_OK;

} // NewSegment


//
//  CDump class
//
CDump::CDump(LPUNKNOWN pUnk, HRESULT *phr) :
    CUnknown(NAME("CDump"), pUnk),
    m_pFilter(NULL),
    m_pPin(NULL),
    m_pPosition(NULL),
    m_hFile(INVALID_HANDLE_VALUE),
    m_pFileName(0),
    m_fWriteError(0),
	currentPosition(0),
	currentFileLength(0),
	chunkReserve(1),
	currentEndPointer(1),
	fileBuffering(1),
//******************************************************************************************
//TimeShift Additions

	timeShiftEnable(1),
	bufferCycleMode(0),
	timeShiftSize(8),
	fileReserveSize(2),
	lastFileLength(0),

//******************************************************************************************
//Shared Memory Additions

	m_pSharedMemory(NULL),

//******************************************************************************************

	flushBeforeReserve(0)
//	startMemFile(0),
//	endMemFile(0),
//	memFileBlockSize(0),
//	lpMapAddress(0),
//	numberOfBlocks(0),
//	hMapFile(INVALID_HANDLE_VALUE)
{
    ASSERT(phr);
    
    m_pFilter = new CDumpFilter(this, GetOwner(), &m_Lock, phr);
    if (m_pFilter == NULL) {
        if (phr)
            *phr = E_OUTOFMEMORY;
        return;
    }

    m_pPin = new CDumpInputPin(this,GetOwner(),
                               m_pFilter,
                               &m_Lock,
                               &m_ReceiveLock,
                               phr);
    if (m_pPin == NULL) {
        if (phr)
            *phr = E_OUTOFMEMORY;
        return;
    }

	// get the overrides from the register
	CRegStore *store = new CRegStore("SOFTWARE\\SMSinkFilter");

	chunkReserve = 1;//store->getInt("chunkReserve", 1);
	flushBeforeReserve = 0;//store->getInt("flushBeforeReserve", 0);
	currentEndPointer = 1;//store->getInt("currentEndPointer", 1);
	fileBuffering = 1;//store->getInt("fileBuffering", 1);

//******************************************************************************************
//TimeShift Additions

	timeShiftEnable = 1;//store->getInt("enableTimeShift", 1);
	timeShiftSize = max(store->getInt("numBlocks", 8), 2);
	fileReserveSize = max(store->getInt("blockSize", 2), 2);
	int sharedMemorySize = timeShiftSize;
	sharedMemorySize *= fileReserveSize;
	sharedMemorySize = min(sharedMemorySize, 64);
	fileReserveSize = min(fileReserveSize, sharedMemorySize/timeShiftSize);
	timeShiftSize = min(sharedMemorySize/fileReserveSize, timeShiftSize);

//******************************************************************************************
//Shared Memory Additions

	m_pSharedMemory = new SharedMemory(fileReserveSize *(__int64)1048576*(__int64)timeShiftSize);

//******************************************************************************************

	delete store;

/*
	// Get the system allocation granularity.
	DWORD dwSysGran;
	SYSTEM_INFO SysInfo;
	GetSystemInfo(&SysInfo);
	dwSysGran = SysInfo.dwAllocationGranularity;

	memFileBlockSize = (100000000 / dwSysGran) * dwSysGran;
*/
}


//
// SetFileName
//
// Implemented for IFileSinkFilter support
//
STDMETHODIMP CDump::SetFileName(LPCOLESTR pszFileName,const AM_MEDIA_TYPE *pmt)
{
    // Is this a valid filename supplied

    CheckPointer(pszFileName,E_POINTER);
    if(wcslen(pszFileName) > MAX_PATH)
        return ERROR_FILENAME_EXCED_RANGE;

    // Take a copy of the filename

    m_pFileName = new WCHAR[1+lstrlenW(pszFileName)];
    if (m_pFileName == 0)
        return E_OUTOFMEMORY;

    lstrcpyW(m_pFileName,pszFileName);

    // Clear the global 'write error' flag that would be set
    // if we had encountered a problem writing the previous dump file.
    // (eg. running out of disk space).
    m_fWriteError = FALSE;

    // Create the file then close it

    HRESULT hr = OpenFile();
    CloseFile();

    return hr;

} // SetFileName


//
// GetCurFile
//
// Implemented for IFileSinkFilter support
//
STDMETHODIMP CDump::GetCurFile(LPOLESTR * ppszFileName,AM_MEDIA_TYPE *pmt)
{
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
        pmt->majortype = MEDIATYPE_NULL;
        pmt->subtype = MEDIASUBTYPE_NULL;
    }

    return S_OK;

} // GetCurFile


// Destructor

CDump::~CDump()
{
    CloseFile();

    delete m_pPin;
    delete m_pFilter;
    delete m_pPosition;
    delete m_pFileName;
	delete m_pSharedMemory;
}


//
// CreateInstance
//
// Provide the way for COM to create a dump filter
//
CUnknown * WINAPI CDump::CreateInstance(LPUNKNOWN punk, HRESULT *phr)
{
    ASSERT(phr);
    
    CDump *pNewObject = new CDump(punk, phr);
    if (pNewObject == NULL) {
        if (phr)
            *phr = E_OUTOFMEMORY;
    }

    return pNewObject;

} // CreateInstance


//
// NonDelegatingQueryInterface
//
// Override this to say what interfaces we support where
//
STDMETHODIMP CDump::NonDelegatingQueryInterface(REFIID riid, void ** ppv)
{
    CheckPointer(ppv,E_POINTER);
    CAutoLock lock(&m_Lock);

    // Do we have this interface

    if (riid == IID_IFileSinkFilter) {
        return GetInterface((IFileSinkFilter *) this, ppv);
    } 
    else if (riid == IID_IBaseFilter || riid == IID_IMediaFilter || riid == IID_IPersist) {
        return m_pFilter->NonDelegatingQueryInterface(riid, ppv);
    } 
    else if (riid == IID_IMediaPosition || riid == IID_IMediaSeeking) {
        if (m_pPosition == NULL) 
        {

            HRESULT hr = S_OK;
            m_pPosition = new CPosPassThru(NAME("Dump Pass Through"),
                                           (IUnknown *) GetOwner(),
                                           (HRESULT *) &hr, m_pPin);
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

} // NonDelegatingQueryInterface


//
// OpenFile
//
// Opens the file ready for dumping
//
HRESULT CDump::OpenFile()
{
    TCHAR *pFileName = NULL;

    // Is the file already opened
    if (m_hFile != INVALID_HANDLE_VALUE) {
        return NOERROR;
    }

    // Has a filename been set yet
    if (m_pFileName == NULL) {
        return ERROR_INVALID_NAME;
    }

    // Convert the UNICODE filename if necessary

#if defined(WIN32) && !defined(UNICODE)
    char convert[MAX_PATH];

    if(!WideCharToMultiByte(CP_ACP,0,m_pFileName,-1,convert,MAX_PATH,0,0))
        return ERROR_INVALID_NAME;

    pFileName = convert;
#else
    pFileName = m_pFileName;
#endif


	// work out the rile flags to use
	DWORD fileCreateFlags = 0;

	if(fileBuffering == 0)
		fileCreateFlags = fileCreateFlags | FILE_FLAG_WRITE_THROUGH;


    m_hFile = m_pSharedMemory->CreateFile((LPCTSTR) pFileName,   // The filename
                         GENERIC_WRITE,         // File access
                         FILE_SHARE_READ,       // Share access
                         NULL,                  // Security
                         CREATE_ALWAYS,         // Open flags
                         fileCreateFlags,			// More flags
                         NULL);                 // Template


	/*
    // Try to open the file
    m_hFile = CreateFile((LPCTSTR) pFileName,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
		NULL);
	*/
	

    if (m_hFile == INVALID_HANDLE_VALUE) 
    {
        DWORD dwErr = GetLastError();
        return HRESULT_FROM_WIN32(dwErr);
    }

//******************************************************************************************
//TimeShift Additions

	if(currentEndPointer == 1 && (chunkReserve | timeShiftEnable) == 1)

// Removed	if(currentEndPointer == 1 && chunkReserve == 1)

//******************************************************************************************

	{
		TCHAR infoFile[512];
		strcpy_s(infoFile, pFileName);
		strcat_s(infoFile, ".info");


		m_hInfoFile = m_pSharedMemory->CreateFile((LPCTSTR) infoFile,
			GENERIC_WRITE,
			FILE_SHARE_READ,
			NULL,
			CREATE_ALWAYS,
			fileCreateFlags,
			NULL);

		/*
		m_hInfoFile = CreateFile((LPCTSTR) infoFile,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
			NULL);
			*/

//******************************************************************************************
//TimeShift Additions

		if (m_hInfoFile)
		{
			LARGE_INTEGER li;
			li.QuadPart = 0;
			li.HighPart = 0;
			m_pSharedMemory->SetFilePointer(
					m_hInfoFile,
					li.LowPart,
					&li.HighPart,
					FILE_BEGIN);

			DWORD written = 0;
			li.QuadPart = 0;
			li.HighPart = 0;
			m_pSharedMemory->WriteFile(m_hInfoFile, &li.QuadPart, sizeof(__int64), &written, NULL);

			li.QuadPart = sizeof(__int64);
			li.HighPart = 0;
			m_pSharedMemory->SetFilePointer(
					m_hInfoFile,
					li.LowPart,
					&li.HighPart,
					FILE_BEGIN);

			li.QuadPart = 0;
			li.HighPart = 0;

			if(timeShiftEnable == 1)
				m_pSharedMemory->WriteFile(m_hInfoFile, &li.QuadPart, sizeof(__int64), &written, NULL);
		}
		bufferCycleMode = 0;
//******************************************************************************************

	}

    return S_OK;

} // Open


//
// CloseFile
//
// Closes any dump file we have opened
//
HRESULT CDump::CloseFile()
{
    // Must lock this section to prevent problems related to
    // closing the file while still receiving data in Receive()
    CAutoLock lock(&m_Lock);

    if (m_hFile == INVALID_HANDLE_VALUE)
	{
        return NOERROR;
    }

//******************************************************************************************
//TimeShift Additions

	if((chunkReserve | timeShiftEnable) == 1)

//Removed	if(chunkReserve == 1)

//******************************************************************************************

	{
		LARGE_INTEGER li;
		li.QuadPart = currentPosition;
		li.HighPart = 0;

		m_pSharedMemory->SetFilePointer(
			m_hFile,
			li.LowPart,
			&li.HighPart,
			FILE_BEGIN);

//******************************************************************************************
//TimeShift Additions

//DWORD written = 0;
//__int64 endpos = 0x4747474747474747;
//m_pSharedMemory->WriteFile(m_hFile, &endpos, sizeof(__int64), &written, NULL);

		if(bufferCycleMode)
		{
			li.QuadPart = currentFileLength;
			li.HighPart = 0;
			m_pSharedMemory->SetFilePointer(
					m_hFile,
					li.LowPart,
					&li.HighPart,
					FILE_BEGIN);
		}

//******************************************************************************************

		SetEndOfFile(m_hFile);

		currentPosition = 0;
		currentFileLength = 0;
	}


	m_pSharedMemory->CloseHandle(m_hFile);
	m_hFile = INVALID_HANDLE_VALUE; // Invalidate the file 


//******************************************************************************************
//TimeShift Additions

	if(currentEndPointer == 1 && (chunkReserve | timeShiftEnable) == 1)

//Removed	if(currentEndPointer == 1 && chunkReserve == 1)

//******************************************************************************************

	{
		if (m_hInfoFile != INVALID_HANDLE_VALUE)
		{
			m_pSharedMemory->CloseHandle(m_hInfoFile);
			m_hInfoFile = INVALID_HANDLE_VALUE;
		}

		TCHAR *pFileName = NULL;
#if defined(WIN32) && !defined(UNICODE)
	    char convert[MAX_PATH];

		if(!WideCharToMultiByte(CP_ACP,0,m_pFileName,-1,convert,MAX_PATH,0,0))
			return ERROR_INVALID_NAME;

		pFileName = convert;
#else
	    pFileName = m_pFileName;
#endif

		TCHAR infoFile[512];
		strcpy_s(infoFile, pFileName);
		strcat_s(infoFile, ".info");

//******************************************************************************************
//TimeShift Additions

		if(timeShiftEnable != 1 )//|| !bufferCycleMode)

//******************************************************************************************

			m_pSharedMemory->DeleteFile(infoFile);
	}

//******************************************************************************************
//TimeShift Additions

	bufferCycleMode = 0;
	lastFileLength = 0;

//******************************************************************************************


    return NOERROR;

} // Open

//
// Write
//
// Write raw data to the file
//
HRESULT CDump::Write(PBYTE pbData, LONG lDataLength)
{
    // If the file has already been closed, don't continue
    if (m_hFile == INVALID_HANDLE_VALUE)
	{
        return S_FALSE;
    }

	HRESULT hr = S_OK;
	LARGE_INTEGER li;

//******************************************************************************************
//TimeShift Additions

	unsigned char *pbDataSave = pbData;

	if(timeShiftEnable == 1)
	{
		// do the reserve chunk is needed
		if((__int64)(lDataLength + currentPosition) > (__int64)(timeShiftSize * (__int64)1048576 * (__int64)fileReserveSize))
		{
			//Enable buffer Cycle Mode
			bufferCycleMode = 1;
			lastFileLength = lDataLength + currentPosition;

			DWORD written = 0;
//UnlockFile(m_hFile, (ULONG)currentPosition, 0, (ULONG)(currentFileLength - currentPosition), 0);
			if (!m_pSharedMemory->WriteFile(m_hFile, pbData, (ULONG)(currentFileLength - currentPosition), &written, NULL)){
				return E_FAIL;
			}
//LockFile(m_hFile, (ULONG)currentPosition, 0, (ULONG)(currentFileLength - currentPosition), 0);

			lDataLength -= (LONG)written;
			pbData += written;

//			li.QuadPart = 0;
//			li.HighPart = 0;

//			m_pSharedMemory->SetFilePointer(
//				m_hFile,
//				li.LowPart,
//				&li.HighPart,
//				FILE_BEGIN);

			currentPosition = 0;
		}
	}

//******************************************************************************************



	// do the reserve chunk is needed
	if((lDataLength + currentPosition) > currentFileLength)
	{
		// increate the length of the file by 100 meg
		currentFileLength = currentFileLength + (__int64)1048576 * (__int64)fileReserveSize;

		if(flushBeforeReserve == 1)
		{
			m_pSharedMemory->FlushFileBuffers(m_hFile);

			if(currentEndPointer == 1)
				m_pSharedMemory->FlushFileBuffers(m_hInfoFile);
		}

//******************************************************************************************
//TimeShift Additions

		if(currentEndPointer == 1 && (chunkReserve | timeShiftEnable) == 1)

//Removed		if(chunkReserve == 1)

//******************************************************************************************

		{
			li.QuadPart = currentFileLength;
			li.HighPart = 0;

			m_pSharedMemory->SetFilePointer(m_hFile, li.LowPart, &li.HighPart, FILE_BEGIN);
			DWORD dwErr = GetLastError();
			if ((DWORD)hr == (DWORD)0xFFFFFFFF && dwErr)
			{
				return E_FAIL;
			}

			m_pSharedMemory->SetEndOfFile(m_hFile);

//******************************************************************************************
//TimeShift Additions
//Removed
//			li.QuadPart = currentPosition;
//			li.HighPart = 0;

//			m_pSharedMemory->SetFilePointer(
//				m_hFile,
//				li.LowPart,
//				&li.HighPart,
//				FILE_BEGIN);
//******************************************************************************************
		}
	}

//******************************************************************************************
//TimeShift Additions

	li.QuadPart = currentPosition;
	li.HighPart = 0;

	hr = m_pSharedMemory->SetFilePointer(
		m_hFile,
		li.LowPart,
		&li.HighPart,
		FILE_BEGIN);

//******************************************************************************************
	DWORD dwErr = GetLastError();
	if ((DWORD)hr == (DWORD)0xFFFFFFFF && dwErr)
	{
		return E_FAIL;
	}

	DWORD written = 0;
//UnlockFile(m_hFile, (ULONG)currentPosition, 0, lDataLength, 0);
	if (!m_pSharedMemory->WriteFile(m_hFile, pbData, lDataLength, &written, NULL)){
		return E_FAIL;
	}
//LockFile(m_hFile, (ULONG)currentPosition, 0, lDataLength, 0);

	currentPosition = currentPosition + written;//lDataLength;
	// only write to end pointer file if it is needed

//******************************************************************************************
//TimeShift Additions

	if(currentEndPointer == 1 && (chunkReserve | timeShiftEnable) == 1)

//Removed	if(currentEndPointer == 1 && chunkReserve == 1)

//******************************************************************************************

	{

//******************************************************************************************
//TimeShift Additions

		if(timeShiftEnable == 1)
		{
			if(!bufferCycleMode)
			{
				li.QuadPart = 0;
				li.HighPart = 0;
				m_pSharedMemory->SetFilePointer(m_hInfoFile, li.LowPart, &li.HighPart, FILE_BEGIN);
				DWORD dwErr = GetLastError();
				if ((DWORD)hr == (DWORD)0xFFFFFFFF && dwErr)
				{
					return E_FAIL;
				}

				if (!m_pSharedMemory->WriteFile(m_hInfoFile, &currentPosition, sizeof(__int64), &written, NULL)){
					return E_FAIL;
				}

				li.QuadPart = sizeof(__int64);
				li.HighPart = 0;
				m_pSharedMemory->SetFilePointer(m_hInfoFile, li.LowPart, &li.HighPart, FILE_BEGIN);
				dwErr = GetLastError();
				if ((DWORD)hr == (DWORD)0xFFFFFFFF && dwErr)
				{
					return E_FAIL;
				}

				__int64 zero = 0;
				if (!m_pSharedMemory->WriteFile(m_hInfoFile, &zero, sizeof(__int64), &written, NULL)){
					return E_FAIL;
				}
			}
			else
			{
//				li.QuadPart = 0;
//				li.HighPart = 0;
//				m_pSharedMemory->SetFilePointer(
//					m_hFile,
//					li.LowPart,
//					&li.HighPart,
//					FILE_BEGIN);

				li.QuadPart = 0;
				li.HighPart = 0;
				m_pSharedMemory->SetFilePointer(m_hInfoFile, li.LowPart, &li.HighPart, FILE_BEGIN);
				DWORD dwErr = GetLastError();
				if ((DWORD)hr == (DWORD)0xFFFFFFFF && dwErr)
				{
					return E_FAIL;
				}

				if (!m_pSharedMemory->WriteFile(m_hInfoFile, &currentFileLength, sizeof(__int64), &written, NULL)){
					return E_FAIL;
				}

				li.QuadPart = sizeof(__int64);
				li.HighPart = 0;
				m_pSharedMemory->SetFilePointer(m_hInfoFile, li.LowPart, &li.HighPart, FILE_BEGIN);
				dwErr = GetLastError();
				if ((DWORD)hr == (DWORD)0xFFFFFFFF && dwErr)
				{
					return E_FAIL;
				}

				if (!m_pSharedMemory->WriteFile(m_hInfoFile, &currentPosition, sizeof(__int64), &written, NULL)){
					return E_FAIL;
				}
			}
		}
		else
		{

//******************************************************************************************

			li.QuadPart = 0;
			li.HighPart = 0;

			m_pSharedMemory->SetFilePointer(m_hInfoFile, li.LowPart, &li.HighPart, FILE_BEGIN);
			DWORD dwErr = GetLastError();
			if ((DWORD)hr == (DWORD)0xFFFFFFFF && dwErr)
			{
				return E_FAIL;
			}

			if (!m_pSharedMemory->WriteFile(m_hInfoFile, &currentPosition, sizeof(__int64), &written, NULL)){
				return E_FAIL;
			}

//******************************************************************************************
//TimeShift Additions

		}

//******************************************************************************************

	}

//******************************************************************************************
//TimeShift Additions

	pbData = pbDataSave;

//******************************************************************************************

    return S_OK;
}


HRESULT CDump::HandleWriteFailure(void)
{
    DWORD dwErr = GetLastError();

    if (dwErr == ERROR_DISK_FULL)
    {
        // Close the dump file and stop the filter, 
        // which will prevent further write attempts
        m_pFilter->Stop();

        // Set a global flag to prevent accidental deletion of the dump file
        m_fWriteError = TRUE;

        // Display a message box to inform the developer of the write failure
        TCHAR szMsg[MAX_PATH + 80];
        wsprintf(szMsg, TEXT("The PC containing dump memory has run out of space, ")
                  TEXT("so the SMSink filter has been stopped.\r\n\r\n")
                  TEXT("You must set a new SMSink name or restart the graph ")
                  TEXT("to clear this filter error."));
        MessageBox(NULL, szMsg, TEXT("SMSink Filter failure"), MB_ICONEXCLAMATION);
    }

    return HRESULT_FROM_WIN32(dwErr);
}

