/**
*  ITSFileSource.h
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

// {559E6E81-FAC4-4EBC-9530-662DAA27EDC2}
DEFINE_GUID(IID_ITSFileSource,
0x559e6e81, 0xfac4, 0x4ebc, 0x95, 0x30, 0x66, 0x2d, 0xaa, 0x27, 0xed, 0xc2);

DECLARE_INTERFACE_(ITSFileSource, IUnknown)
{
	STDMETHOD(GetVideoPid) (THIS_ WORD * vpid) PURE;
	STDMETHOD(GetAudioPid) (THIS_ WORD * apid) PURE;
	STDMETHOD(GetAudio2Pid) (THIS_ WORD * a2pid) PURE;
	STDMETHOD(GetAC3Pid) (THIS_ WORD * ac3pid) PURE;
	STDMETHOD(GetAC3_2Pid) (THIS_ WORD * ac3_2pid) PURE;
	STDMETHOD(GetTelexPid) (THIS_ WORD * telexpid) PURE;
	STDMETHOD(GetNIDPid) (THIS_ WORD * nidpid) PURE;
	STDMETHOD(GetChannelNumber) (THIS_ BYTE * pointer) PURE;
	STDMETHOD(GetNetworkName) (THIS_ BYTE * pointer) PURE;
	STDMETHOD(GetONIDPid) (THIS_ WORD * onidpid) PURE;
	STDMETHOD(GetONetworkName) (THIS_ BYTE * pointer) PURE;
	STDMETHOD(GetChannelName) (THIS_ BYTE * pointer) PURE;
	STDMETHOD(GetTSIDPid) (THIS_ WORD * tsidpid) PURE;
	STDMETHOD(GetEPGFromFile) (void) PURE;
	STDMETHOD(GetShortNextDescr) (THIS_ BYTE * shortnextdesc) PURE;
	STDMETHOD(GetExtendedNextDescr) (THIS_ BYTE * extnextdesc) PURE;
	STDMETHOD(GetPMTPid) (THIS_ WORD * pmtpid) PURE;
	STDMETHOD(GetSIDPid) (THIS_ WORD * sidpid) PURE;
	STDMETHOD(GetPCRPid) (THIS_ WORD * pcrpid) PURE;
	STDMETHOD(GetDuration) (THIS_ REFERENCE_TIME * dur) PURE;
	STDMETHOD(GetShortDescr) (THIS_ BYTE * shortdesc) PURE;
	STDMETHOD(GetExtendedDescr) (THIS_ BYTE * extdesc) PURE;
	STDMETHOD(GetPgmNumb) (THIS_ WORD * pPgmNumb) PURE;
	STDMETHOD(GetPgmCount) (THIS_ WORD * pPgmCount) PURE;
	STDMETHOD(SetPgmNumb) (THIS_ WORD pPgmNumb) PURE;
	STDMETHOD(NextPgmNumb) (void) PURE;
	STDMETHOD(PrevPgmNumb) (void) PURE;
	STDMETHOD(GetTsArray) (THIS_ ULONG * pPidArray) PURE;
	STDMETHOD(GetAC3Mode) (THIS_ WORD * pAC3Mode) PURE;
	STDMETHOD(SetAC3Mode) (THIS_ WORD AC3Mode) PURE;
	STDMETHOD(GetMP2Mode) (THIS_ WORD * pMP2Mode) PURE;
	STDMETHOD(SetMP2Mode) (THIS_ WORD MP2Mode) PURE;
	STDMETHOD (GetAudio2Mode) (THIS_ WORD * pAudio2Mode) PURE;
	STDMETHOD (SetAudio2Mode) (THIS_ WORD Audio2Mode) PURE;
	STDMETHOD(GetAutoMode) (THIS_ WORD * pAutoMode) PURE;
	STDMETHOD(SetAutoMode) (THIS_ WORD AutoMode) PURE;
	STDMETHOD(GetNPControl) (THIS_ WORD *pNPControl) PURE;
	STDMETHOD(SetNPControl) (THIS_ WORD pNPControl) PURE;
	STDMETHOD(GetNPSlave) (THIS_ WORD *pNPSlave) PURE;
	STDMETHOD(SetNPSlave) (THIS_ WORD pNPSlave) PURE;
	STDMETHOD(SetTunerEvent) (void) PURE;
	STDMETHOD(GetDelayMode) (THIS_ WORD * pDelayMode) PURE;
	STDMETHOD(SetDelayMode) (THIS_ WORD DelayMode) PURE;
	STDMETHOD(GetRateControlMode) (THIS_ WORD * pRateControl) PURE;
	STDMETHOD(SetRateControlMode) (THIS_ WORD RateControl) PURE;
	STDMETHOD(GetCreateTSPinOnDemux) (THIS_ WORD * pbCreatePin) PURE;
	STDMETHOD(SetCreateTSPinOnDemux) (THIS_ WORD bCreatePin) PURE;
	STDMETHOD(GetReadOnly) (THIS_ WORD * pFileMode) PURE;
	STDMETHOD(GetBitRate) (THIS_ long *pRate) PURE;
	STDMETHOD(SetBitRate) (THIS_ long Rate) PURE;
	STDMETHOD(SetRegSettings) () PURE;
	STDMETHOD(GetRegSettings) () PURE;
	STDMETHOD(SetRegProgram) () PURE;
	STDMETHOD(ShowFilterProperties)()PURE;
	STDMETHOD(Refresh)()PURE;
};

