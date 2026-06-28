#pragma once

#if !defined(__TIC_TREEITEMDUALREF_H)
#define __TIC_TREEITEMDUALREF_H

#include "TicBase.h"

#include "act/Actor.h"
#include "ptr/WeakPtr.h"

#include <memory>
#include <variant>

// *****************************************************************************
// Section:     DataControllerFlags
// stored in Actor::m_State (last real actor flag is AF_IsPassor = 0x0010)
//	these flags overlap with TreeItem flags
// *****************************************************************************

enum DataControllerFlags
{
	DCF_IsOld              = 0x001 * actor_flag_set::AF_Next,
	DCF_IsTmp              = 0x002 * actor_flag_set::AF_Next,

	#if defined(MG_DEBUG_DATA)
	DCFD_DataCounted     = 0x080 * actor_flag_set::AF_Next,
	#endif

	DCF_CanChange        = 0x400 * actor_flag_set::AF_Next, // op_is_transient
};

// *****************************************************************************
// Section:     DcRef -- the IsNew/IsOld/IsTmp result holder (replaces m_Data WeakPtr + m_OwnedData)
// *****************************************************************************

// One member, a std::variant whose active arm encodes ownership (see doc/development/std-ptr-migration-plan.md
// §4). Owning arms (cache results) keep the result alive exactly as the old m_OwnedData did; weak arms
// (borrowed config item, tmp instantiation) are NON-owning and .lock()-checked, so a vanished item is
// detected (null) instead of dereferenced blind -- the dangling-IsOld fix the raw WeakPtr could not give,
// and it breaks the retain cycle (config item -> mc_DC -> DC graph -> m_Data -> ... -> root).
struct DcRef
{
	std::variant<
		std::monostate,                   // 0: empty
		std::shared_ptr<TreeItem>,        // 1: IsNew  -- root cache result; DualRef is the primary owner
		std::shared_ptr<const TreeItem>,  // 2: IsOld  -- cache sub-item; owned ("like new")
		std::weak_ptr<const TreeItem>,    // 3: IsOld  -- config item; the tree owns it; .lock() to use
		std::weak_ptr<TreeItem>           // 4: IsTmp  -- instantiation borrow at the calling site; .lock() to use
	> m_Holder;

	// THE accessor: lock to an OWNING snapshot. For the weak arms this keeps the target alive for the
	// caller's whole use (no dangling raw from a dying temp -- that was the crash); for the owning arms it
	// shares ownership. Empty shared_tree_ptr if the arm is empty or the weak target has expired. Callers
	// MUST hold the result while using it: `if (auto p = m_Data.get()) p->...`.
	shared_tree_ptr<const TreeItem> get() const
	{
		switch (m_Holder.index())
		{
		case 1: return std::static_pointer_cast<const TreeItem>(std::get<1>(m_Holder));
		case 2: return std::get<2>(m_Holder);
		case 3: return std::get<3>(m_Holder).lock();
		case 4: return std::static_pointer_cast<const TreeItem>(std::get<4>(m_Holder).lock());
		default: return {};
		}
	}
	int             kind()       const noexcept { return int(m_Holder.index()); } // 0 empty,1 new,2 oldCache,3 oldConfig,4 tmp
	void            clear()            noexcept { m_Holder.emplace<std::monostate>(); }

	// Arm-set STATE check (is a result holder present) -- NOT a transient liveness probe. Mirrors
	// TreeItemDualRef::operator bool/operator! exactly (kind()-based); for liveness you must hold get().
	// (The transient raw-deref operators were removed on purpose; this is a pure state read.)
	explicit operator bool() const noexcept { return kind() != 0; }
	bool     operator!()     const noexcept { return kind() == 0; }

	// std owner count of the OWNING arms (0 for weak/empty) -- replaces the old intrusive GetRefCount() check.
	long use_count() const noexcept
	{
		switch (m_Holder.index())
		{
		case 1: return std::get<1>(m_Holder).use_count();
		case 2: return std::get<2>(m_Holder).use_count();
		default: return 0;
		}
	}
};

// *****************************************************************************
// Section:     TreeItemDualRef Interface
// *****************************************************************************

struct TreeItemDualRef : SharedActor
{
	TIC_CALL TreeItemDualRef();
	TIC_CALL ~TreeItemDualRef();
	TreeItemDualRef(const TreeItemDualRef&) = delete;
	TreeItemDualRef(TreeItemDualRef&&) = delete;

	// The owning current-result snapshot -- the safe way to read & hold the result. Callers that need it
	// alive across work MUST keep this: `if (auto p = dc.GetCurr()) p->...`.
	SharedTreeItem GetCurr() const { return m_Data.get(); }
	// Raw borrows: valid only while an owner (the variant's owning arm, or the tree/caller for the weak arms)
	// outlives the use. Prefer GetCurr() when holding across work.
	      TreeItem* GetNew()  const { dms_assert(!IsOld()); return const_cast<TreeItem*>(m_Data.get().get()); }
	const TreeItem* GetOld()  const { return m_Data.get().get(); }
	const TreeItem* GetUlt()  const { if (auto p = m_Data.get()) return p->GetCurrUltimateItem().get(); return nullptr; }

	virtual bool IsSymbDC() const { return false; }
	virtual bool CanResultToConfigItem() const { return false; }


	// "is an arm set" -- a non-transient state check (NOT a liveness probe; for liveness use GetCurr()).
	explicit operator bool () const { return m_Data.kind() != 0; }
	bool operator ! () const { return m_Data.kind() == 0; }
	const TreeItem* operator ->() const { return GetOld(); } // raw borrow (see GetOld/GetCurr)

	// state predicates from the variant arm (kind), not a transient liveness probe.
	bool IsNew() const { return m_Data.kind() == 1; }
	bool IsOld() const { return m_Data.kind() == 2 || m_Data.kind() == 3; }
	bool IsTmp() const { return m_Data.kind() == 4; }
	bool IsTransient() const { return IsTmp() || m_State.Get(DCF_CanChange); };

	void operator =(      TreeItem* rhs) { SetNew(rhs); }
	void operator =(const TreeItem* rhs) { SetOld(rhs); }
	// Takes a freshly created (owned) item; rhs keeps it alive across SetNew's borrow so the item is
	// never seen at refcount 0 (e.g. CreateCacheRoot()). The DualRef's m_Data then owns it.
	void operator =(const SharedMutableTreeItem& rhs) { SetNew(rhs.get()); }

	TIC_CALL void Clear();

	TIC_CALL void SetNew(      TreeItem* newTI);
	TIC_CALL void SetOld(const TreeItem* oldTI);
	TIC_CALL void SetTmp(      TreeItem* tmpTI);

	bool HasBackRef() const { auto p = m_Data.get(); return p && bool(p->m_BackRef); }
	SharedStr GetBackRefStr() const { auto p = m_Data.get(); return p->m_BackRef->GetSourceName(); }

protected:
	void Set(const TreeItem* newTI, bool isNew);

//	override Actor virtuals
	void StartInterest() const override;
	garbage_can StopInterest () const noexcept override;

	virtual void IncDataInterestCount() const;
	garbage_can DecDataInterestCount() const;
	void DoInvalidate () const override;
	bool DoFail(ErrMsgPtr msg, FailType ft) const override;

	friend struct data_swapper;
	friend struct InterestReporter;

	// The single result holder (variant): owning arms keep cache results alive; weak arms (config item,
	// tmp) are non-owning + .lock()-checked. Replaces the old (m_Data WeakPtr + m_OwnedData SharedTreeItem).
	mutable DcRef m_Data;
};

// *****************************************************************************

#include "dbg/DebugContext.h"

struct TreeItemDualRefContextHandle : ObjectContextHandle
{
	TIC_CALL TreeItemDualRefContextHandle(const TreeItemDualRef* currRef);
	TIC_CALL ~TreeItemDualRefContextHandle();

	TIC_CALL static bool HasBackRef();
	TIC_CALL static SharedStr GetBackRefStr();

	TIC_CALL bool HasItemContext() const override { return HasBackRef(); }
	TIC_CALL auto ItemAsStr() const->SharedStr override { return GetBackRefStr(); }

protected:
	TIC_CALL void GenerateDescription() override;

private:
	const TreeItemDualRef* m_PrevRef;
};

// *****************************************************************************

#endif // __TIC_TREEITEMDUALREF_H
