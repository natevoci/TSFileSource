/**
*  Mpeg2TsParser.h
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

#ifndef MPEG2TSPARSER_H
#define MPEG2TSPARSER_H

#include <vector>
#include <list>
#include "Mpeg2TSPacket.h"
#include "Mpeg2Section.h"
#include "PidTableHash.h"

using namespace std;

class PSIInfo
{
public:
	int pat;
};

//////////////////////////////////////////////////////////////////////
// Mpeg2Parser
//////////////////////////////////////////////////////////////////////

#define MAX_PID 0x2000

class Mpeg2Parser
{
public:
	Mpeg2Parser();
	virtual ~Mpeg2Parser();

	void Clear();

	__int64 GetCurrentPosition();
	ULONG GetEndDataPosition();

	HRESULT GetDataBuffer(__int64 filePosition, ULONG ulDataLength, BYTE **ppData);
	HRESULT ParseData(ULONG ulDataLength);

private:
	HRESULT DetectMediaFormat(BYTE *pData, ULONG ulDataLength);

	HRESULT FindSyncByte(BYTE *pbData, ULONG ulDataLength, ULONG* syncBytePosition);
	HRESULT FindTSSyncByte(BYTE *pbData, ULONG ulDataLength, ULONG* syncBytePosition);
	HRESULT FindPSSyncByte(BYTE *pbData, ULONG ulDataLength, ULONG* syncBytePosition);

	HRESULT BuildSections(Mpeg2TSPacket *pTSHeader, BYTE *tsPacket, ULONG ulDataLength);
	HRESULT ProcessSection(Mpeg2Section *pSection);
	HRESULT SectionCRCCheck(Mpeg2Section *pSection);

	HRESULT ParseSections();


private:
	BYTE *m_pData;
	ULONG m_dataBufferSize;
	ULONG m_dataAllocationOffset;

	LONG m_packetSize;
	__int64 m_filePosition;
	ULONG m_syncOffset;

	PidTableHash m_partialSections;
	std::vector<Mpeg2Section *> m_sections;

	std::list<LONGLONG> m_discontinuities;
	std::list<PSIInfo *> m_psiInfoList;

	BOOL m_bLog;
};

#endif
