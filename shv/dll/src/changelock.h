//<HEADER> 
/*
Data & Model Server (DMS) is a server written in C++ for DSS applications. 
Version: see srv/dms/rtc/dll/src/RtcVersion.h for version info.

Copyright (C) 1998-2004  YUSE GSO Object Vision BV. 

Documentation on using the Data & Model Server software can be found at:
http://www.ObjectVision.nl/DMS/

See additional guidelines and notes in srv/dms/Readme-srv.txt 

This library is free software; you can use, redistribute, and/or
modify it under the terms of the GNU General Public License version 2 
(the License) as published by the Free Software Foundation,
provided that this entire header notice and readme-srv.txt is preserved.

See LICENSE.TXT for terms of distribution or look at our web site:
http://www.objectvision.nl/DMS/License.txt
or alternatively at: http://www.gnu.org/copyleft/gpl.html

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details. However, specific warranties might be
granted by an additional written contract for support, assistance and/or development
*/
//</HEADER>
#pragma once

#ifndef __SHV_CHANGELOCK_H
#define __SHV_CHANGELOCK_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "GraphicObject.h"
#include "act/InvalidationBlock.h"

//----------------------------------------------------------------------
// class  : RecursiveLock
//----------------------------------------------------------------------

struct RecursiveChangeLock : InvalidationBlock
{
	RecursiveChangeLock(const GraphicObject* self)
		:	InvalidationBlock(self)
	{
		SizeT n = self->NrEntries();
		if (n)
		{
			m_FirstSubLock = m_LastSubLock = std::allocator<RecursiveChangeLock>().allocate(n);
				//reinterpret_cast<RecursiveChangeLock*>(new char[sizeof(RecursiveChangeLock)*n]);
			while (n)
				new (m_LastSubLock++) RecursiveChangeLock(self->GetConstEntry(--n));
		}
		else
			m_FirstSubLock = m_LastSubLock = nullptr;
	}

	void ProcessChange()
	{
		InvalidationBlock::ProcessChange();
		for(RecursiveChangeLock* currSubLock = m_FirstSubLock; currSubLock != m_LastSubLock; ++currSubLock)
			currSubLock->ProcessChange();
	}
	~RecursiveChangeLock()
	{
		RecursiveChangeLock* currSubLock = m_FirstSubLock;
		if (currSubLock)
		{
			for(; currSubLock != m_LastSubLock; ++currSubLock)
				currSubLock->~RecursiveChangeLock();
			std::allocator<RecursiveChangeLock>().deallocate(m_FirstSubLock, m_LastSubLock - m_FirstSubLock);
		}
	}

private:
	RecursiveChangeLock* m_FirstSubLock;
	RecursiveChangeLock* m_LastSubLock;
};

#endif // __SHV_CHANGELOCK_H

