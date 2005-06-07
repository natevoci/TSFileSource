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

CTSFileSourceProp::~CTSFileSourceProp(void)
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
	wsprintf(sz, TEXT("%u"), PidNr);
	Edit_SetText(GetDlgItem(m_hwnd, IDC_VIDEO), sz);
	m_pProgram->GetAudioPid(&PidNr);
	wsprintf(sz, TEXT("%u"), PidNr);
	Edit_SetText(GetDlgItem(m_hwnd, IDC_AUDIO), sz);
	m_pProgram->GetAudio2Pid(&PidNr);
	wsprintf(sz, TEXT("%u"), PidNr);
	Edit_SetText(GetDlgItem(m_hwnd, IDC_AUDIO2), sz);
	m_pProgram->GetAC3Pid(&PidNr);
	wsprintf(sz, TEXT("%u"), PidNr);
	Edit_SetText(GetDlgItem(m_hwnd, IDC_AC3), sz);

//***********************************************************************************************
//Audio2 Additions

	m_pProgram->GetAC3_2Pid(&PidNr);
	wsprintf(sz, TEXT("%u"), PidNr);
	Edit_SetText(GetDlgItem(m_hwnd, IDC_AC3_2), sz);

//**********************************************************************************************
//NID Additions

	m_pProgram->GetNIDPid(&PidNr);
	wsprintf(sz, TEXT("%u"), PidNr);
	Edit_SetText(GetDlgItem(m_hwnd, IDC_NID), sz);

//NID Additions

	unsigned char netname[128];
	m_pProgram->GetNetworkName((unsigned char*)&netname);
	sprintf(sz, "%s",netname);
	SetWindowText(GetDlgItem(m_hwnd, IDC_NETID), sz);

	unsigned char chnumb[128];
	m_pProgram->GetChannelNumber((unsigned char*)&chnumb);

//ONID Additions

	m_pProgram->GetONIDPid(&PidNr);
	wsprintf(sz, TEXT("%u"), PidNr);
	Edit_SetText(GetDlgItem(m_hwnd, IDC_ONID), sz);


	unsigned char onetname[128];
	unsigned char chname[128];
	m_pProgram->GetONetworkName((unsigned char*)&onetname);
	m_pProgram->GetChannelName((unsigned char*)&chname);
	sprintf(sz, "%s",onetname);
	SetWindowText(GetDlgItem(m_hwnd, IDC_ONETID), sz);
	sprintf(sz, "Ch %s :- %s",chnumb, chname);
	SetWindowText(GetDlgItem(m_hwnd, IDC_CHID), sz);

//TSID Additions
	
	m_pProgram->GetTSIDPid(&PidNr);
	wsprintf(sz, TEXT("%u"), PidNr);
	Edit_SetText(GetDlgItem(m_hwnd, IDC_TSID), sz);

//***********************************************************************************************
	
	m_pProgram->GetPMTPid(&PidNr);
	wsprintf(sz, TEXT("%u"), PidNr);
	Edit_SetText(GetDlgItem(m_hwnd, IDC_PMT), sz);
	m_pProgram->GetSIDPid(&PidNr);
	wsprintf(sz, TEXT("%u"), PidNr);
	Edit_SetText(GetDlgItem(m_hwnd, IDC_SID), sz);
	m_pProgram->GetPCRPid(&PidNr);
	wsprintf(sz, TEXT("%u"), PidNr);
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

	long rate;
	m_pProgram->GetBitRate(&rate);
    wsprintf(sz, TEXT("%lu"), rate);
    Edit_SetText(GetDlgItem(m_hwnd, IDC_DATARATE), sz);

	m_pProgram->GetTelexPid(&PidNr);
	wsprintf(sz, TEXT("%u"), PidNr);
	Edit_SetText(GetDlgItem(m_hwnd, IDC_TXT), sz);

	m_pProgram->GetPgmNumb(&PidNr);
	wsprintf(sz, TEXT("%u"), PidNr);
	Edit_SetText(GetDlgItem(m_hwnd, IDC_PGM), sz);

	m_pProgram->GetPgmCount(&PidNr);
	wsprintf(sz, TEXT("%u"), PidNr);
	Edit_SetText(GetDlgItem(m_hwnd, IDC_CNT), sz);

	m_pProgram->GetCreateTSPinOnDemux(&PidNr);
	CheckDlgButton(m_hwnd,IDC_CREATETSPIN, PidNr);

	m_pProgram->GetAC3Mode(&PidNr);
	CheckDlgButton(m_hwnd,IDC_AC3MODE,PidNr);

	m_pProgram->GetMP2Mode(&PidNr);
	CheckDlgButton(m_hwnd,IDC_MPEG1MODE,!PidNr);
	CheckDlgButton(m_hwnd,IDC_MPEG2MODE,PidNr);

//**********************************************************************************************
//Audio2 Additions

	m_pProgram->GetAudio2Mode(&PidNr);
	CheckDlgButton(m_hwnd,IDC_AUDIO2MODE,PidNr);

//**********************************************************************************************

	m_pProgram->GetAutoMode(&PidNr);
	CheckDlgButton(m_hwnd,IDC_AUTOMODE,PidNr);

//**********************************************************************************************
//NP Control Additions

	m_pProgram->GetNPControl(&PidNr);
	CheckDlgButton(m_hwnd,IDC_NPCTRL,PidNr);

//NP Slave Additions

	m_pProgram->GetNPSlave(&PidNr);
	CheckDlgButton(m_hwnd,IDC_NPSLAVE,PidNr);

//**********************************************************************************************

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
				{
					OnRefreshProgram () ;
					break ;
				}

//**********************************************************************************************
//Registry Additions

				case IDC_SAVE :
				{
					// Save Registry settings
					m_pProgram->SetRegSettings();
					m_pProgram->SetRegProgram();
					OnRefreshProgram () ;
					break ;
				}

//**********************************************************************************************

				case IDC_ENTER:
				{
					m_pProgram->SetPgmNumb((WORD) GetDlgItemInt(hwnd, IDC_PGM, &bRet, TRUE));
					OnRefreshProgram () ;

//**********************************************************************************************
//Property Apply Additions

					SetDirty();

//**********************************************************************************************

					break;
				}

				case IDC_NEXT:
				{
					m_pProgram->NextPgmNumb();
					OnRefreshProgram () ;

//**********************************************************************************************
//Property Apply Additions

					SetDirty();

//**********************************************************************************************

					break;
				}

//**********************************************************************************************
//Prev button Additions

				case IDC_PREV:
				{
					m_pProgram->PrevPgmNumb();
					OnRefreshProgram () ;

//**********************************************************************************************
//Property Apply Additions

					SetDirty();

//**********************************************************************************************

					break;
				}

//**********************************************************************************************

				case IDC_CREATETSPIN:
				{
					checked = (BOOL)IsDlgButtonChecked(hwnd,IDC_CREATETSPIN);
					m_pProgram->SetCreateTSPinOnDemux(checked);
					OnRefreshProgram();

//**********************************************************************************************
//Property Apply Additions

					SetDirty();

//**********************************************************************************************

					break;
				}
				case IDC_AC3MODE:
				{
					checked = (BOOL)IsDlgButtonChecked(hwnd,IDC_AC3MODE);
					m_pProgram->SetAC3Mode(checked);
					OnRefreshProgram () ;

//**********************************************************************************************
//Property Apply Additions

					SetDirty();

//**********************************************************************************************

					break;
				}

				case IDC_MPEG1MODE:
				{
					m_pProgram->SetMP2Mode(FALSE);
					CheckDlgButton(hwnd,IDC_MPEG1MODE,TRUE);
					CheckDlgButton(hwnd,IDC_MPEG2MODE,FALSE);
					OnRefreshProgram () ;

//**********************************************************************************************
//Property Apply Additions

					SetDirty();

//**********************************************************************************************

					break;
				}

				case IDC_MPEG2MODE:
				{
					m_pProgram->SetMP2Mode(TRUE);
					CheckDlgButton(hwnd,IDC_MPEG1MODE,FALSE);
					CheckDlgButton(hwnd,IDC_MPEG2MODE,TRUE);
					OnRefreshProgram () ;

//**********************************************************************************************
//Property Apply Additions

					SetDirty();

//**********************************************************************************************

					break;
				}

//**********************************************************************************************
//Audio2 Additions

				case IDC_AUDIO2MODE:
				{
					checked = (BOOL)IsDlgButtonChecked(hwnd,IDC_AUDIO2MODE);
					m_pProgram->SetAudio2Mode(checked);
					OnRefreshProgram () ;

//**********************************************************************************************
//Property Apply Additions

					SetDirty();

//**********************************************************************************************

					break;
				}

//**********************************************************************************************


				case IDC_AUTOMODE:
				{
					checked = (BOOL)IsDlgButtonChecked(hwnd,IDC_AUTOMODE);
					m_pProgram->SetAutoMode(checked);
					OnRefreshProgram () ;

//**********************************************************************************************
//Property Apply Additions

					SetDirty();

//**********************************************************************************************

					break;
				}

//**********************************************************************************************
//NP Control Additions
					case IDC_NPCTRL:
				{
					checked = (BOOL)IsDlgButtonChecked(hwnd,IDC_NPCTRL);
					m_pProgram->SetNPControl(checked);
					OnRefreshProgram () ;
					SetDirty();
					break;
				}

//NP Slave Additions

					case IDC_NPSLAVE:
				{
					checked = (BOOL)IsDlgButtonChecked(hwnd,IDC_NPSLAVE);
					m_pProgram->SetNPSlave(checked);
					OnRefreshProgram () ;
					SetDirty();
					break;
				}

					case IDC_EPGINFO:
				{
//					checked = (BOOL)IsDlgButtonChecked(hwnd,IDC_NPSLAVE);
//					m_pProgram->SetNPSlave(checked);
					if (m_pProgram->GetEPGFromFile() == S_OK)
					{
						unsigned char netname[128];
						unsigned char onetname[128];
						unsigned char chname[128];
						unsigned char chnumb[128];
						unsigned char shortdescripor[128];
						unsigned char Extendeddescripor[600];
						unsigned char shortnextdescripor[128];
						unsigned char Extendednextdescripor[600];
						m_pProgram->GetNetworkName((unsigned char*)&netname);
						m_pProgram->GetONetworkName((unsigned char*)&onetname);
						m_pProgram->GetChannelName((unsigned char*)&chname);
						m_pProgram->GetChannelNumber((unsigned char*)&chnumb);
						m_pProgram->GetShortDescr((unsigned char*)&shortdescripor);
						m_pProgram->GetExtendedDescr((unsigned char*)&Extendeddescripor);
						m_pProgram->GetShortNextDescr((unsigned char*)&shortnextdescripor);
						m_pProgram->GetExtendedNextDescr((unsigned char*)&Extendednextdescripor);
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
					OnRefreshProgram () ;
					break;
				}

//**********************************************************************************************

				case IDC_DELAYMODE:
				{
					checked = (BOOL)IsDlgButtonChecked(hwnd, IDC_DELAYMODE);
					m_pProgram->SetDelayMode(checked);
					OnRefreshProgram () ;

//**********************************************************************************************
//Property Apply Additions

					SetDirty();

//**********************************************************************************************

					break;
				}

				case IDC_RATECONTROL:
				{
					checked = (BOOL)IsDlgButtonChecked(hwnd, IDC_RATECONTROL);
					m_pProgram->SetRateControlMode(checked);
					OnRefreshProgram ();

//**********************************************************************************************
//Property Apply Additions

					SetDirty();

//**********************************************************************************************

					break;
				}

				case IDC_REFRESH:
					{
						m_pProgram->Refresh();
						OnRefreshProgram();
						break;
					}
				
			};
			return TRUE;
		}

		default:
			return FALSE;
	}

	return TRUE;
}

//**********************************************************************************************
//Property Apply Additions

HRESULT CTSFileSourceProp::OnApplyChanges(void)
{

	TCHAR sz[100];
	sprintf(sz, "%S", L"Do you wish to save these Filter Settings");
	if (MessageBox(NULL, sz, TEXT("TSFileSource Filter Settings"), MB_YESNO) == IDYES)
	{
		m_pProgram->SetRegSettings();
		m_pProgram->SetRegProgram();
		OnRefreshProgram ();
	}
	return NOERROR;
}
//**********************************************************************************************

