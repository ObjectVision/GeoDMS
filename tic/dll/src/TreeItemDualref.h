#pragma once

#if !defined(__TIC_TREEITEMDUALREF_H)
#define __TIC_TREEITEMDUALREF_H

#include "TicBase.h"

#include "act/Actor.h"

// *****************************************************************************
// Section:     DataControllerFlags
// stored in Actor::m_State (last real actor flag is AF_IsPassor = 0x0010)
//	these flags overlap with TreeItem flags
// *****************************************************************************

enum DataControllerFlags
{
	DCF_IsOld              = 0x001 * actor_flag_set::AF_Next,
	DCF_IsTmp              = 0x002 * actor_flag_set::AF_Next,
	DCF_CacheRootKnown     = 0x004 * actor_flag_set::AF_Next,
	DCF_DSM_FileNameKnown  = 0x008 * actor_flag_set::AF_Next,
	DCF_DSM_SmallDataKnown = 0x010 * actor_flag_set::AF_Next,
	DCF_CalcStarted        = 0x020 * actor_flag_set::AF_Next,

	#if defined(MG_DEBUG_DATA)
	DCFD_IsCalculating   = 0x040 * actor_flag_set::AF_Next,
	DCFD_DataCounted     = 0x080 * actor_flag_set::AF_Next,
	#endif

	DCF_IdentifyingExpr  = 0x100 * actor_flag_set::AF_Next,
	DCF_CanChange        = 0x400 * actor_flag_set::AF_Next, // op_is_transient
};

// *****************************************************************************
// Section:     TreeItemDualRef Interface
// *****************************************************************************

struct TreeItemDualRef : Actor
{
	TIC_CALL TreeItemDualRef();
	TIC_CALL ~TreeItemDualRef();
	TreeItemDualRef(const TreeItemDualRef&) = delete;
	TreeItemDualRef(TreeItemDualRef&&) = delete;

	      TreeItem* GetNew()  const { dms_assert(!IsOld()); return const_cast<TreeItem*>(m_Data.get_ptr()); }
	const TreeItem* GetOld()  const { return m_Data; }

	virtual bool IsSymbDC() const { return false; }
	virtual bool CanResultToConfigItem() const { return false; }


	operator       TreeItem* () const { return GetNew(); }
	operator const TreeItem* () const { return GetOld(); }

	const TreeItem* operator ->() const { dms_assert(m_Data); return m_Data; }

	operator       bool      () const { return ( m_Data); }
	bool operator  !         () const { return (!m_Data); }

	bool IsNew() const { return m_Data && !m_State.Get(DCF_IsOld|DCF_IsTmp); }
	bool IsOld() const { return m_Data &&  m_State.Get(DCF_IsOld); }
	bool IsTmp() const { return m_Data &&  m_State.Get(DCF_IsTmp); }
	bool IsTransient() const { return m_State.Get(DCF_IsTmp|DCF_CanChange); };

	bool IsFileNameKnown()  const { return m_State.Get(DCF_DSM_FileNameKnown); }
	bool IsUnitRangeKnown() const { return m_State.Get(DCF_DSM_SmallDataKnown); }

	void operator =(      TreeItem* rhs) { SetNew(rhs); }
	void operator =(const TreeItem* rhs) { SetOld(rhs); }

	TIC_CALL void Clear();

	TIC_CALL void SetNew(      TreeItem* newTI);
	TIC_CALL void SetOld(const TreeItem* oldTI);
	TIC_CALL void SetTmp(      TreeItem* tmpTI);

protected:
	void Set(const TreeItem* newTI, bool isNew);

//	override Actor virtuals
	void StartInterest() const override;
	garbage_t StopInterest () const noexcept override;

	virtual void IncDataInterestCount() const;
	garbage_t DecDataInterestCount() const;
	void DoInvalidate () const override;
	void DoFail(ErrMsgPtr msg, FailType ft) const override;

	friend struct data_swapper;
	friend struct InterestReporter;

	mutable SharedPtr<const TreeItem> m_Data; // sometimes const, new or tmp;
};

// *****************************************************************************

#endif // __TIC_TREEITEMDUALREF_H
