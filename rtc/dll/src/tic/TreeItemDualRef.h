#pragma once

#if !defined(__TIC_TREEITEMDUALREF_H)
#define __TIC_TREEITEMDUALREF_H

#include "TicBase.h"

#include "act/Actor.h"
#include "dbg/Diagnostics.h"
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
	// kind 1: IsNew -- the DualRef is the primary owner of a freshly created cache RESULT. Element [0] is the
	// cache root result; the following elements own the domain/value units of the result's cache items that
	// must be kept alive (the weak m_DomainUnit/m_ValuesUnit on cache result items would otherwise expire once
	// the operator's unit locals drop -- those units have no other owner). The DcRef owns unit-liveness, not
	// the cache items themselves.
	// TODO: split kind 1 into 1a (single value), 1b (specific domain + default values unit),
	//       1c ({attribute, domain unit, value unit}) and 1d (generic: multiple attributes/units) so the
	//       common cases avoid the vector allocation.
	struct NewResult
	{
		std::shared_ptr<TreeItem> m_Root; // the cache root result; mutable: freshly created, primary-owned here
		std::vector<std::shared_ptr<const TreeItem>> m_KeptUnits; // kept-alive domain/values units
		const TreeItem* root() const noexcept { return m_Root.get(); }
	};
	// kind 2: IsOld -- a reference to a cache SUB-ITEM. Owns the sub-item's cache ROOT (which owns the whole
	// subtree via parent-owns-child, so the sub-item AND its ancestor domain unit stay alive) plus a borrowed
	// pointer to the sub-item itself (kept alive by m_Root).
	struct OldCacheSubItem
	{
		std::shared_ptr<const TreeItem> m_Root;              // owning: keeps the cache subtree (and its units) alive
		const TreeItem*                 m_SubItem = nullptr; // non-owning: the referenced sub-item (alive via m_Root)
	};

	std::variant<
		std::monostate,                   // 0: empty
		NewResult,                        // 1: IsNew  -- cache root result + kept-alive units
		OldCacheSubItem,                  // 2: IsOld  -- cache sub-item; owns its cache root
		std::weak_ptr<const TreeItem>,    // 3: IsOld  -- config item; the tree owns it; .lock() to use
		std::weak_ptr<TreeItem>           // 4: IsTmp  -- instantiation borrow at the calling site; .lock() to use
	> m_Holder;

	// THE accessor: lock to an OWNING snapshot. For the weak arms this keeps the target alive for the
	// caller's whole use (no dangling raw from a dying temp -- that was the crash); for the owning arms it
	// shares ownership. Empty shared_tree_ptr if the arm is empty or the weak target has expired. Callers
	// MUST hold the result while using it: `if (auto p = m_Data.get()) p->...`.
	std::shared_ptr<const TreeItem> get() const
	{
		switch (m_Holder.index())
		{
		case 1: return std::get<1>(m_Holder).m_Root;
		case 2: { auto& o = std::get<2>(m_Holder); return o.m_SubItem ? make_shared_tree(o.m_SubItem, existing_obj{}) : std::shared_ptr<const TreeItem>{}; }
		case 3: return std::get<3>(m_Holder).lock();
		case 4: return std::static_pointer_cast<const TreeItem>(std::get<4>(m_Holder).lock());
		default: return {};
		}
	}
	int             kind()       const noexcept { return int(m_Holder.index()); } // 0 empty,1 new,2 oldCache,3 oldConfig,4 tmp
	void            clear()            noexcept { m_Holder.emplace<std::monostate>(); }

	// Append an owning ref to a unit (or other component) that the kind-1 cache result must keep alive.
	// Deduplicated: KeepResultUnitsAlive runs both at SetNew and (re-)from FuncDC_CreateResult once the
	// subtree is complete, so the same unit may be offered twice.
	void keepAlive(std::shared_ptr<const TreeItem> comp)
	{
		if (comp && m_Holder.index() == 1)
		{
			auto& nr = std::get<1>(m_Holder);
			if (comp == nr.m_Root)
				return;
			for (const auto& e : nr.m_KeptUnits)
				if (e == comp)
					return;
			nr.m_KeptUnits.push_back(std::move(comp));
		}
	}

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
		case 1: return std::get<1>(m_Holder).m_Root.use_count();
		case 2: return std::get<2>(m_Holder).m_Root.use_count();
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
	// GetNew: mutable borrow of the result being built -- legal on kind 1 (root, primary-owned here), kind 4
	// (tmp borrow) and the empty holder (null: "no context yet" = create a parentless cache root).
	// MG_CHECK-enforced also in release: a kind-2/3 holder references a SHARED cache/config item that must
	// never be handed out mutably.
	TreeItem* GetNew() const
	{
		MG_CHECK(!IsOld());
		switch (m_Data.kind())
		{
		case 1: return std::get<1>(m_Data.m_Holder).m_Root.get();
		case 4: return std::get<4>(m_Data.m_Holder).lock().get();
		default: return nullptr;
		}
	}
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

	bool HasBackRef() const { auto p = m_Data.get(); return p && !p->m_BackRef.expired(); }
	SharedStr GetBackRefStr() const // null-tolerant: m_BackRef can be reset concurrently after HasBackRef()
	{
		auto p = m_Data.get();
		auto backRef = p ? p->m_BackRef.lock() : nullptr;
		return backRef ? backRef->GetSourceName() : SharedStr();
	}

	// kind 1: own the result subtree's cache units (called after the operator finished building the result).
	TIC_CALL void CaptureResultUnits();

	// Snapshot of the kind-1 owned refs ([0]=root, [1..]=kept-alive units); empty for other kinds. Lets a
	// scheduled OperationContext co-own the result units for its whole run, so a meta-thread
	// DoInvalidate->Clear() cannot free them while a worker still computes with raw borrows.
	auto GetOwnedSnapshot() const -> std::vector<std::shared_ptr<const TreeItem>>
	{
		if (m_Data.kind() != 1)
			return {};
		auto& nr = std::get<1>(m_Data.m_Holder);
		std::vector<std::shared_ptr<const TreeItem>> result;
		result.reserve(1 + nr.m_KeptUnits.size());
		result.push_back(nr.m_Root);
		result.insert(result.end(), nr.m_KeptUnits.begin(), nr.m_KeptUnits.end());
		return result;
	}

	// Have this (kind-1) cache result take an owning ref to a component it references only weakly (e.g. a
	// UnitCreator-produced domain/values unit) that would otherwise be ownerless once the operator's local drops.
	void KeepAlive(std::shared_ptr<const TreeItem> comp) { m_Data.keepAlive(std::move(comp)); }

protected:
	void Set(const TreeItem* newTI, bool isNew);
	void KeepResultUnitsAlive(const TreeItem* root); // kind 1: own the result subtree's cache units (liveness)

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
