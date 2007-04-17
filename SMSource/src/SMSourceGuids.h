/**
*  SMSourceGuids.h
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

//------------------------------------------------------------------------------
// File: SMSourceGuids.h
//
// Desc: GUID definitions for Shared Memory Source filter set
//
//------------------------------------------------------------------------------


#ifndef __SMSOURCEGUIDS_DEFINED
#define __SMSOURCEGUIDS_DEFINED

// Creating the Filters Property Page

// {A1D23962-E70F-4f85-A6A8-93725200292D}
DEFINE_GUID(CLSID_SMSourceProp, 
0xa1d23962, 0xe70f, 0x4f85, 0xa6, 0xa8, 0x93, 0x72, 0x52, 0x0, 0x29, 0x2d);

// Creating the Filters Instance

// {0AF1141F-3B9C-4910-A881-1D076B859C64}
DEFINE_GUID(CLSID_SMSource, 
0xaf1141f, 0x3b9c, 0x4910, 0xa8, 0x81, 0x1d, 0x7, 0x6b, 0x85, 0x9c, 0x64);

#endif

