/**
*  StreamParser.cpp
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

//#include <streams.h>
#include "StreamParser.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


StreamParser::StreamParser(PidParser *pPidParser, Demux * pDemux)
{
	m_pPidParser = pPidParser;
	m_pDemux = pDemux;
}

StreamParser::~StreamParser()
{
}

HRESULT StreamParser::ParsePidArray()
{
	HRESULT hr = S_FALSE;

	StreamArray.Clear();
	if (!m_pPidParser->pidArray.Count())
		return hr;

	int count = 0;
	int index = 0;

	//Setup Network Name
	streams.Clear();
	LoadStreamArray(0);
	AddStreamArray();
	TCHAR szBuffer[128];
	sprintf(szBuffer,"  >>>> %s <<<<", m_pPidParser->m_NetworkName);
	mbstowcs(StreamArray[index].name, (const char*)&szBuffer, 128);
	ZeroMemory(szBuffer, sizeof(szBuffer));
	m_pDemux->GetVideoMedia(&StreamArray[index].media);
	index++;

	while (count < m_pPidParser->pidArray.Count())
	{
		streams.Clear();
		LoadStreamArray(count);

		//Setup Video Track
		AddStreamArray();
		TCHAR szBuffer[400];
		sprintf(szBuffer,"  %s - %i : %s", m_pPidParser->pidArray[count].onetname,
										m_pPidParser->pidArray[count].chnumb,
										m_pPidParser->pidArray[count].chname);
		mbstowcs(StreamArray[index].name, (const char*)&szBuffer, 256);
		StreamArray[index].Pid = m_pPidParser->pidArray[count].vid;
		StreamArray[index].lcid = 0;
		if (m_pPidParser->pidArray[count].vid)
		{
			StreamArray[index].Vid = true;
			m_pDemux->GetVideoMedia(&StreamArray[index].media);
		}
		else if (m_pPidParser->pidArray[count].h264)
		{
			StreamArray[index].Vid = true;
			m_pDemux->GetH264Media(&StreamArray[index].media);
		}
		index++;

		//Setup Audio Tracks
		if (m_pPidParser->pidArray[count].aud)
		{
			AddStreamArray();
			StreamArray[index].Aud = true;
			StreamArray[index].Pid = m_pPidParser->pidArray[count].aud;
			m_pDemux->GetMP2Media(&StreamArray[index].media);
			wcscat(StreamArray[index].name, L"        Mpeg Audio Track");
			StreamArray[index].lcid = MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_NEUTRAL), SORT_DEFAULT);
			index++;
		}
		if (m_pPidParser->pidArray[count].aud2)
		{
			AddStreamArray();
			StreamArray[index].AC3 = true;
			StreamArray[index].Aud2 = true;
			m_pDemux->GetMP2Media(&StreamArray[index].media);
			StreamArray[index].Pid = m_pPidParser->pidArray[count].aud2;
			wcscat(StreamArray[index].name, L"        Mpeg Audio Track 2");
			StreamArray[index].lcid = MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_NEUTRAL), SORT_DEFAULT);
			index++;
		}
		if (m_pPidParser->pidArray[count].ac3)
		{
			AddStreamArray();
			StreamArray[index].AC3 = true;
			StreamArray[index].Pid = m_pPidParser->pidArray[count].ac3;
			m_pDemux->GetAC3Media(&StreamArray[index].media);
			wcscat(StreamArray[index].name, L"        AC3 Audio Track");
			StreamArray[index].lcid = MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_NEUTRAL), SORT_DEFAULT);
			index++;
		}
		if (m_pPidParser->pidArray[count].ac3_2)
		{
			AddStreamArray();
			StreamArray[index].AC3 = true;
			StreamArray[index].Aud2 = true;
			StreamArray[index].Pid = m_pPidParser->pidArray[count].ac3_2;
			m_pDemux->GetAC3Media(&StreamArray[index].media);
			wcscat(StreamArray[index].name, L"        AC3 Audio Track 2");
			StreamArray[index].lcid = MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_NEUTRAL), SORT_DEFAULT);
			index++;
		}
		count++;
	}
	return S_OK;
}
	
void StreamParser::LoadStreamArray(int cnt)
{
		streams.flags = 0; 
		streams.group = cnt;
		streams.object = 0;
		streams.unk = 0;
}
	
void StreamParser::SetStreamActive(int group)
{
	int count = 0;
	while (count < StreamArray.Count())
	{
		StreamArray[count].flags = 0;
		memcpy(StreamArray[count].name, &L"  ", 4);

		if (StreamArray[count].group == group)
		{
			if (StreamArray[count].Vid)
			{
				memcpy(StreamArray[count].name, &L"->", 4);
			}
			else if (StreamArray[count].Pid == m_pDemux->m_SelAudioPid)
			{
				StreamArray[count].flags = AMSTREAMSELECTINFO_EXCLUSIVE; //AMSTREAMSELECTINFO_ENABLED;
				memcpy(StreamArray[count].name, &L"->", 4);
				m_StreamIndex = count;
			}
		}
		count++;
	}

}

bool StreamParser::IsStreamActive(int index)
{
	if (StreamArray[index].flags == AMSTREAMSELECTINFO_EXCLUSIVE)
		return true;

	return false;
}

void StreamParser::AddStreamArray()
{
	StreamInfo *newStreamInfo = new StreamInfo();
	StreamArray.Add(newStreamInfo);
	SetStreamArray(StreamArray.Count() - 1);
}


void StreamParser::SetStreamArray(int n)
{
	if ((n < 0) || (n >= StreamArray.Count()))
		return;
	StreamInfo *StreamInfo = &StreamArray[n];

	StreamInfo->CopyFrom(&streams);
}

void StreamParser::set_StreamArray(int streamIndex)
{
	m_StreamIndex = streamIndex;
	streams.Clear();
	streams.CopyFrom(&StreamArray[streamIndex]);
}

WORD StreamParser::get_StreamIndex()
{
	return m_StreamIndex;
}

void StreamParser::set_StreamIndex(WORD streamIndex)
{
	m_StreamIndex = streamIndex;
}

//				WCHAR test(0x00B3);
//				memcpy(StreamArray[count].name, &test, 2);

