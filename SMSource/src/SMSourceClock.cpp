

#include "stdafx.h"
#include "SMSourceClock.h"

//////////////////////////////////////////////////////////////////////
// SMSourceClock
//////////////////////////////////////////////////////////////////////

CSMSourceClock::CSMSourceClock( TCHAR *pName, LPUNKNOWN pUnk, HRESULT *phr ) :
	CBaseReferenceClock(pName, pUnk, phr, NULL),
	m_dRateSeeking(1.0),
	m_baseTime(0)
{
}

CSMSourceClock::~CSMSourceClock()
{
}

REFERENCE_TIME CSMSourceClock::GetPrivateTime()
{
	// The standard CBaseReferenceClock::GetPrivateTime() method doesn't always start at zero
	// which seems cause problems sometimes. This method is basically the same except it
	// stores the very first value and then subtracts it from every subsequent value
	// to make the times start from zero.

    CAutoLock cObjectLock(this);

    DWORD dwTime = timeGetTime();

	REFERENCE_TIME rtPrivateTime;
    rtPrivateTime = Int32x32To64(UNITS / MILLISECONDS, (DWORD)(dwTime));

	rtPrivateTime = (REFERENCE_TIME)(rtPrivateTime *m_dRateSeeking);

	if (m_baseTime == 0)
	{
		m_baseTime = rtPrivateTime;
	}

	rtPrivateTime -= m_baseTime;

    return rtPrivateTime;
}


