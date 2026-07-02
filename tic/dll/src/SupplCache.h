// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__SUPPLCACHE_H)
#define __SUPPLCACHE_H

#include "act/Actor.h"
#include "ptr/InterestHolders.h"
#include "ptr/SharedTreePtr.h"

#include "ptr/OwningPtrSizedArray.h"

//----------------------------------------------------------------------
// class  : ExplicitSuppliers
//----------------------------------------------------------------------

// All suppliers cached here are TreeItems (configured suppliers found via FindItem / the fenced source),
// which are now Actors carrying their own std control block -- so the cache owns them as shared_tree_ptr,
// not the old intrusive SharedPtr<const SharedActor> (TreeItem is no longer a SharedActor).
using ActorCRef = std::shared_ptr<const TreeItem>;
using ActorCRefArray = std::unique_ptr<ActorCRef[]>;

 // CHECK AND OPTIMIZE ON INVARIANT: all configured suppliers are TreeItems; all implied suppliers are AbstrCalculators

struct SupplCache
{
	SupplCache();
	void SetSupplString(WeakStr val);

	const TreeItem* GetSupplier(UInt32 i) const
	{
		dms_assert(!m_IsDirty);
		dms_assert(i < m_NrConfigured);
		return m_SupplArray[i].get();
	}
	const ActorCRef* begin(const TreeItem* context) const { Update(context); return m_SupplArray.get(); }
	const ActorCRef* end  (const TreeItem* context) const { Update(context); return m_SupplArray.get() + m_NrConfigured; }
	UInt32              GetNrConfigured(const TreeItem* context) const { Update(context); return m_NrConfigured; }

	WeakStr GetSupplString() const { return m_strConfigured; }

	void Reset();

	TIC_CALL void InitAt(const TreeItem* fencedSource);
	TIC_CALL void InitAt(const ActorCRef* first, const ActorCRef* last);

private:

	void BuildSet(const TreeItem* context) const;

	void Update(const TreeItem* context) const
	{
		if (m_IsDirty) 
			BuildSet(context);
	}

	mutable SharedStr           m_strConfigured;
	mutable ActorCRefArray      m_SupplArray;
	mutable UInt32              m_NrConfigured:31;
	mutable bool                m_IsDirty     : 1;
};

#endif //!defined(__SUPPLCACHE_H)
