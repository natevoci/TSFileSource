/**
*  RegStore.ccp
*  Copyright (C) 2004-2006 bear
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

// RegStore.cpp: implementation of the CRegStore class.
//
//////////////////////////////////////////////////////////////////////

#include "RegStore.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CRegStore::CRegStore(LPCSTR lpSubKey)
{
	LONG resp = 0;
	DWORD action_result = 0;

	resp = RegCreateKeyEx(	HKEY_CURRENT_USER, //HKEY_LOCAL_MACHINE,
							lpSubKey,
							NULL,
							NULL,
							REG_OPTION_NON_VOLATILE,
							KEY_ALL_ACCESS,
							NULL,
							&rootkey,
							&action_result);

	removeOld();

}

void FixBool(BOOL &value)
{
	value = (value != 0);
}

BOOL CRegStore::getSettingsInfo(CSettingsStore *setStore)
{
	std::string keyName = "settings\\" + setStore->getName();

	LONG resp = 0;
	DWORD action_result = 0;
	HKEY settingsKey;

	resp = RegCreateKeyEx(	rootkey,
							keyName.c_str(),
							NULL,
							NULL,
							REG_OPTION_NON_VOLATILE,
							KEY_ALL_ACCESS,
							NULL,
							&settingsKey,
							&action_result);

	if(REG_CREATED_NEW_KEY == action_result)
	{
		RegCloseKey(settingsKey);

		// If we're creating the key then we'll save the default
		// values there so that they can be easily edited.
		setSettingsInfo(setStore);

		return FALSE;
	}

	DWORD datalen;
	DWORD type;

	if ((strcmp(setStore->getName().c_str(), "user")!=0) && (strcmp(setStore->getName().c_str(), "default")!=0))
	{
		int regSID = 0;
		datalen = sizeof(int);
		type = 0;

		resp = RegQueryValueEx(settingsKey, "ProgramSID", NULL, &type, (BYTE*)&regSID, &datalen);
		if(resp != ERROR_SUCCESS)
		{
			RegCloseKey(settingsKey);
			return FALSE;
		}
		setStore->setProgramSIDReg(regSID);
	}
	else
	{
		BOOL regAuto(TRUE);
		datalen = sizeof(BOOL);
		type = 0;

		resp = RegQueryValueEx(settingsKey, "enableAuto", NULL, &type, (BYTE*)&regAuto, &datalen);
		if(resp != ERROR_SUCCESS)
		{
			RegCloseKey(settingsKey);
			return FALSE;
		}
		FixBool(regAuto);
		setStore->setAutoModeReg(regAuto);

		BOOL regNPControl(FALSE);
		datalen = sizeof(BOOL);
		type = 0;

		resp = RegQueryValueEx(settingsKey, "enableNPControl", NULL, &type, (BYTE*)&regNPControl, &datalen);
		if(resp != ERROR_SUCCESS)
		{
			RegCloseKey(settingsKey);
			return FALSE;
		}
		FixBool(regNPControl);
		setStore->setNPControlReg(regNPControl);

		BOOL regNPSlave(FALSE);
		datalen = sizeof(BOOL);
		type = 0;

		resp = RegQueryValueEx(settingsKey, "enableNPSlave", NULL, &type, (BYTE*)&regNPSlave, &datalen);
		if(resp != ERROR_SUCCESS)
		{
			RegCloseKey(settingsKey);
			return FALSE;
		}
		FixBool(regNPSlave);
		setStore->setNPSlaveReg(regNPSlave);

		BOOL regMP2(TRUE);
		datalen = sizeof(BOOL);
		type = 0;

		resp = RegQueryValueEx(settingsKey, "enableMP2", NULL, &type, (BYTE*)&regMP2, &datalen);
		if(resp != ERROR_SUCCESS)
		{
			RegCloseKey(settingsKey);
			return FALSE;
		}
		FixBool(regMP2);
		setStore->setMP2ModeReg(regMP2);

		BOOL regFixedAR(FALSE);
		datalen = sizeof(BOOL);
		type = 0;

		resp = RegQueryValueEx(settingsKey, "enableFixedAR", NULL, &type, (BYTE*)&regFixedAR, &datalen);
		if(resp != ERROR_SUCCESS)
		{
			RegCloseKey(settingsKey);
			return FALSE;
		}
		FixBool(regFixedAR);
		setStore->setFixedAspectRatioReg(regFixedAR);

		BOOL regTSPin(FALSE);
		datalen = sizeof(BOOL);
		type = 0;

		resp = RegQueryValueEx(settingsKey, "enableTSPin", NULL, &type, (BYTE*)&regTSPin, &datalen);
		if(resp != ERROR_SUCCESS)
		{
			RegCloseKey(settingsKey);
			return FALSE;
		}
		FixBool(regTSPin);
		setStore->setCreateTSPinOnDemuxReg(regTSPin);

		BOOL regDelay(FALSE);
		datalen = sizeof(BOOL);
		type = 0;

		resp = RegQueryValueEx(settingsKey, "enableDelay", NULL, &type, (BYTE*)&regDelay, &datalen);
		if(resp != ERROR_SUCCESS)
		{
			RegCloseKey(settingsKey);
			return FALSE;
		}
		FixBool(regDelay);
		setStore->setDelayModeReg(regDelay);

		BOOL regShared(FALSE);
		datalen = sizeof(BOOL);
		type = 0;

		resp = RegQueryValueEx(settingsKey, "enableSharedMode", NULL, &type, (BYTE*)&regShared, &datalen);
		if(resp != ERROR_SUCCESS)
		{
			RegCloseKey(settingsKey);
			return FALSE;
		}
		FixBool(regShared);
		setStore->setSharedModeReg(regShared);

		BOOL regInject(FALSE);
		datalen = sizeof(BOOL);
		type = 0;

		resp = RegQueryValueEx(settingsKey, "enableInjectMode", NULL, &type, (BYTE*)&regInject, &datalen);
		if(resp != ERROR_SUCCESS)
		{
			RegCloseKey(settingsKey);
			return FALSE;
		}
		FixBool(regInject);
		setStore->setInjectModeReg(regInject);

		BOOL regRate(FALSE);
		datalen = sizeof(BOOL);
		type = 0;

		resp = RegQueryValueEx(settingsKey, "enableRateControl", NULL, &type, (BYTE*)&regRate, &datalen);
		if(resp != ERROR_SUCCESS)
		{
			RegCloseKey(settingsKey);
			return FALSE;
		}
		FixBool(regRate);
		setStore->setRateControlModeReg(regRate);

		int regSID = 0;
		datalen = sizeof(int);
		type = 0;

		resp = RegQueryValueEx(settingsKey, "ProgramSID", NULL, &type, (BYTE*)&regSID, &datalen);
		if(resp != ERROR_SUCCESS)
		{
			RegCloseKey(settingsKey);
			return FALSE;
		}
		setStore->setProgramSIDReg(regSID);

		BOOL regROT(FALSE);
		datalen = sizeof(BOOL);
		type = 0;

		resp = RegQueryValueEx(settingsKey, "enableROT", NULL, &type, (BYTE*)&regROT, &datalen);
		if(resp != ERROR_SUCCESS)
		{
			RegCloseKey(settingsKey);
			return FALSE;
		}
		FixBool(regROT);
		setStore->setROTModeReg(regROT);

		int regClock = 1; //TSFileSource clock
		datalen = sizeof(int);
		type = 0;

		resp = RegQueryValueEx(settingsKey, "clockType", NULL, &type, (BYTE*)&regClock, &datalen);
		if(resp != ERROR_SUCCESS)
		{
			RegCloseKey(settingsKey);
			return FALSE;
		}
		setStore->setClockModeReg(regClock);

		BOOL regTxtPin(FALSE);
		datalen = sizeof(BOOL);
		type = 0;

		resp = RegQueryValueEx(settingsKey, "enableTxtPin", NULL, &type, (BYTE*)&regTxtPin, &datalen);
		if(resp != ERROR_SUCCESS)
		{
			RegCloseKey(settingsKey);
			return FALSE;
		}
		FixBool(regTxtPin);
		setStore->setCreateTxtPinOnDemuxReg(regTxtPin);

		BOOL regSubPin(FALSE);
		datalen = sizeof(BOOL);
		type = 0;

		resp = RegQueryValueEx(settingsKey, "enableSubPin", NULL, &type, (BYTE*)&regSubPin, &datalen);
		if(resp != ERROR_SUCCESS)
		{
			RegCloseKey(settingsKey);
			return FALSE;
		}
		FixBool(regSubPin);
		setStore->setCreateSubPinOnDemuxReg(regSubPin);
	}
	
	BOOL regAC3(FALSE);//(TRUE)
	datalen = sizeof(BOOL);
	type = 0;

	resp = RegQueryValueEx(settingsKey, "enableAC3", NULL, &type, (BYTE*)&regAC3, &datalen);
	if(resp != ERROR_SUCCESS)
	{
		RegCloseKey(settingsKey);
		return FALSE;
	}
	FixBool(regAC3);
	setStore->setAC3ModeReg(regAC3);

	BOOL regAudio2(FALSE);
	datalen = sizeof(BOOL);
	type = 0;

	resp = RegQueryValueEx(settingsKey, "enableAudio2", NULL, &type, (BYTE*)&regAudio2, &datalen);
	if(resp != ERROR_SUCCESS)
	{
		RegCloseKey(settingsKey);
		return FALSE;
	}
	FixBool(regAudio2);
	setStore->setAudio2ModeReg(regAudio2);

	RegCloseKey(settingsKey);

	return true;
}

BOOL CRegStore::setSettingsInfo(CSettingsStore *setStore)
{
	std::string keyName = "settings\\" + setStore->getName();

	LONG resp = 0;
	DWORD action_result = 0;
	HKEY settingsKey;

	resp = RegCreateKeyEx(	rootkey,
							keyName.c_str(),
							NULL,
							NULL,
							REG_OPTION_NON_VOLATILE,
							KEY_ALL_ACCESS,
							NULL,
							&settingsKey,
							&action_result);


	if ((strcmp(setStore->getName().c_str(), "user")!=0) && (strcmp(setStore->getName().c_str(), "default")!=0))
	{
		int regSID = setStore->getProgramSIDReg();
		resp = RegSetValueEx(settingsKey, "ProgramSID", NULL, REG_DWORD, (BYTE*)&regSID, sizeof(int));

	}
	else
	{
		BOOL regAuto = setStore->getAutoModeReg();
		resp = RegSetValueEx(settingsKey, "enableAuto", NULL, REG_DWORD, (BYTE*)&regAuto, sizeof(DWORD));

		BOOL regNPControl = setStore->getNPControlReg();
		resp = RegSetValueEx(settingsKey, "enableNPControl", NULL, REG_DWORD, (BYTE*)&regNPControl, sizeof(DWORD));

		BOOL regNPSlave = setStore->getNPSlaveReg();
		resp = RegSetValueEx(settingsKey, "enableNPSlave", NULL, REG_DWORD, (BYTE*)&regNPSlave, sizeof(DWORD));

		BOOL regMP2 = setStore->getMP2ModeReg();
		resp = RegSetValueEx(settingsKey, "enableMP2", NULL, REG_DWORD, (BYTE*)&regMP2, sizeof(DWORD));

		BOOL regFixedAR = setStore->getFixedAspectRatioReg();
		resp = RegSetValueEx(settingsKey, "enableFixedAR", NULL, REG_DWORD, (BYTE*)&regFixedAR, sizeof(DWORD));

		BOOL regTSPin = setStore->getCreateTSPinOnDemuxReg();
		resp = RegSetValueEx(settingsKey, "enableTSPin", NULL, REG_DWORD, (BYTE*)&regTSPin, sizeof(DWORD));

		BOOL regDelay = setStore->getDelayModeReg();
		resp = RegSetValueEx(settingsKey, "enableDelay", NULL, REG_DWORD, (BYTE*)&regDelay, sizeof(DWORD));

		BOOL regShared = setStore->getSharedModeReg();
		resp = RegSetValueEx(settingsKey, "enableSharedMode", NULL, REG_DWORD, (BYTE*)&regShared, sizeof(DWORD));

		BOOL regInject = setStore->getInjectModeReg();
		resp = RegSetValueEx(settingsKey, "enableInjectMode", NULL, REG_DWORD, (BYTE*)&regInject, sizeof(DWORD));

		BOOL regRate = setStore->getRateControlModeReg();
		resp = RegSetValueEx(settingsKey, "enableRateControl", NULL, REG_DWORD, (BYTE*)&regRate, sizeof(DWORD));

		int regSID = setStore->getProgramSIDReg();
		resp = RegSetValueEx(settingsKey, "ProgramSID", NULL, REG_DWORD, (BYTE*)&regSID, sizeof(int));

		BOOL regROT = setStore->getROTModeReg();
		resp = RegSetValueEx(settingsKey, "enableROT", NULL, REG_DWORD, (BYTE*)&regROT, sizeof(DWORD));

		int regClock = setStore->getClockModeReg();
		resp = RegSetValueEx(settingsKey, "clockType", NULL, REG_DWORD, (BYTE*)&regClock, sizeof(int));

		BOOL regTxtPin = setStore->getCreateTxtPinOnDemuxReg();
		resp = RegSetValueEx(settingsKey, "enableTxtPin", NULL, REG_DWORD, (BYTE*)&regTxtPin, sizeof(DWORD));

		BOOL regSubPin = setStore->getCreateSubPinOnDemuxReg();
		resp = RegSetValueEx(settingsKey, "enableSubPin", NULL, REG_DWORD, (BYTE*)&regSubPin, sizeof(DWORD));
	}
	BOOL regAudio2 = setStore->getAudio2ModeReg();
	resp = RegSetValueEx(settingsKey, "enableAudio2", NULL, REG_DWORD, (BYTE*)&regAudio2, sizeof(DWORD));

	BOOL regAC3 = setStore->getAC3ModeReg();
	resp = RegSetValueEx(settingsKey, "enableAC3", NULL, REG_DWORD, (BYTE*)&regAC3, sizeof(DWORD));

	RegCloseKey(settingsKey);

	return true;
}

BOOL CRegStore::setInt(char *name, int val)
{
	LONG result = RegSetValueEx(rootkey, name, NULL, REG_DWORD, (BYTE*)&val, 4);
  
	return TRUE;
}

int CRegStore::getInt(char *name, int def)
{
	int val = 0;
	DWORD datalen = sizeof(int);
	DWORD type = 0;

	LONG resp = RegQueryValueEx(rootkey, name, NULL, &type, (BYTE*)&val, &datalen);

	if(resp == 2)
	{
		val = def;
		RegSetValueEx(rootkey, name, NULL, REG_DWORD, (BYTE*)&val, 4);
	}

	return val;
}

int CRegStore::getString(char *name, char *buff, int len)
{
	DWORD datalen = len-1;
	DWORD type = 0;

	LONG resp = RegQueryValueEx(rootkey, name, NULL, &type, (BYTE*)buff, &datalen);

	if(resp == 2)
	{
		char *val = 0;
		RegSetValueEx(rootkey, name, NULL, REG_SZ, (BYTE*)&val, 1);
	}

	return datalen;
}

CRegStore::~CRegStore()
{
	RegCloseKey(rootkey);
}

BOOL CRegStore::removeOld()
{
	LONG resp = 0;
	DWORD action_result = 0;
	HKEY settingsKey;

	resp = RegCreateKeyEx(	rootkey,
							"settings",
							NULL,
							NULL,
							REG_OPTION_NON_VOLATILE,
							KEY_ALL_ACCESS,
							NULL,
							&settingsKey,
							&action_result);

	if(resp != ERROR_SUCCESS)
	{
		return false;
	}

	std::vector <LPTSTR> toRemove;

	TCHAR buff[256];
	int index = 0;

	while(RegEnumKey(settingsKey, index++, buff, 256) == ERROR_SUCCESS)
	{
		HKEY settingsData;
		resp = RegCreateKeyEx(	settingsKey,
								buff,
								NULL,
								NULL,
								REG_OPTION_NON_VOLATILE,
								KEY_ALL_ACCESS,
								NULL,
								&settingsData,
								&action_result);

		if(resp == ERROR_SUCCESS)
		{
			DWORD datalen = sizeof(__int64);
			DWORD type = 0;
			__int64 lastUsed = 0;
			resp = RegQueryValueEx(settingsData, "lastUsed", NULL, &type, (BYTE*)&lastUsed, &datalen);
			if(resp == ERROR_SUCCESS)
			{
				__int64 now = time(NULL);
				if(lastUsed < (now - (3600 * 24 * 30)))
				{
					LPTSTR pStr = new TCHAR[256];
					lstrcpy(pStr, buff);
                    toRemove.push_back(pStr);
				}
			}

			RegCloseKey(settingsData);
		}
	}

	//
	// Now remove old items
	//
	LPTSTR pName = "";
	for(int x = 0; x < (int)toRemove.size(); x++)
	{
		pName = toRemove.at(x);
		RegDeleteKey(settingsKey, pName);
		delete[] pName;
	}

	RegCloseKey(settingsKey);

	return TRUE;
}
