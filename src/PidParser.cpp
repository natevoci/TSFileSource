/**
*  PidParser.cpp
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
#include "PidParser.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


PidParser::PidParser(FileReader *pFileReader)
{
	m_pFileReader = pFileReader;
}

PidParser::~PidParser()
{
}

HRESULT PidParser::ParseFromFile(__int64 fileStartPointer)
{
	ULONG a;

	HRESULT hr = S_OK;

	if (m_pFileReader->IsFileInvalid())
	{
		return NOERROR;
	}

	//Store file pointer so we can reset it before leaving this method
	__int64 originalFilePointer = m_pFileReader->GetFilePointer();

	FileReader *pFileReader = new FileReader();
	LPOLESTR fileName;
	m_pFileReader->GetFileName(&fileName);
	pFileReader->SetFileName(fileName);

	hr = pFileReader->OpenFile();
	if (FAILED(hr))
		return VFW_E_INVALIDMEDIATYPE;

	int iterations = 0;
	m_FileStartPointer = fileStartPointer;
	__int64 filesize = 0;
	pFileReader->GetFileSize(&filesize);

	iterations = ((filesize - m_FileStartPointer) / 2000000); 
	if (iterations >= 8)
	{
		iterations = 8;
	}
	else
	{
		m_FileStartPointer = m_FileStartPointer - (16000000);
		if (m_FileStartPointer < 0)
			m_FileStartPointer = 0;
	}

	// Access the sample's data buffer
	a = 0;
	ULONG ulDataLength = 2000000;
	ULONG ulDataRead = 0;
	PBYTE pData = new BYTE[ulDataLength];

	pFileReader->SetFilePointer(m_FileStartPointer, FILE_BEGIN);
	pFileReader->Read(pData, ulDataLength, &ulDataRead);

	pids.Clear();
	
	m_ATSCFlag = false;
	m_NetworkID = 0; //NID store
	ZeroMemory(m_NetworkName, 128);
	m_ONetworkID = 0; //ONID store
	m_TStreamID = 0; //TSID store
	m_ProgramSID = 0; //SID store for prog search

	ulDataLength = ulDataRead;
	if (ulDataLength > 0)
	{
		//Clear all Pid arrays
		pidArray.Clear();
		BOOL patfound = false;

		hr = S_OK;
		while (hr == S_OK)
		{
			//search at the head of the file
			hr = FindSyncByte(pData, ulDataLength, &a, 1);
			if (hr == S_OK)
			{
				//parse next packet for the PAT
				if (ParsePAT(pData, ulDataLength, a) == S_OK)
				{
					// If second pass
					if (patfound)
					{
						break;
					}
					else
					{
						//Set to exit after next pat & Erase first occuracnce
						pidArray.Clear();
						m_TStreamID = 0; //TSID store
						patfound = true;
					}
				};
				a += 188;
			}
		}

		//if no PAT found Scan for PMTs
		if (pidArray.Count() == 0)
		{
			a = 0;
			pids.sid = 0;
			pids.pmt = 0;
			hr = S_OK;

			//Scan buffer for any PMTs
			while (pids.pmt == 0x00 && hr == S_OK)
			{
				//search at the head of the file
				hr = FindSyncByte(pData, ulDataLength, &a, 1);
				if (hr == S_OK)
				{
					//parse next packet for the PMT
					if (ParsePMT(pData, ulDataLength, a) == S_OK)
					{
						//Check if PMT was already found
						BOOL pmtfound = false;
						for (int i = 0; i < pidArray.Count(); i++)
						{
							//Search PMT Array for the PMT & SID also
							if (pidArray[i].sid == pids.sid)
							{
								pmtfound = true;
								break;
							}
						}
						if (!pmtfound)
							AddPidArray();

						pids.sid = 0x00;
						pids.pmt = 0x00;
					}
					a += 188;
				}
			}
		}

		//Loop through Programs found
		int i = 0;
		while (i < pidArray.Count())
		{
			//filepos = 0;
			a = 0;
			pids.Clear();
			hr = S_OK;

			int curr_pmt = pidArray[i].pmt;
			int pmtfound = 0;
			int curr_sid = pidArray[i].sid;

			while (pids.pmt != curr_pmt && hr == S_OK)
			{
				//search at the head of the file
				hr = FindSyncByte(pData, ulDataLength, &a, 1);
				if (hr != S_OK)
					break;
				//parse next packet for the PMT
				pids.Clear();
				if (ParsePMT(pData, ulDataLength, a) == S_OK)
				{
					//Check PMT & SID matches program
					if (pids.pmt == curr_pmt && pids.sid == curr_sid && pmtfound > 0)
					{
						//Search for valid A/V pids
						if (IsValidPMT(pData, ulDataLength) == S_OK)
						{
							//Set pcr & Store pids from PMT
							RefreshDuration(FALSE, pFileReader);
//							pids.start = GetPCRFromFile(1);
//							pids.end = GetPCRFromFile(-1);
//							pids.dur = (REFERENCE_TIME)((pids.end - pids.start)/9) * 1000;
							SetPidArray(i);
							i++;
							break;
						}
						pids.pmt = 0;
						pids.sid = 0;
						break;
					}

					if (pids.pmt == curr_pmt && pids.sid == curr_sid)
					{
						pmtfound++;
						pids.pmt = 0;
						pids.sid = 0;
					}
				}
				a += 188;
			};

			if (pids.pmt != curr_pmt || pids.sid != curr_sid || hr != S_OK) //Make sure we have a correct packet
				pidArray.RemoveAt(i);
		}

		//Search for A/V pids if no NIT or valid PMT found.
		if (pidArray.Count() == 0)
		{
			//Search for any A/V Pids
			if (ACheckVAPids(pData, ulDataLength) == S_OK)
			{
				//Set the pcr to video or audio incase its included in packet
				USHORT pcrsave = pids.pcr;
				if (pids.vid && !pids.pcr)
					pids.pcr = pids.vid;
				else if (pids.aud && !pids.pcr)
					pids.pcr = pids.aud;
				RefreshDuration(FALSE, pFileReader);
				//pids.start = GetPCRFromFile(1);
				//pids.end = GetPCRFromFile(-1);
				//pids.dur = (REFERENCE_TIME)((pids.end - pids.start)/9) * 1000;

				// restore pcr pid if no matches with A/V pids
				if (!pids.dur)
					pids.pcr = pcrsave;

				AddPidArray();
			}
		}
		
		//Scan for missing durations & Fix
		for (int n = 0; n < pidArray.Count(); n++){
			//Search Duration Array for the first duration
			if (pidArray[n].dur > 1){
			//Search Duration Array for empty durations
				for (int x = 0; x < pidArray.Count(); x++){
					//If empty then fill with first found duration 
					if (pidArray[x].dur < 1)
						pidArray[x].dur = pidArray[n].dur;
				}
			}
		}

		//Check for a ONID in file
		if (!m_ATSCFlag)
			if (CheckONIDInFile(pFileReader) == S_OK)
			{
			}

		//Check for a NID in file
		if (!m_ATSCFlag)
			if(CheckNIDInFile(pFileReader) != S_OK)
			{
			}

		//Set the Program Number to beginning & load back pids
		m_pgmnumb = 0;
		pids.Clear();
		pids.CopyFrom(&pidArray[m_pgmnumb]);

		if (pids.vid != 0 || pids.aud != 0 || pids.ac3 != 0 || pids.txt != 0)
		{
			hr = S_OK;
		}
		else
		{
			hr = S_FALSE;
		}
	}
	else
	{
		hr = E_FAIL;
	}

	delete[] pData;

	pFileReader->CloseFile();
	delete pFileReader;

	return hr;
}

HRESULT PidParser::RefreshPids()
{
	if (m_pFileReader->IsFileInvalid())
	{
		return NOERROR;
	}

	__int64 filesize;
	m_pFileReader->GetFileSize(&filesize);
	__int64 filestartpointer = m_pFileReader->GetFilePointer();

	//Check if file is being recorded
	if((filesize) < 2100000)
	{
		int count = 0;
		__int64 fileSizeSave = (filesize);
		while ((filesize) < 20000000 && count < 20)
		{
			m_pFileReader->GetFileSize(&filesize);
			while (((filesize) < fileSizeSave + 2000000) && (count < 20))
			{
				Sleep(100);
				count++;
			}
			count++;
			m_pFileReader->GetFileSize(&filesize);
			fileSizeSave = filesize;
			ParseFromFile(filestartpointer); 
			if (m_ONetworkID > 0 && m_NetworkID > 0 && m_NetworkID > 0)
				return S_OK;
		}
	}
	else
	{
		ParseFromFile(filestartpointer);
		if (m_ONetworkID > 0 && m_NetworkID > 0 && m_NetworkID > 0)
			return S_OK;
	}
	return S_FALSE;
}

HRESULT PidParser::RefreshDuration(BOOL bStoreInArray, FileReader *pFileReader)
{
	__int64 originalFilePointer = pFileReader->GetFilePointer();

	//pids.start = GetPCRFromFile(1);
	//pids.end = GetPCRFromFile(-1);
	//pids.dur = (REFERENCE_TIME)((pids.end - pids.start)/9) * 1000;
	pids.dur = GetFileDuration(&pids, pFileReader);

	if (bStoreInArray)
		SetPidArray(m_pgmnumb);

	//Restore original file pointer
	pFileReader->SetFilePointer(originalFilePointer, FILE_BEGIN);

	return S_OK;
}


HRESULT PidParser::FindSyncByte(PBYTE pbData, ULONG ulDataLength, ULONG* a, int step)
{
	while ((*a >= 0) && (*a < ulDataLength))
	{
		if (pbData[*a] == 0x47)
		{
			if (*a+188 < ulDataLength)
			{
				if (pbData[*a+188] == 0x47)
					return S_OK;
			}
			else
			{
				if (step > 0)
					return E_FAIL;

				if (*a-188 > 0)
				{
					if (pbData[*a-188] == 0x47)
						return S_OK;
				}
			}
		}
		*a += step;
	}
	return E_FAIL;
}

HRESULT PidParser::ParsePAT(PBYTE pData, ULONG lDataLength, long pos)
{
	HRESULT hr = S_FALSE;

	int channeloffset;

	if ( (0x30 & pData[pos + 3]) == 0x10 && pData[pos + 4] == 0 &&
		  pData[pos + 5] == 0 && (0xf0 & pData[pos + 6]) == 0xB0 ) {

		if ((((0x1F & pData[pos + 1]) << 8)|(0xFF & pData[pos + 2])) == 0x00){

			channeloffset = (WORD)(0x0F & pData[pos + 15]) << 8 | (0xFF & pData[pos + 16]);
			m_TStreamID = ((0xFF & pData[pos + 8]) << 8) | (0xFF & pData[pos + 9]); //Get TSID Pid
			int Numb = (((0xFF & pData[pos + 7]) - channeloffset + 8 - 5) / 4); //Get number of programs

			//If no Program PMT Info as with an ATSC
			if (Numb < 1)
			{
				//Set flag to disable searching for NID
				m_ATSCFlag = true;
			}

			for (long b = channeloffset + pos; b < pos + 182 ; b = b + 4)
			{
				if ( ((0xe0 & pData[b + 3]) == 0xe0) && (pData[b + 3]) != 0xff )
				{
					PidInfo *newPidInfo = new PidInfo();

					newPidInfo->sid =
						(WORD)(0x1F & pData[b + 1]) << 8 | (0xFF & pData[b + 2]);

					newPidInfo->pmt =
						(WORD)(0x1F & pData[b + 3]) << 8 | (0xFF & pData[b + 4]);

					pidArray.Add(newPidInfo);
				}
			}
			if (pidArray.Count() != 0)
			{
				return S_OK;
			}
		}
	}
	return hr;
}

HRESULT PidParser::ParsePMT(PBYTE pData, ULONG ulDataLength, long pos)
{
	WORD pid;
	WORD channeloffset;
	WORD EsDescLen;
	WORD StreamType;
	WORD privatepid = 0;

	if ((0x30&pData[pos+3])==0x10 && pData[pos+4] == 0 && pData[pos+5] == 2 && (0xf0&pData[pos+6])==0xb0)
	{
		pids.pmt      = (WORD)(0x1F & pData[pos+1 ])<<8 | (0xFF & pData[pos+2 ]);
		pids.pcr      = (WORD)(0x1F & pData[pos+13])<<8 | (0xFF & pData[pos+14]);
		pids.sid      = (WORD)(0xFF & pData[pos+8 ])<<8 | (0xFF & pData[pos+9 ]);
		channeloffset = (WORD)(0x0F & pData[pos+15])<<8 | (0xFF & pData[pos+16]);

		for (long b=17+channeloffset+pos; b<pos+182; b=b+5)
		{
			if ( (0xe0&pData[b+1]) == 0xe0 )
			{
				pid = (0x1F & pData[b+1])<<8 | (0xFF & pData[b+2]);
				EsDescLen = (0x0F&pData[b+3]<<8 | 0xFF&pData[b+4]);

				StreamType = (0xFF&pData[b]);

				if (StreamType == 0x02)
					pids.vid = pid;

				if ((StreamType == 0x03) || (StreamType == 0x04))
				{
					if (pids.aud != 0)
						pids.aud2 = pid;
					else
						pids.aud = pid;
				}

				if (StreamType == 0x06)
				{
					if (CheckEsDescriptorForAC3(pData, ulDataLength, b + 5, b + 5 + EsDescLen))
					{
						if (pids.ac3 == 0)
							pids.ac3 = pid;
						else
							pids.ac3_2 = pid;// If already have AC3 then get next.
					}
					else if (CheckEsDescriptorForTeletext(pData, ulDataLength, b + 5, b + 5 + EsDescLen))
						pids.txt = pid;
					else
					{
						//This could be a bid dodgy. What if there is an ac3 or txt in a future loop?
						if (pids.ac3 == 0 && pids.txt != 0)
						{
							pids.ac3 = pid;
						}
						else if (pids.ac3 != 0 && pids.txt == 0)
						{
							pids.txt = pid;
						}
					}
				}

				if (StreamType == 0x81 || StreamType == 0x83 || StreamType == 0x85)
				{
					if (pids.ac3 == 0)
						pids.ac3 = pid;
					else
						pids.ac3_2 = pid;// If already have AC3 then get next.
				}

				if (StreamType == 0x0b)
					if (pids.txt == 0)
						pids.txt = pid;

				b+=EsDescLen;
			}
		}
		return S_OK;
	}
	else
	{
		return S_FALSE;
	}
}

HRESULT PidParser::CheckForPCR(PBYTE pData, ULONG ulDataLength, PidInfo *pPids, int pos, REFERENCE_TIME* pcrtime)
{
	if ((WORD)((0x1F & pData[pos+1])<<8 | (0xFF & pData[pos+2])) == pPids->pcr)
	{
		WORD pcrmask = 0x10;
		if ((pData[pos+3] & 0x30) == 30)
			pcrmask = 0xff;

		if (((pData[pos+3] & 0x30) >= 0x20)
			&& ((pData[pos+4] & 0x11) != 0x00)
			&& ((pData[pos+5] & pcrmask) == 0x10))
		{
				*pcrtime =	((REFERENCE_TIME)(0xFF & pData[pos+6])<<25) |
							((REFERENCE_TIME)(0xFF & pData[pos+7])<<17) |
							((REFERENCE_TIME)(0xFF & pData[pos+8])<<9)  |
							((REFERENCE_TIME)(0xFF & pData[pos+9])<<1)  |
							((REFERENCE_TIME)(0x80 & pData[pos+10])>>7);

				return S_OK;
		};
	}
		return S_FALSE;
}

BOOL PidParser::CheckEsDescriptorForAC3(PBYTE pData, ULONG ulDataLength, int pos, int lastpos)
{
	WORD DescTag;
	while (pos < lastpos)
	{
		DescTag = (0xFF & pData[pos]);
		if (DescTag == 0x6a) return TRUE;
		pos += (int)(0xFF & pData[pos+1]) + 2;
	}
	return FALSE;
}

BOOL PidParser::CheckEsDescriptorForTeletext(PBYTE pData, ULONG ulDataLength, int pos, int lastpos)
{
	WORD DescTag;
	while (pos < lastpos)
	{
		DescTag = (0xFF & pData[pos]);
		if (DescTag == 0x56) return TRUE;
		if (DescTag == 0x59) return TRUE;

		pos += (int)(0xFF & pData[pos+1]) + 2;
	}
	return FALSE;
}

HRESULT PidParser::IsValidPMT(PBYTE pData, ULONG ulDataLength)
{
	HRESULT hr = S_FALSE;

	//exit if no a/v pids to find
	if (pids.aud + pids.vid + pids.ac3 + pids.txt == 0)
		return hr;

	ULONG a, b;
	WORD pid;
	int addlength, addfield;
	WORD error, start;
	DWORD psiID, pesID;
	a = 0;
	hr = S_OK;

	while (hr == S_OK)
	{
		hr = FindSyncByte(pData, ulDataLength, &a, 1);
		if (hr == S_OK)
		{
			b = a;
			error = 0x80&pData[b+1];
			start = 0x40&pData[b+1];
			addfield = (0x30 & pData[b+3])>>4;
			if (addfield > 1) {

				addlength = (0xff & pData[b+4]) + 1;
			} else {

				addlength = 0;
			}

				if (start == 0x40) {
				pesID = ( (255&pData[b+4+addlength])<<24
						| (255&pData[b+5+addlength])<<16
						| (255&pData[b+6+addlength])<<8
						| (255&pData[b+7+addlength]) );
				psiID = pesID>>16;

				pid = ((0x1F & pData[b+1])<<8 | (0xFF & pData[b+2]));
				if (((0xFF0&pesID)  == 0x1e0) && (pids.vid == pid) && pids.vid) {
					return S_OK;
				};

				if (((0xFF0&pesID) == 0x1c0) && (pids.aud == pid) && pids.aud) {
					return S_OK;
				};


				if (((0xFF0&pesID) == 0x1c0) && (pids.ac3 == pid) && pids.ac3) {
					return S_OK;
				};

				if (((0xFF0&pesID) == 0x1c0) && (pids.txt == pid) && pids.txt) {
					return S_OK;
				};

			}
			a += 188;
		}
	}
	return hr;
}

HRESULT PidParser::FindFirstPCR(PBYTE pData, ULONG ulDataLength, PidInfo *pPids, REFERENCE_TIME* pcrtime, ULONG* pulPos)
{
	*pulPos = 0;
	return FindNextPCR(pData, ulDataLength, pPids, pcrtime, pulPos, 1);
}

HRESULT PidParser::FindLastPCR(PBYTE pData, ULONG ulDataLength, PidInfo *pPids, REFERENCE_TIME* pcrtime, ULONG* pulPos)
{
	*pulPos = ulDataLength - 188;
	return FindNextPCR(pData, ulDataLength, pPids, pcrtime, pulPos, -1);
}

HRESULT PidParser::FindNextPCR(PBYTE pData, ULONG ulDataLength, PidInfo *pPids, REFERENCE_TIME* pcrtime, ULONG* pulPos, int step)
{
	HRESULT hr = S_OK;

	*pcrtime = 0;

	while( *pcrtime == 0 && hr == S_OK)
	{
		hr = FindSyncByte(pData, ulDataLength, pulPos, step);
		if (FAILED(hr))
			return hr;

		if (S_FALSE == CheckForPCR(pData, ulDataLength, pPids, *pulPos, pcrtime))
		{
			*pulPos = *pulPos + (step * 188);
		}
		else
		{
			return S_OK;
		}
	}
	return E_FAIL;
}

REFERENCE_TIME PidParser::GetPCRFromFile(int step)
{
	if (step == 0)
		return 0;

	REFERENCE_TIME pcrtime = 0;

	ULONG lDataLength = 2000000;
	PBYTE pData = new BYTE[lDataLength];
	ULONG lDataRead = 0;

	m_pFileReader->Read(pData, lDataLength, &lDataRead, 0, (step > 0) ? FILE_BEGIN : FILE_END);
	if (lDataRead <= 188)
		return 0;
	
	ULONG a = (step > 0) ? 0 : lDataRead-188;

	if (step > 0)
		FindFirstPCR(pData, lDataRead, &pids, &pcrtime, &a);
	else
		FindLastPCR(pData, lDataRead, &pids, &pcrtime, &a);

	delete[] pData;

	return pcrtime;
}

HRESULT PidParser::ACheckVAPids(PBYTE pData, ULONG ulDataLength)
{
	ULONG a, b;
	WORD pid;
	HRESULT hr;
	int addlength, addfield;
	WORD error, start;
	DWORD psiID, pesID;
	a = 0;

	hr = S_OK;
	while (hr == S_OK)
	{
		hr = FindSyncByte(pData, ulDataLength, &a, 1);
		if (hr == S_OK)
		{
			b = a;
			error = 0x80&pData[b+1];
			start = 0x40&pData[b+1];
			addfield = (0x30 & pData[b+3])>>4;
			if (addfield > 1) {

				addlength = (0xff & pData[b+4]) + 1;
			} else {

				addlength = 0;
			}

			if (start == 0x40) {

				pesID = ( (255&pData[b+4+addlength])<<24
						| (255&pData[b+5+addlength])<<16
						| (255&pData[b+6+addlength])<<8
						| (255&pData[b+7+addlength]) );

				psiID = pesID>>16;

				pid = ((0x1F & pData[b+1])<<8 | (0xFF & pData[b+2]));

				if (((0xFF0&pesID) == 0x1e0) && (pids.vid == 0)) {
					pids.vid = pid;

				};
				if ((0xFF0&pesID) == 0x1c0) {
					if (pids.aud == 0) {
						pids.aud = pid;
					} else {
						if (pids.aud != pid) {
							pids.aud2 = pid;
						}
					}
				};
			}
		}
		a += 188;
	}
	return S_OK;
}

HRESULT PidParser::CheckEPGFromFile()
{
	HRESULT hr = S_FALSE;

	if (m_NetworkID != 0 && m_ONetworkID != 0 && m_TStreamID !=0)
	{

		FileReader *pFileReader = new FileReader();
		LPOLESTR fileName;
		m_pFileReader->GetFileName(&fileName);
		pFileReader->SetFileName(fileName);

		hr = pFileReader->OpenFile();
		if (FAILED(hr))
			return VFW_E_INVALIDMEDIATYPE;

		int Event = 0; //Start with NOW event
		bool extPacket = false; //Start at first packet
		int sidCount = 0; //Store for packet cycles
		m_buflen = 0;
		int sectLen = 0;
		ULONG pos;
		int iterations;
		bool epgfound;

		HRESULT hr;
	
		pos = 0;
		iterations = 0;
		epgfound = false;

		__int64 filesize;
		pFileReader->GetFileSize(&filesize);

		__int64 filestartpointer = m_pFileReader->GetFilePointer();
		iterations = ((filesize - filestartpointer) / 2000000); 
		if (iterations >= 64)
			iterations = 64;
		else if (iterations < 32)
		{
			iterations = 32;
			filestartpointer = filesize - (iterations * 2000000);
			if (filestartpointer < 100000)
				filestartpointer = 100000;
		}

		ULONG ulDataLength = 2000000;
		ULONG ulDataRead = 0;
		PBYTE pData = new BYTE[ulDataLength];

		while(sidCount < pidArray.Count())
		{
			pFileReader->SetFilePointer(filestartpointer, FILE_BEGIN);
			pFileReader->Read(pData, ulDataLength, &ulDataRead);

			hr = FindSyncByte(pData, ulDataLength, &pos, 1);
			while ((pos < ulDataLength) && (hr == S_OK) && (epgfound == false))
			{
				epgfound = CheckForEPG(pData, pos, &extPacket, &sectLen, &sidCount, &Event);

				if ((iterations > 64) && (epgfound == false))
				{
					hr = S_FALSE;
				}
				else
				{
					if (epgfound == false)
					{
						pos = pos + 188;
						if (pos >= 2000000)
						{
							ULONG ulBytesRead = 0;
							hr = pFileReader->Read(pData, 2000000, &ulBytesRead);

							iterations++;
							pos = 0;
							if (hr == S_OK)
							{
								hr = FindSyncByte(pData, 2000000, &pos, 1);
							}
						}
					}
				}
			}
			hr = S_OK;
			pos = 0;
			m_buflen = 0;
			extPacket = false;
			iterations = 0;
			epgfound = false;
			sectLen = 0;
			ulDataRead = 0;
			if (Event == 0)
			{
				Event = 1; //Set to NEXT event
			}
			else
			{
				Event = 0; //Reset to NOW event
				sidCount++;
			}
		}

		pids.Clear();
		pids.CopyFrom(&pidArray[m_pgmnumb]);

		pFileReader->CloseFile();
		delete[] pData;
		delete pFileReader;
	}
	return S_OK;
}

bool PidParser::CheckForEPG(PBYTE pData, int pos, bool *extPacket, int *sectlen, int *sidcount, int *event)
{
	if (m_TStreamID ==0)
		return true;

	int b = pos; //Set pos for Start Packet ID
	int endpos = pos + 188; //Set to exclude crc bytes

	if (
		(pData[pos + 11] == *event)
		&& (*extPacket == false) // parse the first packet only 
		&& ((0xF0 & pData[pos+1]) == 0x40)
		&& (((0x1F & pData[pos + 1]) << 8)|(0xFF & pData[pos + 2])) == 0x12 
		&& ((0xFF & pData[pos+5]) == 0x4e)
		&& (pidArray[*sidcount].sid == (((0xFF & pData[pos + 8]) << 8) | (0xFF & pData[pos + 9])))
		)
	{
		*sectlen =((0x0F & pData[pos + 6]) << 8)|(0xFF & pData[pos + 7]) + 8;

		 // test if next packet required 
		if (*sectlen > 176)
		{
			*extPacket = true; //set search for extended packet
		}
		else
		{
			*extPacket = false; // set for next packet

			//if no descriptor info
			if (*sectlen <= 0x0F + 8)
				*sectlen = 0;
		};

	}
	else if ((*extPacket == true) // second time past this test
			&& (0xF0 & pData[pos + 3]) == 0x10 
			&& (0xF0 & pData[pos + 1]) == 0x00 
			&& (((0x1F & pData[pos + 1]) << 8)|(0xFF & pData[pos + 2])) == 0x12
			)
	{
		b += 4; //set for next packet ID
		endpos += 4; //set not to exclude crc bytes
		*sectlen = *sectlen + 4;
	}
	else
		return false; //No Descriptor Found

	while(b < endpos) 
	{

		//Bufer overflow
		if (m_buflen > 0x3FFF) 
		{
			return true; // buffer over flow end parsing
		}
		else if (*sectlen < 1) //end of descriptors
		{
			// If we have ID's
			if (m_buflen == 0)
				return true;

			//Search ID Array for the SID Value
			if (pidArray[*sidcount].sid == (((0xFF & m_pDummy[8]) << 8) | (0xFF & m_pDummy[9])))
			{
				int len =((0x0F & m_pDummy[6]) << 8)|(0xFF & m_pDummy[7]);
				int a = 37;
				int b = min(m_pDummy[36], 128);
				if (m_pDummy[11] == 0x00) //Now Event
					memcpy(pidArray[*sidcount].sdesc, m_pDummy + a, b);
				else
					memcpy(pidArray[*sidcount].sndesc, m_pDummy + a, b);

				a += m_pDummy[36]; 
				b = min(m_pDummy[a], 600); a++;

				if (m_pDummy[11] == 0x00) //Now Event
					memcpy(pidArray[*sidcount].edesc, m_pDummy + a, b);
				else
					memcpy(pidArray[*sidcount].endesc, m_pDummy + a, b);

				//look for extra long descriptors
				for (int i = a + b; i < m_buflen; i++)
				{
					if (m_pDummy[i] == 0x8A)
					{
						a = i + 1;
						b = min(m_pDummy[i - 1], 600);
						if (m_pDummy[11] == 0x00) //Now Event
							memcpy(pidArray[*sidcount].edesc, m_pDummy + a, b);
						else
							memcpy(pidArray[*sidcount].endesc, m_pDummy + a, b);
						
						break;
					}
				};

				return true; // end parsing for this program sid
			}
		
			*extPacket = false; // set for next packet
			return false; //end parsing 
		}
	 
		*sectlen = *sectlen - 1;
		m_pDummy[m_buflen] = pData[b];
		m_buflen++;
		b++;
	};
	return false;
}

HRESULT PidParser::CheckNIDInFile(FileReader *pFileReader)
{

	HRESULT hr = S_FALSE;

	if (m_NetworkID == 0 && m_TStreamID !=0)
	{
		bool extPacket = false; //Start at first packet
		int sectLen = 0;
		m_buflen = 0;
		ULONG pos = 100000;
		int iterations = 0;
		bool nitfound = false;

		ULONG ulDataLength = 2000000;
		ULONG ulDataRead = 0;
		PBYTE pData = new BYTE[ulDataLength];

		pFileReader->SetFilePointer(m_FileStartPointer, FILE_BEGIN);
		pFileReader->Read(pData, ulDataLength, &ulDataRead);

		hr = FindSyncByte(pData, ulDataLength, &pos, 1);

		while ((pos < ulDataLength) && (hr == S_OK) && (nitfound == false))
		{
			nitfound = CheckForNID(pData, pos, &extPacket, &sectLen);

			if ((iterations > 64) && (nitfound == false))
			{
				hr = S_FALSE;
			}
			else
			{
				if (nitfound == false)
				{
					pos = pos + 188;
					if (pos >= 2000000)
					{
						ULONG ulBytesRead = 0;
						hr = pFileReader->Read(pData, 2000000, &ulBytesRead);

						iterations++;
						pos = 0;
						if (hr == S_OK)
						{
							hr = FindSyncByte(pData, 2000000, &pos, 1);
						}
					}
				}
			}
		}

		if (nitfound == true && m_buflen > 0)
		{
			//get network ID Number
			m_NetworkID = (0xFF & m_pDummy[8]) << 8 | (0xFF & m_pDummy[9]);
		
			//get network name
			int a = 17;
			int b = min(m_pDummy[16], 128);
			memcpy(m_NetworkName, m_pDummy + a, b);
			//get channel numbers
			for (int n = 0; n < pidArray.Count(); n++)
			{
			//find channel from sid's
				for (int i = 0 ; i < m_buflen; i++)
				{
					if (((m_pDummy[i]<<8)|m_pDummy[i+1]) == pidArray[n].sid
						&& m_pDummy[i+2] == 0xFC)
					{
						pidArray[n].chnumb = m_pDummy[i+3];
						break;
					}
				};
			}
		}
		delete[] pData;
	}
	if (m_NetworkID == 0)
		return S_FALSE;
	else
		return S_OK;
}

bool PidParser::CheckForNID(PBYTE pData, int pos, bool *extpacket, int *sectlen)
{
	//Return ok if we already found an NID or if we don't have a TSID since we don't have a PAT
	if (m_TStreamID ==0)
		return true;

	int b = pos; //Set pos for Start Packet ID
	int endpos = pos + 188; //Set to exclude crc bytes

	//Test if we have an NID discriptor
	if (pData[pos + 4] == 0
		&& (*extpacket == false) // parse the first packet only 
		&& pData[pos + 5] == 0x40 
		&& (0xF0 & pData[pos + 6]) == 0xF0
		&& (((0x1F & pData[pos + 1]) << 8)|(0xFF & pData[pos + 2])) == 0x10 
		&& (((0x1F & pData[pos + 11]) << 8)|(0xFF & pData[pos + 12])) == 0	//yes if we have the NID flag
		)
	{
		*sectlen =((0x0F & pData[pos + 6]) << 8)|(0xFF & pData[pos + 7]) + 4; //include CRC bytes

		 // test if next packet required 
		if (*sectlen > 176)
		{
			*extpacket = true; //set search for extended packet
		}
		else
		{
			*extpacket = false; // set for next packet

			//if no descriptor info
			if (*sectlen <= 0x0F + 4)
				*sectlen = 0;
		};

	}
	else if ((*extpacket == true) // second time past this test
			&& (0xF0 & pData[pos + 3]) == 0x10 
			&& (0xF0 & pData[pos + 1]) == 0x00 
			&& (((0x1F & pData[pos + 1]) << 8)|(0xFF & pData[pos + 2])) == 0x10
			)
	{
		b += 4; //set for extended packet ID
		endpos += 4; //set not to exclude crc bytes
	}
	else
		return false; //No Descriptor Found

	while(b < endpos) 
	{
		//Bufer overflow
		if (m_buflen > 0x3FFF || *sectlen < 1) 
			return true; // buffer over flow or end parsing
	 
		*sectlen = *sectlen - 1;
		m_pDummy[m_buflen] = pData[b];
		m_buflen++;
		b++;
	};
	return false;
}

HRESULT PidParser::CheckONIDInFile(FileReader *pFileReader)
{

	HRESULT hr = S_FALSE;

	if (m_ONetworkID == 0 && m_TStreamID !=0)
	{
		bool extPacket = false; //Start at first packet
		m_buflen = 0;
		int sectLen = 0;
		ULONG pos = 0;
		int iterations = 0;
		bool onitfound = false;

		ULONG ulDataLength = 2000000;
		ULONG ulDataRead = 0;
		PBYTE pData = new BYTE[ulDataLength];

		pFileReader->SetFilePointer(m_FileStartPointer, FILE_BEGIN);
		pFileReader->Read(pData, ulDataLength, &ulDataRead);

		hr = FindSyncByte(pData, ulDataLength, &pos, 1);

		while ((pos < ulDataLength) && (hr == S_OK) && (onitfound == false)) 
		{
			if (!onitfound)
			{
				onitfound = CheckForONID(pData, pos, &extPacket, &sectLen);
			}


			if ((iterations > 64) && (onitfound == false)) 
			{
				hr = S_FALSE;
			}
			else
			{
				if (onitfound == false)
				{
					pos = pos + 188;
					if (pos >= 2000000)
					{
						ULONG ulBytesRead = 0;
						hr = pFileReader->Read(pData, 2000000, &ulBytesRead);

						iterations++;
						pos = 0;
						if (hr == S_OK)
						{
							hr = FindSyncByte(pData, 2000000, &pos, 1);
						}
					}
				}
			}
		}

		if (onitfound == true && m_buflen > 0)
		{
			//get Onetwork ID Number
			m_ONetworkID = (0xFF & m_pDummy[13]) << 8 | (0xFF & m_pDummy[14]);
		
			for (int n = 0; n < pidArray.Count(); n++)
			{
			//find channel from sid's
				for (int i = 0; i < m_buflen; i++)
				{
					//Search ID Array for the SID Value
					if (pidArray[n].sid == (((0xFF & m_pDummy[i]) << 8) | (0xFF & m_pDummy[i + 1]))
						&& 0xFD80 == (((0xFF & m_pDummy[i + 2]) << 8) | (0xFF & m_pDummy[i + 3])))
					{
						int a = i + 9;
						int b = min(m_pDummy[i + 8], 128);
						memcpy(pidArray[n].onetname, m_pDummy + a, b);
						a += b; 
						b = min(m_pDummy[a], 128); a++;
						memcpy(pidArray[n].chname, m_pDummy + a, b);
						break;
					}
				};
			}
		}
		delete[] pData;
	}
	if (m_ONetworkID == 0)
		return S_FALSE;
	else
		return S_OK;
}
	
bool PidParser::CheckForONID(PBYTE pData, int pos, bool *extpacket, int *sectlen)
{
	//Return ok if we already found an ONID or if we don't have a TSID since we don't have a PAT
	if (m_TStreamID ==0)
		return true;

	int b = pos; //Set pos for Start Packet ID
	int endpos = pos + 188; //Set to exclude crc bytes

	//Test if we have an ONID discriptor
	if ((*extpacket == false)
		&&  (pData[pos + 4] == 0)
		&& (pData[pos + 5] == 0x42) 
		&& ((0xF0 & pData[pos + 6]) == 0xF0)
		&& ((0xF0 & pData[pos + 1]) == 0x40) //first packet of ID's
		&& ((0x01 & pData[pos+11]) == 0x00)    // should be start of parsing
		&& ((((0x1F & pData[pos + 1]) << 8)|(0xFF & pData[pos + 2])) == 0x11) 
		&& ((((0x1F & pData[pos + 11]) << 8)|(0xFF & pData[pos + 12])) == 0)	//yes if we have the ONID flag
		)
	{
		*sectlen =((0x0F & pData[pos + 6]) << 8)|(0xFF & pData[pos + 7]) + 8; //include CRC bytes

		 // test if next packet required 
		if (*sectlen > 176)
			*extpacket = true; //set search for extended packet
		else
		{
			*extpacket = false; // set for next packet

			//if no descriptor info
			if (*sectlen <= 0x10)
				*sectlen = 0;
		};
	}
	else if ((*extpacket == true)	//Test if we have an ext ID discriptor
			&& (0xF0 & pData[pos + 3]) == 0x10 
			&& (0xF0 & pData[pos + 1]) == 0x00 //NOT first packet of ID's
			&& (((0x1F & pData[pos + 1]) << 8)|(0xFF & pData[pos + 2])) == 0x11
			)
	{
		b += 4; //set for extended packet ID
		endpos += 4; //set not to exclude crc bytes
		*sectlen = *sectlen + 4;
	}
	else
		return false; //No Descriptor Found

	while(b < endpos) 
	{
		//Bufer overflow
		if (m_buflen > 0x3FFF || *sectlen < 1) 
			return true; // buffer over flow or end parsing
	 
		*sectlen = *sectlen - 1;
		m_pDummy[m_buflen] = pData[b];
		m_buflen++;
		b++;
	};
	return false;
}

HRESULT PidParser::ParseEISection (ULONG ulDataLength)
{
	int pos = 0;
	WORD DescTag;
	int DescLen;

	while (pos < ulDataLength)
	{
		DescTag = (0xFF & m_pDummy[pos]);
		DescLen = (int)(0xFF & m_pDummy[pos+1]);

		if (DescTag == 0x4d) //0x42) {
		{
			// short event descriptor
			ParseShortEvent(pos, DescLen+2);
		}
		else
		{
			if (DescTag == 0x4e)
			{
				// extended event descriptor
				ParseExtendedEvent(pos, DescLen+2);
			}
		}

		pos = pos + DescLen + 2;
	}
	return S_OK;
}

HRESULT PidParser::ParseShortEvent(int start, ULONG ulDataLength)
{
	memcpy(m_shortdescr, m_pDummy + start, ulDataLength);
	return S_OK;
}

HRESULT PidParser::ParseExtendedEvent(int start, ULONG ulDataLength)
{
	memcpy(m_extenddescr, m_pDummy + start, ulDataLength);
	return S_OK;
}

REFERENCE_TIME PidParser::GetFileDuration(PidInfo *pPids, FileReader *pFileReader)
{

	HRESULT hr = S_OK;
	__int64 filelength;
	REFERENCE_TIME totalduration = 0;
	REFERENCE_TIME startPCRSave = 0;
	REFERENCE_TIME endPCRtotal = 0;
	pPids->start = 0;
	pPids->end = 0;
	__int64 tolerence = 100000UL;


	__int64 startFilePos = 0;
	long lDataLength = 2000000;
	PBYTE pData = new BYTE[lDataLength];

	pFileReader->GetFileSize(&filelength);

	filelength = filelength;
	__int64 endFilePos = filelength;
	m_fileLenOffset = filelength;
	m_fileStartOffset = 0;
	m_fileEndOffset = 0;

	// find first Duration
	while (m_fileStartOffset < filelength){

		hr = GetPCRduration(pData, lDataLength, pPids, filelength, &startFilePos, &endFilePos, pFileReader);
		if (hr == S_OK){
			//Save the start PCR only
			if (startPCRSave == 0 && pPids->start > 0)
				startPCRSave = pPids->start;
			//Add the PCR time difference
			totalduration = totalduration + (__int64)(pPids->end - pPids->start); // add duration to total.
			//Test if at end of file & return

			if (endFilePos >= (filelength - tolerence)){

				REFERENCE_TIME PeriodOfPCR = (REFERENCE_TIME)(((__int64)(pPids->end - pPids->start)/9)*1000); 

				//8bits per byte and convert to sec divide by pcr duration then average it
				if (PeriodOfPCR > 0)
					pids.bitrate = long (((endFilePos - startFilePos)*80000000) / PeriodOfPCR);

				break;
			}

			m_fileStartOffset = endFilePos;
		}
		else
			m_fileStartOffset = m_fileStartOffset + 100000;

		//If unable to find any pids dont go again
		if (pPids->start == 0 || pPids->end == 0)
			break;

		pPids->start = 0;
		pPids->end = 0;
		m_fileLenOffset = filelength - m_fileStartOffset;
		endFilePos = m_fileLenOffset;
		m_fileEndOffset = 0;
	}

	if (startPCRSave != 0){
		pPids->start = startPCRSave; //Restore to first PCR
		delete[] pData;
		return (REFERENCE_TIME)((totalduration)/9) * 1000;
	}

	delete[] pData;
	return 0; //Duration not found
}

HRESULT PidParser::GetPCRduration(PBYTE pData,
								  long lDataLength,
								  PidInfo *pPids,
								  __int64 filelength,
								  __int64* pStartFilePos,
								  __int64* pEndFilePos,
								  FileReader *pFileReader)
{
	HRESULT hr;
	ULONG pos;
	REFERENCE_TIME midPCR;
	__int64 filetolerence = 100000UL;
	ULONG ulBytesRead = 0;
	pos = 0;

	pFileReader->SetFilePointer(m_fileStartOffset, FILE_BEGIN);
	pFileReader->Read(pData, lDataLength, &ulBytesRead);

	hr = FindNextPCR(pData, lDataLength, pPids, &pPids->start, &pos, 1); //Get the PCR
	
	if (hr == S_OK){

		m_fileLenOffset = m_fileLenOffset - (((__int64)pos) - 1);
		*pStartFilePos = m_fileStartOffset + (((__int64)pos) - 1);
//		m_fileLenOffset = m_fileLenOffset - (((__int64)pos) - 1);
//		*pStartFilePos = m_fileStartOffset + (((__int64)pos) - 1);
//		m_fileLenOffset = m_fileLenOffset - (__int64)(pos - 1);
//		*pStartFilePos = m_fileStartOffset + (__int64)(pos - 1); 
	}

	pos = lDataLength - 188;
	hr = FindNextPCR(pData, lDataLength, pPids, &pPids->end, &pos, -1); //Get the PCR
	if (hr != S_OK)
		return S_FALSE; // Unable to get PCR time in first block

	__int64 SaveEndPCR = pPids->end;

	while(m_fileLenOffset > (__int64)(10 * 188))
	{
		if (pPids->start == 0)
			break; //exit if no PCR found

		pos = lDataLength - 188;

		pFileReader->SetFilePointer(-(__int64)(m_fileEndOffset + (__int64)lDataLength), FILE_END);
		pFileReader->Read(pData, lDataLength, &ulBytesRead);

		hr = FindNextPCR(pData, lDataLength, pPids, &pPids->end, &pos, -1); //Get the PCR
		if (hr != S_OK)
			break; //exit if no PCR found
		
		*pEndFilePos = filelength - m_fileEndOffset - (__int64)lDataLength + (__int64)pos - 1;// 

		if (*pEndFilePos <= *pStartFilePos)
			break; //exit if past start time
		
		m_fileEndOffset = m_fileEndOffset + (__int64)(m_fileLenOffset / 2); //Set file mid search pos

		//exit if bad PCR timming found
		if (pPids->end > pPids->start)
		{
			pos = lDataLength - 188;

			pFileReader->SetFilePointer(-(__int64)(m_fileEndOffset + (__int64)lDataLength), FILE_END);
			pFileReader->Read(pData, lDataLength, &ulBytesRead);

			hr = FindNextPCR(pData, lDataLength, pPids, &midPCR, &pos, -1); //Get the PCR
			if (hr != S_OK){
				break; //exit if no PCR found
			}

			//Test if mid file pos is the mid PCR time.
			if ((__int64)((__int64)midPCR - (__int64)pPids->start) <= (__int64)((__int64)(pPids->end - pPids->start) / 2) + filetolerence
				&& (__int64)((__int64)midPCR - (__int64)pPids->start) > (__int64)((__int64)(pPids->end - pPids->start) / 2) - filetolerence){

				m_fileLenOffset = (__int64)(m_fileLenOffset / 2); //Set file length offset for next search  

				return S_OK; // File length matchs PCR time
			}
		}
		m_fileLenOffset = (__int64)(m_fileLenOffset / 2); //Set file length offset for next search 
	}
	return S_FALSE; // File length does not match PCR time
}

void PidParser::AddPidArray()
{
	PidInfo *newPidInfo = new PidInfo();
	pidArray.Add(newPidInfo);
	SetPidArray(pidArray.Count() - 1);
}


void PidParser::SetPidArray(int n)
{
	if ((n < 0) || (n >= pidArray.Count()))
		return;
	PidInfo *pidInfo = &pidArray[n];

	pidInfo->CopyFrom(&pids);

	pidInfo->TsArray[0] = 0;
	AddTsPid(pidInfo, pids.pmt);	AddTsPid(pidInfo, pids.pcr);
	AddTsPid(pidInfo, pids.vid);	AddTsPid(pidInfo, pids.aud);
	AddTsPid(pidInfo, pids.txt);	AddTsPid(pidInfo, pids.ac3);
	AddTsPid(pidInfo, 0x00);			AddTsPid(pidInfo, 0x10);
	AddTsPid(pidInfo, 0x11);			AddTsPid(pidInfo, 0x12);
	AddTsPid(pidInfo, 0x13);			AddTsPid(pidInfo, 0x14);

	if (pids.aud2 != 0) AddTsPid(pidInfo, pids.aud2);
	if (pids.ac3_2 != 0) AddTsPid(pidInfo, pids.ac3_2);
}

void PidParser::AddTsPid(PidInfo *pidInfo, WORD pid)
{
	for (int i = 1; i < (int) pidInfo->TsArray[0]; i++)
	{
		if (pidInfo->TsArray[i] == pid)
			return;
	}

	pidInfo->TsArray[0]++;
	pidInfo->TsArray[pidInfo->TsArray[0]] = pid;
	return;
}

void PidParser::get_ChannelNumber(BYTE *pointer)
{
	TCHAR sz[128];
	sprintf(sz, "%i",pids.chnumb);
	memcpy(pointer, sz, 128);
}

void PidParser::get_NetworkName(BYTE *pointer)
{
		memcpy(pointer, m_NetworkName, 128);
}

void PidParser::get_ONetworkName(BYTE *pointer)
{
		memcpy(pointer, pids.onetname, 128);
}

void PidParser::get_ChannelName(BYTE *pointer)
{
		memcpy(pointer, pids.chname, 128);
}

HRESULT PidParser::get_EPGFromFile()
{
	return CheckEPGFromFile();
}

void PidParser::get_ShortDescr(BYTE *pointer)
{
	memcpy(pointer, pids.sdesc, 128);
}

void PidParser::get_ExtendedDescr(BYTE *pointer)
{
	memcpy(pointer, pids.edesc, 600);
}

void PidParser::get_ShortNextDescr(BYTE *pointer)
{
	memcpy(pointer, pids.sndesc, 128);
}

void PidParser::get_ExtendedNextDescr(BYTE *pointer)
{
	memcpy(pointer, pids.endesc, 600);
}

void PidParser::get_CurrentTSArray(ULONG *pPidArray)
{
	memcpy(pPidArray, pidArray[m_pgmnumb].TsArray, 16*sizeof(ULONG));
}

WORD PidParser::get_ProgramNumber()
{
	return m_pgmnumb;
}

void PidParser::set_ProgramNumber(WORD programNumber)
{
	m_pgmnumb = programNumber;
	pids.Clear();
	pids.CopyFrom(&pidArray[m_pgmnumb]);
}

void PidParser::set_SIDPid(int bProgramSID)
{
	m_ProgramSID = bProgramSID;
	return;
}

HRESULT PidParser::set_ProgramSID()
{
	HRESULT hr = S_FALSE;
	m_pgmnumb = 0;
	
	// fail if there is only one program in file
	if (pidArray.Count() <= 1)
		return S_FALSE;
	//loop through SID's in list
	for (int c = 0; c < pidArray.Count(); c++)
	{
		if (m_ProgramSID == pidArray[c].sid && m_ProgramSID != 0)
		{
			//now copy the pids from the SID program found
			m_pgmnumb = c;
			pids.Clear();
			pids.CopyFrom(&pidArray[m_pgmnumb]);
			return S_OK;
		}
	}
	return S_FALSE;
}
