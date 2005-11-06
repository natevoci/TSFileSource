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

#include "TSFileSinkGuids.h"
#include "TSFileSink.h"
//#include "TSFileSinkProp.h"

#define TSFILESOURCENAME		L"TS File Source"
#define TSFILESOURCEPROPERTIES	L"TS File Source Properties"
#define TSFILESOURCEINTERFACE	L"TS File Source Interface"

#define TSFILESINKNAME			L"TS File Sink"
//#define TSFILESINKPROPERTIES	L"TS File Sink Properties"
#define TSFILESINKINTERFACE		L"TS File Sink Interface"

// Filter setup data

// Pin Mediatypes

const AMOVIESETUP_MEDIATYPE sudTSFileSourceOutputPinTypes =
{
	&MEDIATYPE_Stream,
	&MEDIASUBTYPE_MPEG2_TRANSPORT
};

const AMOVIESETUP_MEDIATYPE sudTSFileSinkInputPinTypes =
{
    &MEDIATYPE_NULL,
    &MEDIASUBTYPE_NULL
};


// Pin Definitions

const AMOVIESETUP_PIN sudTSFileSourcePin =
{
	L"Output",					   // Obsolete, not used.
	FALSE,							// Is this pin rendered?
	TRUE,							// Is it an output pin?
	FALSE,							// Can the filter create zero instances?
	FALSE,							// Does the filter create multiple instances?
	&CLSID_NULL,					// Obsolete.
	NULL,							// Obsolete.
	1,								// Number of media types.
	&sudTSFileSourceOutputPinTypes	// Pointer to media types.
};

const AMOVIESETUP_PIN sudTSFileSinkPin =
{
	L"Input",						// Obsolete, not used.
	FALSE,							// Is this pin rendered?
	FALSE,							// Is it an output pin?
	FALSE,							// Can the filter create zero instances?
	FALSE,							// Does the filter create multiple instances?
	&CLSID_NULL,					// Obsolete.
	NULL,							// Obsolete.
	1,								// Number of media types.
	&sudTSFileSinkInputPinTypes		// Pointer to media types.
};


// Filter Definitions

const AMOVIESETUP_FILTER sudTSFileSourceFilter =
{
	&CLSID_TSFileSource,	// Filter CLSID
	TSFILESOURCENAME,		// String name
	MERIT_DO_NOT_USE,		// Filter merit
	1,						// Number pins
	&sudTSFileSourcePin		// Pin details
};

const AMOVIESETUP_FILTER sudTSFileSinkFilter =
{
	&CLSID_TSFileSink,		// Filter CLSID
	TSFILESINKNAME,			// String name
	MERIT_DO_NOT_USE,		// Filter merit
	1,						// Number pins
	&sudTSFileSinkPin		// Pin details
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
	{
		TSFILESINKNAME,
		&CLSID_TSFileSink,
		CTSFileSink::CreateInstance,
		NULL,
		&sudTSFileSinkFilter
	},
/*	{
		TSFILESINKPROPERTIES,
		&CLSID_TSFileSinkProp,
		CTSFileSinkProp::CreateInstance,
		NULL,
		NULL
	},*/
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

