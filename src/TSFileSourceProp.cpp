/**
*  TSFileSourceProp.cpp
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
#include "TSFileSourceProp.h"
#include "resource.h"

CUnknown * WINAPI CTSFileSourceProp::CreateInstance(LPUNKNOWN pUnk, HRESULT *pHr)
{
	CTSFileSourceProp *pNewObject = new CTSFileSourceProp(pUnk);
	if (pNewObject == NULL)
	{
		*pHr = E_OUTOFMEMORY;
	}
	return pNewObject;
}


CTSFileSourceProp::CTSFileSourceProp(IUnknown *pUnk) :
	CBasePropertyPage(NAME("TSFileSourceProp"), pUnk, IDD_PROPPAGE, IDS_PROPPAGE_TITLE),
	m_pProgram(0)
{
}

HRESULT CTSFileSourceProp::OnConnect(IUnknown *pUnk)
{
	if (pUnk == NULL)
	{
		return E_POINTER;
	}
	ASSERT(m_pProgram == NULL);

	HRESULT hr = pUnk->QueryInterface(IID_ITSFileSource, (void**)(&m_pProgram));

	if(FAILED(hr))
	{
		return E_NOINTERFACE;
	}
	ASSERT(m_pProgram);

	return NOERROR;
}

HRESULT CTSFileSourceProp::OnDisconnect(void)
{
	if (m_pProgram)
	{
		m_pProgram->Release();
		m_pProgram = NULL;
	}
	return S_OK;
}

HRESULT CTSFileSourceProp::OnActivate(void)
{
	ASSERT(m_pProgram != NULL);
	PopulateDialog();
	HRESULT hr = S_OK;
	return hr;
}

void CTSFileSourceProp::SetDirty()
{
	m_bDirty = TRUE;

	if(m_pPageSite)
	{
		m_pPageSite->OnStatusChange(PROPPAGESTATUS_DIRTY);
	}
}

BOOL CTSFileSourceProp::OnRefreshProgram()
{
	PopulateDialog();
	return TRUE;
}

BOOL CTSFileSourceProp::PopulateDialog()
{

	TCHAR sz[60];
	WORD PidNr = 0x00;
	REFERENCE_TIME dur;

	m_pProgram->GetVideoPid(&PidNr);
	wsprintf(sz, TEXT("%x"), PidNr);
	Edit_SetText(GetDlgItem(m_hwnd, IDC_VIDEO), sz);
	m_pProgram->GetAudioPid(&PidNr);
	wsprintf(sz, TEXT("%x"), PidNr);
	Edit_SetText(GetDlgItem(m_hwnd, IDC_AUDIO), sz);
	m_pProgram->GetAudio2Pid(&PidNr);
	wsprintf(sz, TEXT("%x"), PidNr);
	Edit_SetText(GetDlgItem(m_hwnd, IDC_AUDIO2), sz);
	m_pProgram->GetAC3Pid(&PidNr);
	wsprintf(sz, TEXT("%x"), PidNr);
	Edit_SetText(GetDlgItem(m_hwnd, IDC_AC3), sz);
	m_pProgram->GetPMTPid(&PidNr);
	wsprintf(sz, TEXT("%x"), PidNr);
	Edit_SetText(GetDlgItem(m_hwnd, IDC_PMT), sz);
	m_pProgram->GetSIDPid(&PidNr);
	wsprintf(sz, TEXT("%x"), PidNr);
	Edit_SetText(GetDlgItem(m_hwnd, IDC_SID), sz);
	m_pProgram->GetPCRPid(&PidNr);
	wsprintf(sz, TEXT("%x"), PidNr);
	Edit_SetText(GetDlgItem(m_hwnd, IDC_PCR), sz);
	m_pProgram->GetDuration(&dur);
	LONG ms = (LONG)(dur/(LONGLONG)10000);
	LONG secs = ms / 1000;
	LONG mins = secs / 60;
	LONG hours = mins / 60;
	ms -= (secs*1000);
	secs -= (mins*60);
	mins -= (hours*60);
	//wsprintf(sz, TEXT("%lu ms"), ConvertToMilliseconds(dur));
	wsprintf(sz, TEXT("%02i:%02i:%02i.%03i"), hours, mins, secs, ms);
	SetWindowText(GetDlgItem(m_hwnd, IDC_DURATION), sz);

//*********************************************************************************************
//Bitrate addition
	long rate;
	m_pProgram->GetBitRate(&rate);
    wsprintf(sz, TEXT("%lu"), rate);
    Edit_SetText(GetDlgItem(m_hwnd, IDC_DATARATE), sz);

//*********************************************************************************************

//*********************************************************************************************
//AC3 & Teletext addition

	m_pProgram->GetTelexPid(&PidNr);
	wsprintf(sz, TEXT("%x"), PidNr);
	Edit_SetText(GetDlgItem(m_hwnd, IDC_TXT), sz);

	m_pProgram->GetPgmNumb(&PidNr);
	wsprintf(sz, TEXT("%x"), PidNr);
	Edit_SetText(GetDlgItem(m_hwnd, IDC_PGM), sz);

	m_pProgram->GetPgmCount(&PidNr);
	wsprintf(sz, TEXT("%x"), PidNr);
	Edit_SetText(GetDlgItem(m_hwnd, IDC_CNT), sz);

	m_pProgram->GetCreateTSPinOnDemux(&PidNr);
	CheckDlgButton(m_hwnd,IDC_CREATETSPIN, PidNr);

	m_pProgram->GetAC3Mode(&PidNr);
	CheckDlgButton(m_hwnd,IDC_AC3MODE,PidNr);

	m_pProgram->GetMP2Mode(&PidNr);
	CheckDlgButton(m_hwnd,IDC_MPEG1MODE,!PidNr);
	CheckDlgButton(m_hwnd,IDC_MPEG2MODE,PidNr);

	m_pProgram->GetAutoMode(&PidNr);
	CheckDlgButton(m_hwnd,IDC_AUTOMODE,PidNr);

	m_pProgram->GetRateControlMode(&PidNr);
	CheckDlgButton(m_hwnd,IDC_RATECONTROL,PidNr);

	m_pProgram->GetReadOnly(&PidNr);
	wsprintf(sz, (PidNr==0?TEXT("Normal"):TEXT("ReadOnly")));
	SetWindowText(GetDlgItem(m_hwnd, IDC_FILEMODE), sz);
	EnableWindow(GetDlgItem(m_hwnd, IDC_DELAYMODE), PidNr);

	if (PidNr)
		m_pProgram->GetDelayMode(&PidNr);
	else
		PidNr = 0;
	CheckDlgButton(m_hwnd,IDC_DELAYMODE,PidNr);

//*********************************************************************************************

	return TRUE;
}

BOOL CTSFileSourceProp::OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	BOOL    bRet = FALSE;

	switch(uMsg)
	{
		case WM_INITDIALOG:
		{
			PopulateDialog();
			return TRUE;
		}

		case WM_DESTROY:
		{
			DestroyWindow(m_hwnd);
			return TRUE;
		}

		case WM_COMMAND:
		{
			BOOL checked = FALSE;
			switch (LOWORD (wParam))
			{
				case IDCANCEL :
					OnRefreshProgram () ;
					break ;

				case IDC_ENTER:
					m_pProgram->SetPgmNumb((WORD) GetDlgItemInt(hwnd, IDC_PGM, &bRet, TRUE));
					OnRefreshProgram () ;
					break;

				case IDC_NEXT:
					m_pProgram->NextPgmNumb();
					OnRefreshProgram () ;
					break;

				case IDC_CREATETSPIN:
					checked = (BOOL)IsDlgButtonChecked(hwnd,IDC_CREATETSPIN);
					m_pProgram->SetCreateTSPinOnDemux(checked);
					OnRefreshProgram();
					break;

				case IDC_AC3MODE:
					checked = (BOOL)IsDlgButtonChecked(hwnd,IDC_AC3MODE);
					m_pProgram->SetAC3Mode(checked);
					OnRefreshProgram () ;
					break;

				case IDC_MPEG1MODE:
					m_pProgram->SetMP2Mode(FALSE);
					CheckDlgButton(hwnd,IDC_MPEG1MODE,TRUE);
					CheckDlgButton(hwnd,IDC_MPEG2MODE,FALSE);
					OnRefreshProgram () ;
					break;

				case IDC_MPEG2MODE:
					m_pProgram->SetMP2Mode(TRUE);
					CheckDlgButton(hwnd,IDC_MPEG1MODE,FALSE);
					CheckDlgButton(hwnd,IDC_MPEG2MODE,TRUE);
					OnRefreshProgram () ;
					break;

				case IDC_AUTOMODE:
					checked = (BOOL)IsDlgButtonChecked(hwnd,IDC_AUTOMODE);
					m_pProgram->SetAutoMode(checked);
					OnRefreshProgram () ;
					break;

				case IDC_DELAYMODE:
					checked = (BOOL)IsDlgButtonChecked(hwnd, IDC_DELAYMODE);
					m_pProgram->SetDelayMode(checked);
					OnRefreshProgram () ;
					break;

				case IDC_RATECONTROL:
					checked = (BOOL)IsDlgButtonChecked(hwnd, IDC_RATECONTROL);
					m_pProgram->SetRateControlMode(checked);
					OnRefreshProgram () ;
					break;
			};
			return TRUE;
		}

		default:
			return FALSE;
	}

	return TRUE;
}


