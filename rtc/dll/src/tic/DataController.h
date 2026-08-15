// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__DATACONTROLLER_H)
#define __DATACONTROLLER_H

#include "TicBase.h"

#include "LispRef.h"
#include "ptr/SharedObj.h"

#include <map>
#include <set>

#include "TreeItem.h"
#include "TreeItemDualRef.h"


#if defined(MG_DEBUG_DATA)
#	define MG_DEBUG_DCDATA
#endif

// *****************************************************************************
// Section:     DataController Interface
// *****************************************************************************

// the set of IntegrityCheck conditions that evaluating a key expression is certain to evaluate;
// elements are the interned cond LispRefs (arg 2 of an integrity_check application), so
// membership is a pointer comparison and the strong refs keep the conds alive with the set (#1182)
struct lisp_ref_less
{
	using is_transparent = void; // allow probing with a LispPtr without creating a LispRef
	template <typename L, typename R>
	bool operator()(const L& lhs, const R& rhs) const { return lhs.get() < rhs.get(); }
};
using check_set     = std::set<LispRef, lisp_ref_less>;
using check_set_ptr = SharedThingPtr<check_set>;

// A conjunctive condition enforces each of its conjuncts, so a check_set holds the conjuncts as
// separate members rather than the condition as a whole: checks that share a conjunct then
// recognise each other, and the sets of a function's arguments merge on atoms. The converse does
// not hold -- enforcing 'a' says nothing about 'b' -- so a candidate condition may only be
// skipped when EVERY one of its atoms is already enforced.
TIC_CALL void InsertCheckAtoms(check_set& dest, LispPtr cond);
TIC_CALL bool AreCheckAtomsImplied(const check_set& enforced, LispPtr cond);

struct DataController : TreeItemDualRef
{
	typedef TreeItemDualRef base_type;
	typedef LispRef DataControllerKey;

	TIC_CALL FutureData CalcResultWithValuesUnits()  const;
	virtual SharedTreeItem MakeResult()  const = 0;
	virtual FutureData CallCalcResult(std::shared_ptr<Explain::Context> context = {})  const;
	TIC_CALL FutureData CalcCertainResult()  const;
	virtual const Class* GetResultCls() const;
	LispPtr GetLispRef()    const { return m_Key; }

//	specific support for Calcrule Source Tracking
	virtual bool HasTemplSource() const { return false; }
	virtual auto GetTemplSource() const -> SharedTreeItem { return {}; }
	virtual bool IsForEachTemplHolder()  const { return false; }
	virtual auto  GetForEachTemplSource() const -> SharedTreeItem { return {}; }

	virtual bool IsCalculating() const;

	TIC_CALL check_set_ptr GetImpliedChecks() const; // meta-thread only; lazy, derived from m_Key

protected:
	TIC_CALL void DoInvalidate () const override;
	TIC_CALL SharedStr GetSourceName() const override; // override Object

	DataController(LispPtr keyExpr);
	virtual ~DataController();
public:
	TIC_CALL ActorVisitState DoUpdate() override;

#if defined(MG_DEBUG_DCDATA)
public:
	SharedStr md_sKeyExpr;  friend void DataControllerExit();
#endif	

protected:
	DataControllerKey m_Key;  // expr + root of context
	mutable check_set_ptr m_ImpliedChecks; // #1182: null = not yet derived from m_Key; shared empty instance = derived, none

	DECL_RTTI(TIC_CALL, Class)
};

TIC_CALL DataControllerRef GetOrCreateDataController(LispPtr keyExpr);
TIC_CALL DataControllerRef GetExistingDataController(LispPtr keyExpr);

/********** DcRefListElem **********/

struct DcRefListElem
{
	~DcRefListElem()
	{
		// mitigate stack overflow issues
		while (m_Next)
		{
			std::unique_ptr<DcRefListElem> next = std::move(m_Next);
			m_Next = std::move(next->m_Next);
			// next will be deleted by its destructor
		}
	}

	DataControllerRef              m_DC;
	std::unique_ptr<DcRefListElem> m_Next;
};

/********** DataControllerContextHandle **********/

#include "dbg/DebugContext.h"

struct DataControllerContextHandle : ContextHandle
{
	DataControllerContextHandle(const DataController* self) : m_DC(self) {}

protected:
	void GenerateDescription() override;

protected:
	const DataController* m_DC;
};

// *****************************************************************************

#endif // __DATACONTROLLER_H
