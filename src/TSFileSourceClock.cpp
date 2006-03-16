/**
*  TSFileSourceClock.ccp
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

#include <streams.h>
#include "TSFileSourceClock.h"

//////////////////////////////////////////////////////////////////////
// TSFileSourceClock
//////////////////////////////////////////////////////////////////////

CTSFileSourceClock::CTSFileSourceClock( TCHAR *pName, LPUNKNOWN pUnk, HRESULT *phr ) :
	CBaseReferenceClock(pName, pUnk, phr, NULL),
	m_bPause(FALSE),
	m_baseTime(0)
{
}

CTSFileSourceClock::~CTSFileSourceClock()
{
}

REFERENCE_TIME CTSFileSourceClock::GetPrivateTime()
{
	// The standard CBaseReferenceClock::GetPrivateTime() method doesn't always start at zero
	// which seems cause problems sometimes. This method is basically the same except it
	// stores the very first value and then subtracts it from every subsequent value
	// to make the times start from zero.

    CAutoLock cObjectLock(this);

    DWORD dwTime = timeGetTime();

	REFERENCE_TIME rtPrivateTime;
    rtPrivateTime = Int32x32To64(UNITS / MILLISECONDS, (DWORD)(dwTime));

	if (m_baseTime == 0)
	{
		m_baseTime = rtPrivateTime;
	}

	dwTime = timeGetTime();
	if(m_bPause)
	{
//		for (int i = 0; i = 1000; i++){}
		dwTime = timeGetTime() - dwTime;
		m_baseTime = (REFERENCE_TIME)max(0, (__int64)((__int64)m_baseTime + (__int64)((__int64)Int32x32To64(UNITS / MILLISECONDS, (DWORD)(dwTime)))));
	}
	m_bPause = FALSE;

	rtPrivateTime -= m_baseTime;

    return rtPrivateTime;
}

void CTSFileSourceClock::SetClockPause(BOOL bPause)
{
	m_bPause = bPause;
}

