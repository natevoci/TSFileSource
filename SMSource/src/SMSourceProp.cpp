/**
*  SMSourceProp.ccp
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





#include "SMSourceProp.h"


CUnknown * WINAPI CSMSourceProp::CreateInstance(LPUNKNOWN pUnk, HRESULT *pHr) 
{
	ASSERT(pHr);
    CSMSourceProp *pNewObject = new CSMSourceProp(pUnk);
    if (pNewObject == NULL) 
    {
        *pHr = E_OUTOFMEMORY;
    }
    return pNewObject;
} 



// Property Interface

CSMSourceProp::CSMSourceProp(IUnknown *pUnk) : 

  CBasePropertyPage(NAME("SMSourceProp"), pUnk, IDD_PROPPAGE, IDS_PROPPAGE_TITLE),
  m_pProgram(0)

{ };

CSMSourceProp::~CSMSourceProp()
{ };

//
// SetDirty
//
// Sets m_bDirty and notifies the property page site of the change
//
void CSMSourceProp::SetDirty()
{
    m_bDirty = TRUE;

    if(m_pPageSite)
    {
        m_pPageSite->OnStatusChange(PROPPAGESTATUS_DIRTY);
    }

} // SetDirty


BOOL CSMSourceProp::OnRefreshProgram()
{

    PopulateTransportStreamInfo();

    return TRUE;

} //OnRefreshProgram



BOOL CSMSourceProp::OnReceiveMessage(HWND hwnd,
                                           UINT uMsg,
                                           WPARAM wParam,
                                           LPARAM lParam)
{
    switch(uMsg)
    {
        case WM_INITDIALOG:
        {
            m_hwndDialog = hwnd;
            PopulateTransportStreamInfo( );
            return TRUE;
        }

        case WM_DESTROY:
        {
            DestroyWindow(m_hwndDialog);
            return TRUE;
        }

        case WM_COMMAND:
        {
            
            switch (LOWORD (wParam)) {

                case IDCANCEL :
                    OnRefreshProgram () ;
                    break ;
            };
            return TRUE;
        }

        default:
            return FALSE;
    }
    
    return TRUE;
}


HRESULT CSMSourceProp::OnConnect(IUnknown *pUnk)
{
    if (pUnk == NULL)
    {
        return E_POINTER;
    }
    ASSERT(m_pProgram == NULL);

    HRESULT hr = pUnk->QueryInterface(IID_ISMSource, (void**)(&m_pProgram));

	if(FAILED(hr))
    {
        return E_NOINTERFACE;
    }
    ASSERT(m_pProgram);

	IBaseFilter * pParserFilter ;
    hr = m_pProgram->QueryInterface(IID_IBaseFilter, (void **) &pParserFilter);

    FILTER_INFO Info;
    IFilterGraph * pGraph;
    hr = pParserFilter->QueryFilterInfo(&Info);

    pGraph = Info.pGraph;
    pParserFilter->Release();

    hr = pGraph->QueryInterface(IID_IGraphBuilder, (void **) & m_pGraphBuilder);
    if(FAILED(hr))
    {
        return S_FALSE;
    }

    // if there is no streaming, the following variables will not be initialized.
    if(m_pGraphBuilder != NULL){
        hr = m_pGraphBuilder->QueryInterface(IID_IMediaControl, (void **) & m_pMediaControl);
        
        // Get the initial Program value
        // m_pProgram->GetTransportStreamId( &m_stream_id);
        // m_pProgram->GetPatVersionNumber( &m_pat_version);
        // m_pProgram->GetCountOfPrograms( &m_number_of_programs );

    }

    return NOERROR;
}


HRESULT CSMSourceProp::OnActivate(void)
{
//    ASSERT(m_pProp != NULL);
	PopulateTransportStreamInfo();
    HRESULT hr = S_OK;
    return hr;
}


HRESULT CSMSourceProp::OnDisconnect(void)
{
    if (m_pProgram)
    {
        // If the user clicked OK, m_lVal holds the new value.
        // Otherwise, if the user clicked Cancel, m_lVal is the old value.
        // m_pGray->SetSaturation(m_lVal);  
        m_pProgram->Release();
        m_pProgram = NULL;
    }
    return S_OK;
}



BOOL CSMSourceProp::PopulateTransportStreamInfo(  )
{

    TCHAR sz[60];
	WORD PidNr = 0x00;
	REFERENCE_TIME dur;

	m_pProgram->GetVideoPid(&PidNr);
    wsprintf(sz, TEXT("%x"), PidNr);
    Edit_SetText(GetDlgItem(m_hwndDialog, IDC_VIDEO), sz);
	m_pProgram->GetAudioPid(&PidNr);
    wsprintf(sz, TEXT("%x"), PidNr);
    Edit_SetText(GetDlgItem(m_hwndDialog, IDC_AUDIO), sz);
	m_pProgram->GetAudio2Pid(&PidNr);
    wsprintf(sz, TEXT("%x"), PidNr);
    Edit_SetText(GetDlgItem(m_hwndDialog, IDC_AUDIO2), sz);
	m_pProgram->GetAC3Pid(&PidNr);
    wsprintf(sz, TEXT("%x"), PidNr);
    Edit_SetText(GetDlgItem(m_hwndDialog, IDC_AC3), sz);
	m_pProgram->GetPMTPid(&PidNr);
    wsprintf(sz, TEXT("%x"), PidNr);
    Edit_SetText(GetDlgItem(m_hwndDialog, IDC_PMT), sz);
	m_pProgram->GetSIDPid(&PidNr);
    wsprintf(sz, TEXT("%x"), PidNr);
    Edit_SetText(GetDlgItem(m_hwndDialog, IDC_SID), sz);
    m_pProgram->GetPCRPid(&PidNr);
    wsprintf(sz, TEXT("%x"), PidNr);
    Edit_SetText(GetDlgItem(m_hwndDialog, IDC_PCR), sz);
    m_pProgram->GetDur(&dur);
    wsprintf(sz, TEXT("%lu"), ConvertToMilliseconds(dur));
    Edit_SetText(GetDlgItem(m_hwndDialog, IDC_DURATION), sz);

    return TRUE;
}






