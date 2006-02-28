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
	m_bReadOnly = 1;
	m_bDelay = 0;
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
	m_TSFileId = 0;
	return hr;
}

BOOL MultiFileReader::IsFileInvalid()
{
	return m_TSBufferFile.IsFileInvalid();
}

HRESULT MultiFileReader::GetFileSize(__int64 *pStartPosition, __int64 *pLength)
{
//	RefreshTSBufferFile();
	CheckPointer(pStartPosition,E_POINTER);
	CheckPointer(pLength,E_POINTER);
	*pStartPosition = m_startPosition;
	*pLength = (__int64)(m_endPosition - m_startPosition);
	return S_OK;
}

DWORD MultiFileReader::SetFilePointer(__int64 llDistanceToMove, DWORD dwMoveMethod)
{
//	RefreshTSBufferFile();

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

	RefreshTSBufferFile();
	return S_OK;
}

__int64 MultiFileReader::GetFilePointer()
{
//	RefreshTSBufferFile();
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

	RefreshTSBufferFile();
	// Find out which file the currentPosition is in.
	MultiFileReaderFile *file = NULL;
	std::vector<MultiFileReaderFile *>::iterator it = m_tsFiles.begin();
	for ( ; it < m_tsFiles.end() ; it++ )
	{
		file = *it;
		if (m_currentPosition < (file->startPosition + file->length))
			break;
	};

	if(!file)
		return S_FALSE;

	if (m_currentPosition < (file->startPosition + file->length))
	{
		if (m_TSFileId != file->filePositionId)
		{
			m_TSFile.CloseFile();
			m_TSFile.SetFileName(file->filename);
			m_TSFile.OpenFile();

			m_TSFileId = file->filePositionId;

			if (m_bDebugOutput)
			{
				USES_CONVERSION;
				TCHAR sz[MAX_PATH+128];
				wsprintf(sz, TEXT("Current File Changed to %s\n"), W2T(file->filename));
				::OutputDebugString(sz);
			}
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

	*ReadOnly = m_bReadOnly;
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
		long fileID = filesRemoved;
		__int64 nextStartPosition = 0;

		if (m_bDebugOutput)
		{
			TCHAR sz[128];
			wsprintf(sz, TEXT("Files Added %i, Removed %i\n"), filesToAdd, filesToRemove);
			::OutputDebugString(sz);
		}

		// Removed files that aren't present anymore.
		while ((filesToRemove > 0) && (m_tsFiles.size() > 0))
		{
			MultiFileReaderFile *file = m_tsFiles.at(0);

			if (m_bDebugOutput)
			{
				USES_CONVERSION;
				TCHAR sz[MAX_PATH+128];
				wsprintf(sz, TEXT("Removing file %s\n"), W2T(file->filename));
				::OutputDebugString(sz);
			}
			
			delete file;
			m_tsFiles.erase(m_tsFiles.begin());

			filesToRemove--;
		}


		// Figure out what the start position of the next new file will be
		if (m_tsFiles.size() > 0)
		{
			file = m_tsFiles.back();

			if (filesToAdd > 0)
			{
				// If we're adding files the changes are the one at the back has a partial length
				// so we need update it.
				if (m_bDebugOutput)
					GetFileLength(file->filename, file->length);
				else
					GetFileLength(file->filename, file->length);
			}

			nextStartPosition = file->startPosition + file->length;
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

		while (itFiles < m_tsFiles.end())
		{
			file = *itFiles;

			itFiles++;
			fileID++;

			if (itFilenames < filenames.end())
			{
				// TODO: Check that the filenames match
				itFilenames++;
			}
			else
			{
				::OutputDebugString(TEXT("Missing files!!\n"));
			}
		}

		while (itFilenames < filenames.end())
		{
			LPWSTR pFilename = *itFilenames;

			if (m_bDebugOutput)
			{
				USES_CONVERSION;
				TCHAR sz[MAX_PATH+128];
				int nextStPos = nextStartPosition;
				wsprintf(sz, TEXT("Adding file %s (%i)\n"), W2T(pFilename), nextStPos);
				::OutputDebugString(sz);
			}

			file = new MultiFileReaderFile();
			file->filename = pFilename;
			file->startPosition = nextStartPosition;

			fileID++;
			file->filePositionId = fileID;

			GetFileLength(pFilename, file->length);

			m_tsFiles.push_back(file);

			nextStartPosition = file->startPosition + file->length;

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

	
		/*if (m_bDebugOutput)
		{
			TCHAR sz[128];
			int stPos = m_startPosition;
			int endPos = m_endPosition;
			int curPos = m_currentPosition;
			wsprintf(sz, TEXT("StartPosition %i, EndPosition %i, CurrentPosition %i\n"), stPos, endPos, curPos);
			::OutputDebugString(sz);
		}*/
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
						 FILE_SHARE_READ |
						 FILE_SHARE_WRITE,       // Share access
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

HRESULT MultiFileReader::get_DelayMode(WORD *DelayMode)
{
	*DelayMode = m_bDelay;
	return S_OK;
}

HRESULT MultiFileReader::set_DelayMode(WORD DelayMode)
{
	m_bDelay = DelayMode;
	return S_OK;
}

HRESULT MultiFileReader::get_ReaderMode(WORD *ReaderMode)
{
	*ReaderMode = TRUE;
	return S_OK;
}

DWORD MultiFileReader::setFilePointer(__int64 llDistanceToMove, DWORD dwMoveMethod)
{
	//Get the file information
	__int64 fileStart, fileEnd, fileLength;
	GetFileSize(&fileStart, &fileLength);
	fileEnd = (__int64)(fileLength + fileStart);
	if (dwMoveMethod == FILE_BEGIN)
		return SetFilePointer((__int64)min(fileEnd,(__int64)(llDistanceToMove + fileStart)), FILE_BEGIN);
	else
		return SetFilePointer((__int64)max((__int64)-fileLength, llDistanceToMove), FILE_END);
}

__int64 MultiFileReader::getFilePointer()
{
	__int64 fileStart, fileEnd, fileLength;
	GetFileSize(&fileStart, &fileLength);
	fileEnd = fileLength + fileStart;
	return (__int64)(GetFilePointer() - fileStart);
		
}

