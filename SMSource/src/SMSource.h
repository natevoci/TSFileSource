/**
*  SMSource.h
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

#ifndef SMSOURCE_H
#define SMSOURCE_H

// Filter setup data

#include <streams.h>
#include "ISMSource.h"

class CSMSourceFilter;

#include "SMSourceClock.h"
#include "SMSourcePin.h"
#include "SharedMemory.h"
#include "FileReader.h"

/**********************************************
 *
 *  CSMSourceFilter Class
 *
 **********************************************/

class CSMSourceFilter : public ISpecifyPropertyPages,
						public IFileSourceFilter,
						public ISMSource,
						public CSource						
{
    friend class CSMStreamPin;

public:

	DECLARE_IUNKNOWN
	static CUnknown * WINAPI CreateInstance(LPUNKNOWN punk, HRESULT *phr);

    // Constructor should be private because you have to use CreateInstance
    CSMSourceFilter(IUnknown *pUnk, HRESULT *phr);
	~CSMSourceFilter();

   // Pin enumeration
    CBasePin * GetPin(int n);
    int GetPinCount();
	STDMETHODIMP FindPin(LPCWSTR Id, IPin ** ppPin);

	// Overriden to say what interfaces we support where
	STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void ** ppv);

    STDMETHODIMP Run(REFERENCE_TIME tStart);
    STDMETHODIMP Pause();
    STDMETHODIMP Stop();

    // Implements the IFileSourceFilter interface
    STDMETHODIMP Load(LPCOLESTR pszFileName,const AM_MEDIA_TYPE *pmt);
    STDMETHODIMP GetCurFile(LPOLESTR * ppszFileName,AM_MEDIA_TYPE *pmt);

	STDMETHODIMP GetVideoPid(WORD *pVPid);
	STDMETHODIMP GetAudioPid(WORD *pAPid);
    STDMETHODIMP GetAudio2Pid(WORD *pA2Pid);
	STDMETHODIMP GetAC3Pid(WORD *pAC3Pid);
	STDMETHODIMP GetSIDPid(WORD *pSIDPid);
	STDMETHODIMP GetPMTPid(WORD *pPMTPid);
	STDMETHODIMP GetPCRPid(WORD * pcrpid);
	STDMETHODIMP GetDur(REFERENCE_TIME * dur);
	STDMETHODIMP GetShortDescr(BYTE * POINTER);
	STDMETHODIMP GetExtendedDescr(BYTE * POINTER);

	STDMETHODIMP GetPages(CAUUID *pPages);
	HRESULT FileSeek(REFERENCE_TIME seektime);
	HRESULT OnConnect(IUnknown *pUnk);
	HRESULT CheckVAPids(int offset);
	HRESULT SetAccuratePos(REFERENCE_TIME tStart);

	HRESULT SyncBuffer(int* a, int step);
	HRESULT CheckForPMT(int pos);
	HRESULT CheckForPCR(int pos, REFERENCE_TIME* pcrtime);
	REFERENCE_TIME GetNextPCR(int* a, int step);
	bool ParseEsDescriptor(int pos, int lastpos);

	bool CheckForEPG(int pos);
	HRESULT CheckEPG();

	HRESULT ParseEISection (int lDataLength);
	HRESULT ParseShortEvent(int start, int IDataLength);
	HRESULT ParseExtendedEvent(int start, int IDataLength);

	void CpyRegion(PBYTE buffer1, int start1, PBYTE buffer2, int start2, int len);
	
	REFERENCE_TIME  m_StartPCR;
	REFERENCE_TIME  m_EndPCR;
	REFERENCE_TIME  m_Duration;

	__int64 m_filelength;

protected:

	CSMSourceClock *m_pClock;
	CSMStreamPin * m_pPin;          // A simple rendered input pin
	SharedMemory *m_pSharedMemory;
	FileReader *m_pFileReader;

private:

    CCritSec m_Lock;                // Main renderer critical section

    CPosPassThru *m_pPosition;      // Renderer position controls

    LPOLESTR m_pFileName;           // The filename where we stream
    BOOL     m_fReadError;


    // Open and write to the file
    HRESULT OpenFile(REFERENCE_TIME tStart);
    HRESULT CloseFile();
    HRESULT GetPids();
    HRESULT HandleReadFailure();

    int  m_buflen;
	BYTE   m_pData[94000];
	BYTE   m_pDummy[1000];

	WORD   m_vpid;
	WORD   m_apid;
	WORD   m_a2pid;
	WORD   m_ac3pid;
	WORD   m_sidpid;
	WORD   m_pmtpid;
	WORD   m_pcrpid;
	BYTE   m_shortdescr[128];
	BYTE   m_extenddescr[600];


};

#endif

