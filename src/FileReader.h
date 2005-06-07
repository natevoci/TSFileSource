/**
*  FileReader.h
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

#ifndef FILEREADER
#define FILEREADER

#include "PidInfo.h"

class FileReader
{
public:

	FileReader();
	virtual ~FileReader();

	// Open and write to the file
	HRESULT GetFileName(LPOLESTR *lpszFileName);
	HRESULT SetFileName(LPCOLESTR pszFileName);
	HRESULT OpenFile();
	HRESULT CloseFile();

	BOOL IsFileInvalid();

	HRESULT GetFileSize(__int64 *lpllsize);
	DWORD SetFilePointer(__int64 llDistanceToMove, DWORD dwMoveMethod);
	__int64 GetFilePointer();

//***********************************************************************************************
//File Growing Fix

	__int64 get_FileSize(void);

//***********************************************************************************************

	HRESULT Read(PBYTE pbData, ULONG lDataLength, ULONG *dwReadBytes);
	HRESULT Read(PBYTE pbData, ULONG lDataLength, ULONG *dwReadBytes, __int64 llDistanceToMove, DWORD dwMoveMethod);

//***********************************************************************************************
//Found this bug
	
	HRESULT get_ReadOnly(WORD *ReadOnly);

// Removed	HRESULT get_ReadOnly(WORD *AutoMode);

//**********************************************************************************************
	HRESULT get_DelayMode(WORD *DelayMode);
	HRESULT set_DelayMode(WORD DelayMode);

protected:
	HANDLE   m_hFile;               // Handle to file for streaming
	LPOLESTR m_pFileName;           // The filename where we stream
	BOOL     m_bReadOnly;
	BOOL     m_bDelay;
//***********************************************************************************************
//File Growing Fix

	__int64 m_fileSize;

//File Writer Additions

	HANDLE   m_hInfoFile;               // Handle to Infofile for filesize

//***********************************************************************************************


};

#endif
