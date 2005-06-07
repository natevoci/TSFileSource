/**
*  TunerEvent.h
*  Copyright (C) 2004-2005 bear
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
*  bear can be reached on the forums at
*    http://forums.dvbowners.com/
*/

#ifndef TUNEREVENT_H
#define TUNEREVENT_H

#include "tuner.h"
#include <commctrl.h>
#include <atlbase.h>
#include "KS.H"
#include "KSMEDIA.H"
#include "bdamedia.h"

#include "Demux.h"

/**********************************************
 *
 *  TunerEvent Class
 *
 **********************************************/

class TunerEvent : public IBroadcastEvent
{

private:
    long m_nRefCount; // Holds the reference count.
	Demux *m_pDemux;
	IFilterGraph *m_pFilterGraph;

public:

    // IUnknown methods
    STDMETHODIMP_(ULONG) AddRef()
    {
        return InterlockedIncrement(&m_nRefCount);
    }
    STDMETHODIMP_(ULONG) Release()
    {
        _ASSERT(m_nRefCount >= 0);
        ULONG uCount = InterlockedDecrement(&m_nRefCount);
        if (uCount == 0)
        {
            delete this;
        }
        // Return the temporary variable, not the member
        // variable, for thread safety.
        return uCount;
    }
    STDMETHODIMP QueryInterface(REFIID riid, void **ppvObject)
    {
        if (NULL == ppvObject)
            return E_POINTER;
        if (riid == __uuidof(IUnknown))
            *ppvObject = static_cast<IUnknown*>(this);
        else if (riid == __uuidof(IBroadcastEvent))
            *ppvObject = static_cast<IBroadcastEvent*>(this);
        else 
            return E_NOINTERFACE;
        AddRef();
        return S_OK;
    }

    // The one IBroadcastEvent method.
    STDMETHOD(Fire)(GUID eventID)
    {
        // The one defined event.
        if (eventID == EVENTID_TuningChanged)
        {
			// The tuner changed stations or channels.

			int bNPControl = m_pDemux->get_NPControl(); //Save NP Control mode
			m_pDemux->set_NPControl(false); //Turn off NP Control else we will loop

			int bNPSlave = m_pDemux->get_NPSlave(); //Save NP Slave mode
			m_pDemux->set_NPSlave(true); //Turn on NP Slave mode to change SID to set Demux control

			m_pDemux->AOnConnect(m_pFilterGraph); //Update Demux Pins & Pid mapping

			m_pDemux->set_NPControl(bNPControl); //Restore NP Control mode
			m_pDemux->set_NPSlave(bNPSlave); //Restore NP Control mode
        }
        
        return S_OK;
    }

    TunerEvent(Demux *pDemux) : m_dwBroadcastEventCookie(0), m_nRefCount(1)
	{
		m_pDemux = pDemux;
	};

public: //private
    HRESULT HookupGraphEventService(IFilterGraph *pGraph);
    HRESULT RegisterForTunerEvents();
    HRESULT UnRegisterForTunerEvents();
    HRESULT Fire_Event(GUID eventID);

    CComPtr <IBroadcastEvent> m_spBroadcastEvent; 
    DWORD m_dwBroadcastEventCookie;
};

#endif
