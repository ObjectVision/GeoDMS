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

#include "ptr/OwningPtrSizedArray.h"

//----------------------------------------------------------------------
// class  : ExplicitSuppliers
//----------------------------------------------------------------------

using ActorCRef = SharedPtr<const Actor> ;
using ActorCRefArray = std::unique_ptr<ActorCRef[]>;

 // CHECK AND OPTIMIZE ON INVARIANT: all configured suppliers are TreeItems; all implied suppliers are AbstrCalculators

struct SupplCache
{
	SupplCache();
	void SetSupplString(WeakStr val);

	const Actor* GetSupplier(UInt32 i) const
	{
		dms_assert(!m_IsDirty);
		dms_assert(i < m_NrConfigured);
		return m_SupplArray[i];
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
