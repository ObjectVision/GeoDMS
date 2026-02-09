// Copyright (C) 1998-2026 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__COPYTREECONTEXT_H)
#define __COPYTREECONTEXT_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "mci/PropdefEnums.h"
#include "utl/IncrementalLock.h"
#include "ptr/SharedPtr.h"

#include "TreeItem.h"

struct DcRefListElem;

enum class DataCopyMode
{
	default_ = 0,
	CopyExpr = 0x01, // YES: TemplateInstance, sub-items from cache (different root); NO: create refs to org (f.e. parameter instantiation)
	NoRoot = 0x02, // YES: for copying sub-items from data-cache or case parameter value (without destroying the expr or calc-ref of the referent)
	MakeEndogenous = 0x04, // YES: don't save this in config file and (NYI) remove when (suppliers van) meta info generating operator have invalidated
	SetInheritFlag = 0x08,
	MergeProps = 0x10,
	DontCreateNew = 0x20,
	MakePassor = 0x40,
	DontCopySubItems = 0x80,
	DontUpdateMetaInfo = 0x100,
	CopyReferredItems = 0x200,
	InFenceOperator = 0x400,
};

inline DataCopyMode operator |(DataCopyMode a, DataCopyMode b) { return DataCopyMode(int(a) | int(b)); }
inline bool         operator &(DataCopyMode a, DataCopyMode b) { return int(a) & int(b); }

//----------------------------------------------------------------------
// struct CopyTreeContext
//----------------------------------------------------------------------

struct CopyTreeContext
{
	TIC_CALL CopyTreeContext(TreeItem* dest, const TreeItem* src, CharPtr name, DataCopyMode dcm, LispPtr argList = LispPtr());
	
	bool MustCopyExpr      () const { return (m_Dcm & DataCopyMode::CopyExpr      ); }
	bool MustCopyRoot      () const { return (m_Dcm & DataCopyMode::NoRoot        ) == false; }
	bool MustMakeEndogenous() const { return (m_Dcm & DataCopyMode::MakeEndogenous); }
	bool MergeProps        () const { return (m_Dcm & DataCopyMode::MergeProps    ); }
	bool DontCreateNew     () const { return (m_Dcm & DataCopyMode::DontCreateNew ); }
	bool MustMakePassor    () const { return (m_Dcm & DataCopyMode::MakePassor    ); }
	bool SetInheritFlag    () const { return (m_Dcm & DataCopyMode::SetInheritFlag); }
	bool DontCopySubItems  () const { return (m_Dcm & DataCopyMode::DontCopySubItems); }
	bool MustUpdateMetaInfo() const { return (m_Dcm & DataCopyMode::DontUpdateMetaInfo) == false; }
	bool InFenceOperator   () const { return (m_Dcm & DataCopyMode::InFenceOperator); }
	bool CopyReferredItems () const { return (m_Dcm & DataCopyMode::CopyReferredItems); }
	DataCopyMode GetDCM()          const { return m_Dcm; }

	cpy_mode  MinCpyMode        (bool isRoot) const 
	{
		return MustCopyExpr() 
			?	(isRoot ? cpy_mode::expr : cpy_mode::expr_noroot)
			:	cpy_mode::all;
	}

	TIC_CALL TokenID GetAbsOrRelNameID(const TreeItem*  si, const TreeItem* srcTI, TreeItem* dstTI) const;
	TIC_CALL TokenID GetAbsOrRelUnitID(const AbstrUnit* su, const AbstrDataItem* srcADI, AbstrDataItem* dstADI) const;

	OwningPtr<TreeItem> Apply()
	{
//		dms_assert(m_DstContext);

		if (MustMakeEndogenous()) {
			StaticStIncrementalLock<TreeItem::s_MakeEndoLockCount> makeEndoLock;
			return ApplyImpl();
		}
		return ApplyImpl();
	}
	OwningPtr<TreeItem> ApplyImpl()
	{
		return m_SrcRoot->Copy(m_DstContext, m_DstRootID, *this);
	}
	const TreeItem* FindAnchestor(const TreeItem* src) const;

	TreeItem*            m_DstContext;
	const TreeItem*      m_SrcRoot;
	TokenID              m_DstRootID;
	mutable TreeItem*    m_DstRoot = nullptr;
	LispPtr              m_ArgList;
//	phase_number         m_PhaseNumber = 0;
	DataCopyMode         m_Dcm;
	using copy_pair = std::pair<SharedPtr<TreeItem>, SharedPtr<const TreeItem> >;
	std::vector<copy_pair> m_AnchestorStack;

 private:
	StaticMtIncrementalLock<TreeItem::s_NotifyChangeLockCount> m_NotifyChangeLock;
};

struct AnchestorStackGuard
{
	TIC_CALL AnchestorStackGuard(CopyTreeContext& ctc, SharedPtr<TreeItem> dst, SharedPtr<const TreeItem> src)
		: m_CTC(ctc)
	{
		m_CTC.m_AnchestorStack.emplace_back(dst, src);
	}
	TIC_CALL ~AnchestorStackGuard()
	{
		m_CTC.m_AnchestorStack.pop_back();
	}
	CopyTreeContext& m_CTC;
};

//----------------------------------------------------------------------
// struct CopyPropsContext
//----------------------------------------------------------------------

struct CopyPropsContext 
{
	TIC_CALL CopyPropsContext(TreeItem*  dst, const TreeItem* src, cpy_mode minCpyMode, bool doClearDest);

	TIC_CALL ~CopyPropsContext();

	TIC_CALL void Apply();

private:
	bool MustCopy(AbstrPropDef* propDef);
	void CopyExternalProps (const Class* cls);

	const TreeItem* m_Src;
	      TreeItem* m_Dst;
	cpy_mode        m_MinCpyMode;
	bool            m_DoClearDest;
};

//----------------------------------------------------------------------
// VisitImplSupplFromIndirectProps
//----------------------------------------------------------------------

ActorVisitState VisitImplSupplFromIndirectProps(SupplierVisitFlag svf, const ActorVisitor& visitor, const TreeItem* curr);

#endif //!defined(__COPYTREECONTEXT_H)
