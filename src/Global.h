/**
*  Global.h
*  Copyright (C) 2003      bisswanger
*  Copyright (C) 2004-2005 bear
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
*  bisswanger can be reached at WinSTB@hotmail.com
*    Homepage: http://www.winstb.de
*
*  bear and nate can be reached on the forums at
*    http://forums.dvbowners.com/
*/
#ifndef GLOBAL_H
#define GLOBAL_H

struct BoostThread
{
   BoostThread()
   {
		#ifndef DEBUG
		   if ((int)GetPriorityClass(GetCurrentProcess()) != IDLE_PRIORITY_CLASS)
		   {
				m_nPriority = GetThreadPriority(GetCurrentThread());
				SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL - 1);
		   }
		#endif
   }

   ~BoostThread()
   {
		#ifndef DEBUG
		   if ((int)GetPriorityClass(GetCurrentProcess()) != IDLE_PRIORITY_CLASS)
		   {
				SetThreadPriority(GetCurrentThread(), m_nPriority);
		   }
		#endif
   }
     
   int m_nPriority;
};

struct LowBoostThread
{
   LowBoostThread()
   {
		#ifndef DEBUG
		   if ((int)GetPriorityClass(GetCurrentProcess()) != IDLE_PRIORITY_CLASS)
		   {
				m_nPriority = GetThreadPriority(GetCurrentThread());
				SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL - 2);
		   }
		#endif
   }

   ~LowBoostThread()
   {
		#ifndef DEBUG
		   if ((int)GetPriorityClass(GetCurrentProcess()) != IDLE_PRIORITY_CLASS)
		   {
				SetThreadPriority(GetCurrentThread(), m_nPriority);
		   }
		#endif
   }
     
   int m_nPriority;
};

struct HighestThread
{
   HighestThread()
   {
		#ifndef DEBUG
		   if ((int)GetPriorityClass(GetCurrentProcess()) != IDLE_PRIORITY_CLASS)
		   {
				m_nPriority = GetThreadPriority(GetCurrentThread());
				SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
		   }
		#endif
   }

   ~HighestThread()
   {
		#ifndef DEBUG
		   if ((int)GetPriorityClass(GetCurrentProcess()) != IDLE_PRIORITY_CLASS)
		   {
				SetThreadPriority(GetCurrentThread(), m_nPriority);
		   }
		#endif
   }
      
   int m_nPriority;
};

struct AbnormalThread
{
   AbnormalThread()
   {
		#ifndef DEBUG
		   if ((int)GetPriorityClass(GetCurrentProcess()) != IDLE_PRIORITY_CLASS)
		   {
				m_nPriority = GetThreadPriority(GetCurrentThread());
				SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
		   }
		#endif
   }

   ~AbnormalThread()
   {
		#ifndef DEBUG
		   if ((int)GetPriorityClass(GetCurrentProcess()) != IDLE_PRIORITY_CLASS)
		   {
				SetThreadPriority(GetCurrentThread(), m_nPriority);
		   }
		#endif
   }
      
   int m_nPriority;
};

struct NormalThread
{
   NormalThread()
   {
		#ifndef DEBUG
		   if ((int)GetPriorityClass(GetCurrentProcess()) != IDLE_PRIORITY_CLASS)
		   {
				m_nPriority = GetThreadPriority(GetCurrentThread());
				SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
		   }
		#endif
   }

   ~NormalThread()
   {
		#ifndef DEBUG
		   if ((int)GetPriorityClass(GetCurrentProcess()) != IDLE_PRIORITY_CLASS)
		   {
				SetThreadPriority(GetCurrentThread(), m_nPriority);
		   }
		#endif
   }
      
   int m_nPriority;
};

struct BrakeThread
{
   BrakeThread()
   {
		#ifndef DEBUG
		   if ((int)GetPriorityClass(GetCurrentProcess()) != IDLE_PRIORITY_CLASS)
		   {
				m_nPriority = GetThreadPriority(GetCurrentThread());
				SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
		   }
		#endif
   }

   ~BrakeThread()
   {
		#ifndef DEBUG
		   if ((int)GetPriorityClass(GetCurrentProcess()) != IDLE_PRIORITY_CLASS)
		   {
				SetThreadPriority(GetCurrentThread(), m_nPriority);
		   }
		#endif
   }
     
   int m_nPriority;
};

#endif

