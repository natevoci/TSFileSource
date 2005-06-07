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
	//m_buflen = 1504000;	//94000*16
	//m_pData = new BYTE[m_buflen];
	m_pFileReader = pFileReader;
}

PidParser::~PidParser()
{
	//delete[] m_pData;
}

HRESULT PidParser::ParseFromFile()
{
	long a;
	DWORD filepoint;
	HRESULT hr = S_OK;

	if (m_pFileReader->IsFileInvalid())
	{
		return NOERROR;
	}

	// Access the sample's data buffer
	//filepos = 0;
	a = 0;

	long lDataLength = 2000000;
	PBYTE pData = new BYTE[lDataLength];
	m_pFileReader->Read(pData, lDataLength, 0, FILE_BEGIN);

	//Clear all Pid arrays
	pidArray.Clear();

	hr = S_OK;
	while (hr == S_OK)
	{
		//search at the head of the file
		hr = FindSyncByte(pData, lDataLength, &a, 1);
		if (hr == S_OK)
		{
			//parse next packet for the PAT
			if (ParsePAT(pData, lDataLength, a) == S_OK)
			{
				break;
			};
			a += 188;
		}
	}

	//if no PAT found Scan for PMTs
	if (pidArray.Count() == 0)
	{
		//filepos = 0;
		a = 0;
		pids.pmt = 0;
		hr = S_OK;
		//Scan buffer for any PMTs
		while (pids.pmt == 0x00 && hr == S_OK)
		{
			//search at the head of the file
			hr = FindSyncByte(pData, lDataLength, &a, 1);
			if (hr == S_OK)
			{
				//parse next packet for the PMT
				if (ParsePMT(pData, lDataLength, a) == S_OK)
				{
					//Check if PMT was already found
					BOOL pmtfound = FALSE;
					for (int i = 0; i < pidArray.Count(); i++)
					{
						//Search PMT Array for the PMT
						if (pidArray[i].pmt == pids.pmt)
							pmtfound = TRUE;
					}
					if (!pmtfound)
						AddPidArray();

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
		bool foundpmt; //Flag for PMT found

		int curr_pmt = pidArray[i].pmt;
		while (pids.pmt != curr_pmt && hr == S_OK)
		{
			//search at the head of the file
			hr = FindSyncByte(pData, lDataLength, &a, 1);
			if (hr == S_OK)
			{
				//parse next packet for the PMT
				foundpmt = S_FALSE; //Set Flag to PMT not found
				pids.Clear();
				if (ParsePMT(pData, lDataLength, a) == S_OK)
				{
					//Check PMT matches program
					if (pids.pmt == curr_pmt)
					{
						//Search for valid A/V pids
						if (IsValidPMT(pData, lDataLength) == S_OK)
						{
							//Set pcr & Store pids from PMT

//**********************************************************************************************
//Duration Additions

							pids.dur = GetFileDuration(&pids);

//Removed:-			pids.start = GetPCRFromFile(1);
//Removed:-			pids.end = GetPCRFromFile(-1);
//Removed:-			pids.dur = (REFERENCE_TIME)((pids.end - pids.start)/9) * 1000;

//**********************************************************************************************


							SetPidArray(i);
//							filepos = 0;
							i++;
							foundpmt = S_OK; //Set Flag to found PMT
							continue;
						}
					}
				}
				a += 188;
			}
		}
		//remove program from program List if we have not found
		//a matching PMT with A/V pids in the stream.
		if (foundpmt == S_FALSE)
			pidArray.RemoveAt(i); // remove program from program List
	}

	//Search for A/V pids if no NIT or valid PMT found.
	if (pidArray.Count() == 0)
	{
		//Search for any A/V Pids
		if (ACheckVAPids(pData, lDataLength) == S_OK)
		{
			//Set the pcr to video or audio incase its included in packet
			USHORT pcrsave = pids.pcr;
			if (pids.vid && !pids.pcr)
				pids.pcr = pids.vid;
			else if (pids.aud && !pids.pcr)
				pids.pcr = pids.aud;

//**********************************************************************************************
//Duration Additions

			pids.dur = GetFileDuration(&pids);

//Removed:-			pids.start = GetPCRFromFile(1);
//Removed:-			pids.end = GetPCRFromFile(-1);
//Removed:-			pids.dur = (REFERENCE_TIME)((pids.end - pids.start)/9) * 1000;

//**********************************************************************************************

			// restore pcr pid if no matches with A/V pids
			if (!pids.dur)
				pids.pcr = pcrsave;

			AddPidArray();
		}
	}

	//Set the Program Number to beginning & load back pids
	m_pgmnumb = 0;
	//GetPidArray(m_pgmnumb);
	pids.CopyFrom(&pidArray[m_pgmnumb]);

	filepoint = 0;
	CheckEPGFromFile();

	delete[] pData;

	if (pids.vid != 0 || pids.aud != 0 || pids.ac3 != 0 || pids.txt != 0)
	{
		return S_OK;
	}
	else
	{
		return S_FALSE;
	}
}

HRESULT PidParser::FindSyncByte(PBYTE pbData, long lDataLength, PLONG a, int step)
{
	while ((*a > -1) && (*a < lDataLength))
	{
		if (pbData[*a] == 0x47)
		{
			if (*a+188 < lDataLength)
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

HRESULT PidParser::ParsePAT(PBYTE pData, LONG lDataLength, long pos)
{
	HRESULT hr = S_FALSE;
	WORD channeloffset;

	if ( (0x30 & pData[pos + 3]) == 0x10 && pData[pos + 4] == 0 &&
		  pData[pos + 5] == 0 && (0xf0 & pData[pos + 6]) == 0xb0 ) {

		if ((((0x1F & pData[pos + 1]) <<  8)|(0xFF & pData[pos + 2])) == 0x00){

			channeloffset = (WORD)(0x0F & pData[pos + 15]) << 8 | (0xFF & pData[pos + 16]);

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
					//IncPidCount();
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

HRESULT PidParser::ParsePMT(PBYTE pData, LONG lDataLength, long pos)
{
	WORD pid;
	WORD channeloffset;
	WORD EsDescLen;
	WORD StreamType;
	WORD privatepid = 0;

	if ( (0x30&pData[pos+3])==0x10 && pData[pos+4]==0 && pData[pos+5]==2 && (0xf0&pData[pos+6])==0xb0 )
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
					if (CheckEsDescriptorForAC3(pData, lDataLength, b + 5, b + 5 + EsDescLen))
						pids.ac3 = pid;
					else if (CheckEsDescriptorForTeletext(pData, lDataLength, b + 5, b + 5 + EsDescLen))
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

HRESULT PidParser::CheckForPCR(PBYTE pData, LONG lDataLength, PidInfo *pPids, int pos, REFERENCE_TIME* pcrtime)
{
	if ((WORD)((0x1F & pData[pos+1])<<8 | (0xFF & pData[pos+2])) == pPids->pcr)
	{
		WORD pcrbit = 0x30;
		if (pPids->pcr != pPids->vid)
			pcrbit = 0x20;

		if (((pData[pos+3] & 0x30) == pcrbit)
			&& (pData[pos+4] != 0x00)
			&& ((pData[pos+5] & 0x10) != 0x00))
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

BOOL PidParser::CheckEsDescriptorForAC3(PBYTE pData, LONG lDataLength, int pos, int lastpos)
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

BOOL PidParser::CheckEsDescriptorForTeletext(PBYTE pData, LONG lDataLength, int pos, int lastpos)
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

HRESULT PidParser::IsValidPMT(PBYTE pData, LONG lDataLength)
{
	HRESULT hr = S_FALSE;
	//exit if no a/v pids to find
	if (pids.aud + pids.vid + pids.ac3 + pids.txt == 0) {return hr;};

	long a, b;
	WORD pid;
	int addlength, addfield;
	WORD error, start;
	DWORD psiID, pesID;
	//filepos = 0;
	a = 0;
	hr = S_OK;

	while (hr == S_OK)
	{
		hr = FindSyncByte(pData, lDataLength, &a, 1);
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

			pid = ((0x1F & pData[b+1])<<8 | (0xFF & pData[b+2]));

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

HRESULT PidParser::FindFirstPCR(PBYTE pData, long lDataLength, PidInfo *pPids, REFERENCE_TIME* pcrtime, long* pos)
{
	*pos = 0;
	return FindNextPCR(pData, lDataLength, pPids, pcrtime, pos, 1);
}

HRESULT PidParser::FindLastPCR(PBYTE pData, long lDataLength, PidInfo *pPids, REFERENCE_TIME* pcrtime, long* pos)
{
	*pos = lDataLength - 188;
	return FindNextPCR(pData, lDataLength, pPids, pcrtime, pos, -1);
}

HRESULT PidParser::FindNextPCR(PBYTE pData, long lDataLength, PidInfo *pPids, REFERENCE_TIME* pcrtime, long* pos, int step)
{
	HRESULT hr = S_OK;

	*pcrtime = 0;

	while( *pcrtime == 0 && hr == S_OK)
	{
		hr = FindSyncByte(pData, lDataLength, pos, step);
		if (FAILED(hr))
			return hr;

		if (S_FALSE == CheckForPCR(pData, lDataLength, pPids, *pos, pcrtime))
		{
			*pos = *pos + (step * 188);
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

	long lDataLength = 2000000;
	PBYTE pData = new BYTE[lDataLength];
	m_pFileReader->Read(pData, lDataLength, 0, (step > 0) ? FILE_BEGIN : FILE_END);
	long a = (step > 0) ? 0 : lDataLength-188;

	if (step > 0)
		FindFirstPCR(pData, lDataLength, &pids, &pcrtime, &a);
	else
		FindLastPCR(pData, lDataLength, &pids, &pcrtime, &a);

	delete[] pData;

	return pcrtime;
}

HRESULT PidParser::ACheckVAPids(PBYTE pData, LONG lDataLength)
{
	long a, b;
	WORD pid;
	HRESULT hr;
	int addlength, addfield;
	WORD error, start;
	DWORD psiID, pesID;
	//filepos = 0;
	a = 0;

	hr = S_OK;
	while (hr == S_OK)
	{
		hr = FindSyncByte(pData, lDataLength, &a, 1);
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
	long pos;
	int len;
	int len1;
	int DummyPos;
	int iterations;
	bool epgfound;

	HRESULT hr;

	pos = 0;
	iterations = 0;
	epgfound = false;
	m_shortdescr[0] = 0xFF;
	m_extenddescr[0] = 0xFF;

	long lDataLength = 2000000;
	PBYTE pData = new BYTE[lDataLength];
	m_pFileReader->SetFilePointer(188000, FILE_BEGIN);
	m_pFileReader->Read(pData, lDataLength);

	hr = FindSyncByte(pData, lDataLength, &pos, 1);

	while ((pos < lDataLength) && (hr == S_OK) && (epgfound == false))
	{
		epgfound = CheckForEPG(pData, lDataLength, pos);

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
					hr = m_pFileReader->Read(pData, 2000000);
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

	if (hr == S_FALSE)
	{
		return S_FALSE;
	}

	len = ((0x0F & pData[pos+29])<<8 | (0xFF & pData[pos+30]));

	if (len > 1000) { return S_FALSE;};

	DummyPos = min(len, 157);

	ParseEISection(DummyPos);

	//CpyRegion(pData, pos+31, m_pDummy, 0, DummyPos);
	memcpy(m_pDummy, pData+pos+31, DummyPos);

	while (pos < lDataLength && DummyPos < len)
	{
		pos = pos + 188;
		if (((0xFF & pData[pos+1]) == 0x00) &&
			((0xFF & pData[pos+2]) == 0x12) )
		{
			len1 = min(len-DummyPos, 183);

			//CpyRegion(pData, pos+4, m_pDummy, DummyPos, len1);
			memcpy(m_pDummy+DummyPos, pData+pos+4, len1);

			DummyPos += len1;
		}
	}

	delete[] pData;

	return S_OK;
}

bool PidParser::CheckForEPG(PBYTE pData, LONG lDataLength, int pos)
{

	WORD sidpid;

	if (   ((0xFF & pData[pos+1]) == 0x40)
		&& ((0xFF & pData[pos+2]) == 0x12)
		&& ((0xFF & pData[pos+5]) == 0x4e)
		&& ((0x01 & pData[pos+11]) == 0x00)    // should be 0x01
		) {

		sidpid = (WORD)(((0xFF & pData[pos+8])<<8) | (0xFF & pData[pos+9]));

		if (sidpid == pids.sid) {

			return true;
		}

	};

	return false;
}

HRESULT PidParser::ParseEISection (int lDataLength)
{
	int pos = 0;
	WORD DescTag;
	int DescLen;

	while (pos < lDataLength)
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

HRESULT PidParser::ParseShortEvent(int start, int lDataLength)
{
	//CpyRegion(m_pDummy, start, m_shortdescr, 0, lDataLength);
	memcpy(m_shortdescr, m_pDummy + start, lDataLength);

	return S_OK;
}

HRESULT PidParser::ParseExtendedEvent(int start, int lDataLength)
{
	//CpyRegion(m_pDummy, start, m_extenddescr, 0, lDataLength);
	memcpy(m_extenddescr, m_pDummy + start, lDataLength);

	return S_OK;
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

void PidParser::get_ShortDescr(BYTE *pointer)
{
	if ((int)m_shortdescr[1] < 126)
	{
		//CpyRegion(m_shortdescr, 0, pointer, 0, (int)m_shortdescr[1]+2);
		memcpy(pointer, m_shortdescr, m_shortdescr[1]+2);
	};
}

void PidParser::get_ExtendedDescr(BYTE *pointer)
{
	//CpyRegion(m_extenddescr, 0, pointer, 0, 600);
	memcpy(pointer, m_extenddescr, 600);
}

void PidParser::get_CurrentTSArray(ULONG *pPidArray)
{
	//for (int i=0; i < 15; i++)
	//{
	//	pointer[i] = pidArray[m_pgmnumb].TsArray[i];
	//}
	memcpy(pPidArray, pidArray[m_pgmnumb].TsArray, 15*sizeof(ULONG));
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


//**********************************************************************************************
//Duration Additions


REFERENCE_TIME PidParser::GetFileDuration(PidInfo *pPids)
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
	m_pFileReader->GetFileSize(&filelength);	

	__int64 endFilePos = filelength;
	m_fileLenOffset = filelength;
	m_fileStartOffset = 0;
	m_fileEndOffset = 0;

	// find first Duration
	while (m_fileStartOffset < filelength){
		
		hr = GetPCRduration(pData, lDataLength, pPids, filelength, &startFilePos, &endFilePos);
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
				pids.rate = long (((endFilePos - startFilePos)*80000000) / PeriodOfPCR);

				break;
			}

			m_fileStartOffset = endFilePos;
		}
		else
			m_fileStartOffset = m_fileStartOffset + 100000;
		
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

HRESULT PidParser::GetPCRduration(PBYTE pData, long lDataLength, PidInfo *pPids, __int64 filelength, __int64* pStartFilePos, __int64* pEndFilePos)
{
	HRESULT hr;
	long pos;
	REFERENCE_TIME midPCR;
	__int64 filetolerence = 100000UL;
	
	pos = 0;
	m_pFileReader->SetFilePointer(m_fileStartOffset, FILE_BEGIN);
	m_pFileReader->Read(pData, lDataLength);
	hr = FindNextPCR(pData, lDataLength, pPids, &pPids->start, &pos, 1); //Get the PCR
	if (hr == S_OK){

		m_fileLenOffset = m_fileLenOffset - (__int64)(pos - 1);
		*pStartFilePos = m_fileStartOffset + (__int64)(pos - 1); 
	}

	while(m_fileLenOffset > (__int64)(10 * 188))
	{
		if (pPids->start == 0)
			break; //exit if no PCR found

		pos = lDataLength - 188;
		m_pFileReader->SetFilePointer(-(__int64)(m_fileEndOffset + (__int64)lDataLength), FILE_END);
		m_pFileReader->Read(pData, lDataLength);
		hr = FindNextPCR(pData, lDataLength, pPids, &pPids->end, &pos, -1); //Get the PCR
		if (hr != S_OK){
			break; //exit if no PCR found
		}
		*pEndFilePos = filelength - m_fileEndOffset - (__int64)lDataLength + (__int64)pos - 1;// 

		if (*pEndFilePos <= *pStartFilePos){
			break; //exit if past start time
		}
		m_fileEndOffset = m_fileEndOffset + (__int64)(m_fileLenOffset / 2); //Set file mid search pos

		//exit if bad PCR timming found
		if (pPids->end > pPids->start)
		{

			pos = lDataLength - 188;
			m_pFileReader->SetFilePointer(-(__int64)(m_fileEndOffset + (__int64)lDataLength), FILE_END);
			m_pFileReader->Read(pData, lDataLength);
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


//**********************************************************************************************


















/*
TCHAR sz[100];
wsprintf(sz, TEXT("%u %u %u"), ((DWORD)0),((DWORD)0),((DWORD)pidArray.Count())); // 1f f0
MessageBox(NULL, sz, TEXT("kk"), MB_OK);
*/
