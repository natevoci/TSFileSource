/**
*  ISMSource.h
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

#ifndef ISMSOURCE_H
#define ISMSOURCE_H


// {6130B16C-CA62-4403-8D67-746723D2FF6E}
DEFINE_GUID(IID_ISMSource, 
0x6130b16c, 0xca62, 0x4403, 0x8d, 0x67, 0x74, 0x67, 0x23, 0xd2, 0xff, 0x6e);

DECLARE_INTERFACE_(ISMSource, IUnknown)
{
	STDMETHOD(GetVideoPid) (THIS_ WORD * vpid) PURE;
	STDMETHOD(GetAudioPid) (THIS_ WORD * apid) PURE;
    STDMETHOD(GetAudio2Pid) (THIS_ WORD * a2pid) PURE;
	STDMETHOD(GetPMTPid) (THIS_ WORD * pmtpid) PURE;
	STDMETHOD(GetSIDPid) (THIS_ WORD * sidpid) PURE;
	STDMETHOD(GetAC3Pid) (THIS_ WORD * ac3pid) PURE;
    STDMETHOD(GetPCRPid) (THIS_ WORD * pcrpid) PURE;
	STDMETHOD(GetDur) (THIS_ REFERENCE_TIME * dur) PURE;
	STDMETHOD(GetShortDescr) (THIS_ BYTE * shortdesc) PURE;
	STDMETHOD(GetExtendedDescr) (THIS_ BYTE * extdesc) PURE;
};


#endif
