/**
*  NetInfo.cpp
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

#include "NetInfo.h"
#include <crtdbg.h>
#include <streams.h>

NetInfo::NetInfo() 
{
	Clear();
}

NetInfo::~NetInfo()
{
}

void NetInfo::Clear()
{
	userIP = 0;
	userPort = 0;
	userNic = 0;
	pNetworkGraph = NULL;
	pNetSource = NULL;
	pFileSink = NULL;
	rotEnable = 0;
	dwGraphRegister = 0;
	lstrcpyW(fileName, L"");
	lstrcpyW(pathName, L"");
	lstrcpyW(strIP, L"");
	lstrcpyW(strPort, L"");
	lstrcpyW(strNic, L"");
	lstrcpyW(strTime, L"");
	time = 0;
	tmTime = NULL;
	playing = 0;
}

void NetInfo::CopyFrom(NetInfo *netInfo)
{
	userIP   = netInfo->userIP;
	userPort   = netInfo->userPort;
	userNic   = netInfo->userNic;
	pNetworkGraph   = netInfo->pNetworkGraph;
	pNetSource   = netInfo->pNetSource;
	pFileSink   = netInfo->pFileSink;
	rotEnable   = netInfo->rotEnable;
	dwGraphRegister   = netInfo->dwGraphRegister;
	lstrcpyW(fileName, netInfo->fileName);
	lstrcpyW(pathName, netInfo->pathName);
	lstrcpyW(strIP, netInfo->strIP);
	lstrcpyW(strPort, netInfo->strPort);
	lstrcpyW(strNic, netInfo->strNic);
	lstrcpyW(strTime, netInfo->strTime);
	time = netInfo->time;
	tmTime = netInfo->tmTime;
	playing = netInfo->playing;
}

void NetInfo::CopyTo(NetInfo *netInfo)
{
	netInfo->userIP = userIP;
	netInfo->userPort = userPort;
	netInfo->userNic = userNic;
	netInfo->pNetworkGraph = pNetworkGraph;
	netInfo->pNetSource = pNetSource;
	netInfo->pFileSink = pFileSink;
	netInfo->rotEnable = rotEnable;
	netInfo->dwGraphRegister = dwGraphRegister;
	lstrcpyW(netInfo->fileName, fileName);
	lstrcpyW(netInfo->pathName, pathName);
	lstrcpyW(netInfo->strIP, strIP);
	lstrcpyW(netInfo->strPort, strPort);
	lstrcpyW(netInfo->strNic, strNic);
	lstrcpyW(netInfo->strTime, strTime);
	netInfo->time = time;
	netInfo->tmTime = tmTime;
	netInfo->playing = playing;
}

NetInfoArray::NetInfoArray()
{
}

NetInfoArray::~NetInfoArray()
{
	Clear();
}

void NetInfoArray::Clear()
{
	std::vector<NetInfo *>::iterator it = m_NetArray.begin();
	for ( ; it != m_NetArray.end() ; it++ )
	{
		CNetRender::DeleteNetworkGraph((NetInfo *)*it);
		delete *it;
	}
	m_NetArray.clear();
}

void NetInfoArray::Add(NetInfo *newNetInfo)
{
	m_NetArray.push_back(newNetInfo);
}

void NetInfoArray::RemoveAt(int nPosition)
{
	if ((nPosition >= 0) && (nPosition < m_NetArray.size()))
	{
		m_NetArray.erase(m_NetArray.begin() + nPosition);
	}
}

NetInfo &NetInfoArray::operator[](int nPosition)
{
	int size = m_NetArray.size();
	_ASSERT(nPosition >= 0);
	_ASSERT(nPosition < size);

	return *m_NetArray.at(nPosition);
}

int NetInfoArray::Count()
{
	return m_NetArray.size();
}

