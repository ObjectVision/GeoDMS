// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__SHV_LOCKEDINDEXCOLLECTORPTR_H)
#define __SHV_LOCKEDINDEXCOLLECTORPTR_H

#include "ptr/WeakPtr.h"

#include "DataArray.h"
#include "DataLocks.h"

struct IndexCollector;

//----------------------------------------------------------------------
// struct  : LockedIndexCollectorPtr
//----------------------------------------------------------------------

struct LockedIndexCollectorPtr : WeakPtr<const IndexCollector>
{
	LockedIndexCollectorPtr(const IndexCollector* ptr);

	DataReadLock m_Lock;
};

//----------------------------------------------------------------------
// struct  : LockedIndexCollectorPtr
//----------------------------------------------------------------------

struct OptionalIndexCollectorAray : std::optional<typename DataArray<entity_id>::locked_cseq_t>
{
	OptionalIndexCollectorAray(const IndexCollector* ptr, tile_id t);

	entity_id GetEntityIndex(feature_id f) const
	{
		if (!*this)
			return f;
		assert(f < value().size());
		return value().begin()[f];
	}
};

#endif // !defined(__SHV_LOCKEDINDEXCOLLECTORPTR_H)
