/**
*  TSBuffer.cpp
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
#include "TSBuffer.h"
#include <crtdbg.h>
#include <math.h>

CTSBuffer::CTSBuffer(CTSFileSourceClock *pClock)
{
	m_pFileReader = NULL;
	m_pClock = pClock;
	m_lItemOffset = 0;
	m_lTSBufferItemSize = 188000;
	debugcount = 0;
	m_lbuflen = 1;
	Clear();
}

CTSBuffer::~CTSBuffer()
{
	Clear();
}

void CTSBuffer::SetFileReader(FileReader *pFileReader)
{
	CAutoLock BufferLock(&m_BufferLock);
	m_pFileReader = pFileReader;
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
	m_lbuflen = 1;
}

long CTSBuffer::Count()
{
	long bytesAvailable = 0;
	long itemCount = m_Array.size();

	if (itemCount > 0)
	{
		bytesAvailable += m_lTSBufferItemSize - m_lItemOffset;
		bytesAvailable += m_lTSBufferItemSize * (itemCount - 1);
	}
	return bytesAvailable;
}

HRESULT CTSBuffer::UpdateBuffer()
{
	CAutoLock BufferLock(&m_BufferLock);
	long bytesAvailable = Count();
	if (bytesAvailable < m_lTSBufferItemSize*m_lbuflen)
	{
		Require(m_lTSBufferItemSize*m_lbuflen);
	}

	return S_OK;
}

HRESULT CTSBuffer::GetRequire(long nBytes)
{
	CAutoLock BufferLock(&m_BufferLock);
	return Require(nBytes);
}

HRESULT CTSBuffer::Require(long nBytes)
{
	if (!m_pFileReader)
		return E_POINTER;

	long bytesAvailable = Count();

	while (nBytes > bytesAvailable)
	{
		BYTE *newItem = new BYTE[m_lTSBufferItemSize];
		ULONG ulBytesRead = 0;

		__int64 currPosition = m_pFileReader->GetFilePointer();
//PrintLongLong("Require-GetFilePointer", currPosition);
		HRESULT hr = m_pFileReader->Read(newItem, m_lTSBufferItemSize, &ulBytesRead);
		if (FAILED(hr)){

			delete[] newItem;
			m_lbuflen = 1;
			return hr;
		}

		if (ulBytesRead < m_lTSBufferItemSize) 
		{
			WORD wReadOnly = 0;
			m_pFileReader->get_ReadOnly(&wReadOnly);
			if (wReadOnly)
			{
				int count = 200; // 2 second max delay
				while (ulBytesRead < m_lTSBufferItemSize && count) 
				{
					::OutputDebugString(TEXT("TSBuffer::Require() Waiting for file to grow.\n"));

					WORD bDelay = 0;
					m_pFileReader->get_DelayMode(&bDelay);
					count--;

					if (bDelay > 0)
					{
						Sleep(2000);
						count = 0;
					}
					else
					{
						if (!count)
						{
							delete[] newItem;
							m_lbuflen = 1;
							return hr;
						}
						Sleep(10);
					}

					ULONG ulNextBytesRead = 0;				
					m_pFileReader->SetFilePointer(currPosition, FILE_BEGIN);
					HRESULT hr = m_pFileReader->Read(newItem, m_lTSBufferItemSize, &ulNextBytesRead);
					if (FAILED(hr) && !count){

						delete[] newItem;
						m_lbuflen = 1;
						return hr;
					}

					if (((ulNextBytesRead == 0) | (ulNextBytesRead == ulBytesRead)) && !count){

						delete[] newItem;
						m_lbuflen = 1;
						return E_FAIL;
					}

					ulBytesRead = ulNextBytesRead;
				}
			}
			else
			{
				delete[] newItem;
				m_lbuflen = 1;
				return E_FAIL;
			}

			m_lbuflen--;
		}
		else if (m_lbuflen < 8)
			m_lbuflen++;

		if(m_lbuflen < 1)
			m_lbuflen = 1;

		m_Array.push_back(newItem);
		bytesAvailable += m_lTSBufferItemSize;
	}
	return S_OK;
}

HRESULT CTSBuffer::DequeFromBuffer(BYTE *pbData, long lDataLength)
{
	if (!m_pFileReader)
		return E_POINTER;

	CAutoLock BufferLock(&m_BufferLock);
	HRESULT hr = Require(lDataLength);
	if (FAILED(hr))
		return hr;

	long bytesWritten = 0;
	while (bytesWritten < lDataLength)
	{
		BYTE *item = m_Array.at(0);

		long copyLength = min(m_lTSBufferItemSize-m_lItemOffset, lDataLength-bytesWritten);
		memcpy(pbData + bytesWritten, item + m_lItemOffset, copyLength);

		bytesWritten += copyLength;
		m_lItemOffset += copyLength;

		if (m_lItemOffset >= m_lTSBufferItemSize)
		{
			m_Array.erase(m_Array.begin());
			delete[] item;
			m_lItemOffset -= m_lTSBufferItemSize;	//should result in zero
		}
	}
//PrintLongLong("DequeFromBuffer-Count", Count());
	return S_OK;
}

HRESULT CTSBuffer::ReadFromBuffer(BYTE *pbData, long lDataLength, long lOffset)
{
	if (!m_pFileReader)
		return E_POINTER;

	CAutoLock BufferLock(&m_BufferLock);
	HRESULT hr = Require(lOffset + lDataLength);
	if (FAILED(hr))
		return hr;

	long bytesWritten = 0;
	long itemIndex = 0;
	lOffset += m_lItemOffset;

	while (bytesWritten < lDataLength)
	{
		while (lOffset >= m_lTSBufferItemSize)
		{
			itemIndex++;
			lOffset -= m_lTSBufferItemSize;
		}


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

void CTSBuffer::PrintLongLong(LPCTSTR lstring, __int64 value)
{
	TCHAR sz[100];
	double dVal = value;
	double len = log10(dVal);
	int pos = len;
	sz[pos+1] = '\0';
	while (pos >= 0)
	{
		int val = value % 10;
		sz[pos] = '0' + val;
		value /= 10;
		pos--;
	}
	TCHAR szout[100];
	wsprintf(szout, TEXT("%05i - %s %s\n"), debugcount, lstring, sz);
	::OutputDebugString(szout);
	debugcount++;
}
