/**
*  NetRender.ccp
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
*  nate can be reached on the forums at
*    http://forums.dvbowners.com/
*/
#include <Winsock2.h>

#include <streams.h>
#include "NetRender.h"
#include "TSFileSinkGuids.h"
#include "ITSFileSink.h"
#include "NetworkGuids.h"
#include "TSFileSourcePin.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/timeb.h>


//////////////////////////////////////////////////////////////////////
// NetRender
//////////////////////////////////////////////////////////////////////

CNetRender::CNetRender()
{
}

CNetRender::~CNetRender()
{
}

HRESULT CNetRender::CreateNetworkGraph(NetInfo *netAddr)
{
	HRESULT hr = S_OK;

	//
	// Re-Create the FilterGraph if already Active
	//
	if (netAddr->pNetworkGraph)
		DeleteNetworkGraph(netAddr);
	//
    // Instantiate filter graph interface
	//
	hr = CoCreateInstance(CLSID_FilterGraph,
					NULL, CLSCTX_INPROC, 
                    IID_IGraphBuilder,
					(void **)&netAddr->pNetworkGraph);
    if (FAILED (hr))
    {
		DeleteNetworkGraph(netAddr);
        return hr;
    }

	//
	// Create the either types of NetSource Filters and add it to the FilterGraph
	//
    hr = CoCreateInstance(
                        CLSID_BDA_DSNetReceive, 
                        NULL, 
                        CLSCTX_INPROC_SERVER,
                        IID_IBaseFilter, 
                        reinterpret_cast<void**>(&netAddr->pNetSource)
                        );
    if (FAILED (hr))
    {
		hr = CoCreateInstance(
                        CLSID_DSNetReceive, 
                        NULL, 
                        CLSCTX_INPROC_SERVER,
                        IID_IBaseFilter, 
                        reinterpret_cast<void**>(&netAddr->pNetSource)
                        );
		if (FAILED (hr))
		{
			DeleteNetworkGraph(netAddr);
			return hr;
		}
	}

	hr = netAddr->pNetworkGraph->AddFilter(netAddr->pNetSource, L"Network Source");
    if (FAILED (hr))
    {
		DeleteNetworkGraph(netAddr);
        return hr;
    }

	//
	// Setup NetSource Filter, Get the Control Interface
	//
	CComPtr <IMulticastConfig> piMulticastConfig = NULL;
	if (FAILED(hr = netAddr->pNetSource->QueryInterface(IID_IMulticastConfig, (void**)&piMulticastConfig)))
	{
		DeleteNetworkGraph(netAddr);
        return hr;
	}

	//
	// if no NIC then set default
	//
	if (!netAddr->userNic)
		netAddr->userNic = inet_addr ("127.0.0.1");

	if (FAILED(hr = piMulticastConfig->SetNetworkInterface(netAddr->userNic))) //0 /*INADDR_ANY == 0*/ )))
	{
		DeleteNetworkGraph(netAddr);
        return hr;
	}

	//
	// if no Multicast IP then set default
	//
	if (!netAddr->userIP)
		netAddr->userIP = inet_addr ("224.0.0.0");

	//
	// if no Multicast port then set default
	//
	if (!netAddr->userPort)
		netAddr->userPort = htons ((USHORT) 1234);

	if (FAILED(hr = piMulticastConfig->SetMulticastGroup(netAddr->userIP, netAddr->userPort)))
	{
		DeleteNetworkGraph(netAddr);
        return hr;
	}

	//
	// Create the Sink Filter and add it to the FilterGraph
	//
    hr = CoCreateInstance(
                        CLSID_TSFileSink, 
                        NULL, 
                        CLSCTX_INPROC_SERVER,
                        IID_IBaseFilter, 
                        reinterpret_cast<void**>(&netAddr->pFileSink)
                        );
    if (FAILED (hr))
    {
		DeleteNetworkGraph(netAddr);
        return hr;
    }

	hr = netAddr->pNetworkGraph->AddFilter(netAddr->pFileSink, L"File Sink");
    if (FAILED (hr))
    {
		DeleteNetworkGraph(netAddr);
        return hr;
    }

	//
	// Connect the NetSource Filter to the Sink Filter 
	//
	hr = CTSFileSourcePin::RenderOutputPins(netAddr->pNetSource);
    if (FAILED (hr))
    {
		DeleteNetworkGraph(netAddr);
        return hr;
    }

	//
	// Get the Sink Filter Interface 
	//
	CComPtr <IFileSinkFilter> pIFileSink;
	hr = netAddr->pFileSink->QueryInterface(&pIFileSink);
    if(FAILED (hr))
    {
		DeleteNetworkGraph(netAddr);
        return hr;
    }

	//
	// Add the Date/Time Stamp to the FileName 
	//
	WCHAR wfileName[MAX_PATH] = L"";
	_tzset();
	time(&netAddr->time);
	netAddr->tmTime = localtime(&netAddr->time);
	wcsftime(wfileName, 32, L"(%Y-%m-%d %H-%M-%S)", netAddr->tmTime);

	wsprintfW(netAddr->fileName, L"%S%S UDP (%S-%S-%S).tsbuffer",
								netAddr->pathName,
								wfileName,
								netAddr->strIP,
								netAddr->strPort,
								netAddr->strNic);

	//
	// Set the Sink Filter File Name 
	//
	hr = pIFileSink->SetFileName(netAddr->fileName, NULL);
    if (FAILED (hr))
    {
		DeleteNetworkGraph(netAddr);
        return hr;
    }
//MessageBoxW(NULL, netAddr->fileName, L"time", NULL);

	//
	// Get the IMediaControl Interface 
	//
	CComPtr <IMediaControl> pMediaControl;
	hr = netAddr->pNetworkGraph->QueryInterface(&pMediaControl);
    if (FAILED (hr))
    {
		DeleteNetworkGraph(netAddr);
        return hr;
    }

	//
	//Register the FilterGraph in the Object Running Table
	//
	if (netAddr->rotEnable)
	{
		if (!netAddr->dwGraphRegister && FAILED(AddGraphToRot (netAddr->pNetworkGraph, &netAddr->dwGraphRegister)))
			netAddr->dwGraphRegister = 0;
	}

	//
	//Run the FilterGraph
	//
	hr = pMediaControl->Run(); 
    if (FAILED (hr))
    {
		DeleteNetworkGraph(netAddr);
        return hr;
    }

	//
	//Wait for data to build before testing data flow
	//
	Sleep(500);

	//
	// Get the Sink Filter Interface 
	//
	CComPtr <ITSFileSink> pITSFileSink;
	hr = netAddr->pFileSink->QueryInterface(IID_ITSFileSink, (void**)&pITSFileSink);
    if(FAILED (hr))
    {
		DeleteNetworkGraph(netAddr);
        return hr;
    }

	__int64 llDataFlow = 0;
	__int64 llDataFlowSave = 0;
	int count = 0;

	//
	// Loop until we have data or time out 
	//
	while(llDataFlow < 2000000 && count < 10)
	{
		Sleep(100);
		hr = pITSFileSink->GetFileBufferSize(&llDataFlow);
		if(FAILED (hr))
		{
			DeleteNetworkGraph(netAddr);
			return hr;
		}

		if (llDataFlow > (__int64)(llDataFlowSave + (__int64)1000))
		{
			llDataFlowSave = llDataFlow; 
			count--;
		}
		count++;
	}

	//
	// Check for data flow one last time incase it has stopped 
	//
	Sleep(100);
	pITSFileSink->GetFileBufferSize(&llDataFlow);

	if(llDataFlow < 2000000 || (llDataFlow < (__int64)(llDataFlowSave + (__int64)1)))
    {
		DeleteNetworkGraph(netAddr);
        return ERROR_CANNOT_MAKE;
    }

	//
	// Get the filename from the sink filter just in case it has changed 
	//
	WCHAR ptFileName[MAX_PATH] = L"";
	hr = pITSFileSink->GetBufferFileName((unsigned short*)&ptFileName);
	if(FAILED (hr))
	{
		DeleteNetworkGraph(netAddr);
		return hr;
	}
	wsprintfW(netAddr->fileName, L"%s", ptFileName);
//MessageBoxW(NULL, netAddr->fileName, ptFileName, NULL);

    return hr;
}

void CNetRender::DeleteNetworkGraph(NetInfo *netAddr)
{
	//
	// Test if the filtergraph exists 
	//
	if(netAddr->pNetworkGraph)
	{
		//
		// Stop the graph just in case 
		//
		IMediaControl *pMediaControl;
		if (SUCCEEDED(netAddr->pNetworkGraph->QueryInterface(IID_IMediaControl, (void **) &pMediaControl)))
		{
			pMediaControl->Stop(); 
			pMediaControl->Release();
		}

		//
		// Unregister the filtergraph from the object table
		//
		if (netAddr->dwGraphRegister)
		{
			RemoveGraphFromRot(netAddr->dwGraphRegister);
			netAddr->dwGraphRegister = 0;
		}
	}

	//
	// Release the filtergraph filters & graph
	//
	if(netAddr->pNetSource) netAddr->pNetSource->Release();
	if(netAddr->pFileSink) netAddr->pFileSink->Release();
	if(netAddr->pNetworkGraph) netAddr->pNetworkGraph->Release();
	netAddr->pNetworkGraph = NULL;
	netAddr->pNetSource = NULL;
	netAddr->pFileSink = NULL;
}

BOOL CNetRender::IsMulticastActive(NetInfo *netAddr, NetInfoArray *netArray, int *pos)
{
	//
	// return if the array is empty, no running graphs
	//
	if (!(*netArray).Count())
		return FALSE;

	//
	// loop through the array for graphs that are already using the net IP
	for((*pos) = 0; (*pos) < (int)(*netArray).Count(); (*pos)++)
	{
		if (netAddr->userIP == (*netArray)[*pos].userIP
			&& netAddr->userPort == (*netArray)[*pos].userPort
			&& netAddr->userNic == (*netArray)[*pos].userNic)
		{
			//
			// return the filename of the found graph
			//
			lstrcpyW(netAddr->fileName, (*netArray)[*pos].fileName);
//MessageBoxW(NULL, netAddr->fileName, L"IsMulticastActive", NULL);
			return TRUE;
		}
	}
	return FALSE;
}

BOOL CNetRender::IsMulticastAddress(LPOLESTR lpszFileName, NetInfo *netAddr)
{
	wcslwr(lpszFileName);
	WCHAR *addrPosFile = wcsstr(lpszFileName, L"udp@");
	WCHAR *addrPosUrl = wcsstr(lpszFileName, L"udp://@");
	WCHAR *portPosFile = wcsstr(lpszFileName, L"#");
	WCHAR *portPosUrl = wcsstr(lpszFileName, L":");
	WCHAR *nicPosFile = wcsstr(lpszFileName, L"$");
	WCHAR *nicPosUrl = wcsstr(lpszFileName, L"$");
	WCHAR *endString = lpszFileName + wcslen(lpszFileName);

//MessageBoxW(NULL, lpszFileName,lpszFileName, NULL);
	//Check if we have a valid Network Address
	if (addrPosFile)
		addrPosFile = min(addrPosFile + 4, endString);
	else if (addrPosUrl)
		addrPosUrl = min(addrPosUrl + 7, endString);
	else
		return FALSE;

	//Check if we have a valid Port
	if (portPosFile)
		portPosFile = min(portPosFile + 1, endString);
	else if (portPosUrl)
		portPosUrl = min(portPosUrl + 1, endString);
	else
		return FALSE;

	//Check if we have a valid nic
	if (nicPosFile)
		nicPosFile = min(nicPosFile + 1, endString);
	else if (nicPosUrl)
		nicPosUrl = min(nicPosUrl + 1, endString);
	else
		return FALSE;

	wcsrev(lpszFileName);

	WCHAR *endPos = wcsstr(lpszFileName, L".");
	if (!endPos)
		endPos = lpszFileName + wcslen(lpszFileName);
	else
		endPos = min(endString - (endPos - lpszFileName), endString);

	wcsrev(lpszFileName);

	if (addrPosFile)
		lstrcpynW(netAddr->strIP, addrPosFile, min(15,max(0,(portPosFile - addrPosFile))));
	else if(addrPosUrl)
		lstrcpynW(netAddr->strIP, addrPosUrl, min(15,max(0,(portPosUrl - addrPosUrl))));

//MessageBoxW(NULL, netAddr->strIP,L"netAddr->strIP", NULL);

	TCHAR temp[MAX_PATH];
	sprintf((char *)temp, "%S", netAddr->strIP);	
	netAddr->userIP = inet_addr ((const char*)&temp);
	if (!IsMulticastingIP(netAddr->userIP))
		return FALSE;

	if (portPosFile)
		lstrcpynW(netAddr->strPort, portPosFile, min(5,max(0,(nicPosFile - portPosFile))));
	else if(portPosUrl)
		lstrcpynW(netAddr->strPort, portPosUrl, min(5,max(0,(nicPosUrl - portPosUrl))));

//MessageBoxW(NULL, netAddr->strPort,L"netAddr->strPort", NULL);

	sprintf((char *)temp, "%S", netAddr->strPort);	
//MessageBox(NULL, temp,"temp", NULL);
	netAddr->userPort = htons ((USHORT) (StrToInt((const char*)&temp) & 0x0000ffff));
	if (!netAddr->userPort)
		return FALSE;

	if (nicPosFile)
		lstrcpynW(netAddr->strNic, nicPosFile, min(15,max(0,(endPos - nicPosFile))));
	else if(nicPosUrl)
		lstrcpynW(netAddr->strNic, nicPosUrl, min(15,max(0,(endPos - nicPosUrl))));

//MessageBoxW(NULL, netAddr->strNic,L"netAddr->strNic", NULL);

	sprintf((char *)temp, "%S", netAddr->strNic);	
	netAddr->userNic = inet_addr ((const char*)&temp);
	if (!IsUnicastingIP(netAddr->userNic))
		return FALSE;

	//copy path to new filename
	if (addrPosFile)
		lstrcpynW(netAddr->pathName, lpszFileName, min(MAX_PATH,max(0,(addrPosFile - lpszFileName - 3))));
	else if(addrPosUrl)
		lstrcpynW(netAddr->pathName, lpszFileName, min(MAX_PATH,max(0,(addrPosFile - lpszFileName - 6))));

//MessageBoxW(NULL, netAddr->pathName,L"netAddr->pathName", NULL);
	//Addon in Multicast Address
	wsprintfW(netAddr->fileName, L"%SUDP (%S-%S-%S).tsbuffer",
								netAddr->pathName,
								netAddr->strIP,
								netAddr->strPort,
								netAddr->strNic);

//MessageBoxW(NULL, netAddr->fileName,netAddr->pathName, NULL);

	return TRUE;
}

static TBYTE Highest_IP[] = {239, 255, 255, 255};
static TBYTE Lowest_IP[] = {224, 0, 0, 0};

BOOL CNetRender::IsUnicastingIP(DWORD dwNetIP)
{
    return (((TBYTE *)&dwNetIP)[0] < Lowest_IP[0]) ;
}

BOOL CNetRender::IsMulticastingIP(DWORD dwNetIP)
{
    return (((TBYTE *)&dwNetIP)[0] >= Lowest_IP[0] &&
            ((TBYTE *)&dwNetIP)[0] <= Highest_IP[0]) ;
}

// Adds a DirectShow filter graph to the Running Object Table,
// allowing GraphEdit to "spy" on a remote filter graph.
HRESULT CNetRender::AddGraphToRot(
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

    hr = CreateItemMoniker(L"!", wsz, &pMoniker);
    if (SUCCEEDED(hr))
	{
        hr = pROT->Register(ROTFLAGS_REGISTRATIONKEEPSALIVE, pUnkGraph, 
                            pMoniker, pdwRegister);
	}
    return hr;
}
        
// Removes a filter graph from the Running Object Table
void CNetRender::RemoveGraphFromRot(DWORD pdwRegister)
{
    CComPtr <IRunningObjectTable> pROT;

    if (SUCCEEDED(GetRunningObjectTable(0, &pROT))) 
        pROT->Revoke(pdwRegister);
}

HRESULT CNetRender::GetObjectFromROT(WCHAR* wsFullName, IUnknown **ppUnk)
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

