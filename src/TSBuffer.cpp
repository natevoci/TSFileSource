/**
*  TSBuffer.cpp
*  Copyright (C) 2005      nate
*  Copyright (C) 2006      bear
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
#include "TSBuffer.h"
#include <crtdbg.h>
#include <math.h>
#include "global.h"
#include "FileReader.h"
#include "MultiFileReader.h"

#define TSBUFFER_ITEM_SIZE 96256	// 96256 is the least common multiple of 188 and 2048

CTSBuffer::CTSBuffer()
{
	m_lFilePosition = 0;
	m_lItemOffset = 0;
	m_lTSBufferItemSize = TSBUFFER_ITEM_SIZE;
	m_loopCount = 20;
	debugcount = 0;

	m_pFileReader = new FileReader();
	m_pParser = new Mpeg2Parser();
}

CTSBuffer::~CTSBuffer()
{
	Clear();

	m_pFileReader->CloseFile();
	delete m_pFileReader;
	delete m_pParser;
}

//IFileReader Interface

IFileReader* CTSBuffer::CreateFileReader()
{
	return NULL;
}

HRESULT CTSBuffer::GetFileName(LPOLESTR *lpszFileName)
{
	if (m_pFileReader)
		return m_pFileReader->GetFileName(lpszFileName);
	
	*lpszFileName = NULL;
	return S_OK;
}
HRESULT CTSBuffer::SetFileName(LPCOLESTR pszFileName)
{
	if (m_pFileReader)
	{
		m_pFileReader->CloseFile();
		delete m_pFileReader;
	}

	long length = wcslen(pszFileName);
	if ((length < 9) || (_wcsicmp(pszFileName+length-9, L".tsbuffer") != 0))
	{
		m_pFileReader = new FileReader();
	}
	else
	{
		m_pFileReader = new MultiFileReader();
	}
	return m_pFileReader->SetFileName(pszFileName);
}

HRESULT CTSBuffer::OpenFile()
{
	HRESULT hr = m_pFileReader->OpenFile();
	if (SUCCEEDED(hr))
		m_lFilePosition = 0;
	return hr;
}
HRESULT CTSBuffer::CloseFile()
{
	return m_pFileReader->CloseFile();
}

HRESULT CTSBuffer::Read(PBYTE pbData, ULONG lDataLength, ULONG *dwReadBytes)
{
	return E_NOTIMPL;
}
HRESULT CTSBuffer::Read(PBYTE pbData, ULONG lDataLength, ULONG *dwReadBytes, __int64 llDistanceToMove, DWORD dwMoveMethod)
{
	return E_NOTIMPL;
}
HRESULT CTSBuffer::GetFileSize(__int64 *pStartPosition, __int64 *pLength)
{
	return E_NOTIMPL;
}
BOOL CTSBuffer::IsFileInvalid()
{
	return m_pFileReader->IsFileInvalid();
}

DWORD CTSBuffer::SetFilePointer(__int64 llDistanceToMove, DWORD dwMoveMethod)
{
	Clear();
	HRESULT hr = m_pFileReader->SetFilePointer(llDistanceToMove, dwMoveMethod);
	if (SUCCEEDED(hr))
		m_lFilePosition = m_pFileReader->GetFilePointer();
	return hr;
}
__int64 CTSBuffer::GetFilePointer()
{
	return m_lFilePosition;
}

HRESULT CTSBuffer::get_ReadOnly(WORD *ReadOnly)
{
	return m_pFileReader->get_ReadOnly(ReadOnly);
}
HRESULT CTSBuffer::get_DelayMode(WORD *DelayMode)
{
	return m_pFileReader->get_DelayMode(DelayMode);
}
HRESULT CTSBuffer::set_DelayMode(WORD DelayMode)
{
	return m_pFileReader->set_DelayMode(DelayMode);
}


void CTSBuffer::Clear()
{
	CAutoLock BufferLock(&m_BufferLock);
	std::vector<BYTE *>::iterator it = m_Array.begin();
	for ( ; it != m_Array.end() ; it++ )
	{
		delete[] *it;
	}
	m_Array.clear();

	m_lItemOffset = 0;
	m_loopCount = 2;
}

long CTSBuffer::Count()
{
	CAutoLock BufferLock(&m_BufferLock);
	long bytesAvailable = 0;
	long itemCount = m_Array.size();

	if (itemCount > 0)
	{
		bytesAvailable += m_lTSBufferItemSize - m_lItemOffset;
		bytesAvailable += m_lTSBufferItemSize * (itemCount - 1);
	}
	return bytesAvailable;
}

HRESULT CTSBuffer::Require(long nBytes, BOOL bIgnoreDelay)
{
	if (!m_pFileReader)
		return E_POINTER;

	if (m_pFileReader->IsFileInvalid())
		return E_FAIL;

	HRESULT hr = S_OK;
	CAutoLock BufferLock(&m_BufferLock);
	long bytesAvailable = Count();
	if (nBytes <= bytesAvailable)
		return S_OK;

	while (nBytes > bytesAvailable)
	{
		BYTE *newItem = new BYTE[m_lTSBufferItemSize];
		ULONG ulBytesRead = 0;

		__int64 currPosition = m_pFileReader->GetFilePointer();
		hr = m_pFileReader->Read(newItem, m_lTSBufferItemSize, &ulBytesRead);
		if (FAILED(hr))
		{
			delete[] newItem;
			break;
		}

		if (ulBytesRead < (ULONG)m_lTSBufferItemSize) 
		{
			WORD wReadOnly = 0;
			m_pFileReader->get_ReadOnly(&wReadOnly);
			if (wReadOnly && !bIgnoreDelay)
			{
				m_loopCount = max(2, m_loopCount);
				m_loopCount = min(20, m_loopCount);
//				int count = 220; // 2 second max delay
				while (ulBytesRead < (ULONG)m_lTSBufferItemSize && m_loopCount) 
				{
//					::OutputDebugString(TEXT("TSBuffer::Require() Waiting for file to grow.\n"));

					WORD bDelay = 0;
					m_pFileReader->get_DelayMode(&bDelay);
					m_loopCount--;

					if (bDelay > 0)
					{
						Sleep(2000);
						m_loopCount = 0;
					}
					else
					{
						if (!m_loopCount)
						{
							m_loopCount = 20;
							delete[] newItem;
							return hr;
						}
						Sleep(100);
					}

					ULONG ulNextBytesRead = 0;				
					m_pFileReader->SetFilePointer(currPosition, FILE_BEGIN);
					HRESULT hr = m_pFileReader->Read(newItem, m_lTSBufferItemSize, &ulNextBytesRead);
					if (FAILED(hr) && !m_loopCount){

						m_loopCount = 20;
						delete[] newItem;
						return hr;
					}

					if (((ulNextBytesRead == 0) | (ulNextBytesRead == ulBytesRead)) && !m_loopCount){

						m_loopCount = 20;
						delete[] newItem;
						return E_FAIL;
					}

					ulBytesRead = ulNextBytesRead;
				}
			}
			else
			{
				delete[] newItem;
				return E_FAIL;
			}
		}

		if (newItem)
		{
			m_Array.push_back(newItem);
			bytesAvailable += m_lTSBufferItemSize;
		}
	}

	OnNewDataAvailable();

	return hr;
}

HRESULT CTSBuffer::DequeFromBuffer(BYTE *pbData, long lDataLength)
{
	CAutoLock BufferLock(&m_BufferLock);
	HRESULT hr = Require(lDataLength);
	if (FAILED(hr))
		return hr;

	long bytesWritten = 0;
	while (bytesWritten < lDataLength)
	{
		if (m_Array.size() <= 0)
			return E_FAIL;

		BYTE *item = m_Array.at(0);

		long copyLength = min(m_lTSBufferItemSize-m_lItemOffset, lDataLength-bytesWritten);
		memcpy(pbData + bytesWritten, item + m_lItemOffset, copyLength);

		bytesWritten += copyLength;
		m_lItemOffset += copyLength;
		m_lFilePosition += copyLength;

		if (m_lItemOffset >= m_lTSBufferItemSize)
		{
			m_Array.erase(m_Array.begin());
			delete[] item;
			m_lItemOffset -= m_lTSBufferItemSize;	//should result in zero
		}
	}
	return S_OK;
}

HRESULT CTSBuffer::ReadFromBuffer(BYTE *pbData, long lDataLength, long lOffset)
{
	if (!m_pFileReader)
		return E_POINTER;

	CAutoLock BufferLock(&m_BufferLock);
	HRESULT hr = Require(m_lItemOffset + lOffset + lDataLength);
	if (FAILED(hr))
		return hr;

	long bytesWritten = 0;
	long itemIndex = 0;
	lOffset += m_lItemOffset;

	while (bytesWritten < lDataLength)
	{
		while (lOffset >= m_lTSBufferItemSize)
		{
			lOffset -= m_lTSBufferItemSize;

			itemIndex++;
			if((m_Array.size() == 0) || ((long)m_Array.size() <= itemIndex))
				return E_FAIL;
		}

		if((m_Array.size() == 0) || ((long)m_Array.size() <= itemIndex))
			return E_FAIL;

		BYTE *item = m_Array.at(itemIndex);

		long copyLength = min(m_lTSBufferItemSize-lOffset, lDataLength-bytesWritten);
		{
			memcpy(pbData + bytesWritten, item + lOffset, copyLength);

			bytesWritten += copyLength;
			lOffset += copyLength;
		}
	}

	return S_OK;
}


void CTSBuffer::OnNewDataAvailable()
{
	HRESULT hr;

	__int64 parserPosition = m_pParser->GetEndDataPosition();
	long count = Count();

	if ((parserPosition < m_lFilePosition) || (parserPosition >= (m_lFilePosition + count)))
	{
		parserPosition = m_lFilePosition;
	}

	long lOffset = (parserPosition - m_lFilePosition);
	long dataLength = count - lOffset;

	BYTE *pData = NULL;
	m_pParser->GetDataBuffer(parserPosition, dataLength, &pData);

	hr = this->ReadFromBuffer(pData, dataLength, lOffset);
	if (FAILED(hr))
		return;

	m_pParser->ParseData(dataLength);
}

HRESULT CTSBuffer::ParseData()
{
	HRESULT hr;
	long dataLength = Count();

	if (dataLength > 0)
	{
		// Parse any data already in the buffer
		BYTE *pData;
		m_pParser->GetDataBuffer(m_lFilePosition, dataLength, &pData);

		hr = this->ReadFromBuffer(pData, dataLength, 0);
		if (FAILED(hr))
			return hr;

		m_pParser->ParseData(dataLength);
	}

	long require = dataLength;
	while (TRUE /* until we have filled all the required PSI information */)
	{
		require += TSBUFFER_ITEM_SIZE;
		Require(require);
	}

	return S_OK;
}

