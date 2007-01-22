/**
*  Mpeg2TsParser.cpp
*  Copyright (C) 2007      nate
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
*  bear and nate can be reached on the forums at
*    http://forums.dvbowners.com/
*/

#include <windows.h>
#include <shlobj.h>
#include <iostream>
#include "Mpeg2Parser.h" 
#include "crc32.h"

static char logbuffer[2000]; 

#ifndef CSIDL_COMMON_APPDATA
#define CSIDL_COMMON_APPDATA 0x0023
#endif

void LogDebug(const char *fmt, ...) 
{
	va_list ap;
	va_start(ap,fmt);
	
	int tmp;
	va_start(ap,fmt);
	tmp=vsprintf(logbuffer, fmt, ap);
	va_end(ap); 
	
	TCHAR folder[MAX_PATH];
	TCHAR fileName[MAX_PATH];
	::SHGetSpecialFolderPath(NULL,folder,CSIDL_COMMON_APPDATA,FALSE);
	sprintf(fileName,"%s\\MediaPortal TV Server\\log\\TSFileSource.Log",folder);
	FILE* fp = fopen(fileName,"a+");
	if (fp!=NULL)
	{
		SYSTEMTIME systemTime;
		GetLocalTime(&systemTime);
		fprintf(fp,"%02.2d-%02.2d-%04.4d %02.2d:%02.2d:%02.2d %s\n",
			systemTime.wDay, systemTime.wMonth, systemTime.wYear,
			systemTime.wHour,systemTime.wMinute,systemTime.wSecond,
			logbuffer);
		fclose(fp);
		//::OutputDebugStringA(logbuffer);::OutputDebugStringA("\n");
	}
};

//////////////////////////////////////////////////////////////////////
// Mpeg2Parser
//////////////////////////////////////////////////////////////////////

Mpeg2Parser::Mpeg2Parser()
{
	m_pData = NULL;
	m_dataBufferSize = 0;
	m_dataAllocationOffset = 0;

	m_packetSize = 0;
	m_filePosition = 0;
	m_syncOffset = 0;
	//memset(&m_partialSections, NULL, MAX_PID*sizeof(Mpeg2Section *));

	m_bLog = TRUE;
}

Mpeg2Parser::~Mpeg2Parser()
{
	Clear();

	m_discontinuities.clear();
	std::list<PSIInfo *>::iterator it = m_psiInfoList.begin();
	for ( ; it != m_psiInfoList.end() ; it++ )
	{
		delete *it;
	}
	m_psiInfoList.clear();

	if (m_pData)
		delete[] m_pData;
}

void Mpeg2Parser::Clear()
{
	m_syncOffset = 0;
	
	std::vector<Mpeg2Section *>::iterator it = m_sections.begin();
	for ( ; it != m_sections.end() ; it++ )
	{
		delete *it;
	}
	m_sections.clear();

	m_dataAllocationOffset = 0;
}

__int64 Mpeg2Parser::GetCurrentPosition()
{
	return m_filePosition;
}

ULONG Mpeg2Parser::GetEndDataPosition()
{
	return m_filePosition + m_dataAllocationOffset;
}

HRESULT Mpeg2Parser::GetDataBuffer(__int64 filePosition, ULONG ulDataLength, BYTE **ppData)
{
	if (m_filePosition != filePosition)
	{
		Clear();
		m_filePosition = filePosition;
	}

	ULONG sizeRequired = ulDataLength + m_dataAllocationOffset;

	if ((m_pData == NULL) || (sizeRequired > m_dataBufferSize))
	{
		sizeRequired *= 2;	//Let's just double it so we don't do as many reallocations.
		BYTE* newBuffer = new BYTE[sizeRequired];

		if (m_pData != NULL)
		{
			if (m_dataAllocationOffset > 0)
			{
				memcpy(newBuffer, m_pData, m_dataAllocationOffset);
			}
			delete[] m_pData;
		}

		m_pData = newBuffer;
		m_dataBufferSize = sizeRequired;
	}
	*ppData = m_pData + m_dataAllocationOffset;
	return S_OK;
}

HRESULT Mpeg2Parser::ParseData(ULONG ulDataLength)
{
	HRESULT hr;

	if (m_packetSize == 0)
	{
		hr = DetectMediaFormat(m_pData, ulDataLength);
		if (hr != S_OK)
			return hr;
	}

	m_syncOffset = m_syncOffset % m_packetSize;

	//Just show the legend the first time
	static BOOL bShowLegend = TRUE;
	if (bShowLegend && m_bLog)
	{
		Mpeg2TSPacket::LogLegend();
		bShowLegend = FALSE;
	}

	// Go through data and create sections.
	hr = S_FALSE;
	while ((SUCCEEDED(hr)) && (m_syncOffset + m_packetSize <= ulDataLength))
	{
		hr = FindSyncByte(m_pData, ulDataLength, &m_syncOffset);
		if (hr != S_OK)
			break;

		while (m_syncOffset + m_packetSize <= ulDataLength)
		{
			Mpeg2TSPacket tsPacket;
			hr = tsPacket.ParseTSPacket(m_pData + m_syncOffset, ulDataLength - m_syncOffset, m_filePosition + m_syncOffset);
			if (hr != S_OK)
			{
				break;
			}

			if (m_bLog)
				tsPacket.Log();

			BuildSections(&tsPacket, m_pData + m_syncOffset, ulDataLength - m_syncOffset);

			m_syncOffset += m_packetSize;
		}
	}

	// Buffer remaining bytes for next time
	if (m_syncOffset < ulDataLength)
	{
		int len = ulDataLength - m_syncOffset;
		memcpy(m_pData, m_pData+m_syncOffset, len);
		m_dataAllocationOffset = len;
	}

	m_filePosition += m_syncOffset;

	// TODO: Deal with Sections

	return S_OK;
}

HRESULT Mpeg2Parser::DetectMediaFormat(BYTE *pData, ULONG ulDataLength)
{
	m_syncOffset = 0;
	HRESULT hr = FindTSSyncByte(pData, ulDataLength, &m_syncOffset);
	if (hr == S_OK)
	{
		m_packetSize = TS_PACKET_SIZE;
		return S_OK;
	}

	m_syncOffset = 0;
	hr = FindPSSyncByte(pData, ulDataLength, &m_syncOffset);
	if (hr == S_OK)
	{
		m_packetSize = PS_PACKET_SIZE;
		return S_OK;
	}
	return hr;
}

HRESULT Mpeg2Parser::FindSyncByte(PBYTE pbData, ULONG ulDataLength, ULONG* syncBytePosition)
{
	if (m_packetSize == TS_PACKET_SIZE)
		return FindTSSyncByte(pbData, ulDataLength, syncBytePosition);
	else if (m_packetSize == PS_PACKET_SIZE)
		return FindTSSyncByte(pbData, ulDataLength, syncBytePosition);

	return E_FAIL;
}

HRESULT Mpeg2Parser::FindTSSyncByte(PBYTE pbData, ULONG ulDataLength, ULONG* syncBytePosition)
{
	while ((*syncBytePosition >= 0) && (*syncBytePosition+TS_PACKET_SIZE < ulDataLength))
	{
		ULONG pos = *syncBytePosition;

		if (pbData[pos] == 0x47 && (pbData[pos+1]&0x80) == 0)
		{
			pos += TS_PACKET_SIZE;
			if ((pos >= 0) && (pos+1 < ulDataLength))
			{
				if (pbData[pos] == 0x47 && (pbData[pos+1]&0x80) == 0)
					return S_OK;
			}
		}
		*syncBytePosition ++;
	}

	return E_FAIL;
}

HRESULT Mpeg2Parser::FindPSSyncByte(PBYTE pbData, ULONG ulDataLength, ULONG* syncBytePosition)
{
/*
	//look for Program Pin Mode
	if (pPidParser->m_ProgPinMode)
	{
		//Look for Mpeg Program Pack headers
		while ((*a >= 0) && (*a < ulDataLength))
		{
			if ((ULONG)((0xFF&pbData[*a])<<24
					| (0xFF&pbData[*a+1])<<16
					| (0xFF&pbData[*a+2])<<8
					| (0xFF&pbData[*a+3])) == 0x1BA)
			{
				if (*a+pPidParser->m_PacketSize < ulDataLength)
				{
					if ((ULONG)((0xFF&pbData[*a + pPidParser->m_PacketSize])<<24
						| (0xFF&pbData[*a+1 + pPidParser->m_PacketSize])<<16
						| (0xFF&pbData[*a+2 + pPidParser->m_PacketSize])<<8
						| (0xFF&pbData[*a+3 + pPidParser->m_PacketSize])) == 0x1BA)
						return S_OK;
				}
				else
				{
					if (step > 0)
						return E_FAIL;

					if (*a-pPidParser->m_PacketSize > 0)
					{
						if ((ULONG)((0xFF&pbData[*a - pPidParser->m_PacketSize])<<24
						| (0xFF&pbData[*a+1 - pPidParser->m_PacketSize])<<16
						| (0xFF&pbData[*a+2 - pPidParser->m_PacketSize])<<8
						| (0xFF&pbData[*a+3 - pPidParser->m_PacketSize])) == 0x1BA)
							return S_OK;
					}
				}
			}
			*a += step;
		}
		return E_FAIL;
	}
*/
	return S_FALSE;
}

HRESULT Mpeg2Parser::BuildSections(Mpeg2TSPacket *pTSHeader, BYTE *tsPacket, ULONG ulDataLength)
{
	HRESULT hr;
	Mpeg2Section *pSection = m_partialSections.GetSection(pTSHeader->PID);

	if ((pSection != NULL) && (pSection->m_sectionPos > 0) && (pSection->section_length > 0) && (pTSHeader->pointer_field > 0))
	{
		int len = pSection->section_length - pSection->m_sectionPos;

		if (pTSHeader->payload_unit_start_indicator)
		{
			len = pTSHeader->pointer_field - pTSHeader->payload_start;

			if (len > (pSection->section_length - pSection->m_sectionPos))
				len = (pSection->section_length - pSection->m_sectionPos);
		}

		if (len > (TS_PACKET_SIZE - pTSHeader->payload_start))
			len = (TS_PACKET_SIZE - pTSHeader->payload_start);

		if ((pSection->m_sectionPos+len) >= MAX_SECTION_LENGTH)
		{
			LogDebug("section decoder:section length to large3 pid:%x table:%x", pSection->PID, pSection->table_id);
			return S_FALSE;
		}

		if (len <= 0)
		{
			LogDebug("section decoder:section len < 0 pid:%x table:%x", pSection->PID, pSection->table_id);
			return S_FALSE;
		}

		memcpy(&pSection->m_data[pSection->m_sectionPos], &tsPacket[pTSHeader->payload_start], len);
		pSection->m_sectionPos += len;
		if (m_bLog)
			LogDebug("pid:%03.3x tid:%03.3x add %d %d",pSection->PID, pSection->table_id, pSection->m_sectionPos, pSection->section_length);
		
		if (pSection->m_sectionPos >= pSection->section_length)
		{
			ProcessSection(pSection);
		}
	}

	if (pTSHeader->payload_unit_start_indicator)
	{
		int start = pTSHeader->pointer_field;

		// Detect PES packets
		if ((tsPacket[start] == 0x00) && (tsPacket[start+1] == 0x00) && (tsPacket[start+2] == 0x01))
		{
			LogDebug("pid:%03.3x pes sid:%03.3x",pTSHeader->PID, tsPacket[start+3]);
		}
		// Detect Section (looks for section_syntax_indicator set to 1, and second bit set to 0)
		else if ((tsPacket[start+1] & 0xC0) == 0x80)
		{
			// Load partial section. Hopefully previous section was processed. If not we discard it
			pSection = m_partialSections.GetSection(pTSHeader->PID);
			if (pSection != NULL)
			{
				m_partialSections.Remove(pSection->PID);
				delete pSection;
			}

			//Create new section
			pSection = new Mpeg2Section(pTSHeader->PID);
			m_partialSections.SetSection(pTSHeader->PID, pSection);

			pSection->m_pTSHeader = new Mpeg2TSPacket();
			pSection->m_pTSHeader->ParseTSPacket(tsPacket, ulDataLength, pTSHeader->filePosition);

			pSection->table_id = tsPacket[start];

			while ((tsPacket[start] == pSection->table_id) && (start+10 < TS_PACKET_SIZE))
			{
				pSection->section_syntax_indicator = (tsPacket[start+1]>>7) & 0x01;
				pSection->section_length = ((tsPacket[start+1] & 0x0F) << 8) + tsPacket[start+2];
				pSection->transport_stream_id = (tsPacket[start+3] << 8 ) + tsPacket[start+4];
				pSection->version_number = ((tsPacket[start+5] & 0x3E) >> 1);
				pSection->current_next_indicator = tsPacket[start+5] & 0x01;
				pSection->section_number = tsPacket[start+6];
				pSection->last_section_number = tsPacket[start+7];

				if (pSection->section_length == 0)
					break;

				pSection->section_length += 3;

				int len = pSection->section_length;
				if (len > (TS_PACKET_SIZE - start))
					len = (TS_PACKET_SIZE - start);

				memcpy(&pSection->m_data[0], &tsPacket[start], len);
				pSection->m_sectionPos = len;
				
				if (m_bLog)
				{
					LogDebug("                                                                  pid:%03.3x tid:%03.3x len:%.4i tsid:%.4x ver:%i %i sec:%i lastsec:%i  pos:%03.3x",
						pSection->PID,
						pSection->table_id,
						pSection->section_length,
						pSection->transport_stream_id,
						pSection->version_number,
						pSection->current_next_indicator,
						pSection->section_number,
						pSection->last_section_number,
						pSection->m_sectionPos);
				}

				if (pSection->m_sectionPos < pSection->section_length)
				{
					break;
				}

				hr = ProcessSection(pSection);
				if (FAILED(hr))
					break;

				start = start + pSection->section_length;

				//TODO: What do we need to do to handle more data in the packet (possible a different tableId??)
				break;
			}
		}
	}

	return S_OK;
}

HRESULT Mpeg2Parser::ProcessSection(Mpeg2Section *pSection)
{
	if (SUCCEEDED(SectionCRCCheck(pSection)))
	{
		m_sections.push_back(pSection);
	}
	else
	{
		delete pSection;
	}
	m_partialSections.Remove(pSection->PID);

	return S_OK;
}

HRESULT Mpeg2Parser::SectionCRCCheck(Mpeg2Section *pSection)
{
	if (pSection->section_length <= 0)
		return E_FAIL;

	DWORD crc = crc32((char*)pSection->m_data, pSection->section_length);
	if (crc != 0)
		return E_FAIL;

	return S_OK;
}

HRESULT Mpeg2Parser::ParseSections()
{
	std::vector<Mpeg2Section *>::iterator it = m_sections.begin();
	for ( ; it != m_sections.end() ; it++ )
	{
		Mpeg2Section *pSection = *it;
		switch (pSection->table_id)
		{
			case 0x00:
			{
				//ParsePAT(pSection);
				break;
			}
			case 0x02:
			{
				//ParsePMT(pSection);
				break;
			}
		};
		//TODO: for old files without PSI tables we need to keep these sections. If we don't find a PAT then we look for PES
		m_sections.erase(it);
	}
	return S_OK;
}

