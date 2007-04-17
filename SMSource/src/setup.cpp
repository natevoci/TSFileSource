/*
*  Setup.cpp.h
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
*  bear can be reached on the forums at
*    http://forums.dvbowners.com/
*/


#include <streams.h>
#include <objbase.h>
#include <initguid.h>

#include "SMSource.h"
#include "SMSourceGuids.h"
#include "SMSourceProp.h"
#include "SMSink.h"
#include "SMSinkGuids.h"


// Note: It is better to register no media types than to register a partial 
// media type (subtype == GUID_NULL) because that can slow down intelligent connect 
// for everyone else.

// For a specialized source filter like this, it is best to leave out the 
// AMOVIESETUP_FILTER altogether, so that the filter is not available for 
// intelligent connect. Instead, use the CLSID to create the filter or just 
// use 'new' in your application.

#define SMSOURCENAME			L"MPEG2 Shared Memory Source Filter"
#define SMSOURCEPROPERTIES		L"MPEG2 Shared Memory Source Properties"
#define SMSINKNAME				L"MPEG2 Shared Memory Sink Filter"

const AMOVIESETUP_MEDIATYPE sudOpPinTypes =
{
    &MEDIATYPE_Video,                  // Major type
    &MEDIASUBTYPE_MPEG2_TRANSPORT      // Minor type
};


const AMOVIESETUP_PIN sudSMSourcePin = 
{
    L"Output",      // Obsolete, not used.
    FALSE,          // Is this pin rendered?
    TRUE,           // Is it an output pin?
    FALSE,          // Can the filter create zero instances?
    FALSE,          // Does the filter create multiple instances?
    &CLSID_NULL,    // Obsolete.
    NULL,           // Obsolete.
    1,              // Number of media types.
    &sudOpPinTypes  // Pointer to media types.
};

const AMOVIESETUP_FILTER sudSMSource =
{
    &CLSID_SMSource,        // Filter CLSID
    SMSOURCENAME,            // String name
    MERIT_DO_NOT_USE,       // Filter merit
    1,                      // Number pins
    &sudSMSourcePin         // Pin details
};

// Setup data SM Sink Filter

const AMOVIESETUP_MEDIATYPE sudPinTypes =
{
    &MEDIATYPE_NULL,            // Major type
    &MEDIASUBTYPE_NULL          // Minor type
};

const AMOVIESETUP_PIN sudPins =
{
    L"Input",                   // Pin string name
    FALSE,                      // Is it rendered
    FALSE,                      // Is it an output
    FALSE,                      // Allowed none
    FALSE,                      // Likewise many
    &CLSID_NULL,                // Connects to filter
    L"Output",                  // Connects to pin
    1,                          // Number of types
    &sudPinTypes                // Pin information
};

const AMOVIESETUP_FILTER sudDump =
{
    &CLSID_SMSink,              // Filter CLSID
    SMSINKNAME,                 // String name
    MERIT_DO_NOT_USE,           // Filter merit
    1,                          // Number pins
    &sudPins                    // Pin details
};


// List of class IDs and creator functions for the class factory. This
// provides the link between the OLE entry point in the DLL and an object
// being created. The class factory will call the static CreateInstance.
// We provide a set of filters in this one DLL.

CFactoryTemplate g_Templates[] = 
{
	{	SMSOURCENAME,
		&CLSID_SMSource,
		CSMSourceFilter::CreateInstance,
		NULL,
		&sudSMSource
	},
	{ 
        SMSOURCEPROPERTIES,
        &CLSID_SMSourceProp,
        CSMSourceProp::CreateInstance, 
        NULL,
		NULL
    },
	{ 
        SMSINKNAME,
        &CLSID_SMSink,
        CDump::CreateInstance, 
        NULL,
		&sudDump
    }
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

