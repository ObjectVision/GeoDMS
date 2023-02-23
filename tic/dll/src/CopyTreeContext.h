//<HEADER> 
/*
Data & Model Server (DMS) is a server written in C++ for DSS applications. 
Version: see srv/dms/rtc/dll/src/RtcVersion.h for version info.

Copyright (C) 1998-2004  YUSE GSO Object Vision BV. 

Documentation on using the Data & Model Server software can be found at:
http://www.ObjectVision.nl/DMS/

See additional guidelines and notes in srv/dms/Readme-srv.txt 

This library is free software; you can use, redistribute, and/or
modify it under the terms of the GNU General Public License version 2 
(the License) as published by the Free Software Foundation,
provided that this entire header notice and readme-srv.txt is preserved.

See LICENSE.TXT for terms of distribution or look at our web site:
http://www.objectvision.nl/DMS/License.txt
or alternatively at: http://www.gnu.org/copyleft/gpl.html

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details. However, specific warranties might be
granted by an additional written contract for support, assistance and/or development
*/
//</HEADER>
#pragma once

#if !defined(__COPYTREECONTEXT_H)
#define __COPYTREECONTEXT_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "mci/PropDefEnums.h"
#include "utl/IncrementalLock.h"
#include "ptr/PersistentSharedObj.h"
#include "ptr/SharedPtr.h"
#include "dbg/DebugCast.h"

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
	CopyAlsoReferredItems = 0x200,
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
	bool CopyReferredItems () const { return (m_Dcm & DataCopyMode::CopyAlsoReferredItems); }

	DataCopyMode GetDCM()          const { return m_Dcm; }

	cpy_mode  MinCpyMode        (bool isRoot) const 
	{
		return MustCopyExpr() 
			?	(isRoot ? cpy_mode::expr : cpy_mode::expr_noroot)
			:	cpy_mode::all;
	}

	TIC_CALL TokenID GetAbsOrRelNameID(const TreeItem*  si, const TreeItem* srcTI, TreeItem* dstTI) const;
	TIC_CALL TokenID GetAbsOrRelUnitID(const AbstrUnit* su, const AbstrDataItem* srcADI, AbstrDataItem* dstADI) const;

	TreeItem* Apply()
	{
//		dms_assert(m_DstContext);

		if (MustMakeEndogenous()) {
			StaticStIncrementalLock<TreeItem::s_MakeEndoLockCount> makeEndoLock;
			return ApplyImpl();
		}
		return ApplyImpl();
	}
	TreeItem* ApplyImpl()
	{
		return
			debug_cast<const TreeItem*>(m_SrcRoot)
			->Copy(debug_cast<TreeItem*>(m_DstContext), m_DstRootID, *this);
	}


	TreeItem*            m_DstContext;
	const TreeItem*      m_SrcRoot;
	TokenID              m_DstRootID;
	mutable TreeItem*    m_DstRoot = nullptr;
	LispPtr              m_ArgList;

private:
	DataCopyMode            m_Dcm;
	StaticMtIncrementalLock<TreeItem::s_NotifyChangeLockCount> m_NotifyChangeLock;
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
