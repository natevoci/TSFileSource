/**
*  SMSourceProp.h
*  Copyright (C) 2003      bisswanger
*  Copyright (C) 2004-2006 bear
*
*  This file is part of SMSource, a directshow Shared Memory push source filter that
*  provides an MPEG transport stream output.
*
*  SMSource is free software; you can redistribute it and/or modify
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
*  along with SMSource; if not, write to the Free Software
*  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*  bisswanger can be reached at WinSTB@hotmail.com
*    Homepage: http://www.winstb.de
*
*  bear can be reached on the forums at
*    http://forums.dvbowners.com/
*/





#ifndef SMSOURCEPROP_H
#define SMSOURCEPROP_H



#include <streams.h>
#include "SMSource.h"
#include "resource.h"



class CSMSourceProp : public CBasePropertyPage
{
  private:

	ISMSource   *m_pProgram;    // Pointer to the filter's custom interface.
    HWND    m_hwndDialog;
	IBaseFilter   * m_pSMSourceFilter;
	IGraphBuilder * m_pGraphBuilder;
    IMediaControl * m_pMediaControl;



  public:
	static CUnknown * WINAPI CreateInstance(LPUNKNOWN pUnk, HRESULT *pHr);
	CSMSourceProp(IUnknown *pUnk);	
	~CSMSourceProp(); 
	HRESULT OnConnect(IUnknown *pUnk);
	HRESULT OnActivate(void);
	HRESULT OnDisconnect(void);
	void SetDirty();
	BOOL OnRefreshProgram();
	BOOL PopulateTransportStreamInfo(  );
	BOOL OnReceiveMessage(HWND hwnd,
                            UINT uMsg,
                            WPARAM wParam,
                            LPARAM lParam);

  private:
	      
	  
};

#endif
