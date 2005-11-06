/**
*  MultiFileWriter.cpp
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
#include "MultiFileWriter.h"
#include <atlbase.h>

MultiFileWriter::MultiFileWriter() :
	m_hTSBufferFile(INVALID_HANDLE_VALUE),
	m_pTSBufferFileName(NULL),
	m_pCurrentTSFile(NULL),
	m_filesAdded(0),
	m_filesRemoved(0),
	m_currentFilenameId(0),
	m_minTSFiles(5),
	m_maxTSFiles(50),
	m_maxTSFileSize(200000000),	//200Mb
	m_chunkReserve(20000000) //20Mb
{
	m_pCurrentTSFile = new FileWriter();
	m_pCurrentTSFile->SetChunkReserve(TRUE, m_chunkReserve, m_maxTSFileSize);
}

MultiFileWriter::~MultiFileWriter()
{
	CloseFile();
	if (m_pTSBufferFileName)
		delete m_pTSBufferFileName;
}

HRESULT MultiFileWriter::GetFileName(LPOLESTR *lpszFileName)
{
	*lpszFileName = m_pTSBufferFileName;
	return S_OK;
}

HRESULT MultiFileWriter::OpenFile(LPCWSTR pszFileName)
{
	USES_CONVERSION;

	// Is the file already opened
	if (m_hTSBufferFile != INVALID_HANDLE_VALUE)
	{
		return E_FAIL;
	}

	// Is this a valid filename supplied
	CheckPointer(pszFileName,E_POINTER);

	if(wcslen(pszFileName) > MAX_PATH)
		return ERROR_FILENAME_EXCED_RANGE;

	// Take a copy of the filename
	if (m_pTSBufferFileName)
	{
		delete[] m_pTSBufferFileName;
		m_pTSBufferFileName = NULL;
	}
	m_pTSBufferFileName = new WCHAR[1+lstrlenW(pszFileName)];
	if (m_pTSBufferFileName == NULL)
		return E_OUTOFMEMORY;
	lstrcpyW(m_pTSBufferFileName, pszFileName);
	
	TCHAR *pFileName = NULL;

	// Try to open the file
	m_hTSBufferFile = CreateFile(W2T(m_pTSBufferFileName),  // The filename
								 GENERIC_WRITE,             // File access
								 FILE_SHARE_READ,           // Share access
								 NULL,                      // Security
								 CREATE_ALWAYS,             // Open flags
								 (DWORD) 0,                 // More flags
								 NULL);                     // Template

	if (m_hTSBufferFile == INVALID_HANDLE_VALUE)
	{
        DWORD dwErr = GetLastError();
        return HRESULT_FROM_WIN32(dwErr);
	}

	return S_OK;

}

//
// CloseFile
//
HRESULT MultiFileWriter::CloseFile()
{
	CAutoLock lock(&m_Lock);

	if (m_hTSBufferFile == INVALID_HANDLE_VALUE)
	{
		return S_OK;
	}

	CloseHandle(m_hTSBufferFile);
	m_hTSBufferFile = INVALID_HANDLE_VALUE;

	m_pCurrentTSFile->CloseFile();

	CleanupFiles();

	return S_OK;
}

HRESULT MultiFileWriter::GetFileSize(__int64 *lpllsize)
{
	*lpllsize = 0;
	return S_OK;
}

HRESULT MultiFileWriter::Write(PBYTE pbData, ULONG lDataLength)
{
	HRESULT hr;

	CheckPointer(pbData,E_POINTER);
	if (lDataLength == 0)
		return S_OK;

	// If the file has already been closed, don't continue
	if (m_hTSBufferFile == INVALID_HANDLE_VALUE)
		return S_FALSE;

	if (m_pCurrentTSFile->IsFileInvalid())
	{
		::OutputDebugString(TEXT("Creating first file\n"));
		if FAILED(hr = PrepareTSFile())
			return hr;
	}

	//Get File Position
	__int64 filePosition = m_pCurrentTSFile->GetFilePointer();

	// See if we will need to create more ts files.
	if (filePosition + lDataLength > m_maxTSFileSize)
	{
		__int64 dataToWrite = m_maxTSFileSize - filePosition;

		// Write some data to the current file if it's not full
		if (dataToWrite > 0)
		{
			m_pCurrentTSFile->Write(pbData, dataToWrite);
		}

		// Try to create a new file
		if FAILED(hr = PrepareTSFile())
		{
			// Buffer is probably full and the oldest file is still locked.
			// We'll just start dropping data
			return hr;
		}

		// Try writing the remaining data now that a new file has been created.
		pbData += dataToWrite;
		lDataLength -= dataToWrite;
		return Write(pbData, lDataLength);
	}
	else
	{
		m_pCurrentTSFile->Write(pbData, lDataLength);
	}

	WriteTSBufferFile();

	return S_OK;
}

HRESULT MultiFileWriter::PrepareTSFile()
{
	USES_CONVERSION;
	HRESULT hr;

	::OutputDebugString(TEXT("PrepareTSFile()\n"));

	// Make sure the old file is closed
	m_pCurrentTSFile->CloseFile();


	//TODO: disk space stuff
	/*
	if (m_diskSpaceLimit > 0)
	{
		diskSpaceAvailable = WhateverFunctionDoesThat();
		if (diskSpaceAvailable < m_diskSpaceLimit)
		{
			hr = ReuseTSFile();
		}
		else
		{
			hr = CreateNewTSFile();
		}
	}
	else */
	{
		if (m_tsFileNames.size() >= m_minTSFiles) 
		{
			if FAILED(hr = ReuseTSFile())
			{
				if (m_tsFileNames.size() < m_maxTSFiles)
				{
					if (hr != 0x80070020) // ERROR_SHARING_VIOLATION
						::OutputDebugString(TEXT("Failed to reopen old file. Unexpected reason. Trying to create a new file.\n"));

					hr = CreateNewTSFile();
				}
				else
				{
					if (hr != 0x80070020) // ERROR_SHARING_VIOLATION
						::OutputDebugString(TEXT("Failed to reopen old file. Unexpected reason. Dropping data!\n"));
					else
						::OutputDebugString(TEXT("Failed to reopen old file. It's currently in use. Dropping data!\n"));

					Sleep(500);
				}
			}
		}	
		else
		{
			hr = CreateNewTSFile();
		}
	}

	return hr;
}

HRESULT MultiFileWriter::CreateNewTSFile()
{
	USES_CONVERSION;
	HRESULT hr;

	LPWSTR pFilename = new wchar_t[MAX_PATH];
	WIN32_FIND_DATA findData;
	HANDLE handleFound = INVALID_HANDLE_VALUE;

	while (TRUE)
	{
		// Create new filename
		m_currentFilenameId++;
		swprintf(pFilename, L"%s%i.ts", m_pTSBufferFileName, m_currentFilenameId);

		// Check if file already exists
		handleFound = FindFirstFile(W2T(pFilename), &findData);
		if (handleFound == INVALID_HANDLE_VALUE)
			break;

		::OutputDebugString(TEXT("Newly generated filename already exists.\n"));

		// If it exists we loop and try the next number
		FindClose(handleFound);
	}
	
	if FAILED(hr = m_pCurrentTSFile->SetFileName(pFilename))
	{
		::OutputDebugString(TEXT("Failed to set filename for new file.\n"));
		delete pFilename;
		return hr;
	}

	if FAILED(hr = m_pCurrentTSFile->OpenFile())
	{
		::OutputDebugString(TEXT("Failed to open new file\n"));
		delete pFilename;
		return hr;
	}

	m_tsFileNames.push_back(pFilename);
	m_filesAdded++;

	wchar_t msg[MAX_PATH];
	swprintf((LPWSTR)&msg, L"New file created : %s\n", pFilename);
	::OutputDebugString(W2T((LPWSTR)&msg));

	return S_OK;
}

HRESULT MultiFileWriter::ReuseTSFile()
{
	USES_CONVERSION;
	HRESULT hr;

	LPWSTR pFilename = m_tsFileNames.at(0);

	if FAILED(hr = m_pCurrentTSFile->SetFileName(pFilename))
	{
		::OutputDebugString(TEXT("Failed to set filename to reuse old file\n"));
		return hr;
	}

	if FAILED(hr = m_pCurrentTSFile->OpenFile())
	{
		return hr;
	}

	// if stuff worked then move the filename to the end of the files list
	m_tsFileNames.erase(m_tsFileNames.begin());
	m_filesRemoved++;

	m_tsFileNames.push_back(pFilename);
	m_filesAdded++;

	wchar_t msg[MAX_PATH];
	swprintf((LPWSTR)&msg, L"Old file reused : %s\n", pFilename);
	::OutputDebugString(W2T((LPWSTR)&msg));

	return S_OK;
}


HRESULT MultiFileWriter::WriteTSBufferFile()
{
	LARGE_INTEGER li;
	DWORD written = 0;

	// Move to the start of the file
	li.QuadPart = 0;
	SetFilePointer(m_hTSBufferFile, li.LowPart, &li.HighPart, FILE_BEGIN);

	// Write current position of most recent file.
	__int64 currentPointer = m_pCurrentTSFile->GetFilePointer();
	WriteFile(m_hTSBufferFile, &currentPointer, sizeof(currentPointer), &written, NULL);

	// Write filesAdded and filesRemoved values
	WriteFile(m_hTSBufferFile, &m_filesAdded, sizeof(m_filesAdded), &written, NULL);
	WriteFile(m_hTSBufferFile, &m_filesRemoved, sizeof(m_filesRemoved), &written, NULL);

	// Write out all the filenames (null terminated)
	std::vector<LPWSTR>::iterator it = m_tsFileNames.begin();
	for ( ; it < m_tsFileNames.end() ; it++ )
	{
		LPWSTR pFilename = *it;
		long length = wcslen(pFilename)+1;
		length *= sizeof(wchar_t);
		WriteFile(m_hTSBufferFile, pFilename, length, &written, NULL);
	}

	// Finish up with a unicode null character in case we want to put stuff after this in the future.
	wchar_t temp = 0;
	WriteFile(m_hTSBufferFile, &temp, sizeof(temp), &written, NULL);

	return S_OK;
}

HRESULT MultiFileWriter::CleanupFiles()
{
	USES_CONVERSION;

	// Check if .tsbuffer file is being read by something.
	if (IsFileLocked(m_pTSBufferFileName) == TRUE)
		return S_OK;

	std::vector<LPWSTR>::iterator it;
	for (it = m_tsFileNames.begin() ; it < m_tsFileNames.end() ; it++ )
	{
		if (IsFileLocked(*it) == TRUE)
		{
			// If any of the files are being read then we won't
			// delete any so that the full buffer stays intact.
			wchar_t msg[MAX_PATH];
			swprintf((LPWSTR)&msg, L"CleanupFiles: A file is still locked : %s\n", *it);
			::OutputDebugString(W2T((LPWSTR)&msg));
			return S_OK;
		}
	}

	// Now we know we can delete all the files.

	for (it = m_tsFileNames.begin() ; it < m_tsFileNames.end() ; it++ )
	{
		if (DeleteFile(W2T(*it)))
		{
			wchar_t msg[MAX_PATH];
			swprintf((LPWSTR)&msg, L"Failed to delete file %s : 0x%x\n", *it, GetLastError());
			::OutputDebugString(W2T((LPWSTR)&msg));
		}
		delete[] *it;
	}
	m_tsFileNames.clear();

	if (DeleteFile(W2T(m_pTSBufferFileName)))
	{
		wchar_t msg[MAX_PATH];
		swprintf((LPWSTR)&msg, L"Failed to delete tsbuffer file : 0x%x\n", GetLastError());
		::OutputDebugString(W2T((LPWSTR)&msg));
	}
	return S_OK;
}

BOOL MultiFileWriter::IsFileLocked(LPWSTR pFilename)
{
	USES_CONVERSION;

	HANDLE hFile;
	hFile = CreateFile(W2T(pFilename),        // The filename
					   GENERIC_READ,          // File access
					   NULL,                  // Share access
					   NULL,                  // Security
					   OPEN_EXISTING,         // Open flags
					   (DWORD) 0,             // More flags
					   NULL);                 // Template

	if (hFile == INVALID_HANDLE_VALUE)
		return TRUE;

	CloseHandle(hFile);
	return FALSE;
}
