/**
 *	LogLogFileWriter.cpp
 *	Copyright (C) 2004 Nate
 *
 *	This file is part of DigitalWatch, a free DTV watching and recording
 *	program for the VisionPlus DVB-T.
 *
 *	DigitalWatch is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	DigitalWatch is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with DigitalWatch; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "LogFileWriter.h"
#include "GlobalFunctions.h"

//////////////////////////////////////////////////////////////////////
// LogFileWriter
//////////////////////////////////////////////////////////////////////

LogFileWriter::LogFileWriter()
{
	m_hFile = INVALID_HANDLE_VALUE;
}

LogFileWriter::~LogFileWriter()
{
	if (m_hFile != INVALID_HANDLE_VALUE)
		Close();
}

BOOL LogFileWriter::Open(LPCWSTR filename, BOOL append)
{
	USES_CONVERSION;

	LPWSTR pCurr = (LPWSTR)filename;

	switch (pCurr[0])
	{
	case 0:
		return FALSE;
	case '\\':
		switch (pCurr[1])
		{
		case '\\':
			pCurr += 2;
			break;
		default:
			return FALSE;
		}
		break;
	default:
		switch (pCurr[1])
		{
		case ':':
			switch (pCurr[2])
			{
			case 0:
				return FALSE;
			case '\\':
				pCurr += 3;
				break;
			default:
				return FALSE;
			}
			break;
		default:
			return FALSE;
		}
	}

	//Make sure the directory exists.
	LPWSTR pBackslash;
	while (pBackslash = wcschr(pCurr, '\\'))
	{
		LPWSTR pPartial = NULL;
		strCopy(pPartial, filename, pBackslash - filename);

		HANDLE hDir;
		hDir = CreateFile(W2T(pPartial), (DWORD) GENERIC_READ, (DWORD) (FILE_SHARE_READ|FILE_SHARE_DELETE), NULL, (DWORD) OPEN_EXISTING, (DWORD) FILE_FLAG_BACKUP_SEMANTICS, NULL);
		if (hDir == INVALID_HANDLE_VALUE)
		{
			if (!CreateDirectory(W2T(pPartial), NULL))
			{
				delete[] pPartial;
				return FALSE;
			}
			hDir = CreateFile(W2T(pPartial), (DWORD) GENERIC_READ, (DWORD) (FILE_SHARE_READ|FILE_SHARE_DELETE), NULL, (DWORD) OPEN_EXISTING, (DWORD) FILE_FLAG_BACKUP_SEMANTICS, NULL);
			if (hDir == INVALID_HANDLE_VALUE)
			{
				delete[] pPartial;
				return FALSE;
			}
		}
		CloseHandle(hDir);
		delete[] pPartial;


		pCurr = pBackslash + 1;
	}

	//Open the file for writing
	if (append)
	{
		m_hFile = CreateFile(W2T(filename), (DWORD) GENERIC_WRITE, (DWORD) NULL, NULL, (DWORD) OPEN_ALWAYS, (DWORD) FILE_ATTRIBUTE_NORMAL, NULL);
		LARGE_INTEGER li;
		li.QuadPart = 0;
		::SetFilePointer(m_hFile, li.LowPart, &li.HighPart, FILE_END);
	}
	else
	{
		m_hFile = CreateFile(W2T(filename), (DWORD) GENERIC_WRITE, (DWORD) NULL, NULL, (DWORD) CREATE_ALWAYS, (DWORD) FILE_ATTRIBUTE_NORMAL, NULL);
	}
	return (m_hFile != INVALID_HANDLE_VALUE);
}

void LogFileWriter::Close()
{
	CloseHandle(m_hFile);
	m_hFile = INVALID_HANDLE_VALUE;
}


LogFileWriter& LogFileWriter::operator<< (const int& val)
{
	if (m_hFile == INVALID_HANDLE_VALUE)
		return *this;

	char str[25];
	sprintf((char*)&str, "%i", val);
	DWORD written = 0;
	WriteFile(m_hFile, str, strlen(str), &written, NULL);
	return *this;
}
LogFileWriter& LogFileWriter::operator<< (const double& val)
{
	if (m_hFile == INVALID_HANDLE_VALUE)
		return *this;

	char str[25];
	sprintf((char*)str, "%f", val);
	DWORD written = 0;
	WriteFile(m_hFile, str, strlen(str), &written, NULL);
	return *this;
}
LogFileWriter& LogFileWriter::operator<< (const __int64& val)
{
	if (m_hFile == INVALID_HANDLE_VALUE)
		return *this;

	char str[25];
	sprintf((char*)str, "%i", val);
	DWORD written = 0;
	WriteFile(m_hFile, str, strlen(str), &written, NULL);
	return *this;
}

//Characters
LogFileWriter& LogFileWriter::operator<< (const char& val)
{
	if (m_hFile == INVALID_HANDLE_VALUE)
		return *this;

	DWORD written = 0;
	WriteFile(m_hFile, &val, 1, &written, NULL);
	return *this;
}
LogFileWriter& LogFileWriter::operator<< (const wchar_t& val)
{
	if (m_hFile == INVALID_HANDLE_VALUE)
		return *this;

	DWORD written = 0;
	WriteFile(m_hFile, &val, 1, &written, NULL);
	return *this;
}

//Strings
LogFileWriter& LogFileWriter::operator<< (const LPSTR& val)
{
	if (m_hFile == INVALID_HANDLE_VALUE)
		return *this;

	DWORD written = 0;
	WriteFile(m_hFile, val, strlen(val), &written, NULL);
	return *this;
}
LogFileWriter& LogFileWriter::operator<< (const LPWSTR& val)
{
	USES_CONVERSION;

	if (m_hFile == INVALID_HANDLE_VALUE)
		return *this;

	DWORD written = 0;
	WriteFile(m_hFile, W2A(val), wcslen(val), &written, NULL);
	return *this;
}
LogFileWriter& LogFileWriter::operator<< (const LPCSTR& val)
{
	if (m_hFile == INVALID_HANDLE_VALUE)
		return *this;

	DWORD written = 0;
	WriteFile(m_hFile, val, strlen(val), &written, NULL);
	return *this;
}
LogFileWriter& LogFileWriter::operator<< (const LPCWSTR& val)
{
	USES_CONVERSION;

	if (m_hFile == INVALID_HANDLE_VALUE)
		return *this;

	DWORD written = 0;
	WriteFile(m_hFile, W2A(val), wcslen(val), &written, NULL);
	return *this;
}
LogFileWriter& LogFileWriter::operator<< (const LogFileWriterEOL& val)
{
	if (m_hFile == INVALID_HANDLE_VALUE)
		return *this;

	DWORD written = 0;
	WriteFile(m_hFile, "\r\n", 2, &written, NULL);
	return *this;
}

