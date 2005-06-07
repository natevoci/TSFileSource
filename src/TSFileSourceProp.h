/**
*  TSFileSourceProp.h
*  Copyright (C) 2003      bisswanger
*  Copyright (C) 2004-2005 bear
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
*  bisswanger can be reached at WinSTB@hotmail.com
*    Homepage: http://www.winstb.de
*
*  bear and nate can be reached on the forums at
*    http://forums.dvbowners.com/
*/

#ifndef TSFILESOURCEPROP_H
#define TSFILESOURCEPROP_H

#include "TSFileSource.h"

class CTSFileSourceProp : public CBasePropertyPage
{
public:
	static CUnknown * WINAPI CreateInstance(LPUNKNOWN pUnk, HRESULT *pHr);
	CTSFileSourceProp(IUnknown *pUnk);
	virtual ~CTSFileSourceProp(void);

	HRESULT OnConnect(IUnknown *pUnk);
	HRESULT OnDisconnect(void);
	HRESULT OnActivate(void);

//**********************************************************************************************
//Property Apply Additions

	HRESULT OnApplyChanges(void);

//**********************************************************************************************

	void SetDirty();
	BOOL OnRefreshProgram();
	BOOL PopulateDialog();
	BOOL OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);


private:
	ITSFileSource   *m_pProgram;    // Pointer to the filter's custom interface.
};

#endif