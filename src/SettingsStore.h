/**
*  SettingsStore.h
*  Copyright (C) 2004-2005 bear
*  Copyright (C) 2003  Shaun Faulds
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

#pragma once

#include <windows.h>
#include <time.h>
#include <string>

class CSettingsStore
{
public:
	CSettingsStore(void);
	~CSettingsStore(void);

	__int64 getLastUsed();
	__int64 getStartPosition();
	std::string getName();

	void setLastUsed(__int64 time);
	void setStartPosition(__int64 start);
	void setName(std::string newName);

	
	BOOL getAutoModeReg();
	BOOL getMP2ModeReg();
	BOOL getAC3ModeReg();
	BOOL getCreateTSPinOnDemuxReg();
	BOOL getDelayModeReg();
	BOOL getRateControlModeReg();
	BOOL getAudio2ModeReg();
	int  getProgramSIDReg();
	void setAutoModeReg(BOOL bAuto);
	void setMP2ModeReg(BOOL bMP2);
	void setAC3ModeReg(BOOL bAC3);
	void setCreateTSPinOnDemuxReg(BOOL bTSPin);
	void setDelayModeReg(BOOL bDelay);
	void setRateControlModeReg(BOOL bRate);
	void setAudio2ModeReg(BOOL bAudio2);
	void setProgramSIDReg(int bSID);

private:
	__int64 lastUsed;
	__int64 startAT;
	std::string name;


	BOOL 	autoMode;
	BOOL	mp2Mode;
	BOOL	ac3Mode;
	BOOL	tsPinMode;
	BOOL	delayMode;
	BOOL	rateMode;
	BOOL	audio2Mode;
	int		programSID;
};
