/**
*  TSFileSource.h
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

#ifndef TSFILESOURCE_H
#define TSFILESOURCE_H

#include <objbase.h>
#include "ITSFileSource.h"

class CTSFileSourceFilter;
#include "TSFileSourcePin.h"

#include "PidInfo.h"
#include "PidParser.h"
#include "FileReader.h"
#include "Demux.h"

/**********************************************
 *
 *  CTSFileSourceFilter Class
 *
 **********************************************/

class CTSFileSourceFilter : public CSource,
							public IFileSourceFilter,
							public ITSFileSource,
							public ISpecifyPropertyPages
{
	//friend class CTSFileSourcePin;
public:
	DECLARE_IUNKNOWN
	static CUnknown * WINAPI CreateInstance(LPUNKNOWN punk, HRESULT *phr);

private:
	//private contructor because CreateInstance creates this object
	CTSFileSourceFilter(IUnknown *pUnk, HRESULT *phr);
	~CTSFileSourceFilter();

	// Overriden to say what interfaces we support where
	STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void ** ppv);

public:
	// Pin enumeration
	CBasePin * GetPin(int n);
	int GetPinCount();

	STDMETHODIMP Run(REFERENCE_TIME tStart);
	STDMETHODIMP Pause();
	STDMETHODIMP Stop();



	HRESULT FileSeek(REFERENCE_TIME seektime);

	HRESULT OnConnect();

	PidParser *get_Pids();
	FileReader *get_FileReader();


protected:

	// ISpecifyPropertyPages
	STDMETHODIMP GetPages(CAUUID *pPages);

	// IFileSourceFilter
	STDMETHODIMP Load(LPCOLESTR pszFileName,const AM_MEDIA_TYPE *pmt);
	STDMETHODIMP GetCurFile(LPOLESTR * ppszFileName,AM_MEDIA_TYPE *pmt);

	// ITSFileSource
	STDMETHODIMP GetVideoPid(WORD *pVPid);
	STDMETHODIMP GetAudioPid(WORD *pAPid);
	STDMETHODIMP GetAudio2Pid(WORD *pA2Pid);
	STDMETHODIMP GetAC3Pid(WORD *pAC3Pid);
	STDMETHODIMP GetTelexPid(WORD *pTelexPid);
	STDMETHODIMP GetPMTPid(WORD *pPMTPid);
	STDMETHODIMP GetSIDPid(WORD *pSIDPid);
	STDMETHODIMP GetPCRPid(WORD * pcrpid);
	STDMETHODIMP GetDuration(REFERENCE_TIME * dur);
	STDMETHODIMP GetShortDescr(BYTE * POINTER);
	STDMETHODIMP GetExtendedDescr(BYTE * POINTER);

	STDMETHODIMP GetPgmNumb(WORD * pPgmNumb);
	STDMETHODIMP GetPgmCount(WORD * pPgmCount);
	STDMETHODIMP SetPgmNumb(WORD pPgmNumb);
	STDMETHODIMP NextPgmNumb(void);
	STDMETHODIMP GetTsArray(ULONG * pPidArray);

	STDMETHODIMP GetAC3Mode(WORD* pAC3Mode);
	STDMETHODIMP SetAC3Mode(WORD AC3Mode);

	STDMETHODIMP GetMP2Mode(WORD* pMP2Mode);
	STDMETHODIMP SetMP2Mode(WORD MP2Mode);

	STDMETHODIMP GetAutoMode(WORD* pDelayMode);
	STDMETHODIMP SetAutoMode(WORD AutoMode);

	STDMETHODIMP GetDelayMode(WORD* pAutoMode);
	STDMETHODIMP SetDelayMode(WORD DelayMode);

	STDMETHODIMP GetRateControlMode(WORD* pRateControl);
	STDMETHODIMP SetRateControlMode(WORD RateControl);

	STDMETHODIMP GetCreateTSPinOnDemux(WORD *pbCreatePin);
	STDMETHODIMP SetCreateTSPinOnDemux(WORD bCreatePin);

	STDMETHODIMP GetReadOnly(WORD* pFileMode);

//*********************************************************************************************
//Bitrate addition

	STDMETHODIMP GetBitRate(long* pRate);
	STDMETHODIMP SetBitRate(long Rate);

//*********************************************************************************************

protected:
	CTSFileSourcePin       *m_pPin;          // A simple rendered input pin

	PidParser *m_pPidParser;
	FileReader *m_pFileReader;
	Demux *m_pDemux;

	CCritSec m_Lock;                // Main renderer critical section

	__int64 m_filelength;

};

#endif