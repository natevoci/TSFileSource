
#ifndef SMSOURCECLOCK_H
#define SMSOURCECLOCK_H

class CSMSourceClock : public CBaseReferenceClock
{
public:
	CSMSourceClock( TCHAR *pName, LPUNKNOWN pUnk, HRESULT *phr );
	virtual ~CSMSourceClock();

    virtual REFERENCE_TIME GetPrivateTime();

private:
	REFERENCE_TIME m_baseTime;
	double m_dRateSeeking;

};

#endif
