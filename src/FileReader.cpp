/**
*  FileReader.cpp
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
*  nate can be reached on the forums at
*    http://forums.dvbowners.com/
*/

#include <streams.h>
#include "FileReader.h"

FileReader::FileReader() :
	m_hFile(INVALID_HANDLE_VALUE),
	m_pFileName(0),
	m_bReadOnly(FALSE),
	m_bDelay(FALSE)
{

}

FileReader::~FileReader()
{
	CloseFile();
	if (m_pFileName)
		delete m_pFileName;
}

HRESULT FileReader::GetFileName(LPOLESTR *lpszFileName)
{
	*lpszFileName = m_pFileName;
	return S_OK;
}

HRESULT FileReader::SetFileName(LPCOLESTR pszFileName)
{
	// Is this a valid filename supplied
	CheckPointer(pszFileName,E_POINTER);

	if(wcslen(pszFileName) > MAX_PATH)
		return ERROR_FILENAME_EXCED_RANGE;

	// Take a copy of the filename

	if (m_pFileName)
	{
		delete[] m_pFileName;
		m_pFileName = NULL;
	}
	m_pFileName = new WCHAR[1+lstrlenW(pszFileName)];
	if (m_pFileName == NULL)
		return E_OUTOFMEMORY;

	lstrcpyW(m_pFileName,pszFileName);

	return S_OK;
}

//
// OpenFile
//
// Opens the file ready for streaming
//
HRESULT FileReader::OpenFile()
{
	TCHAR *pFileName = NULL;

	// Is the file already opened
	if (m_hFile != INVALID_HANDLE_VALUE) {

		return NOERROR;
	}

	// Has a filename been set yet
	if (m_pFileName == NULL) {
		return ERROR_INVALID_NAME;
	}

	// Convert the UNICODE filename if necessary

#if defined(WIN32) && !defined(UNICODE)
	char convert[MAX_PATH];

	if(!WideCharToMultiByte(CP_ACP,0,m_pFileName,-1,convert,MAX_PATH,0,0))
		return ERROR_INVALID_NAME;

	pFileName = convert;
#else
	pFileName = m_pFileName;
#endif

	m_bReadOnly = FALSE;

	// Try to open the file
	m_hFile = CreateFile((LPCTSTR) pFileName,   // The filename
						 GENERIC_READ,          // File access
						 FILE_SHARE_READ,       // Share access
						 NULL,                  // Security
						 OPEN_EXISTING,         // Open flags
						 (DWORD) 0,             // More flags
						 NULL);                 // Template

	if (m_hFile == INVALID_HANDLE_VALUE) {

		//Test incase file is being recorded to
		m_hFile = CreateFile((LPCTSTR) pFileName,		// The filename
							GENERIC_READ,				// File access
							FILE_SHARE_READ |
							FILE_SHARE_WRITE,			// Share access
							NULL,						// Security
							OPEN_EXISTING,				// Open flags
							FILE_ATTRIBUTE_NORMAL |
							FILE_FLAG_RANDOM_ACCESS, // More flags
							NULL);						// Template

		if (m_hFile == INVALID_HANDLE_VALUE)
		{
			DWORD dwErr = GetLastError();
			return HRESULT_FROM_WIN32(dwErr);
		}

		m_bReadOnly = TRUE;
	}

	return S_OK;

} // Open

//
// CloseFile
//
// Closes any dump file we have opened
//
HRESULT FileReader::CloseFile()
{
	// Must lock this section to prevent problems related to
	// closing the file while still receiving data in Receive()


	if (m_hFile == INVALID_HANDLE_VALUE) {

		return S_OK;
	}

	CloseHandle(m_hFile);
	m_hFile = INVALID_HANDLE_VALUE; // Invalidate the file

	return NOERROR;

} // CloseFile

BOOL FileReader::IsFileInvalid()
{
	return (m_hFile == INVALID_HANDLE_VALUE);
}

HRESULT FileReader::GetFileSize(__int64 *lpllsize)
{
	DWORD dwSizeLow;
	DWORD dwSizeHigh;

	dwSizeLow = ::GetFileSize(m_hFile, &dwSizeHigh);
	if ((dwSizeLow == 0xFFFFFFFF) && (GetLastError() != NO_ERROR ))
	{
		return E_FAIL;
	}

	LARGE_INTEGER li;
	li.LowPart = dwSizeLow;
	li.HighPart = dwSizeHigh;

	*lpllsize = li.QuadPart;
	return S_OK;
}

DWORD FileReader::SetFilePointer(__int64 llDistanceToMove, DWORD dwMoveMethod)
{
	LARGE_INTEGER li;
	li.QuadPart = llDistanceToMove;
	return ::SetFilePointer(m_hFile, li.LowPart, &li.HighPart, dwMoveMethod);
}

HRESULT FileReader::Read(PBYTE pbData, LONG lDataLength)
{
	DWORD dwRead = 0;
	HRESULT hr;

	// If the file has already been closed, don't continue
	if (m_hFile == INVALID_HANDLE_VALUE)
		return S_FALSE;

	//Get File Position
	LARGE_INTEGER li;
	li.QuadPart = 0;
	li.LowPart = ::SetFilePointer(m_hFile, 0, &li.HighPart, FILE_CURRENT);
	__int64 m_filecurrent = li.QuadPart;

	//Read file data into buffer
	hr = ReadFile(m_hFile, (PVOID)pbData, (DWORD)lDataLength, &dwRead, NULL);
	if (FAILED(hr) || dwRead < (ULONG)lDataLength)
	{
//*********************************************************************************************
//Live File Additions
		if (m_bReadOnly)
		{
			if (dwRead < (ULONG)lDataLength)
			{
				if (m_bDelay)
					Sleep(10000);
				else
					Sleep(100);

				LARGE_INTEGER li;
				li.QuadPart = (__int64)(m_filecurrent);
				::SetFilePointer(m_hFile, li.LowPart, &li.HighPart, FILE_BEGIN);
				hr = ReadFile(m_hFile, (PVOID)pbData, (DWORD)lDataLength, &dwRead, NULL);
			}
			if (FAILED(hr) || dwRead == 0)
			{
				return S_FALSE;
			}
			return S_OK;
		}

//*********************************************************************************************
		return S_FALSE;
	}
	return S_OK;
}

HRESULT FileReader::Read(PBYTE pbData, LONG lDataLength, __int64 llDistanceToMove, DWORD dwMoveMethod)
{
	//If end method then we want llDistanceToMove to be the end of the buffer that we read.
	if (dwMoveMethod == FILE_END)
		llDistanceToMove = 0 - llDistanceToMove - lDataLength;

	SetFilePointer(llDistanceToMove, dwMoveMethod);

	return Read(pbData, lDataLength);
}

HRESULT FileReader::get_ReadOnly(WORD *ReadOnly)
{
	*ReadOnly = m_bReadOnly;
	return S_OK;
}

HRESULT FileReader::get_DelayMode(WORD *DelayMode)
{
	*DelayMode = m_bDelay;
	return S_OK;
}

HRESULT FileReader::set_DelayMode(WORD DelayMode)
{
	m_bDelay = DelayMode;
	return S_OK;
}

