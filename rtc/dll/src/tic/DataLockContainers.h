// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#if !defined(__TIC_DATALOCKCONTAINERS_H)
#define __TIC_DATALOCKCONTAINERS_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "DataLocks.h"

#include <vector>

//----------------------------------------------------------------------
// class  : DataReadLockContainer
//----------------------------------------------------------------------

struct DataReadLockContainer : std::vector<DataReadLock>
{
	TIC_CALL bool Add(const AbstrDataItem* adi, DrlType type);
};

//----------------------------------------------------------------------
// class  : DataWriteLockContainer
//----------------------------------------------------------------------

struct DataWriteLockContainer
{
	void Add(AbstrDataItem* adi, dms_rw_mode rwm);
	void Commit();

private:
	std::vector<DataWriteLock> m_Locks;
};


#endif
