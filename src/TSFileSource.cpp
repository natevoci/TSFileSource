/**
*  TSFileSource.cpp
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

#include <streams.h>

#include "bdaiface.h"
#include "ks.h"
#include "ksmedia.h"
#include "bdamedia.h"
#include "mediaformats.h"

#include "TSFileSource.h"
#include "TSFileSourceGuids.h"

CUnknown * WINAPI CTSFileSourceFilter::CreateInstance(LPUNKNOWN punk, HRESULT *phr)
{
	ASSERT(phr);
	CTSFileSourceFilter *pNewObject = new CTSFileSourceFilter(punk, phr);

	if (pNewObject == NULL) {
		if (phr)
			*phr = E_OUTOFMEMORY;
	}

	return pNewObject;
}

// Constructor
CTSFileSourceFilter::CTSFileSourceFilter(IUnknown *pUnk, HRESULT *phr) :
	CSource(NAME("CTSFileSourceFilter"), pUnk, CLSID_TSFileSource),
	m_pPin(NULL)
{
	ASSERT(phr);

	m_pFileReader = new FileReader();
	m_pPidParser = new PidParser(m_pFileReader);
	m_pDemux = new Demux(m_pPidParser);
	m_pPin = new CTSFileSourcePin(GetOwner(), this, m_pFileReader, m_pPidParser, phr);

	if (m_pPin == NULL) {
		if (phr)
			*phr = E_OUTOFMEMORY;
		return;
	}

//**********************************************************************************************
//Registry Additions

	m_pRegStore = new CRegStore();
	m_pSettingsStore = new CSettingsStore();

	// Load Registry Settings data
	GetRegStore("default");

//Property Page Additions

	m_PropOpen = false;
//*********************************************************************************************

}

CTSFileSourceFilter::~CTSFileSourceFilter()
{

//**********************************************************************************************
//Registry Additions

	delete 	m_pRegStore;
	delete  m_pSettingsStore;

//**********************************************************************************************

	delete m_pDemux;
	delete m_pFileReader;
	delete m_pPidParser;

	delete m_pPin;
}

STDMETHODIMP CTSFileSourceFilter::NonDelegatingQueryInterface(REFIID riid, void ** ppv)
{
	CheckPointer(ppv,E_POINTER);
	CAutoLock lock(&m_Lock);

	// Do we have this interface

	if (riid == IID_ITSFileSource)
	{
		return GetInterface((ITSFileSource*)this, ppv);
	}
	if (riid == IID_IFileSourceFilter)
	{
		return GetInterface((IFileSourceFilter*)this, ppv);
	}
	if (riid == IID_ISpecifyPropertyPages)
	{
		return GetInterface((ISpecifyPropertyPages*)(this), ppv);
	}
	if ((riid == IID_IMediaPosition || riid == IID_IMediaSeeking))
	{
		return m_pPin->NonDelegatingQueryInterface(riid, ppv);
	}

	return CSource::NonDelegatingQueryInterface(riid, ppv);

} // NonDelegatingQueryInterface

CBasePin * CTSFileSourceFilter::GetPin(int n)
{
	if (n == 0) {
		return m_pPin;
	} else {
		return NULL;
	}
}

int CTSFileSourceFilter::GetPinCount()
{
	return 1;
}


STDMETHODIMP CTSFileSourceFilter::Run(REFERENCE_TIME tStart)
{
	CAutoLock cObjectLock(m_pLock);

	if (m_pFileReader->IsFileInvalid())
	{
		HRESULT hr = m_pFileReader->OpenFile();
		if (FAILED(hr))
			return hr;

		REFERENCE_TIME start, stop;
		m_pPin->GetPositions(&start, &stop);

		if (start > 0)
		{
			FileSeek(start);
		}
	}

//**********************************************************************************************
//Property Page Additions
/*
	HRESULT hr2 = CSource::Run(tStart);
	if (m_pPidParser->pidArray.Count() >= 2 && !m_PropOpen)
	{
		m_PropOpen = true;
		ShowFilterProperties();
		m_PropOpen = false;
	}
	return hr2;
*/
//Removed	return CSource::Run(tStart);
	return CSource::Run(tStart);
//**********************************************************************************************

}

HRESULT CTSFileSourceFilter::Pause()
{
	CAutoLock cObjectLock(m_pLock);
	return CSource::Pause();
}

STDMETHODIMP CTSFileSourceFilter::Stop()
{
	CAutoLock cObjectLock(m_pLock);
	CAutoLock lock(&m_Lock);
	m_pFileReader->CloseFile();
	return CSource::Stop();
}

HRESULT CTSFileSourceFilter::FileSeek(REFERENCE_TIME seektime)
{
	if (m_pFileReader->IsFileInvalid())
	{
		return S_FALSE;
	}

	if (seektime > m_pPidParser->pids.dur)
	{
		return S_FALSE;
	}

	if (m_pPidParser->pids.dur > 10)
	{
		__int64 filelength = 0;
		m_pFileReader->GetFileSize(&filelength);

		//DOUBLE fileindex;
		//fileindex = (DOUBLE)((DOUBLE)((DOUBLE)(filelength / (DOUBLE)m_pPidParser->pids.dur) * (DOUBLE)seektime));
		//m_pFileReader->SetFilePointer(fileindex, FILE_BEGIN);

		// shifting right by 14 rounds the seek and duration time down to the
		// nearest multiple 16.384 ms. More than accurate enough for our seeks.

		__int64 nFileIndex;
		nFileIndex = filelength * (seektime>>14) / (m_pPidParser->pids.dur>>14);
		m_pFileReader->SetFilePointer(nFileIndex, FILE_BEGIN);

	}

	return S_OK;
}

HRESULT CTSFileSourceFilter::OnConnect()
{
	return m_pDemux->AOnConnect(GetFilterGraph());
}

/*PidParser *CTSFileSourceFilter::get_Pids()
{
	return m_pPidParser;
}

FileReader *CTSFileSourceFilter::get_FileReader()
{
	return m_pFileReader;
}*/


STDMETHODIMP CTSFileSourceFilter::GetPages(CAUUID *pPages)
{
	if (pPages == NULL) return E_POINTER;
	pPages->cElems = 1;
	pPages->pElems = (GUID*)CoTaskMemAlloc(sizeof(GUID));
	if (pPages->pElems == NULL)
	{
		return E_OUTOFMEMORY;
	}
	pPages->pElems[0] = CLSID_TSFileSourceProp;
	return S_OK;
}

STDMETHODIMP CTSFileSourceFilter::Load(LPCOLESTR pszFileName,const AM_MEDIA_TYPE *pmt)
{
	HRESULT hr;
	hr = m_pFileReader->SetFileName(pszFileName);
	if (FAILED(hr))
		return hr;

	hr = m_pFileReader->OpenFile();
	if (FAILED(hr))
		return VFW_E_INVALIDMEDIATYPE;

	RefreshPids();

//**********************************************************************************************
//Program Registry Additions

	if (m_pPidParser->m_TStreamID && m_pPidParser->pidArray.Count() >= 2)
	{
		std::string saveName = m_pSettingsStore->getName();

		TCHAR cNID_TSID_ID[20];
		sprintf(cNID_TSID_ID, "%i:%i", m_pPidParser->m_NetworkID, m_pPidParser->m_TStreamID);

		// Load Registry Settings data
		GetRegStore(cNID_TSID_ID);

		if (m_pPidParser->set_ProgramSID() == S_OK)
		{
			WORD pgmNumber = m_pPidParser->get_ProgramNumber();
			m_pPin->DeliverBeginFlush();
			m_pPidParser->set_ProgramNumber(pgmNumber);
			m_pPin->SetDuration(m_pPidParser->pids.dur);
			m_pPin->DeliverEndFlush();
		}
		m_pSettingsStore->setName(saveName);
	}

//*********************************************************************************************

	RefreshDuration();

	CAutoLock lock(&m_Lock);
	m_pFileReader->CloseFile();

	return hr;
}

HRESULT CTSFileSourceFilter::RefreshPids()
{
	CAutoLock lock(&m_Lock);
	return m_pPidParser->ParseFromFile();
}

HRESULT CTSFileSourceFilter::RefreshDuration()
{
	return m_pPin->SetDuration(m_pPidParser->pids.dur);
}

STDMETHODIMP CTSFileSourceFilter::GetCurFile(LPOLESTR * ppszFileName,AM_MEDIA_TYPE *pmt)
{

	CheckPointer(ppszFileName, E_POINTER);
	*ppszFileName = NULL;

	LPOLESTR pFileName = NULL;
	HRESULT hr = m_pFileReader->GetFileName(&pFileName);
	if (FAILED(hr))
		return hr;

	if (pFileName != NULL)
	{
		*ppszFileName = (LPOLESTR)
		QzTaskMemAlloc(sizeof(WCHAR) * (1+lstrlenW(pFileName)));

		if (*ppszFileName != NULL)
		{
			lstrcpyW(*ppszFileName, pFileName);
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

STDMETHODIMP CTSFileSourceFilter::GetVideoPid(WORD *pVPid)
{
	if(!pVPid)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);

	*pVPid = m_pPidParser->pids.vid;

	return NOERROR;

}


STDMETHODIMP CTSFileSourceFilter::GetAudioPid(WORD *pAPid)
{
	if(!pAPid)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);

	*pAPid = m_pPidParser->pids.aud;

	return NOERROR;

}

STDMETHODIMP CTSFileSourceFilter::GetAudio2Pid(WORD *pA2Pid)
{
	if(!pA2Pid)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);

	*pA2Pid = m_pPidParser->pids.aud2;

	return NOERROR;

}

STDMETHODIMP CTSFileSourceFilter::GetAC3Pid(WORD *pAC3Pid)
{
	if(!pAC3Pid)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);

	*pAC3Pid = m_pPidParser->pids.ac3;

	return NOERROR;

}

//**********************************************************************************************
//Audio2 Additions

STDMETHODIMP CTSFileSourceFilter::GetAC3_2Pid(WORD *pAC3_2Pid)
{
	if(!pAC3_2Pid)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);

	*pAC3_2Pid = m_pPidParser->pids.ac3_2;

	return NOERROR;

}

//**********************************************************************************************

STDMETHODIMP CTSFileSourceFilter::GetTelexPid(WORD *pTelexPid)
{
	if(!pTelexPid)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);

	*pTelexPid = m_pPidParser->pids.txt;

	return NOERROR;

}

//***********************************************************************************************
//NID Additions

STDMETHODIMP CTSFileSourceFilter::GetNIDPid(WORD *pNIDPid)
{
	if(!pNIDPid)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);

	*pNIDPid = m_pPidParser->m_NetworkID;

	return NOERROR;

}

//ONID Additions
	
STDMETHODIMP CTSFileSourceFilter::GetONIDPid(WORD *pONIDPid)
{
	if(!pONIDPid)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);

	*pONIDPid = m_pPidParser->m_ONetworkID;

	return NOERROR;

}

//TSID Additions

STDMETHODIMP CTSFileSourceFilter::GetTSIDPid(WORD *pTSIDPid)
{
	if(!pTSIDPid)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);

	*pTSIDPid = m_pPidParser->m_TStreamID;

	return NOERROR;

}
	
//***********************************************************************************************

STDMETHODIMP CTSFileSourceFilter::GetPMTPid(WORD *pPMTPid)
{
	if(!pPMTPid)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);

	*pPMTPid = m_pPidParser->pids.pmt;

	return NOERROR;

}

STDMETHODIMP CTSFileSourceFilter::GetSIDPid(WORD *pSIDPid)
{
	if(!pSIDPid)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);

	*pSIDPid = m_pPidParser->pids.sid;

	return NOERROR;

}

STDMETHODIMP CTSFileSourceFilter::GetPCRPid(WORD *pPCRPid)
{
	if(!pPCRPid)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);

	*pPCRPid = m_pPidParser->pids.pcr;

	return NOERROR;

}

STDMETHODIMP CTSFileSourceFilter::GetDuration(REFERENCE_TIME *dur)
{
	if(!dur)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);

	*dur = m_pPidParser->pids.dur;

	return NOERROR;

}

STDMETHODIMP CTSFileSourceFilter::GetShortDescr (BYTE *pointer)
{
	if (!pointer)
		  return E_INVALIDARG;

	CAutoLock lock(&m_Lock);

	m_pPidParser->get_ShortDescr(pointer);

	return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::GetExtendedDescr (BYTE *pointer)
{
	if (!pointer)
		  return E_INVALIDARG;

	CAutoLock lock(&m_Lock);

	m_pPidParser->get_ExtendedDescr(pointer);

	return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::GetPgmNumb(WORD *pPgmNumb)
{
	if(!pPgmNumb)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);

	//*pPgmNumb = m_pgmnumb + 1;
	*pPgmNumb = m_pPidParser->get_ProgramNumber() + 1;

	return NOERROR;

}

STDMETHODIMP CTSFileSourceFilter::GetPgmCount(WORD *pPgmCount)
{
	if(!pPgmCount)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);

	*pPgmCount = m_pPidParser->pidArray.Count();

	return NOERROR;

}

STDMETHODIMP CTSFileSourceFilter::SetPgmNumb(WORD PgmNumb)
{
	CAutoLock lock(&m_Lock);

//**********************************************************************************************
//Another bug fix

	//If only one program don't change it
	if (m_pPidParser->pidArray.Count() < 2)
		return NOERROR;

	int PgmNumber = PgmNumb;
	PgmNumber --;
	if (PgmNumber >= m_pPidParser->pidArray.Count())
	{
		PgmNumber = m_pPidParser->pidArray.Count() - 1;
	}
	else if (PgmNumber <= -1)
	{
		PgmNumber = 0;
	}

	m_pPin->DeliverBeginFlush();
	m_pPidParser->set_ProgramNumber((WORD)PgmNumber);
	m_pPin->SetDuration(m_pPidParser->pids.dur);
	m_pDemux->AOnConnect(GetFilterGraph());
	m_pPin->DeliverEndFlush();
	return NOERROR;
	
//Removed	PgmNumb -= 1;
//Removed	if (PgmNumb >= m_pPidParser->pidArray.Count())
//Removed	{
//Removed		PgmNumb = m_pPidParser->pidArray.Count() - 1;
//Removed	}
//Removed	else if (PgmNumb < 0)
//Removed	{
//Removed		PgmNumb = 0;
//Removed	}

//Removed	m_pPin->DeliverBeginFlush();
//Removed	m_pPidParser->set_ProgramNumber(PgmNumb);
//Removed	m_pPin->SetDuration(m_pPidParser->pids.dur);
//Removed	m_pDemux->AOnConnect(GetFilterGraph());
//Removed	m_pPin->DeliverEndFlush();
//Removed	return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::NextPgmNumb(void)
{
	CAutoLock lock(&m_Lock);

//**********************************************************************************************
//Another bug fix

	//If only one program don't change it
	if (m_pPidParser->pidArray.Count() < 2)
		return NOERROR;

//**********************************************************************************************

	WORD PgmNumb = m_pPidParser->get_ProgramNumber();
	PgmNumb++;
	if (PgmNumb >= m_pPidParser->pidArray.Count())
	{
		PgmNumb = 0;
	}
	m_pPin->DeliverBeginFlush();
	m_pPidParser->set_ProgramNumber(PgmNumb);
	m_pPin->SetDuration(m_pPidParser->pids.dur);
	m_pDemux->AOnConnect(GetFilterGraph());
	m_pPin->DeliverEndFlush();
	return NOERROR;
}

//**********************************************************************************************
//Prev button Additions

STDMETHODIMP CTSFileSourceFilter::PrevPgmNumb(void)
{
	CAutoLock lock(&m_Lock);

	//If only one program don't change it
	if (m_pPidParser->pidArray.Count() < 2)
		return NOERROR;

	int PgmNumb = m_pPidParser->get_ProgramNumber();
	PgmNumb--;
	if (PgmNumb < 0)
	{
		PgmNumb = m_pPidParser->pidArray.Count() - 1;
	}

	m_pPin->DeliverBeginFlush();
	m_pPidParser->set_ProgramNumber((WORD)PgmNumb);
	m_pPin->SetDuration(m_pPidParser->pids.dur);
	m_pDemux->AOnConnect(GetFilterGraph());
	m_pPin->DeliverEndFlush();
	return NOERROR;
}

//**********************************************************************************************
STDMETHODIMP CTSFileSourceFilter::GetTsArray(ULONG *pPidArray)
{
	if(!pPidArray)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);

	m_pPidParser->get_CurrentTSArray(pPidArray);
	return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::GetAC3Mode(WORD *pAC3Mode)
{
	if(!pAC3Mode)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);
	*pAC3Mode = m_pDemux->get_AC3Mode();
	return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::SetAC3Mode(WORD AC3Mode)
{
	CAutoLock lock(&m_Lock);
	m_pDemux->set_AC3Mode(AC3Mode);
	m_pDemux->AOnConnect(GetFilterGraph());
	return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::GetMP2Mode(WORD *pMP2Mode)
{
	if(!pMP2Mode)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);
	*pMP2Mode = m_pDemux->get_MPEG2AudioMediaType();
	return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::SetMP2Mode(WORD MP2Mode)
{
	CAutoLock lock(&m_Lock);
	m_pDemux->set_MPEG2AudioMediaType(MP2Mode);
	m_pDemux->AOnConnect(GetFilterGraph());
	return NOERROR;
}

//**********************************************************************************************
//Audio2 Additions

STDMETHODIMP CTSFileSourceFilter::GetAudio2Mode(WORD *pAudio2Mode)
{
	if(!pAudio2Mode)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);
	*pAudio2Mode = m_pDemux->get_MPEG2Audio2Mode();
	return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::SetAudio2Mode(WORD Audio2Mode)
{
	CAutoLock lock(&m_Lock);
	m_pDemux->set_MPEG2Audio2Mode(Audio2Mode);
	m_pDemux->AOnConnect(GetFilterGraph());
	return NOERROR;
}

//**********************************************************************************************

STDMETHODIMP CTSFileSourceFilter::GetAutoMode(WORD *AutoMode)
{
	if(!AutoMode)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);
	*AutoMode = m_pDemux->get_Auto();
	return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::SetAutoMode(WORD AutoMode)
{
	CAutoLock lock(&m_Lock);
	m_pDemux->set_Auto(AutoMode);
	m_pDemux->AOnConnect(GetFilterGraph());
	return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::GetDelayMode(WORD *DelayMode)
{
	if(!DelayMode)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);
	m_pFileReader->get_DelayMode(DelayMode);
	return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::SetDelayMode(WORD DelayMode)
{
	CAutoLock lock(&m_Lock);
	m_pFileReader->set_DelayMode(DelayMode);
	m_pDemux->AOnConnect(GetFilterGraph());
	return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::GetRateControlMode(WORD* pRateControl)
{
	if (!pRateControl)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);
	*pRateControl = m_pPin->get_RateControl();
	return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::SetRateControlMode(WORD RateControl)
{
	CAutoLock lock(&m_Lock);
	m_pPin->set_RateControl(RateControl);
//	m_pDemux->AOnConnect(GetFilterGraph());
	return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::GetCreateTSPinOnDemux(WORD *pbCreatePin)
{
	if(!pbCreatePin)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);
	*pbCreatePin = m_pDemux->get_CreateTSPinOnDemux();
	return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::SetCreateTSPinOnDemux(WORD bCreatePin)
{
	CAutoLock lock(&m_Lock);
	m_pDemux->set_CreateTSPinOnDemux(bCreatePin);
	m_pDemux->AOnConnect(GetFilterGraph());
	return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::GetReadOnly(WORD *ReadOnly)
{
	if(!ReadOnly)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);
	m_pFileReader->get_ReadOnly(ReadOnly);
	return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::GetBitRate(long *pRate)
{
    if(!pRate)
        return E_INVALIDARG;

    CAutoLock lock(&m_Lock);

    *pRate = m_pPin->get_BitRate();

    return NOERROR;

}

STDMETHODIMP CTSFileSourceFilter::SetBitRate(long Rate)
{
    CAutoLock lock(&m_Lock);

    m_pPin->set_BitRate(Rate);

    return NOERROR;

}

//**********************************************************************************************
//Registry Additions

STDMETHODIMP CTSFileSourceFilter::SetRegStore(LPTSTR nameReg)
{

	char name[128] = "";
	sprintf(name, "%s", nameReg);

	if ((strcmp(name, "user")!=0) && (strcmp(name, "default")!=0))
	{
		std::string saveName = m_pSettingsStore->getName();
		m_pSettingsStore->setName(nameReg);
		m_pSettingsStore->setProgramSIDReg((int)m_pPidParser->pids.sid);
		m_pSettingsStore->setAudio2ModeReg((BOOL)m_pDemux->get_MPEG2Audio2Mode());
		m_pSettingsStore->setAC3ModeReg((BOOL)m_pDemux->get_AC3Mode());
		m_pRegStore->setSettingsInfo(m_pSettingsStore);
		m_pSettingsStore->setName(saveName);
	}
	else
	{
		WORD delay;
		m_pFileReader->get_DelayMode(&delay);
		m_pSettingsStore->setDelayModeReg((BOOL)delay);
		m_pSettingsStore->setRateControlModeReg((BOOL)m_pPin->get_RateControl());
		m_pSettingsStore->setAutoModeReg((BOOL)m_pDemux->get_Auto());
		m_pSettingsStore->setMP2ModeReg((BOOL)m_pDemux->get_MPEG2AudioMediaType());
		m_pSettingsStore->setAudio2ModeReg((BOOL)m_pDemux->get_MPEG2Audio2Mode());
		m_pSettingsStore->setAC3ModeReg((BOOL)m_pDemux->get_AC3Mode());
		m_pSettingsStore->setCreateTSPinOnDemuxReg((BOOL)m_pDemux->get_CreateTSPinOnDemux());

		m_pRegStore->setSettingsInfo(m_pSettingsStore);
	}
    return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::GetRegStore(LPTSTR nameReg)
{
	char name[128] = "";
	sprintf(name, "%s", nameReg);

	std::string saveName = m_pSettingsStore->getName();

	// Load Registry Settings data
	m_pSettingsStore->setName(nameReg);

	if(m_pRegStore->getSettingsInfo(m_pSettingsStore))
	{
		if ((strcmp(name, "user")!=0) && (strcmp(name, "default")!=0))
		{
			m_pPidParser->set_SIDPid(m_pSettingsStore->getProgramSIDReg());
			m_pDemux->set_AC3Mode(m_pSettingsStore->getAC3ModeReg());
			m_pDemux->set_MPEG2Audio2Mode(m_pSettingsStore->getAudio2ModeReg());
			m_pSettingsStore->setName(saveName);
		}
		else
		{	
			m_pFileReader->set_DelayMode(m_pSettingsStore->getDelayModeReg());
			m_pDemux->set_Auto(m_pSettingsStore->getAutoModeReg());
			m_pDemux->set_MPEG2AudioMediaType(m_pSettingsStore->getMP2ModeReg());
			m_pDemux->set_MPEG2Audio2Mode(m_pSettingsStore->getAudio2ModeReg());
			m_pDemux->set_AC3Mode(m_pSettingsStore->getAC3ModeReg());
			m_pDemux->set_CreateTSPinOnDemux(m_pSettingsStore->getCreateTSPinOnDemuxReg());
			m_pPin->set_RateControl(m_pSettingsStore->getRateControlModeReg());
		}
	}

    return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::SetRegSettings()
{
	SetRegStore("user");
    return NOERROR;
}


STDMETHODIMP CTSFileSourceFilter::GetRegSettings()
{
	GetRegStore("user");
    return NOERROR;
}

//Program Registry Additions

STDMETHODIMP CTSFileSourceFilter::SetRegProgram()
{
	if (m_pPidParser->pids.sid && m_pPidParser->m_TStreamID)
	{
		TCHAR cNID_TSID_ID[10];
		sprintf(cNID_TSID_ID, "%i:%i", m_pPidParser->m_NetworkID, m_pPidParser->m_TStreamID);
		SetRegStore(cNID_TSID_ID);
	}
    return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::ShowFilterProperties()
{

	HRESULT hr = 0;
    HWND    phWnd = (HWND)CreateEvent(NULL, FALSE, FALSE, NULL);

	ULONG refCount;
	IEnumFilters * piEnumFilters2 = NULL;
	hr = GetFilterGraph()->EnumFilters(&piEnumFilters2);
	if SUCCEEDED(hr)
	{
		IBaseFilter * piFilter2;
		while ( piEnumFilters2->Next(1, &piFilter2, 0) == NOERROR )
		{
			ISpecifyPropertyPages* piProp2 = NULL;
			hr = piFilter2->QueryInterface(IID_ISpecifyPropertyPages, (void **)&piProp2);
			if ((hr == S_OK) && (piProp2 != NULL))
			{
				FILTER_INFO filterInfo2;
				hr = piFilter2->QueryFilterInfo(&filterInfo2);
				LPOLESTR fileName;
				m_pFileReader->GetFileName(&fileName);

				if (wcscmp(fileName, filterInfo2.achName) == 0)
				{
					IUnknown *piFilterUnk;
					piFilter2->QueryInterface(IID_IUnknown, (void **)&piFilterUnk);

					CAUUID caGUID;
					piProp2->GetPages(&caGUID);
					piProp2->Release();
					OleCreatePropertyFrame(phWnd, 0, 0, filterInfo2.achName, 1, &piFilterUnk, caGUID.cElems, caGUID.pElems, 0, 0, NULL);
					piFilterUnk->Release();
					CoTaskMemFree(caGUID.pElems);
				}
			}
			refCount = piFilter2->Release();
		}
		refCount = piEnumFilters2->Release();
	}
	if (hr == S_OK)
		return S_FALSE;

	return hr;
}
//**********************************************************************************************

//////////////////////////////////////////////////////////////////////////
// End of interface implementations
//////////////////////////////////////////////////////////////////////////
//MessageBox(NULL, name, TEXT("name"), MB_OK);
//MessageBox(NULL, user, TEXT("user"), MB_OK);
//MessageBox(NULL, ddefault, TEXT("ddefault"), MB_OK);
//TCHAR sz[100];
//sprintf(sz, "%u", m_pPidParser->get_ProgramNumber()+1);
//MessageBox(NULL, sz, TEXT("m_pPidParser"), MB_OK);



///////////////////////////////////////////////////////////////////////////////////////



