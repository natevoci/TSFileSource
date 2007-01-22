// PidTableHash.cpp: implementation of the PidTableHash class.
//
//////////////////////////////////////////////////////////////////////

#include <windows.h>
#include "PidTableHash.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

PidTableHash::PidTableHash()
{

}

PidTableHash::~PidTableHash()
{
	Clear();
}

void PidTableHash::Clear()
{
	map<SHORT, Mpeg2Section *>::iterator it = m_map.begin();
	for ( ; it != m_map.end() ; it++ )
	{
		delete it->second;
	}
	m_map.clear();
}

Mpeg2Section * PidTableHash::GetSection(SHORT pid)
{
	map<SHORT, Mpeg2Section *>::iterator it = m_map.find(pid);
	if (it != m_map.end())
		return it->second;
	return NULL;
}

void PidTableHash::SetSection(SHORT pid, Mpeg2Section *pSection)
{
	m_map[pid] = pSection;
}

void PidTableHash::Remove(SHORT pid)
{
	map<SHORT, Mpeg2Section *>::iterator it = m_map.find(pid);
	if (it != m_map.end())
		m_map.erase(it);
}

