/**
*  PidParser.h
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

#ifndef PIDPARSER_H
#define PIDPARSER_H

#include "PidInfo.h"
#include "FileReader.h"

class PidParser
{
public:
	PidParser(FileReader *pFileReader);
	virtual ~PidParser();

	HRESULT ParseFromFile();

	void get_ShortDescr(BYTE *pointer);
	void get_ExtendedDescr(BYTE *pointer);
	void get_CurrentTSArray(ULONG *pPidArray);

	WORD get_ProgramNumber();
	void set_ProgramNumber(WORD programNumber);

	PidInfo pids;
	PidInfoArray pidArray;	//Currently selected pids

	static HRESULT FindSyncByte(PBYTE pbData, long lDataLength, LPLONG a, int step);

	static HRESULT FindFirstPCR(PBYTE pData, long lDataLength, PidInfo *pPids, REFERENCE_TIME* pcrtime, long* pos);
	static HRESULT FindLastPCR(PBYTE pData, long lDataLength, PidInfo *pPids, REFERENCE_TIME* pcrtime, long* pos);
	static HRESULT FindNextPCR(PBYTE pData, long lDataLength, PidInfo *pPids, REFERENCE_TIME* pcrtime, long* pos, int step);

protected:
	static HRESULT CheckForPCR(PBYTE pData, LONG lDataLength, PidInfo *pPids, int pos, REFERENCE_TIME* pcrtime);

protected:
	HRESULT ParsePAT(PBYTE pData, LONG lDataLength, int pos);
	HRESULT ParsePMT(PBYTE pData, LONG lDataLength, int pos);
	BOOL CheckEsDescriptorForAC3(PBYTE pData, LONG lDataLength, int pos, int lastpos);
	BOOL CheckEsDescriptorForTeletext(PBYTE pData, LONG lDataLength, int pos, int lastpos);

	HRESULT IsValidPMT(PBYTE pData, LONG lDataLength);

	REFERENCE_TIME GetPCRFromFile(int step);

	HRESULT ACheckVAPids(PBYTE pData, LONG lDataLength);

	HRESULT CheckEPGFromFile();
	bool CheckForEPG(PBYTE pData, LONG lDataLength, int pos);
	HRESULT ParseEISection (int lDataLength);
	HRESULT ParseShortEvent(int start, int IDataLength);
	HRESULT ParseExtendedEvent(int start, int IDataLength);

	void AddPidArray();
	void SetPidArray(int n);
	void AddTsPid(PidInfo *pidInfo, WORD pid);

protected:
	FileReader *m_pFileReader;

	//int		m_buflen;
	//PBYTE	m_pData;
	BYTE	m_pDummy[1000];

	BYTE	m_shortdescr[128];
	BYTE	m_extenddescr[600];

	__int64	filepos;
	WORD	m_pgmnumb;

};

#endif
