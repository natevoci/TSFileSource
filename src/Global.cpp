/**
*  Global.cpp
*  Copyright (C) 2007 nate
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

#include "Globals.h"
#include <shlobj.h>

//static char logbuffer[2000]; 

void LogDebug(const char *fmt, ...) 
{
	va_list ap;
	va_start(ap,fmt);
	
	int tmp;
	va_start(ap,fmt);
	tmp=vsprintf(logbuffer, fmt, ap);
	va_end(ap); 
	
	TCHAR folder[MAX_PATH];
	TCHAR fileName[MAX_PATH];
	::SHGetSpecialFolderPath(NULL,folder,CSIDL_COMMON_APPDATA,FALSE);
	sprintf(fileName,"%s\\MediaPortal TV Server\\log\\TSFileSource.Log",folder);
	FILE* fp = fopen(fileName,"a+");
	if (fp!=NULL)
	{
		SYSTEMTIME systemTime;
		GetLocalTime(&systemTime);
		fprintf(fp,"%02.2d-%02.2d-%04.4d %02.2d:%02.2d:%02.2d %s\n",
			systemTime.wDay, systemTime.wMonth, systemTime.wYear,
			systemTime.wHour,systemTime.wMinute,systemTime.wSecond,
			logbuffer);
		fclose(fp);
		//::OutputDebugStringA(logbuffer);::OutputDebugStringA("\n");
	}
};

void DebugString(const char *fmt, ...) 
{
	va_list ap;
	va_start(ap,fmt);
	
	int tmp;
	va_start(ap,fmt);
	tmp=vsprintf(logbuffer, fmt, ap);
	va_end(ap); 
	
	::OutputDebugStringA(logbuffer);
};

