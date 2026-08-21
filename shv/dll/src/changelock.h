// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#ifndef __SHV_CHANGELOCK_H
#define __SHV_CHANGELOCK_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "GraphicObject.h"
#include "InvalidationBlock.h"

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

