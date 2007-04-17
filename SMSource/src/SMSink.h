/**
*  SMSink.h
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

//******************************************************************************************
//Shared memory Additions
#include "SharedMemory.h"
//******************************************************************************************



class CDumpInputPin;
class CDump;
class CDumpFilter;

#define BYTES_PER_LINE 20
#define FIRST_HALF_LINE TEXT  ("   %2x %2x %2x %2x %2x %2x %2x %2x %2x %2x")
#define SECOND_HALF_LINE TEXT (" %2x %2x %2x %2x %2x %2x %2x %2x %2x %2x")


// Main filter object

class CDumpFilter : public CBaseFilter
{
    CDump * const m_pDump;

public:

    // Constructor
    CDumpFilter(CDump *pDump,
                LPUNKNOWN pUnk,
                CCritSec *pLock,
                HRESULT *phr);

    // Pin enumeration
    CBasePin * GetPin(int n);
    int GetPinCount();

    // Open and close the file as necessary
    STDMETHODIMP Run(REFERENCE_TIME tStart);
    STDMETHODIMP Pause();
    STDMETHODIMP Stop();
};


//  Pin object

class CDumpInputPin : public CRenderedInputPin
{
    CDump    * const m_pDump;           // Main renderer object
    CCritSec * const m_pReceiveLock;    // Sample critical section
    REFERENCE_TIME m_tLast;             // Last sample receive time

public:

    CDumpInputPin(CDump *pDump,
                  LPUNKNOWN pUnk,
                  CBaseFilter *pFilter,
                  CCritSec *pLock,
                  CCritSec *pReceiveLock,
                  HRESULT *phr);

    // Do something with this media sample
    STDMETHODIMP Receive(IMediaSample *pSample);
    STDMETHODIMP EndOfStream(void);
    STDMETHODIMP ReceiveCanBlock();

    // Write detailed information about this sample to a file
//    HRESULT WriteStringInfo(IMediaSample *pSample);

    // Check if the pin can support this specific proposed type and format
    HRESULT CheckMediaType(const CMediaType *);

    // Break connection
    HRESULT BreakConnect();

    // Track NewSegment
    STDMETHODIMP NewSegment(REFERENCE_TIME tStart,
                            REFERENCE_TIME tStop,
                            double dRate);
};


//  CDump object which has filter and pin members

class CDump : public CUnknown, public IFileSinkFilter
{
    friend class CDumpFilter;
    friend class CDumpInputPin;

    CDumpFilter   *m_pFilter;       // Methods for filter interfaces
    CDumpInputPin *m_pPin;          // A simple rendered input pin

    CCritSec m_Lock;                // Main renderer critical section
    CCritSec m_ReceiveLock;         // Sublock for received samples

    CPosPassThru *m_pPosition;      // Renderer position controls

    HANDLE   m_hFile;               // Handle to file for dumping
	HANDLE   m_hInfoFile;               // Handle to file for dumping

    LPOLESTR m_pFileName;           // The filename where we dump
    BOOL     m_fWriteError;

	__int64 currentPosition;
	__int64 currentFileLength;


	//__int64 startMemFile;
	//__int64 endMemFile;
	//__int64 memFileBlockSize;
	///LONG numberOfBlocks;
	//LPVOID lpMapAddress;
	//HANDLE hMapFile;

public:

    DECLARE_IUNKNOWN

    CDump(LPUNKNOWN pUnk, HRESULT *phr);
    ~CDump();

    static CUnknown * WINAPI CreateInstance(LPUNKNOWN punk, HRESULT *phr);

    // Write string, followed by CR/LF, to a file
    //void WriteString(TCHAR *pString);

    // Write raw data stream to a file
    HRESULT Write(PBYTE pbData, LONG lDataLength);

    // Implements the IFileSinkFilter interface
    STDMETHODIMP SetFileName(LPCOLESTR pszFileName,const AM_MEDIA_TYPE *pmt);
    STDMETHODIMP GetCurFile(LPOLESTR * ppszFileName,AM_MEDIA_TYPE *pmt);

private:

    // Overriden to say what interfaces we support where
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void ** ppv);

    // Open and write to the file
    HRESULT OpenFile();
    HRESULT CloseFile();
    HRESULT HandleWriteFailure();
//	HRESULT CreateNewFileMapping();

	int chunkReserve;
	int flushBeforeReserve;
	int currentEndPointer;
	int fileBuffering;

//******************************************************************************************
//TimeShift Additions

	int timeShiftEnable;
	int bufferCycleMode;
	int timeShiftSize;
	int	fileReserveSize;
	__int64 lastFileLength;

//******************************************************************************************
//Shared memory Additions

	SharedMemory *m_pSharedMemory;

//******************************************************************************************
};

