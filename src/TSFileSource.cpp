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
#include "TunerEvent.h"


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
	m_bRotEnable(FALSE),
	m_pPin(NULL)
{
	ASSERT(phr);

	m_pFileReader = new FileReader();
	m_pFileDuration = new FileReader();//Get Live File Duration Thread
	m_pPidParser = new PidParser(m_pFileReader);
	m_pDemux = new Demux(m_pPidParser, this);
	m_pStreamParser = new StreamParser(m_pPidParser, m_pDemux, &netArray);

	m_pPin = new CTSFileSourcePin(GetOwner(), this, phr);
	if (m_pPin == NULL)
	{
		if (phr)
			*phr = E_OUTOFMEMORY;
		return;
	}

	m_pClock = new CTSFileSourceClock( NAME(""), GetOwner(), phr );
	if (m_pClock == NULL)
	{
		if (phr)
			*phr = E_OUTOFMEMORY;
		return;
	}

	m_pTunerEvent = new TunerEvent(m_pDemux, GetOwner());
	m_pRegStore = new CRegStore();
	m_pSettingsStore = new CSettingsStore();

	// Load Registry Settings data
	GetRegStore("default");

	CMediaType cmt;
	cmt.InitMediaType();
	cmt.SetType(&MEDIATYPE_Stream);
	cmt.SetSubtype(&MEDIASUBTYPE_MPEG2_TRANSPORT);
	cmt.SetSubtype(&MEDIASUBTYPE_NULL);
	m_pPin->SetMediaType(&cmt);

	m_bThreadRunning = FALSE;
	m_bReload = FALSE;
	m_llLastMultiFileStart = 0;
	m_llLastMultiFileLength = 0;

}

CTSFileSourceFilter::~CTSFileSourceFilter()
{
	//Make sure the worker thread is stopped before we exit.
	//Also closes the files.m_hThread
	if (ThreadExists())
	{
		CAMThread::CallWorker(CMD_STOP);
		CAMThread::CallWorker(CMD_EXIT);
		CAMThread::Close();
	}

    if (m_dwGraphRegister)
    {
        RemoveGraphFromRot(m_dwGraphRegister);
        m_dwGraphRegister = 0;
    }

	m_pTunerEvent->UnRegisterForTunerEvents();
	m_pTunerEvent->Release();
	delete  m_pClock;
	delete	m_pDemux;
	delete 	m_pRegStore;
	delete  m_pSettingsStore;
	delete  m_pPidParser;
	delete	m_pStreamParser;
	delete	m_pPin;
	delete	m_pFileReader;
	delete  m_pFileDuration;
	netArray.Clear();

}

DWORD CTSFileSourceFilter::ThreadProc(void)
{
    HRESULT hr;  // the return code from calls
    Command com;

    do
    {
        com = GetRequest();
        if(com != CMD_INIT)
        {
			m_bThreadRunning = FALSE;
            DbgLog((LOG_ERROR, 1, TEXT("Thread expected init command")));
            Reply((DWORD) E_UNEXPECTED);
        }

    } while(com != CMD_INIT);

    DbgLog((LOG_TRACE, 1, TEXT("Worker thread initializing")));

	LPOLESTR fileName;
	m_pFileReader->GetFileName(&fileName);

	hr = m_pFileDuration->SetFileName(fileName);
	if (FAILED(hr))
    {
		m_bThreadRunning = FALSE;
		DbgLog((LOG_ERROR, 1, TEXT("ThreadCreate failed. Aborting thread.")));

        Reply(hr);  // send failed return code from ThreadCreate
        return 1;
    }

	hr = m_pFileDuration->OpenFile();
    if(FAILED(hr))
    {
		m_bThreadRunning = FALSE;
        DbgLog((LOG_ERROR, 1, TEXT("ThreadCreate failed. Aborting thread.")));

		hr = m_pFileDuration->CloseFile();
        Reply(hr);  // send failed return code from ThreadCreate
        return 1;
    }

	hr = m_pFileDuration->CloseFile();

    // Initialisation suceeded
    Reply(NOERROR);

    Command cmd;
    do
    {
        cmd = GetRequest();

        switch(cmd)
        {
            case CMD_EXIT:
				m_bThreadRunning = FALSE;
                Reply(NOERROR);
                break;

            case CMD_RUN:
                DbgLog((LOG_ERROR, 1, TEXT("CMD_RUN received before a CMD_PAUSE???")));
                // !!! fall through

            case CMD_PAUSE:
				if (SUCCEEDED(m_pFileDuration->OpenFile()))
				{
					m_bThreadRunning = TRUE;
					Reply(NOERROR);
					DoProcessingLoop();
					m_bThreadRunning = FALSE;
				}
                break;

            case CMD_STOP:
				m_pFileDuration->CloseFile();
				m_bThreadRunning = FALSE;
                Reply(NOERROR);
                break;

            default:
                DbgLog((LOG_ERROR, 1, TEXT("Unknown command %d received!"), cmd));
                Reply((DWORD) E_NOTIMPL);
                break;
        }

    } while(cmd != CMD_EXIT);

	m_pFileDuration->CloseFile();
	m_bThreadRunning = FALSE;
    DbgLog((LOG_TRACE, 1, TEXT("Worker thread exiting")));
    return 0;
}

//
// DoProcessingLoop
//
HRESULT CTSFileSourceFilter::DoProcessingLoop(void)
{
    Command com;

	m_pFileDuration->GetFileSize(&m_llLastMultiFileStart, &m_llLastMultiFileLength);
	m_rtLastCurrentTime = (REFERENCE_TIME)((REFERENCE_TIME)timeGetTime() * (REFERENCE_TIME)10000);

	int count = 1;

    do
    {
        while(!CheckRequest(&com))
        {
			HRESULT hr = S_OK;// if an error occurs.

			REFERENCE_TIME rtCurrentTime = (REFERENCE_TIME)((REFERENCE_TIME)timeGetTime() * (REFERENCE_TIME)10000);

			//Reparse the file for service change	
			if ((REFERENCE_TIME)(m_rtLastCurrentTime + (REFERENCE_TIME)10000000) < rtCurrentTime)
			{
				CNetRender::UpdateNetFlow(&netArray);
				count++;
				if(m_State == State_Running
					&& 1 == 1)
				{
					__int64 fileStart, filelength;
					m_pFileDuration->GetFileSize(&fileStart, &filelength);

					//Get the FileReader Type
					WORD bMultiMode;
					m_pFileDuration->get_ReaderMode(&bMultiMode);
					//Do MultiFile timeshifting mode
					if((bMultiMode & ((__int64)(fileStart + (__int64)5000000) < m_llLastMultiFileStart))
						|| (bMultiMode & (fileStart == 0) & ((__int64)(filelength + (__int64)5000000) < m_llLastMultiFileLength))
						|| (!bMultiMode & ((__int64)(filelength + (__int64)5000000) < m_llLastMultiFileLength))
						&& 1 == 1)
					{
						LPOLESTR pszFileName;
						if (m_pFileDuration->GetFileName(&pszFileName) == S_OK)
						{
							LPOLESTR pFileName = new WCHAR[1+lstrlenW(pszFileName)];
							if (pFileName != NULL)
							{
								lstrcpyW(pFileName, pszFileName);
								Load(pFileName, NULL);
								delete[] pFileName;
							}
						}
					}
					//check pids every 5sec or quicker if no pids parsed
					else if (count > 5 || !m_pPidParser->pidArray.Count())
					{
						//update the parser
							UpdatePidParser();
							count = 0;
					}
					
					m_llLastMultiFileStart = fileStart;
					m_llLastMultiFileLength = filelength;
				}
				m_rtLastCurrentTime = (REFERENCE_TIME)((REFERENCE_TIME)timeGetTime() * (REFERENCE_TIME)10000);
			}

			if(!m_pPidParser->m_ParsingLock && m_State != State_Stopped
				&& 1 == 1)
			{
				hr = m_pPin->UpdateDuration(m_pFileDuration);
				if (m_pDemux->CheckDemuxPids() == S_FALSE)
				{
					m_pDemux->AOnConnect();
					m_pStreamParser->SetStreamActive(m_pPidParser->get_ProgramNumber());
				}
			}

			Sleep(100);
        }

        // For all commands sent to us there must be a Reply call!
        if(com == CMD_RUN || com == CMD_PAUSE)
        {
			m_bThreadRunning = TRUE;
            Reply(NOERROR);
        }
        else if(com != CMD_STOP)
        {
            Reply((DWORD) E_UNEXPECTED);
            DbgLog((LOG_ERROR, 1, TEXT("Unexpected command!!!")));
        }
    } while(com != CMD_STOP);

	m_bThreadRunning = FALSE;

    return S_FALSE;
}

BOOL CTSFileSourceFilter::ThreadRunning(void)
{ 
	return m_bThreadRunning;
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
		return GetInterface((ISpecifyPropertyPages*)this, ppv);
	}
	if (riid == IID_IMediaPosition || riid == IID_IMediaSeeking)
	{
		return m_pPin->NonDelegatingQueryInterface(riid, ppv);
	}
    if (riid == IID_IAMFilterMiscFlags)
    {
		return GetInterface((IAMFilterMiscFlags*)this, ppv);
    }
	if (riid == IID_IAMStreamSelect && m_pDemux->get_Auto())
	{
		return GetInterface((IAMStreamSelect*)this, ppv);
	}
	if (riid == IID_IReferenceClock)
	{
		return GetInterface((IReferenceClock*)m_pClock, ppv);
	}
	if (riid == IID_IAsyncReader)
		if ((!m_pPidParser->pids.pcr
			&& !get_AutoMode()
			&& m_pPidParser->get_ProgPinMode())
			| m_pPidParser->get_AsyncMode())
		{
			return GetInterface((IAsyncReader*)this, ppv);
		}
	return CSource::NonDelegatingQueryInterface(riid, ppv);

} // NonDelegatingQueryInterface

//IAMFilterMiscFlags
ULONG STDMETHODCALLTYPE CTSFileSourceFilter::GetMiscFlags(void)
{
	return (ULONG)AM_FILTER_MISC_FLAGS_IS_SOURCE; 
}//IAMFilterMiscFlags

STDMETHODIMP  CTSFileSourceFilter::Count(DWORD *pcStreams) //IAMStreamSelect
{
	if(!pcStreams)
		return E_INVALIDARG;

	*pcStreams = 0;

	CAutoLock lock(&m_Lock);
	if (!m_pStreamParser->StreamArray.Count())
		return VFW_E_NOT_CONNECTED;

	*pcStreams = m_pStreamParser->StreamArray.Count();
	
	return S_OK;
} //IAMStreamSelect

STDMETHODIMP  CTSFileSourceFilter::Info( 
						long lIndex,
						AM_MEDIA_TYPE **ppmt,
						DWORD *pdwFlags,
						LCID *plcid,
						DWORD *pdwGroup,
						WCHAR **ppszName,
						IUnknown **ppObject,
						IUnknown **ppUnk) //IAMStreamSelect
{

	CAutoLock lock(&m_Lock);

	m_pStreamParser->ParsePidArray();
	m_pStreamParser->SetStreamActive(m_pPidParser->get_ProgramNumber());

	//Check if file has been parsed
	if (!m_pStreamParser->StreamArray.Count())
		return E_FAIL;
	
	//Check if in the bounds of index
	if(lIndex >= m_pStreamParser->StreamArray.Count() || lIndex < 0)
		return S_FALSE;

	if(ppmt) {

        *ppmt = (AM_MEDIA_TYPE *)CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE));
        if (*ppmt == NULL)
            return E_OUTOFMEMORY;

        **ppmt = (AM_MEDIA_TYPE)m_pStreamParser->StreamArray[lIndex].media;
	};

	if(pdwGroup)
		*pdwGroup = m_pStreamParser->StreamArray[lIndex].group;

	if(pdwFlags)
		*pdwFlags = m_pStreamParser->StreamArray[lIndex].flags;

	if(plcid)
		*plcid = m_pStreamParser->StreamArray[lIndex].lcid;

	if(ppszName) {

        *ppszName = (WCHAR *)CoTaskMemAlloc(sizeof(m_pStreamParser->StreamArray[lIndex].name));
        if (*ppszName == NULL)
            return E_OUTOFMEMORY;

		ZeroMemory(*ppszName, sizeof(m_pStreamParser->StreamArray[lIndex].name));
		wcscpy(*ppszName, m_pStreamParser->StreamArray[lIndex].name);
	}

	if(ppObject)
		*ppObject = (IUnknown *)m_pStreamParser->StreamArray[lIndex].object;

	if(ppUnk)
		*ppUnk = (IUnknown *)m_pStreamParser->StreamArray[lIndex].unk;

	return NOERROR;
} //IAMStreamSelect

STDMETHODIMP  CTSFileSourceFilter::Enable(long lIndex, DWORD dwFlags) //IAMStreamSelect
{
	//Test if ready
	if (!m_pStreamParser->StreamArray.Count())
		return VFW_E_NOT_CONNECTED;

	//Test if out of bounds
	if (lIndex >= m_pStreamParser->StreamArray.Count() || lIndex < 0)
		return E_INVALIDARG;

	int indexOffset = netArray.Count() + (int)(netArray.Count() != 0);

	if (!lIndex)
		ShowEPGInfo();
	else if (lIndex && lIndex < m_pStreamParser->StreamArray.Count() - indexOffset - 2){

		m_pDemux->m_StreamVid = m_pStreamParser->StreamArray[lIndex].Vid;
		m_pDemux->m_StreamH264 = m_pStreamParser->StreamArray[lIndex].H264;
		m_pDemux->m_StreamMpeg4 = m_pStreamParser->StreamArray[lIndex].Mpeg4;
		m_pDemux->m_StreamAC3 = m_pStreamParser->StreamArray[lIndex].AC3;
		m_pDemux->m_StreamMP2 = m_pStreamParser->StreamArray[lIndex].Aud;
		m_pDemux->m_StreamAAC = m_pStreamParser->StreamArray[lIndex].AAC;
		m_pDemux->m_StreamAud2 = m_pStreamParser->StreamArray[lIndex].Aud2;
		SetPgmNumb(m_pStreamParser->StreamArray[lIndex].group + 1);
		m_pStreamParser->SetStreamActive(m_pStreamParser->StreamArray[lIndex].group);
		m_pDemux->m_StreamVid = 0;
		m_pDemux->m_StreamH264 = 0;
		m_pDemux->m_StreamAC3 = 0;
		m_pDemux->m_StreamMP2 = 0;
		m_pDemux->m_StreamAud2 = 0;
		m_pDemux->m_StreamAAC = 0;
		SetRegProgram();
	}
	else if (lIndex == m_pStreamParser->StreamArray.Count() - indexOffset - 2) //File Menu title
	{}
	else if (lIndex == m_pStreamParser->StreamArray.Count() - indexOffset - 1) //Load file Browser
		Load(L"", NULL);
	else if (lIndex == m_pStreamParser->StreamArray.Count() - indexOffset) //Multicasting title
	{}
	else if (lIndex > m_pStreamParser->StreamArray.Count() - indexOffset) //Select multicast streams
	{
		WCHAR wfilename[MAX_PATH];
		lstrcpyW(wfilename, netArray[lIndex - (m_pStreamParser->StreamArray.Count() - netArray.Count())].fileName);
		if (SUCCEEDED(Load(wfilename, NULL)))
		{
//			m_pFileReader->set_DelayMode(TRUE);
//			m_pFileDuration->set_DelayMode(TRUE);
			REFERENCE_TIME stop, start = (__int64)max(0,(__int64)(m_pPidParser->pids.dur - 20000000));
			IMediaSeeking *pMediaSeeking;
			if(SUCCEEDED(GetFilterGraph()->QueryInterface(IID_IMediaSeeking, (void **) &pMediaSeeking)))
			{
				pMediaSeeking->SetPositions(&start, AM_SEEKING_AbsolutePositioning , &stop, AM_SEEKING_AbsolutePositioning);
				pMediaSeeking->Release();
			}
		}
	}
	return S_OK;

} //IAMStreamSelect


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

STDMETHODIMP CTSFileSourceFilter::FindPin(LPCWSTR Id, IPin ** ppPin)
{
	if (ppPin == NULL)
		return E_POINTER;

	CAutoLock lock(&m_Lock);
	if (!wcscmp(Id, m_pPin->CBasePin::Name())) {
		*ppPin = m_pPin;
		return S_OK;
	}

	return CSource::FindPin(Id, ppPin);
}

void CTSFileSourceFilter::ResetStreamTime(void)
{
	CAutoLock cObjectLock(m_pLock);
	CRefTime cTime;
	StreamTime(cTime);
	m_tStart = REFERENCE_TIME(m_tStart) + REFERENCE_TIME(cTime);
}


STDMETHODIMP CTSFileSourceFilter::Run(REFERENCE_TIME tStart)
{
	CAutoLock cObjectLock(m_pLock);

	if(IsActive() == State_Stopped)
	{
		if (m_pFileReader->IsFileInvalid())
		{
			HRESULT hr = m_pFileReader->OpenFile();
			if (FAILED(hr))
				return hr;
		}

		//Set our StreamTime Reference offset to zero
		m_tStart = tStart;

		REFERENCE_TIME start, stop;
		m_pPin->GetPositions(&start, &stop);

		//Start at least 100ms into file to skip header
		if (start == 0)
			start += 1000000;

//***********************************************************************************************
//Old Capture format Additions
		if (m_pPidParser->pids.pcr){ 
//***********************************************************************************************
			m_pPin->m_DemuxLock = TRUE;
			m_pPin->setPositions(&start, AM_SEEKING_AbsolutePositioning , &stop, AM_SEEKING_NoPositioning);
//m_pPin->PrintTime(TEXT("Run"), (__int64) start, 10000);
			m_pPin->m_DemuxLock = FALSE;
			m_pPin->m_IntBaseTimePCR = m_pPidParser->pids.start;
			m_pPin->m_IntStartTimePCR = m_pPidParser->pids.start;
			m_pPin->m_IntCurrentTimePCR = m_pPidParser->pids.start;
			m_pPin->m_IntEndTimePCR = m_pPidParser->pids.end;
		}

		SetTunerEvent();

		if (!ThreadRunning() && ThreadExists())
			CAMThread::CallWorker(CMD_RUN);
	}
	return CSource::Run(tStart);
}

HRESULT CTSFileSourceFilter::Pause()
{
	CAutoLock cObjectLock(m_pLock);

	if(!IsActive())
	{
		if (m_pFileReader->IsFileInvalid())
		{
			HRESULT hr = m_pFileReader->OpenFile();
			if (FAILED(hr))
				return hr;
		}

		REFERENCE_TIME start, stop;
		m_pPin->GetPositions(&start, &stop);
//m_pPin->PrintTime(TEXT("Pause"), (__int64) start, 10000);

		//Start at least 100ms into file to skip header
		if (start == 0)
			start += 1000000;

//***********************************************************************************************
//Old Capture format Additions
		if (m_pPidParser->pids.pcr){ 
//***********************************************************************************************
			m_pPin->m_DemuxLock = TRUE;
			m_pPin->setPositions(&start, AM_SEEKING_AbsolutePositioning , &stop, AM_SEEKING_NoPositioning);
			m_pPin->m_DemuxLock = FALSE;
			m_pPin->m_IntBaseTimePCR = m_pPidParser->pids.start;
			m_pPin->m_IntStartTimePCR = m_pPidParser->pids.start;
			m_pPin->m_IntCurrentTimePCR = m_pPidParser->pids.start;
			m_pPin->m_IntEndTimePCR = m_pPidParser->pids.end;
		}

		SetTunerEvent();

		if (!ThreadRunning() && ThreadExists())
			CAMThread::CallWorker(CMD_PAUSE);
	}

	return CSource::Pause();
}

STDMETHODIMP CTSFileSourceFilter::Stop()
{
	CAutoLock cObjectLock(m_pLock);
	CAutoLock lock(&m_Lock);

//	if (ThreadRunning() && ThreadExists())
//		CAMThread::CallWorker(CMD_STOP);

	HRESULT hr = CSource::Stop();

	m_pTunerEvent->UnRegisterForTunerEvents();
	m_pFileReader->CloseFile();
	return hr;
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
		__int64 fileStart;
		__int64 filelength = 0;
		m_pFileReader->GetFileSize(&fileStart, &filelength);

		// shifting right by 14 rounds the seek and duration time down to the
		// nearest multiple 16.384 ms. More than accurate enough for our seeks.
		__int64 nFileIndex = 0;

		if (m_pPidParser->pids.dur>>14)
			nFileIndex = filelength * (__int64)(seektime>>14) / (__int64)(m_pPidParser->pids.dur>>14);

		nFileIndex = min(filelength, nFileIndex);
		nFileIndex = max(300000, nFileIndex);
		m_pFileReader->setFilePointer(nFileIndex, FILE_BEGIN);
	}
	return S_OK;
}

HRESULT CTSFileSourceFilter::OnConnect()
{
	BOOL wasThreadRunning = FALSE;
	if (ThreadRunning() && ThreadExists()) {

		CAMThread::CallWorker(CMD_STOP);
		while (m_bThreadRunning){};
		wasThreadRunning = TRUE;
	}

	HRESULT hr = m_pDemux->AOnConnect();

	m_pStreamParser->SetStreamActive(m_pPidParser->get_ProgramNumber());

	if (wasThreadRunning)
		CAMThread::CallWorker(CMD_RUN);

	return hr;
}

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

STDMETHODIMP CTSFileSourceFilter::Load(LPCOLESTR pszFileName, const AM_MEDIA_TYPE *pmt)
{
	// Is this a valid filename supplied
	CheckPointer(pszFileName,E_POINTER);

	LPOLESTR wFileName = new WCHAR[lstrlenW(pszFileName)];
	lstrcpyW(wFileName, pszFileName);

	if (_wcsicmp(wFileName, L"") == 0)
	{
		TCHAR tmpFile[MAX_PATH];
		LPTSTR ptFilename = (LPTSTR)&tmpFile;
		ptFilename[0] = '\0';

		// Setup the OPENFILENAME structure
		OPENFILENAME ofn = { sizeof(OPENFILENAME), NULL, NULL,
							 TEXT("Transport Stream Files (*.mpg, *.ts, *.tsbuffer, *.vob)\0*.mpg;*.ts;*.tsbuffer;*.vob\0All Files\0*.*\0\0"), NULL,
							 0, 1,
							 ptFilename, MAX_PATH,
							 NULL, 0,
							 NULL,
							 TEXT("Open Files (TS File Source Filter)"),
							 OFN_FILEMUSTEXIST|OFN_HIDEREADONLY, 0, 0,
							 NULL, 0, NULL, NULL };

//		HWND hWnd = GetForegroundWindow();
//		ATLASSERT(::IsWindow(hWnd));
//		SetForegroundWindow(GetParent(hWnd));

//		SetActiveWindow(GetParent(hWnd));
//		BringWindowToTop(GetParent(hWnd));
		

		// Display the SaveFileName dialog.
		if( GetOpenFileName( &ofn ) != FALSE )
		{
			USES_CONVERSION;
			if(wFileName)
				delete[] wFileName;
			wFileName = new WCHAR[lstrlenW(T2W(ptFilename))];
			lstrcpyW(wFileName, T2W(ptFilename));
		}
		else
		{
			delete[] wFileName;
			return NO_ERROR;
		}
	}

	HRESULT hr;

	//
	// Check & create a NetSource Filtergraph and play the file 
	//
	NetInfo *netAddr = new NetInfo();
	netAddr->rotEnable = m_bRotEnable;

	//
	// Check if the FileName is a Network address 
	//
	if (CNetRender::IsMulticastAddress(wFileName, netAddr))
	{
		//
		// Check in the local array if the Network address already active 
		//
		int pos = 0;
		if (!CNetRender::IsMulticastActive(netAddr, &netArray, &pos))
		{
			//
			// Create the Network Filtergraph 
			//
			hr = CNetRender::CreateNetworkGraph(netAddr);
			if(FAILED(hr)  || (hr > 31))
			{
				delete[] netAddr;
				delete[] wFileName;
//				MessageBoxW(NULL, netAddr->fileName, L"Graph Builder Failed", NULL);
				return hr;
			}
			//Add the new filtergraph settings to the local array
			netArray.Add(netAddr);
			if(wFileName)
				delete[] wFileName;
			wFileName = new WCHAR[lstrlenW(netAddr->fileName)];
			lstrcpyW(wFileName, netAddr->fileName);
//			m_pFileReader->set_DelayMode(TRUE);
//			m_pFileDuration->set_DelayMode(TRUE);

		}
		else // If already running
		{
			if(wFileName)
				delete[] wFileName;
			wFileName = new WCHAR[lstrlenW(netArray[pos].fileName)];
			lstrcpyW(wFileName, netArray[pos].fileName);
			delete[] netAddr;
		}
	}
	else
		delete[] netAddr;

	for (int pos = 0; pos < netArray.Count(); pos++)
	{
		if (!lstrcmpiW(wFileName, netArray[pos].fileName))
			netArray[pos].playing = TRUE;
		else
			netArray[pos].playing = FALSE;
	}

	//Jump to a different Load method if already been set.
	if (ThreadExists() || IsActive())
	{
		hr = ReLoad(wFileName, pmt);
		delete[] wFileName;
		return hr;
	}

	//Get delay if we had been told to previously
	USHORT delay;
	m_pFileReader->get_DelayMode(&delay);

	delete m_pStreamParser;
	delete m_pDemux;
	delete m_pPidParser;
	delete m_pFileReader;
	delete m_pFileDuration;

	long length = lstrlenW(wFileName);
	if ((length < 9) || (_wcsicmp(wFileName+length-9, L".tsbuffer") != 0))
	{
		m_pFileReader = new FileReader();
		m_pFileDuration = new FileReader();//Get Live File Duration Thread
	}
	else
	{
		m_pFileReader = new MultiFileReader();
		m_pFileDuration = new MultiFileReader();
	}
	//m_pFileReader->SetDebugOutput(TRUE);
	//m_pFileDuration->SetDebugOutput(TRUE);

	m_pPidParser = new PidParser(m_pFileReader);
	m_pDemux = new Demux(m_pPidParser, this);
	m_pStreamParser = new StreamParser(m_pPidParser, m_pDemux, &netArray);

	// Load Registry Settings data
	GetRegStore("default");
	
	//Set delay if we had been told to previously
	if (delay)
	{
		m_pFileReader->set_DelayMode(delay);
		m_pFileDuration->set_DelayMode(delay);
	}

	hr = m_pFileReader->SetFileName(wFileName);
	if (FAILED(hr))
	{
		delete[] wFileName;
		return hr;
	}

	hr = m_pFileReader->OpenFile();
	if (FAILED(hr))
	{
		delete[] wFileName;
		return VFW_E_INVALIDMEDIATYPE;
	}

	CAMThread::Create();			 //Create our GetDuration thread
	if (ThreadExists())
		CAMThread::CallWorker(CMD_INIT); //Initalize our GetDuration thread

	set_ROTMode();

	__int64 fileStart;
	__int64	fileSize = 0;
	m_pFileReader->GetFileSize(&fileStart, &fileSize);
	//If this a file start then return null.
	if(fileSize < 2000000)
	{
		CAutoLock lock(&m_Lock);
		m_pFileReader->CloseFile();
		delete[] wFileName;
		return hr;
	}

	m_pFileReader->setFilePointer(300000, FILE_BEGIN);

	RefreshPids();

	LoadPgmReg();
	RefreshDuration();

	CAutoLock lock(&m_Lock);
	m_pFileReader->CloseFile();
	delete[] wFileName;

	m_pPin->m_IntBaseTimePCR = m_pPidParser->pids.start;
	m_pPin->m_IntStartTimePCR = m_pPidParser->pids.start;
	m_pPin->m_IntCurrentTimePCR = m_pPidParser->pids.start;
	m_pPin->m_IntEndTimePCR = m_pPidParser->pids.end;

	return hr;
}

STDMETHODIMP CTSFileSourceFilter::ReLoad(LPCOLESTR pszFileName, const AM_MEDIA_TYPE *pmt)
{
	HRESULT hr;

	BOOL wasThreadRunning = FALSE;
	if (ThreadRunning() && ThreadExists()) {

		CAMThread::CallWorker(CMD_STOP);
		while (m_bThreadRunning){};
		wasThreadRunning = TRUE;
	}

	BOOL bState_Running = FALSE;
	BOOL bState_Paused = FALSE;

	if (m_State == State_Running)
		bState_Running = TRUE;
	else if (m_State == State_Paused)
		bState_Paused = TRUE;

	if (bState_Paused || bState_Running)
	{
		CAutoLock lock(&m_Lock);
		m_pDemux->DoStop();
	}

	BOOL pinModeSave = m_pPidParser->get_ProgPinMode();

	//Set delay if we had been told to previously
	USHORT delay;
	m_pFileReader->get_DelayMode(&delay);

	delete m_pStreamParser;
	delete m_pDemux;
	delete m_pPidParser;
	delete m_pFileReader;
	delete m_pFileDuration;

	long length = lstrlenW(pszFileName);
	if ((length < 9) || (_wcsicmp(pszFileName+length-9, L".tsbuffer") != 0))
	{
		m_pFileReader = new FileReader();
		m_pFileDuration = new FileReader();//Get Live File Duration Thread
	}
	else
	{
		m_pFileReader = new MultiFileReader();
		m_pFileDuration = new MultiFileReader();
	}
	//m_pFileReader->SetDebugOutput(TRUE);
	//m_pFileDuration->SetDebugOutput(TRUE);

	m_pPidParser = new PidParser(m_pFileReader);
	m_pDemux = new Demux(m_pPidParser, this);
	m_pStreamParser = new StreamParser(m_pPidParser, m_pDemux, &netArray);

	// Load Registry Settings data
	GetRegStore("default");

	//Set delay if we had been told to previously
	if (delay)
	{
		m_pFileReader->set_DelayMode(delay);
		m_pFileDuration->set_DelayMode(delay);
	}

	hr = m_pFileReader->SetFileName(pszFileName);
	if (FAILED(hr))
		return hr;

	hr = m_pFileReader->OpenFile();
	if (FAILED(hr))
		return VFW_E_INVALIDMEDIATYPE;

	hr = m_pFileDuration->SetFileName(pszFileName);
	if (FAILED(hr))
		return hr;

	hr = m_pFileDuration->OpenFile();
	if (FAILED(hr))
		return VFW_E_INVALIDMEDIATYPE;

	set_ROTMode();

	__int64 fileStart;
	__int64	fileSize = 0;
	m_pFileReader->GetFileSize(&fileStart, &fileSize);
	m_llLastMultiFileStart = fileStart;
	m_llLastMultiFileLength = fileSize;

	int count = 0;
	__int64 filsSizeSave = fileSize;
	while(fileSize < 5000000 && count < 10)
	{
		count++;
		Sleep(500);
		m_pFileReader->GetFileSize(&fileStart, &fileSize);
		if (fileSize <= filsSizeSave)
		{
			NotifyEvent(EC_NEED_RESTART, NULL, NULL);
			Sleep(1000);
			break;
		}

		filsSizeSave = fileSize;
	};

	m_pFileReader->setFilePointer(300000, FILE_BEGIN);
	m_pPidParser->RefreshPids();
	LoadPgmReg();
	m_pStreamParser->ParsePidArray();
	RefreshDuration();
	m_pPin->m_IntBaseTimePCR = m_pPidParser->pids.start;
	m_pPin->m_IntStartTimePCR = m_pPidParser->pids.start;
	m_pPin->m_IntCurrentTimePCR = m_pPidParser->pids.start;
	m_pPin->m_IntEndTimePCR = m_pPidParser->pids.end;

	// Reconnect Demux if pin mode has changed and Source is connected
	if (m_pPidParser->get_ProgPinMode() != pinModeSave && (IPin*)m_pPin->IsConnected())
	{
		m_pPin->ReNewDemux();
	}

	{
		CAutoLock lock(&m_Lock);
		m_pDemux->AOnConnect();
	}
	m_pStreamParser->SetStreamActive(m_pPidParser->get_ProgramNumber());
	m_rtLastCurrentTime = (REFERENCE_TIME)((REFERENCE_TIME)timeGetTime() * (REFERENCE_TIME)10000);


	if (bState_Paused || bState_Running)
	{
		CAutoLock lock(&m_Lock);
		m_pDemux->DoStart();
	}
				
	if (bState_Paused)
	{
		CAutoLock lock(&m_Lock);
		m_pDemux->DoPause();
	}

	IMediaSeeking *pMediaSeeking;
	if(SUCCEEDED(GetFilterGraph()->QueryInterface(IID_IMediaSeeking, (void **) &pMediaSeeking)))
	{
		REFERENCE_TIME stop, start = 1000000;
		stop = m_pPidParser->pids.dur;
		hr = pMediaSeeking->SetPositions(&start, AM_SEEKING_AbsolutePositioning , &stop, AM_SEEKING_AbsolutePositioning);
		pMediaSeeking->Release();
	}

	{
		CAutoLock lock(&m_Lock);
		NotifyEvent(EC_LENGTH_CHANGED, NULL, NULL);	
	}

	if (wasThreadRunning)
		CAMThread::CallWorker(CMD_RUN);


	return hr;
}

HRESULT CTSFileSourceFilter::LoadPgmReg(void)
{

	HRESULT hr = S_OK;

	if (m_pPidParser->m_TStreamID && m_pPidParser->pidArray.Count() >= 2)
	{
		std::string saveName = m_pSettingsStore->getName();

		TCHAR cNID_TSID_ID[20];
		sprintf(cNID_TSID_ID, "%i:%i", m_pPidParser->m_NetworkID, m_pPidParser->m_TStreamID);

		// Load Registry Settings data
		GetRegStore(cNID_TSID_ID);

		if (m_pPidParser->set_ProgramSID() == S_OK)
		{
		}
		m_pSettingsStore->setName(saveName);
	}
	return hr;
}

HRESULT CTSFileSourceFilter::Refresh()
{
	CAutoLock lock(&m_Lock);
	UpdatePidParser();

	return S_OK;
}

HRESULT CTSFileSourceFilter::UpdatePidParser(void)
{
	CAutoLock lock(&m_Lock);
	HRESULT hr = S_FALSE;// if an error occurs.

	PidParser *pPidParser = new PidParser(m_pFileReader);
	
	int sid = m_pPidParser->pids.sid;
	int sidsave = m_pPidParser->m_ProgramSID;

	int count = 0;
	while(m_pDemux->m_bConnectBusyFlag && count < 20)
	{
		Sleep(100);
		count++;
	}
	m_pDemux->m_bConnectBusyFlag = TRUE;

	if (pPidParser->RefreshPids() == S_OK)
	{
		if (pPidParser->pidArray.Count())
		{
			hr = S_OK;

			//Lock the parser out
			m_pPidParser->m_ParsingLock	= TRUE;
			m_pPidParser->m_TStreamID = pPidParser->m_TStreamID;
			m_pPidParser->m_NetworkID = pPidParser->m_NetworkID;
			m_pPidParser->m_ONetworkID = pPidParser->m_ONetworkID;
//			m_pPidParser->m_ProgramSID = pPidParser->m_ProgramSID;
			m_pPidParser->m_ProgPinMode = pPidParser->m_ProgPinMode;
			m_pPidParser->m_AsyncMode = pPidParser->m_AsyncMode;
			m_pPidParser->m_PacketSize = pPidParser->m_PacketSize;
			m_pPidParser->m_ATSCFlag = pPidParser->m_ATSCFlag;
			memcpy(m_pPidParser->m_NetworkName, pPidParser->m_NetworkName, 128);
			memcpy(m_pPidParser->m_NetworkName + 127, "\0", 1);
			m_pPidParser->pids.CopyFrom(&pPidParser->pids);
			m_pPidParser->pidArray.Clear();

			m_pPin->WaitPinLock();
			for (int i = 0; i < pPidParser->pidArray.Count(); i++){
				PidInfo *pPids = new PidInfo;
				pPids->CopyFrom(&pPidParser->pidArray[i]);
				m_pPidParser->pidArray.Add(pPids);
			}
			//UnLock the parser
			m_pPidParser->m_ParsingLock	= FALSE;
		}

		if (m_pPidParser->m_TStreamID) 
		{
			m_pPidParser->set_SIDPid(sid); //Setup for search
			m_pPidParser->set_ProgramSID(); //set to same sid as before
			m_pPidParser->m_ProgramSID = sidsave; // restore old sid reg setting.
		}
						
		m_pStreamParser->ParsePidArray();
		m_pDemux->m_bConnectBusyFlag = FALSE;

		if (m_pDemux->CheckDemuxPids() == S_FALSE)
		{
			m_pDemux->AOnConnect();
		}
		m_pStreamParser->SetStreamActive(m_pPidParser->get_ProgramNumber());

	}
	delete  pPidParser;

	m_pDemux->m_bConnectBusyFlag = FALSE;

	return hr;
}

HRESULT CTSFileSourceFilter::RefreshPids()
{
	HRESULT hr;

	CAutoLock lock(&m_Lock);
	hr = m_pPidParser->ParseFromFile(m_pFileReader->getFilePointer());
	m_pStreamParser->ParsePidArray();
	return hr;
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
		pmt->majortype = MEDIATYPE_Stream;
//		pmt->majortype = MEDIATYPE_Video;
		pmt->subtype = MEDIASUBTYPE_MPEG2_TRANSPORT;
		pmt->subtype = MEDIASUBTYPE_NULL;
	}

	return S_OK;
} 

STDMETHODIMP CTSFileSourceFilter::GetVideoPid(WORD *pVPid)
{
	if(!pVPid)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);
	if (m_pPidParser->pids.vid)
		*pVPid = m_pPidParser->pids.vid;
	else if (m_pPidParser->pids.h264)
		*pVPid = m_pPidParser->pids.h264;
	else
		*pVPid = 0;

	return NOERROR;
}


STDMETHODIMP CTSFileSourceFilter::GetVideoPidType(BYTE *pointer)
{
	if (!pointer)
		  return E_INVALIDARG;

	CAutoLock lock(&m_Lock);
	if (m_pPidParser->pids.vid)
		sprintf((char *)pointer, "MPEG 2");
	else if (m_pPidParser->pids.h264)
		sprintf((char *)pointer, "H.264");
	else if (m_pPidParser->pids.mpeg4)
		sprintf((char *)pointer, "MPEG 4");
	else
		sprintf((char *)pointer, "None");

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

STDMETHODIMP CTSFileSourceFilter::GetAACPid(WORD *pAacPid)
{
	if(!pAacPid)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);
	*pAacPid = m_pPidParser->pids.aac;

	return NOERROR;

}

STDMETHODIMP CTSFileSourceFilter::GetAAC2Pid(WORD *pAac2Pid)
{
	if(!pAac2Pid)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);
	*pAac2Pid = m_pPidParser->pids.aac2;

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

STDMETHODIMP CTSFileSourceFilter::GetAC3_2Pid(WORD *pAC3_2Pid)
{
	if(!pAC3_2Pid)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);

	*pAC3_2Pid = m_pPidParser->pids.ac3_2;

	return NOERROR;

}

STDMETHODIMP CTSFileSourceFilter::GetTelexPid(WORD *pTelexPid)
{
	if(!pTelexPid)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);

	*pTelexPid = m_pPidParser->pids.txt;

	return NOERROR;

}

STDMETHODIMP CTSFileSourceFilter::GetNIDPid(WORD *pNIDPid)
{
	if(!pNIDPid)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);

	*pNIDPid = m_pPidParser->m_NetworkID;

	return NOERROR;

}

STDMETHODIMP CTSFileSourceFilter::GetONIDPid(WORD *pONIDPid)
{
	if(!pONIDPid)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);
	*pONIDPid = m_pPidParser->m_ONetworkID;

	return NOERROR;

}

STDMETHODIMP CTSFileSourceFilter::GetTSIDPid(WORD *pTSIDPid)
{
	if(!pTSIDPid)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);
	*pTSIDPid = m_pPidParser->m_TStreamID;

	return NOERROR;

}
	
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
	*pPCRPid = m_pPidParser->pids.pcr - m_pPidParser->pids.opcr;

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

STDMETHODIMP CTSFileSourceFilter::GetChannelNumber (BYTE *pointer)
{
	if (!pointer)
		  return E_INVALIDARG;

	CAutoLock lock(&m_Lock);
	m_pPidParser->get_ChannelNumber(pointer);

	return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::GetNetworkName (BYTE *pointer)
{
	if (!pointer)
		  return E_INVALIDARG;

	CAutoLock lock(&m_Lock);
	m_pPidParser->get_NetworkName(pointer);

	return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::GetONetworkName (BYTE *pointer)
{
	if (!pointer)
		  return E_INVALIDARG;

	CAutoLock lock(&m_Lock);
	m_pPidParser->get_ONetworkName(pointer);

	return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::GetChannelName (BYTE *pointer)
{
	if (!pointer)
		  return E_INVALIDARG;

	CAutoLock lock(&m_Lock);
	m_pPidParser->get_ChannelName(pointer);

	return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::GetEPGFromFile(void)
{
	CAutoLock lock(&m_Lock);
	return m_pPidParser->get_EPGFromFile();
}

STDMETHODIMP CTSFileSourceFilter::GetShortNextDescr (BYTE *pointer)
{
	if (!pointer)
		  return E_INVALIDARG;

	CAutoLock lock(&m_Lock);
	m_pPidParser->get_ShortNextDescr(pointer);

	return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::GetExtendedNextDescr (BYTE *pointer)
{
	if (!pointer)
		  return E_INVALIDARG;

	CAutoLock lock(&m_Lock);
	m_pPidParser->get_ExtendedNextDescr(pointer);

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

	//If only one program don't change it
	if (m_pPidParser->pidArray.Count() < 1)
		return NOERROR;

	REFERENCE_TIME start, stop;
	m_pPin->GetPositions(&start, &stop);

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
	
	BOOL wasThreadRunning = FALSE;
	if (ThreadRunning() && ThreadExists()) {

		CAMThread::CallWorker(CMD_STOP);
		while (m_bThreadRunning){};
		wasThreadRunning = TRUE;
	}

	m_pPin->m_IntBaseTimePCR = (__int64)max(0, (__int64)(m_pPin->m_IntStartTimePCR - m_pPin->m_IntBaseTimePCR));

//	m_pPin->m_DemuxLock = TRUE;
	m_pPidParser->set_ProgramNumber((WORD)PgmNumber);
	m_pPin->SetDuration(m_pPidParser->pids.dur);

	m_pPin->m_IntBaseTimePCR = (__int64)max(0, (__int64)(m_pPidParser->pids.start - m_pPin->m_IntBaseTimePCR));

	m_pPin->m_IntStartTimePCR = m_pPidParser->pids.start;
	m_pPin->m_IntEndTimePCR = m_pPidParser->pids.end;
	OnConnect();

//	Sleep(200);
	ResetStreamTime();

	IMediaSeeking *pMediaSeeking;
	if(SUCCEEDED(GetFilterGraph()->QueryInterface(IID_IMediaSeeking, (void **) &pMediaSeeking)))
	{
		pMediaSeeking->SetPositions(&start, AM_SEEKING_AbsolutePositioning , &stop, AM_SEEKING_NoPositioning);
		pMediaSeeking->Release();
	}

//	m_pPin->setPositions(&start,AM_SEEKING_AbsolutePositioning, NULL, NULL);
//	m_pPin->m_DemuxLock = FALSE;

	if (wasThreadRunning)
		CAMThread::CallWorker(CMD_RUN);

	return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::NextPgmNumb(void)
{
	CAutoLock lock(&m_Lock);

	//If only one program don't change it
	if (m_pPidParser->pidArray.Count() < 2)
		return NOERROR;

	REFERENCE_TIME start, stop;
	m_pPin->GetPositions(&start, &stop);

	WORD PgmNumb = m_pPidParser->get_ProgramNumber();
	PgmNumb++;
	if (PgmNumb >= m_pPidParser->pidArray.Count())
	{
		PgmNumb = 0;
	}

	BOOL wasThreadRunning = FALSE;
	if (ThreadRunning() && ThreadExists()) {

		CAMThread::CallWorker(CMD_STOP);
		while (m_bThreadRunning){};
		wasThreadRunning = TRUE;
	}

	m_pPin->m_IntBaseTimePCR = (__int64)max(0, (__int64)(m_pPin->m_IntStartTimePCR - m_pPin->m_IntBaseTimePCR));

//	m_pPin->m_DemuxLock = TRUE;
	m_pPidParser->set_ProgramNumber(PgmNumb);
	m_pPin->SetDuration(m_pPidParser->pids.dur);

	m_pPin->m_IntBaseTimePCR = (__int64)max(0, (__int64)(m_pPidParser->pids.start - m_pPin->m_IntBaseTimePCR));

	m_pPin->m_IntStartTimePCR = m_pPidParser->pids.start;
	m_pPin->m_IntEndTimePCR = m_pPidParser->pids.end;
	OnConnect();
//	Sleep(200);
	ResetStreamTime();

	IMediaSeeking *pMediaSeeking;
	if(SUCCEEDED(GetFilterGraph()->QueryInterface(IID_IMediaSeeking, (void **) &pMediaSeeking)))
	{
		pMediaSeeking->SetPositions(&start, AM_SEEKING_AbsolutePositioning , &stop, AM_SEEKING_NoPositioning);
		pMediaSeeking->Release();
	}

//	m_pPin->setPositions(&start, AM_SEEKING_AbsolutePositioning, NULL, NULL);

//	m_pPin->m_DemuxLock = FALSE;

	if (wasThreadRunning)
		CAMThread::CallWorker(CMD_RUN);

	return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::PrevPgmNumb(void)
{
	CAutoLock lock(&m_Lock);

	//If only one program don't change it
	if (m_pPidParser->pidArray.Count() < 2)
		return NOERROR;

	REFERENCE_TIME start, stop;
	m_pPin->GetPositions(&start, &stop);

	int PgmNumb = m_pPidParser->get_ProgramNumber();
	PgmNumb--;
	if (PgmNumb < 0)
	{
		PgmNumb = m_pPidParser->pidArray.Count() - 1;
	}

	BOOL wasThreadRunning = FALSE;
	if (ThreadRunning() && ThreadExists()) {

		CAMThread::CallWorker(CMD_STOP);
		while (m_bThreadRunning){};
		wasThreadRunning = TRUE;
	}

	m_pPin->m_IntBaseTimePCR = (__int64)max(0, (__int64)(m_pPin->m_IntStartTimePCR - m_pPin->m_IntBaseTimePCR));

//	m_pPin->m_DemuxLock = TRUE;
	m_pPidParser->set_ProgramNumber((WORD)PgmNumb);
	m_pPin->SetDuration(m_pPidParser->pids.dur);

	m_pPin->m_IntBaseTimePCR = (__int64)max(0, (__int64)(m_pPidParser->pids.start - m_pPin->m_IntBaseTimePCR));

	m_pPin->m_IntStartTimePCR = m_pPidParser->pids.start;
	m_pPin->m_IntEndTimePCR = m_pPidParser->pids.end;
	OnConnect();
//	Sleep(200);

	ResetStreamTime();

	IMediaSeeking *pMediaSeeking;
	if(SUCCEEDED(GetFilterGraph()->QueryInterface(IID_IMediaSeeking, (void **) &pMediaSeeking)))
	{
		pMediaSeeking->SetPositions(&start, AM_SEEKING_AbsolutePositioning , &stop, AM_SEEKING_NoPositioning);
		pMediaSeeking->Release();
	}

//	m_pPin->setPositions(&start, AM_SEEKING_AbsolutePositioning, NULL, NULL);

//	m_pPin->m_DemuxLock = FALSE;

	if (wasThreadRunning)
		CAMThread::CallWorker(CMD_RUN);

	return NOERROR;
}

HRESULT CTSFileSourceFilter::GetFileSize(__int64 *pStartPosition, __int64 *pEndPosition)
{
	CAutoLock lock(&m_Lock);
	return m_pFileReader->GetFileSize(pStartPosition, pEndPosition);
}

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
	OnConnect();
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
	OnConnect();
	return NOERROR;
}

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
	OnConnect();
	return NOERROR;
}

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
	OnConnect();
	return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::GetNPControl(WORD *NPControl)
{
	if(!NPControl)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);
	*NPControl = m_pDemux->get_NPControl();
	return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::SetNPControl(WORD NPControl)
{
	CAutoLock lock(&m_Lock);
	m_pDemux->set_NPControl(NPControl);
	OnConnect();
	return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::GetNPSlave(WORD *NPSlave)
{
	if(!NPSlave)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);
	*NPSlave = m_pDemux->get_NPSlave();
	return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::SetNPSlave(WORD NPSlave)
{
	CAutoLock lock(&m_Lock);
	m_pDemux->set_NPSlave(NPSlave);
	OnConnect();
	return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::SetTunerEvent(void)
{
	CAutoLock lock(&m_Lock);

	if (SUCCEEDED(m_pTunerEvent->HookupGraphEventService(GetFilterGraph())))
	{
		m_pTunerEvent->RegisterForTunerEvents();
	}
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
	OnConnect();
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
	OnConnect();
	return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::GetCreateTxtPinOnDemux(WORD *pbCreatePin)
{
	if(!pbCreatePin)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);
	*pbCreatePin = m_pDemux->get_CreateTxtPinOnDemux();
	return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::SetCreateTxtPinOnDemux(WORD bCreatePin)
{
	CAutoLock lock(&m_Lock);
	m_pDemux->set_CreateTxtPinOnDemux(bCreatePin);
	OnConnect();
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

STDMETHODIMP CTSFileSourceFilter::GetROTMode(WORD *ROTMode)
{
	if(!ROTMode)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);
	*ROTMode = m_bRotEnable;
	return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::SetROTMode(WORD ROTMode)
{
	CAutoLock lock(&m_Lock);
	m_bRotEnable = ROTMode;
	set_ROTMode();
	return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::GetClockMode(WORD *ClockMode)
{
	if(!ClockMode)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);
	*ClockMode = m_pDemux->get_ClockMode();
	return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::SetClockMode(WORD ClockMode)
{
	CAutoLock lock(&m_Lock);
	m_pDemux->set_ClockMode(ClockMode);
	m_pDemux->SetRefClock();
	return NOERROR;
}

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
		m_pSettingsStore->setNPControlReg((BOOL)m_pDemux->get_NPControl());
		m_pSettingsStore->setNPSlaveReg((BOOL)m_pDemux->get_NPSlave());
		m_pSettingsStore->setMP2ModeReg((BOOL)m_pDemux->get_MPEG2AudioMediaType());
		m_pSettingsStore->setAudio2ModeReg((BOOL)m_pDemux->get_MPEG2Audio2Mode());
		m_pSettingsStore->setAC3ModeReg((BOOL)m_pDemux->get_AC3Mode());
		m_pSettingsStore->setCreateTSPinOnDemuxReg((BOOL)m_pDemux->get_CreateTSPinOnDemux());
		m_pSettingsStore->setROTModeReg((int)m_bRotEnable);
		m_pSettingsStore->setClockModeReg((BOOL)m_pDemux->get_ClockMode());
		m_pSettingsStore->setCreateTxtPinOnDemuxReg((BOOL)m_pDemux->get_CreateTxtPinOnDemux());

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
			m_pFileDuration->set_DelayMode(m_pSettingsStore->getDelayModeReg());
			m_pDemux->set_Auto(m_pSettingsStore->getAutoModeReg());
			m_pDemux->set_NPControl(m_pSettingsStore->getNPControlReg());
			m_pDemux->set_NPSlave(m_pSettingsStore->getNPSlaveReg());
			m_pDemux->set_MPEG2AudioMediaType(m_pSettingsStore->getMP2ModeReg());
			m_pDemux->set_MPEG2Audio2Mode(m_pSettingsStore->getAudio2ModeReg());
			m_pDemux->set_AC3Mode(m_pSettingsStore->getAC3ModeReg());
			m_pDemux->set_CreateTSPinOnDemux(m_pSettingsStore->getCreateTSPinOnDemuxReg());
			m_pPin->set_RateControl(m_pSettingsStore->getRateControlModeReg());
			m_bRotEnable = m_pSettingsStore->getROTModeReg();
			m_pDemux->set_ClockMode(m_pSettingsStore->getClockModeReg());
			m_pDemux->set_CreateTxtPinOnDemux(m_pSettingsStore->getCreateTxtPinOnDemuxReg());
		}
	}

    return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::SetRegSettings()
{
	CAutoLock lock(&m_Lock);
	SetRegStore("user");
    return NOERROR;
}


STDMETHODIMP CTSFileSourceFilter::GetRegSettings()
{
	CAutoLock lock(&m_Lock);
	GetRegStore("user");
    return NOERROR;
}

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

//    HWND    phWnd = (HWND)CreateEvent(NULL, FALSE, FALSE, NULL);

	ULONG refCount;
	IEnumFilters * piEnumFilters2 = NULL;
	if SUCCEEDED(GetFilterGraph()->EnumFilters(&piEnumFilters2))
	{
		IBaseFilter * piFilter2;
		while (piEnumFilters2->Next(1, &piFilter2, 0) == NOERROR )
		{
			ISpecifyPropertyPages* piProp2 = NULL;
			if ((piFilter2->QueryInterface(IID_ISpecifyPropertyPages, (void **)&piProp2) == S_OK) && (piProp2 != NULL))
			{
				FILTER_INFO filterInfo2;
				piFilter2->QueryFilterInfo(&filterInfo2);
				LPOLESTR fileName;
				m_pFileReader->GetFileName(&fileName);
			
				if (!wcsicmp(fileName, filterInfo2.achName))
				{
					IUnknown *piFilterUnk;
					piFilter2->QueryInterface(IID_IUnknown, (void **)&piFilterUnk);

					CAUUID caGUID;
					piProp2->GetPages(&caGUID);
					piProp2->Release();
					OleCreatePropertyFrame(0, 0, 0, filterInfo2.achName, 1, &piFilterUnk, caGUID.cElems, caGUID.pElems, 0, 0, NULL);
					piFilterUnk->Release();
					CoTaskMemFree(caGUID.pElems);
				}
				piProp2->Release();
			}
			refCount = piFilter2->Release();
		}
		refCount = piEnumFilters2->Release();
	}
//	CloseHandle(phWnd);
	return NOERROR;
}

BOOL CTSFileSourceFilter::get_AutoMode()
{
	return m_pDemux->get_Auto();
}

BOOL CTSFileSourceFilter::get_PinMode()
{
	return m_pPidParser->get_ProgPinMode();;
}

// Adds a DirectShow filter graph to the Running Object Table,
// allowing GraphEdit to "spy" on a remote filter graph.
HRESULT CTSFileSourceFilter::AddGraphToRot(
        IUnknown *pUnkGraph, 
        DWORD *pdwRegister
        ) 
{
    CComPtr <IMoniker>              pMoniker;
    CComPtr <IRunningObjectTable>   pROT;
    WCHAR wsz[128];
    HRESULT hr;

    if (FAILED(GetRunningObjectTable(0, &pROT)))
        return E_FAIL;

    wsprintfW(wsz, L"FilterGraph %08x pid %08x\0", (DWORD_PTR) pUnkGraph, 
              GetCurrentProcessId());
/*	
	//Search the ROT for the same reference
	IUnknown *pUnk = NULL;
	if (SUCCEEDED(GetObjectFromROT(wsz, &pUnk)))
	{
		//Exit out if we have an object running in ROT
		if (pUnk)
		{
			pUnk->Release();
			return S_OK;
		}
	}
*/
    hr = CreateItemMoniker(L"!", wsz, &pMoniker);
    if (SUCCEEDED(hr))
	{
        hr = pROT->Register(ROTFLAGS_REGISTRATIONKEEPSALIVE, pUnkGraph, 
                            pMoniker, pdwRegister);
	}
    return hr;
}
        
// Removes a filter graph from the Running Object Table
void CTSFileSourceFilter::RemoveGraphFromRot(DWORD pdwRegister)
{
    CComPtr <IRunningObjectTable> pROT;

    if (SUCCEEDED(GetRunningObjectTable(0, &pROT))) 
        pROT->Revoke(pdwRegister);
}

void CTSFileSourceFilter::set_ROTMode()
{
	if (m_bRotEnable)
	{
		if (!m_dwGraphRegister && FAILED(AddGraphToRot (GetFilterGraph(), &m_dwGraphRegister)))
			m_dwGraphRegister = 0;
	}
	else if (m_dwGraphRegister)
	{
			RemoveGraphFromRot(m_dwGraphRegister);
			m_dwGraphRegister = 0;
	}
}

HRESULT CTSFileSourceFilter::GetObjectFromROT(WCHAR* wsFullName, IUnknown **ppUnk)
{
	if( *ppUnk )
		return E_FAIL;

	HRESULT	hr;

	IRunningObjectTablePtr spTable;
	IEnumMonikerPtr	spEnum = NULL;
	_bstr_t	bstrtFullName;

	bstrtFullName = wsFullName;

	// Get the IROT interface pointer
	hr = GetRunningObjectTable( 0, &spTable ); 
	if (FAILED(hr))
		return E_FAIL;

	// Get the moniker enumerator
	hr = spTable->EnumRunning( &spEnum ); 
	if (SUCCEEDED(hr))
	{
		_bstr_t	bstrtCurName; 

		// Loop thru all the interfaces in the enumerator looking for our reqd interface 
		IMonikerPtr spMoniker = NULL;
		while (SUCCEEDED(spEnum->Next(1, &spMoniker, NULL)) && (NULL != spMoniker))
		{
			// Create a bind context 
			IBindCtxPtr spContext = NULL;
			hr = CreateBindCtx(0, &spContext); 
			if (SUCCEEDED(hr))
			{
				// Get the display name
				WCHAR *wsCurName = NULL;
				hr = spMoniker->GetDisplayName(spContext, NULL, &wsCurName );
				bstrtCurName = wsCurName;

				// We have got our required interface pointer //
				if (SUCCEEDED(hr) && bstrtFullName == bstrtCurName)
				{ 
					hr = spTable->GetObject( spMoniker, ppUnk );
					return hr;
				}	
			}
			spMoniker.Release();
		}
	}
	return E_FAIL;
}

HRESULT CTSFileSourceFilter::ShowEPGInfo()
{
	HRESULT hr = GetEPGFromFile();

	if (hr == S_OK)
	{
		unsigned char netname[128] = "";
		unsigned char onetname[128] ="";
		unsigned char chname[128] ="";
		unsigned char chnumb[128] ="";
		unsigned char shortdescripor[128] ="";
		unsigned char Extendeddescripor[600] ="";
		unsigned char shortnextdescripor[128] ="";
		unsigned char Extendednextdescripor[600] ="";
		GetNetworkName((unsigned char*)&netname);
		GetONetworkName((unsigned char*)&onetname);
		GetChannelName((unsigned char*)&chname);
		GetChannelNumber((unsigned char*)&chnumb);
		GetShortDescr((unsigned char*)&shortdescripor);
		GetExtendedDescr((unsigned char*)&Extendeddescripor);
		GetShortNextDescr((unsigned char*)&shortnextdescripor);
		GetExtendedNextDescr((unsigned char*)&Extendednextdescripor);
		TCHAR szBuffer[(6*128)+ (2*600)];
		sprintf(szBuffer, "Network Name:- %s\n"
		"ONetwork Name:- %s\n"
		"Channel Number:- %s\n"
		"Channel Name:- %s\n\n"
		"Program Name: - %s\n"
		"Program Description:- %s\n\n"
		"Next Program Name: - %s\n"
		"Next Program Description:- %s\n"
			,netname,
			onetname,
			chnumb,
			chname,
			shortdescripor,
			Extendeddescripor,
			shortnextdescripor,
			Extendednextdescripor
			);
			MessageBox(NULL, szBuffer, TEXT("Program Infomation"), MB_OK);
	}
	return hr;
}

STDMETHODIMP CTSFileSourceFilter::GetPCRPosition(REFERENCE_TIME *pos)
{
	if(!pos)
		return E_INVALIDARG;

	CAutoLock lock(&m_Lock);
	*pos = m_pPin->getPCRPosition();

	return NOERROR;

}

STDMETHODIMP CTSFileSourceFilter::ShowStreamMenu(HWND hwnd)
{
	HRESULT hr;

	POINT mouse;
	GetCursorPos(&mouse);

	HMENU hMenu = CreatePopupMenu();
	if (hMenu)
	{
		IAMStreamSelect *pIAMStreamSelect;
		hr = this->QueryInterface(IID_IAMStreamSelect, (void**)&pIAMStreamSelect);
		if (SUCCEEDED(hr))
		{
			ULONG count;
			pIAMStreamSelect->Count(&count);

			ULONG flags, group, lastgroup = -1;
				
			for(UINT i = 0; i < count; i++)
			{
				WCHAR* pStreamName = NULL;

				if(S_OK == pIAMStreamSelect->Info(i, 0, &flags, 0, &group, &pStreamName, 0, 0))
				{
					if(lastgroup != group && i) 
						::AppendMenu(hMenu, MF_SEPARATOR, NULL, NULL);

					lastgroup = group;

					if(pStreamName)
					{
//						UINT uFlags = MF_STRING | MF_ENABLED;
//						if (flags & AMSTREAMSELECTINFO_EXCLUSIVE)
//							uFlags |= MF_CHECKED; //MFT_RADIOCHECK;
//						else if (flags & AMSTREAMSELECTINFO_ENABLED)
//							uFlags |= MF_CHECKED;
						
						UINT uFlags = (flags?MF_CHECKED:MF_UNCHECKED) | MF_STRING | MF_ENABLED;
						::AppendMenuW(hMenu, uFlags, (i + 0x100), LPCWSTR(pStreamName));
						CoTaskMemFree(pStreamName);
					}
				}
			}

			SetForegroundWindow(hwnd);
			UINT index = ::TrackPopupMenu(hMenu, TPM_LEFTBUTTON|TPM_RETURNCMD, mouse.x, mouse.y, 0, hwnd, 0);
			PostMessage(hwnd, NULL, 0, 0);

			if(index & 0x100) 
				pIAMStreamSelect->Enable((index & 0xff), AMSTREAMSELECTENABLE_ENABLE);

			pIAMStreamSelect->Release();
		}
		DestroyMenu(hMenu);
	}
	return hr;
}























//*****************************************************************************************
//ASync Additions

STDMETHODIMP CTSFileSourceFilter::RequestAllocator(
                      IMemAllocator* pPreferred,
                      ALLOCATOR_PROPERTIES* pProps,
                      IMemAllocator ** ppActual)
{
	CAutoLock cObjectLock(m_pLock);
	Pause();
	Stop();

    return S_OK;
}

STDMETHODIMP CTSFileSourceFilter::Request(
                     IMediaSample* pSample,
                     DWORD_PTR dwUser)
{
	return E_NOTIMPL;
}

STDMETHODIMP CTSFileSourceFilter::WaitForNext(
                      DWORD dwTimeout,
                      IMediaSample** ppSample,  
                      DWORD_PTR * pdwUser)
{
	return E_NOTIMPL;
}

STDMETHODIMP CTSFileSourceFilter::SyncReadAligned(
                      IMediaSample* pSample)
{
	return E_NOTIMPL;
}

STDMETHODIMP CTSFileSourceFilter::SyncRead(
                      LONGLONG llPosition,  // absolute file position
                      LONG lLength,         // nr bytes required
                      BYTE* pBuffer)
{
    CheckPointer(pBuffer, E_POINTER);

	HRESULT hr;
	LONG dwBytesToRead = lLength;
    CAutoLock lck(&m_Lock);
    DWORD dwReadLength;

	if (m_pFileReader->IsFileInvalid())
	{
		hr = m_pFileReader->OpenFile();
		if (FAILED(hr))
			return E_FAIL;

		int count = 0;
		__int64 fileStart, fileSize = 0;
		m_pFileReader->GetFileSize(&fileStart, &fileSize);

		//If this a file start then return null.
		while(fileSize < 500000 && count < 10)
		{
			Sleep(100);
			m_pFileReader->GetFileSize(&fileStart, &fileSize);
			count++;
		}
	}

	__int64 fileStart, fileSize = 0;
	m_pFileReader->GetFileSize(&fileStart, &fileSize);
	// Read the data from the file
	llPosition = min(fileSize, llPosition);
	llPosition = max(0, llPosition);
//	hr = m_pFileReader->Read(pBuffer, dwBytesToRead, &dwReadLength, (__int64)(llPosition - fileSize), FILE_END);
	hr = m_pFileReader->Read(pBuffer, dwBytesToRead, &dwReadLength, llPosition, FILE_BEGIN);
	if (FAILED(hr))
		return hr;

	if (dwReadLength < dwBytesToRead) 
	{
		WORD wReadOnly = 0;
		m_pFileReader->get_ReadOnly(&wReadOnly);
		if (wReadOnly)
		{
			while (dwReadLength < dwBytesToRead) 
			{
				WORD bDelay = 0;
				m_pFileReader->get_DelayMode(&bDelay);

				if (bDelay > 0)
					Sleep(2000);
				else
					Sleep(100);

				__int64 fileStart, filelength;
				m_pFileReader->GetFileSize(&fileStart, &filelength);
				ULONG ulNextBytesRead = 0;				
				llPosition = min(filelength, llPosition);
				llPosition = max(0, llPosition);
//				HRESULT hr = m_pFileReader->Read(pBuffer, dwBytesToRead, &dwReadLength, (__int64)(llPosition - filelength), FILE_END);
				HRESULT hr = m_pFileReader->Read(pBuffer, dwBytesToRead, &dwReadLength, llPosition, FILE_BEGIN);
				if (FAILED(hr))
					return hr;

				if ((ulNextBytesRead == 0) || (ulNextBytesRead == dwReadLength))
					return E_FAIL;

				dwReadLength = ulNextBytesRead;
			}
		}
		else
		{
			m_pFileReader->CloseFile();
			return E_FAIL;
		}
	}

	return NOERROR;
}

    // return total length of stream, and currently available length.
    // reads for beyond the available length but within the total length will
    // normally succeed but may block for a long period.
STDMETHODIMP CTSFileSourceFilter::Length(
                      LONGLONG* pTotal,
                      LONGLONG* pAvailable)
{
    CAutoLock lck(&m_Lock);

    CheckPointer(pTotal, E_POINTER);
    CheckPointer(pAvailable, E_POINTER);


	HRESULT hr;

	__int64 fileStart;
	__int64	fileSize = 0;

	if (m_pFileReader->IsFileInvalid())
	{
		hr = m_pFileReader->OpenFile();
		if (FAILED(hr))
			return E_FAIL;

		int count = 0;
		m_pFileReader->GetFileSize(&fileStart, &fileSize);

		//If this a file start then return null.
		while(fileSize < 500000 && count < 10)
		{
			Sleep(100);
			m_pFileReader->GetFileSize(&fileStart, &fileSize);
			count++;
		}
	}

	m_pFileReader->GetFileSize(&fileStart, &fileSize);

	*pTotal = fileSize;		
	*pAvailable = fileSize;		
	return NOERROR;
}

STDMETHODIMP CTSFileSourceFilter::BeginFlush(void)
{
	return E_NOTIMPL;
}

STDMETHODIMP CTSFileSourceFilter::EndFlush(void)
{
	return E_NOTIMPL;
}
//m_pPin->PrintTime(TEXT("Run"), (__int64) tStart, 10000);

//*****************************************************************************************


//////////////////////////////////////////////////////////////////////////
// End of interface implementations
//////////////////////////////////////////////////////////////////////////

