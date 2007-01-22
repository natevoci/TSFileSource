/**
*  Mpeg2Section.cpp
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
#include "Mpeg2Section.h" 

Mpeg2Section::Mpeg2Section(SHORT pid)
{
	PID = pid;
	table_id = 0;

	m_pTSHeader = NULL;

	Reset();
}

Mpeg2Section::~Mpeg2Section()
{
	Reset();
}

void Mpeg2Section::Reset()
{
	section_syntax_indicator = 0;
	section_length = -1;
	transport_stream_id = 0;	// tableIdExt
	version_number = -1;
	current_next_indicator = 0;
	section_number = -1;
	last_section_number = 0;

	memset(&m_sectionPartDone, 0, MAX_SECTION_PARTS * sizeof(BYTE));

	if (m_pTSHeader)
		delete m_pTSHeader;
	m_pTSHeader = NULL;
	memset(&m_data, 0, MAX_SECTION_LENGTH * sizeof(BYTE));
	m_sectionPos = 0;
}

