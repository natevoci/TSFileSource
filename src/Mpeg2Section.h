/**
*  Mpeg2Section.h
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

#ifndef MPEG2SECTION_H
#define MPEG2SECTION_H

#include "Mpeg2TSPacket.h"

//////////////////////////////////////////////////////////////////////
// MPEG2Section
//////////////////////////////////////////////////////////////////////

#define MAX_SECTION_LENGTH 1024
#define MAX_SECTION_PARTS 32

class Mpeg2Section
{
public:
	Mpeg2Section(SHORT pid);
	virtual ~Mpeg2Section();

	void Reset();

private:

public:
	SHORT PID;
	BYTE table_id;
	BOOL section_syntax_indicator;
	SHORT section_length;
	SHORT transport_stream_id;	// tableIdExt
	BYTE version_number;
	BOOL current_next_indicator;
	BYTE section_number;
	BYTE last_section_number;

public:
	BYTE m_sectionPartDone[MAX_SECTION_PARTS];
	Mpeg2TSPacket *m_pTSHeader;
	BYTE m_data[MAX_SECTION_LENGTH];
	BYTE m_sectionPos;
};



#endif
