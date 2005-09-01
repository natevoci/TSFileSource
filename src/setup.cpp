/**
*  setup.cpp
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

#include <streams.h>
#include <initguid.h>

#include "TSFileSourceGuids.h"
#include "TSFileSource.h"
#include "TSFileSourceProp.h"

#define TSFILESOURCENAME		L"TS File Filter(AU)"
#define TSFILESOURCEPROPERTIES	L"TS File Filter(AU) Properties"
#define TSFILESOURCEINTERFACE	L"TS File Filter(AU) Interface"


// Filter setup data
const AMOVIESETUP_MEDIATYPE sudOpPinTypes =
{
	&MEDIATYPE_Video,                  // Major type
	&MEDIASUBTYPE_MPEG2_TRANSPORT      // Minor type
};

const AMOVIESETUP_PIN sudTSFileSourcePin =
{
	L"Output",		    // Obsolete, not used.
	FALSE,			// Is this pin rendered?
	TRUE,			// Is it an output pin?
	FALSE,			// Can the filter create zero instances?
	FALSE,			// Does the filter create multiple instances?
	&CLSID_NULL,	// Obsolete.
	NULL,			// Obsolete.
	1,				// Number of media types.
	&sudOpPinTypes	// Pointer to media types.
};

const AMOVIESETUP_FILTER sudTSFileSourceFilter =
{
	&CLSID_TSFileSource,	// Filter CLSID
	TSFILESOURCENAME,		// String name
	MERIT_DO_NOT_USE,		// Filter merit
	1,						// Number pins
	&sudTSFileSourcePin		// Pin details
};

CFactoryTemplate g_Templates[] =
{
	{
		TSFILESOURCENAME,
		&CLSID_TSFileSource,
		CTSFileSourceFilter::CreateInstance,
		NULL,
		&sudTSFileSourceFilter
	},
	{
		TSFILESOURCEPROPERTIES,
		&CLSID_TSFileSourceProp,
		CTSFileSourceProp::CreateInstance,
		NULL,
		NULL
	},
};

int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);



////////////////////////////////////////////////////////////////////////
//
// Exported entry points for registration and unregistration
// (in this case they only call through to default implementations).
//
////////////////////////////////////////////////////////////////////////

STDAPI DllRegisterServer()
{
	return AMovieDllRegisterServer2( TRUE );
}

STDAPI DllUnregisterServer()
{
	return AMovieDllRegisterServer2( FALSE );
}

//
// DllEntryPoint
//
extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL APIENTRY DllMain(HANDLE hModule,
					  DWORD  dwReason,
					  LPVOID lpReserved)
{
	return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}

