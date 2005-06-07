/**
*  PidInfo.h
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


#ifndef TSSTREAMTOOLS_H
#define TSSTREAMTOOLS_H

#include <vector>

class PidInfo
{
public:
	PidInfo();
	virtual ~PidInfo();

	void Clear();
	void CopyFrom(PidInfo *pidInfo);
	void CopyTo(PidInfo *pidInfo);

	int vid;
	int aud;
	int aud2;
	int ac3;
//***********************************************************************************************
//Audio 2 Additions

	int ac3_2;

//***********************************************************************************************
	int txt;
	int sid;
	int pmt;
	int pcr;
	__int64 start;
	__int64 end;
	__int64 dur;

	long bitrate;

	unsigned long TsArray[15];
};

class PidInfoArray
{
public:
	PidInfoArray();
	virtual ~PidInfoArray();

	void Clear();
	void Add(PidInfo *newPidInfo);
	void RemoveAt(int nPosition);

	PidInfo &operator[](int nPosition);
	int Count();

private:
	std::vector<PidInfo *> m_Array;

};

#endif
