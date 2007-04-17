/*
   FileWriter
   Copyright (C) 2003  Shaun Faulds

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

// RegStore.cpp: implementation of the CRegStore class.
//
//////////////////////////////////////////////////////////////////////

#include "RegStore.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CRegStore::CRegStore(char *base)
{
	LONG resp = 0;
	DWORD action_result = 0;

	resp = RegCreateKeyEx(	HKEY_CURRENT_USER, //HKEY_LOCAL_MACHINE, 
							base,
							NULL,
							NULL,
							REG_OPTION_NON_VOLATILE,
							KEY_ALL_ACCESS,
							NULL,
							&rootkey,
							&action_result);

}



BOOL CRegStore::setInt(char *name, int val)
{
	LONG result = RegSetValueEx(rootkey, name, NULL, REG_DWORD, (BYTE*)&val, 4);
  
	return TRUE;
}

int CRegStore::getInt(char *name, int def)
{
	int val = 0;
	DWORD datalen = 4;
	DWORD type = 0;

	LONG resp = RegQueryValueEx(rootkey, name, NULL, &type, (BYTE*)&val, &datalen);

	if(resp == 2)
	{
		val = def;
		RegSetValueEx(rootkey, name, NULL, REG_DWORD, (BYTE*)&val, 4);
	}

	return val;
}

CRegStore::~CRegStore()
{
	RegCloseKey(rootkey);
}

