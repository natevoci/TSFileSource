/**
*  MultiFileReader.cpp
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
#include "MultiFileReader.h"
#include <atlbase.h>

MultiFileReader::MultiFileReader()
{
	m_startPosition = 0;
	m_endPosition = 0;
	m_currentPosition = 0;
	m_filesAdded = 0;
	m_filesRemoved = 0;
	m_TSFileId = 0;
}

MultiFileReader::~MultiFileReader()
{
	//CloseFile called by ~FileReader
}

FileReader* MultiFileReader::CreateFileReader()
{
	return (FileReader *)new MultiFileReader();
}

HRESULT MultiFileReader::GetFileName(LPOLESTR *lpszFileName)
{
	CheckPointer(lpszFileName,E_POINTER);
	return m_TSBufferFile.GetFileName(lpszFileName);
}

HRESULT MultiFileReader::SetFileName(LPCOLESTR pszFileName)
{
	CheckPointer(pszFileName,E_POINTER);
	return m_TSBufferFile.SetFileName(pszFileName);
}

//
// OpenFile
//
HRESULT MultiFileReader::OpenFile()
{
	HRESULT hr = m_TSBufferFile.OpenFile();

	RefreshTSBufferFile();

	m_currentPosition = 0;

	return hr;
}

//
// CloseFile
//
HRESULT MultiFileReader::CloseFile()
{
	HRESULT hr;
	hr = m_TSBufferFile.CloseFile();
	hr = m_TSFile.CloseFile();
	return hr;
}

BOOL MultiFileReader::IsFileInvalid()
{
	return m_TSBufferFile.IsFileInvalid();
}

HRESULT MultiFileReader::GetFileSize(__int64 *pStartPosition, __int64 *pEndPosition)
{
	CheckPointer(pStartPosition,E_POINTER);
	CheckPointer(pEndPosition,E_POINTER);
	*pStartPosition = m_startPosition;
	*pEndPosition = m_endPosition;
	return S_OK;
}

DWORD MultiFileReader::SetFilePointer(__int64 llDistanceToMove, DWORD dwMoveMethod)
{
	if (dwMoveMethod == FILE_END)
	{
		m_currentPosition = m_endPosition + llDistanceToMove;
	}
	else if (dwMoveMethod == FILE_CURRENT)
	{
		m_currentPosition += llDistanceToMove;
	}
	else // if (dwMoveMethod == FILE_BEGIN)
	{
		m_currentPosition = llDistanceToMove;
	}

	if (m_currentPosition < m_startPosition)
		m_currentPosition = m_startPosition;

	if (m_currentPosition > m_endPosition)
		m_currentPosition = m_endPosition;

	return S_OK;
}

__int64 MultiFileReader::GetFilePointer()
{
	return m_currentPosition;
}

HRESULT MultiFileReader::Read(PBYTE pbData, ULONG lDataLength, ULONG *dwReadBytes)
{
	HRESULT hr;

	// If the file has already been closed, don't continue
	if (m_TSBufferFile.IsFileInvalid())
		return S_FALSE;

	if (m_currentPosition < m_startPosition)
		m_currentPosition = m_startPosition;

	// Find out which file the currentPosition is in.
	MultiFileReaderFile *file = NULL;
	std::vector<MultiFileReaderFile *>::iterator it = m_tsFiles.begin();
	for ( ; it < m_tsFiles.end() ; it++ )
	{
		file = *it;
		if (m_currentPosition < (file->startPosition + file->length))
			break;
	}

	if (m_currentPosition < (file->startPosition + file->length))
	{
		if (m_TSFileId != file->filePositionId)
		{
			m_TSFile.CloseFile();
			m_TSFile.SetFileName(file->filename);
			m_TSFile.OpenFile();

			m_TSFileId = file->filePositionId;
		}

		__int64 seekPosition = m_currentPosition - file->startPosition;
		m_TSFile.SetFilePointer(seekPosition, FILE_BEGIN);

		ULONG bytesRead = 0;

		__int64 bytesToRead = file->length - seekPosition;
		if (lDataLength > bytesToRead)
		{
			hr = m_TSFile.Read(pbData, bytesToRead, &bytesRead);
			m_currentPosition += bytesToRead;

			hr = this->Read(pbData + bytesToRead, lDataLength - bytesToRead, dwReadBytes);
			*dwReadBytes += bytesRead;
		}
		else
		{
			hr = m_TSFile.Read(pbData, lDataLength, dwReadBytes);
			m_currentPosition += lDataLength;
		}
	}
	else
	{
		// The current position is past the end of the last file
		*dwReadBytes = 0;
	}

	return S_OK;
}

HRESULT MultiFileReader::Read(PBYTE pbData, ULONG lDataLength, ULONG *dwReadBytes, __int64 llDistanceToMove, DWORD dwMoveMethod)
{
	//If end method then we want llDistanceToMove to be the end of the buffer that we read.
	if (dwMoveMethod == FILE_END)
		llDistanceToMove = 0 - llDistanceToMove - lDataLength;

	SetFilePointer(llDistanceToMove, dwMoveMethod);

	return Read(pbData, lDataLength, dwReadBytes);
}

HRESULT MultiFileReader::get_ReadOnly(WORD *ReadOnly)
{
	CheckPointer(ReadOnly, E_POINTER);

	*ReadOnly = TRUE;
	return S_OK;
}

HRESULT MultiFileReader::RefreshTSBufferFile()
{
	if (m_TSBufferFile.IsFileInvalid())
		return S_FALSE;

	ULONG bytesRead;
	MultiFileReaderFile *file;

	m_TSBufferFile.SetFilePointer(0, FILE_END);
	__int64 fileLength = m_TSBufferFile.GetFilePointer();
	if (fileLength <= (sizeof(__int64) + sizeof(long) + sizeof(long) + sizeof(wchar_t)))
		return S_FALSE;

	m_TSBufferFile.SetFilePointer(0, FILE_BEGIN);
	
	__int64 currentPosition;
	m_TSBufferFile.Read((LPBYTE)&currentPosition, sizeof(currentPosition), &bytesRead);

	long filesAdded, filesRemoved;
	m_TSBufferFile.Read((LPBYTE)&filesAdded, sizeof(filesAdded), &bytesRead);
	m_TSBufferFile.Read((LPBYTE)&filesRemoved, sizeof(filesRemoved), &bytesRead);

	if ((m_filesAdded != filesAdded) || (m_filesRemoved != filesRemoved))
	{
		long filesToRemove = filesRemoved - m_filesRemoved;
		long filesToAdd = filesAdded - m_filesAdded;

		// Removed files that aren't present anymore.
		while (filesToRemove > 0)
		{
			if (m_tsFiles.size() == 0)
			{
				::OutputDebugString(TEXT("there should be items to remove here. Something went wrong\n"));
				return E_FAIL;
			}

			MultiFileReaderFile *file = m_tsFiles.at(0);
			delete file;
			m_tsFiles.erase(m_tsFiles.begin());
			filesToRemove--;
		}

		__int64 remainingLength = fileLength - sizeof(__int64) - sizeof(long) - sizeof(long);
		LPWSTR pBuffer = (LPWSTR)new BYTE[remainingLength];
		m_TSBufferFile.Read((LPBYTE)pBuffer, remainingLength, &bytesRead);
		if (bytesRead < remainingLength)
		{
			::OutputDebugString(TEXT("What's going on?\n"));
			return E_FAIL;
		}

		// Create a list of files in the .tsbuffer file.
		std::vector<LPWSTR> filenames;

		LPWSTR pCurr = pBuffer;
		long length = wcslen(pCurr);
		while (length > 0)
		{
			LPWSTR pFilename = new wchar_t[length+1];
			wcscpy(pFilename, pCurr);
			filenames.push_back(pFilename);

			pCurr += (length + 1);
			length = wcslen(pCurr);
		}

		delete[] pBuffer;

		// Go through files

		std::vector<MultiFileReaderFile *>::iterator itFiles = m_tsFiles.begin();
		std::vector<LPWSTR>::iterator itFilenames = filenames.begin();

		__int64 startPosition = 0;

		while (itFiles < m_tsFiles.end())
		{
			file = *itFiles;
			startPosition = file->startPosition + file->length;

			filesToAdd--;

			itFiles++;

			if (itFilenames < filenames.end())
			{
				itFilenames++;
			}
			else
			{
				::OutputDebugString(TEXT("Missing files!!"));
			}
		}

		if ((filesToAdd > 0) && (m_tsFiles.size() > 0))
		{
			// If we're adding files the changes are the one at the back has a partial length
			// so we need update it.
			file = m_tsFiles.back();
			GetFileLength(file->filename, file->length);
		}

		while (itFilenames < filenames.end())
		{
			LPWSTR pFilename = *itFilenames;

			file = new MultiFileReaderFile();
			file->filename = pFilename;
			file->startPosition = startPosition;

			m_filesAdded++;
			file->filePositionId = m_filesAdded;

			GetFileLength(pFilename, file->length);

			m_tsFiles.push_back(file);

			startPosition = file->startPosition + file->length;

			itFilenames++;
		}

		m_filesAdded = filesAdded;
		m_filesRemoved = filesRemoved;
	}

	if (m_tsFiles.size() > 0)
	{
		file = m_tsFiles.front();
		m_startPosition = file->startPosition;

		file = m_tsFiles.back();
		file->length = currentPosition;
		m_endPosition = file->startPosition + currentPosition;
	}
	else
	{
		m_startPosition = 0;
		m_endPosition = 0;
	}

	return S_OK;
}

HRESULT MultiFileReader::GetFileLength(LPWSTR pFilename, __int64 &length)
{
	USES_CONVERSION;

	length = 0;

	// Try to open the file
	HANDLE hFile = CreateFile(W2T(pFilename),   // The filename
						 GENERIC_READ,          // File access
						 FILE_SHARE_READ,       // Share access
						 NULL,                  // Security
						 OPEN_EXISTING,         // Open flags
						 (DWORD) 0,             // More flags
						 NULL);                 // Template
	if (hFile != INVALID_HANDLE_VALUE)
	{
		LARGE_INTEGER li;
		li.QuadPart = 0;
		li.LowPart = ::SetFilePointer(hFile, 0, &li.HighPart, FILE_END);
		CloseHandle(hFile);
		
		length = li.QuadPart;
	}
	else
	{
		wchar_t msg[MAX_PATH];
		DWORD dwErr = GetLastError();
		swprintf((LPWSTR)&msg, L"Failed to open file %s : 0x%x\n", pFilename, dwErr);
		::OutputDebugString(W2T((LPWSTR)&msg));
		return HRESULT_FROM_WIN32(dwErr);
	}
	return S_OK;
}
