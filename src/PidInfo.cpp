/**
*  PidInfo.cpp
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
*  nate can be reached on the forums at
*    http://forums.dvbowners.com/
*/

#include "PidInfo.h"
#include <crtdbg.h>

PidInfo::PidInfo()
{
	Clear();
}

PidInfo::~PidInfo()
{
}

void PidInfo::Clear()
{
	vid   = 0;
	aud   = 0;
	aud2  = 0;
	ac3   = 0;
	txt   = 0;
	sid   = 0;
	pmt   = 0;
	pcr   = 0;
	start = 0;
	end   = 0;
	dur   = 0;
	for (int i = 0 ; i < 15 ; i++ )
	{
		TsArray[i] = 0;
	}
}

void PidInfo::CopyFrom(PidInfo *pidInfo)
{
	vid   = pidInfo->vid;
	aud   = pidInfo->aud;
	aud2  = pidInfo->aud2;
	ac3   = pidInfo->ac3;
	txt   = pidInfo->txt;
	sid   = pidInfo->sid;
	pmt   = pidInfo->pmt;
	pcr   = pidInfo->pcr;
	start = pidInfo->start;
	end   = pidInfo->end;
	dur   = pidInfo->dur;
	for (int i = 0 ; i < 15 ; i++ )
	{
		TsArray[i] = pidInfo->TsArray[i];
	}
}

void PidInfo::CopyTo(PidInfo *pidInfo)
{
}

PidInfoArray::PidInfoArray()
{
}

PidInfoArray::~PidInfoArray()
{
	Clear();
}

void PidInfoArray::Clear()
{
	std::vector<PidInfo *>::iterator it = m_Array.begin();
	for ( ; it != m_Array.end() ; it++ )
	{
		delete *it;
	}
	m_Array.clear();
}

void PidInfoArray::Add(PidInfo *newPidInfo)
{
	m_Array.push_back(newPidInfo);
}

void PidInfoArray::RemoveAt(int nPosition)
{
	if ((nPosition >= 0) && (nPosition < m_Array.size()))
	{
		m_Array.erase(m_Array.begin() + nPosition);
	}
}

PidInfo &PidInfoArray::operator[](int nPosition)
{
	int size = m_Array.size();
	_ASSERT(nPosition >= 0);
	_ASSERT(nPosition < size);

	return *m_Array.at(nPosition);
}

int PidInfoArray::Count()
{
	return m_Array.size();
}

