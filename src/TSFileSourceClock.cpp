/**
*  TSFileSourceClock.ccp
*  Copyright (C) 2005      nate
*  Copyright (C) 2006      bear
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

#include "stdafx.h"
#include "TSFileSourceClock.h"

//////////////////////////////////////////////////////////////////////
// TSFileSourceClock
//////////////////////////////////////////////////////////////////////

CTSFileSourceClock::CTSFileSourceClock( TCHAR *pName, LPUNKNOWN pUnk, HRESULT *phr ) :
	CBaseReferenceClock(pName, pUnk, phr, NULL),
	m_dRateSeeking(1.0),
	m_baseTime(0),
	m_baseRefTime(0)
{
	m_ticksPerSecond.QuadPart = 0;
	QueryPerformanceFrequency(&m_ticksPerSecond);
	m_baseTicks.QuadPart = 0;
	QueryPerformanceCounter(&m_baseTicks);
}

CTSFileSourceClock::~CTSFileSourceClock()
{
}

REFERENCE_TIME CTSFileSourceClock::GetPrivateTime()
{
    CAutoLock cObjectLock(this);

    DWORD dwTime;
	REFERENCE_TIME rtTime;
	GetTimes(dwTime, rtTime);
    return rtTime;
}

void CTSFileSourceClock::GetTimes(DWORD &dwTime, REFERENCE_TIME &rtTime)
{
    CAutoLock cObjectLock(this);

	LARGE_INTEGER ticks;
	QueryPerformanceCounter(&ticks);
	dwTime = (DWORD)((ticks.QuadPart - m_baseTicks.QuadPart) * 1000LL / m_ticksPerSecond.QuadPart);

	DWORD dwTimeDelta = dwTime - m_baseTime;

    rtTime = Int32x32To64(UNITS / MILLISECONDS, (DWORD)(dwTimeDelta));
	rtTime = (REFERENCE_TIME)(rtTime * m_dRateSeeking);

	// Nate: By slowing the clock slightly I get a lot less breaks in the audio when doing near live playback.
	//       The problem will be that our viewing will slowly get behind live tv. I'm pretty sure we can
	//       do rate matching to compare (bitrate recieved/bitrate delivered) to auto-adjust this value.
	//rtTime = (REFERENCE_TIME)(rtTime * 0.995);

	rtTime += m_baseRefTime;
}

void CTSFileSourceClock::SetClockRate(double dRateSeeking)
{
    CAutoLock cObjectLock(this);

	DWORD dwTime;
	REFERENCE_TIME rtTime;
	GetTimes(dwTime, rtTime);

	m_baseTime = dwTime;
	m_baseRefTime = rtTime;

	m_dRateSeeking = dRateSeeking;

	GetTimes(dwTime, rtTime);

	DWORD dwTimeDelta = dwTime - m_baseTime;
	REFERENCE_TIME rtTimeDelta = rtTime - m_baseRefTime;
}

