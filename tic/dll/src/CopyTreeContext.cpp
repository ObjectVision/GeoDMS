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
#include "TicPCH.h"
#pragma hdrstop

#include "mci/PropDef.h"
#include "mci/ValueClass.h"

#include "CopyTreeContext.h"
#include "AbstrDataItem.h"
#include "AbstrUnit.h"
#include "UnitClass.h"


//----------------------------------------------------------------------
// struct CopyTreeContext
//----------------------------------------------------------------------

CopyTreeContext::CopyTreeContext(TreeItem* dest, const TreeItem* src, 
	CharPtr name, DataCopyMode dcm, LispPtr argList)
:	m_DstContext  (dest)
,	m_SrcRoot     (src)
,	m_DstRootID   (GetTokenID_mt(name))
,	m_Dcm         (dcm)
,	m_ArgList     (argList)
{
}


TokenID CopyTreeContext::GetAbsOrRelUnitID(const AbstrUnit* sourceUnit, const AbstrDataItem* srcADI, AbstrDataItem* dstADI) const
{
	dms_assert(sourceUnit); // orginialSourceUnit
	auto currUnit = sourceUnit;
	if (!currUnit->IsDefaultUnit())
	{
		if (m_SrcRoot->DoesContain(currUnit))
			return TokenID(srcADI->GetTreeParent()->GetFindableName(currUnit));

		while (true)
		{
			if (!currUnit->IsCacheItem() && currUnit->GetTreeParent())
				return TokenID( currUnit->GetFullName() );

			currUnit = AsUnit(currUnit->GetBackRef());
			if (!currUnit)
			{
//				if (isDomain)
//					dstADI->throwItemErrorF("Cannot create a findable name for domain unit");
				break;
			}
		}
	}
	return sourceUnit->GetUnitClass()->GetValueType()->GetID();
}

TokenID CopyTreeContext::GetAbsOrRelNameID(const TreeItem* si, const TreeItem* srcTI, TreeItem* dstTI) const
{
	dms_assert(si);
	dms_assert(m_SrcRoot->DoesContain(srcTI));
	if (m_SrcRoot->DoesContain(si))
		return TokenID(srcTI->GetTreeParent()->GetFindableName(si));

	dms_assert(si);
	while (si->IsCacheItem())
	{
		si = si->GetBackRef();
		if (!si)
			dstTI->throwItemErrorF("Cannot create a findable name");
	}
	SharedStr fullName = si->GetFullName();
	return TokenID(fullName);
}

/********** StoredProperty management **********/

typedef std::set<std::pair<const TreeItem*, AbstrPropDef*> > RemoveRequestSet;
static_ptr<RemoveRequestSet> g_RemoveRequestSet;
std::mutex g_RemoveRequestMutex;

void RemoveStoredPropValues(TreeItem* item)
{
	if (!g_RemoveRequestSet)
		return; // apparently item has no props and all other props have already been destroyed.

	auto lock = std::scoped_lock(g_RemoveRequestMutex);

	RemoveRequestSet::iterator i = g_RemoveRequestSet->lower_bound(RemoveRequestSet::value_type(item, nullptr));
	RemoveRequestSet::iterator e = g_RemoveRequestSet->end();
	while (g_RemoveRequestSet && i != e && i->first == item)
	{
		i->second->RemoveValue(item);
		g_RemoveRequestSet->erase(i++);// i must be incremented before erase which invalidates i
	}
	if (!g_RemoveRequestSet->size())
		g_RemoveRequestSet.reset();
	item->ClearTSF(TSF_HasStoredProps);
}

void RemoveStoredPropValues(TreeItem* item, cpy_mode minCpyMode)
{
	if (!g_RemoveRequestSet)
		return; // apparently item has no props and all other props have already been destroyed.

	auto lock = std::scoped_lock(g_RemoveRequestMutex);

	bool remainingProps = false;
	RemoveRequestSet::iterator i = g_RemoveRequestSet->lower_bound(RemoveRequestSet::value_type(item, nullptr));
	RemoveRequestSet::iterator e = g_RemoveRequestSet->end();

	while (g_RemoveRequestSet && i != e && i->first == item)
	{
		AbstrPropDef* propDef = i->second;
		if (propDef->GetCpyMode() >= minCpyMode)
		{
			propDef->RemoveValue(item);
			g_RemoveRequestSet->erase(i++); // i must be incremented before erase which invalidates i
		}
		else
		{
			remainingProps = true;
			++i;
		}
	}

	if (!g_RemoveRequestSet->size())
		g_RemoveRequestSet.reset();
	if(!remainingProps)
		item->ClearTSF(TSF_HasStoredProps);
}

void TreeItem::AddPropAssoc(AbstrPropDef* propDef) const
{
	assert(IsMetaThread());

	auto lock = std::scoped_lock(g_RemoveRequestMutex);
	assert(!IsCacheItem());
	if (!g_RemoveRequestSet) 
		g_RemoveRequestSet.assign( new RemoveRequestSet );
	g_RemoveRequestSet->insert(RemoveRequestSet::value_type(this, propDef));
	SetTSF(TSF_HasStoredProps);
}

void TreeItem::SubPropAssoc(AbstrPropDef* propDef) const
{
	auto lock = std::scoped_lock(g_RemoveRequestMutex);
	assert(g_RemoveRequestSet);
	RemoveRequestSet::iterator
		it = g_RemoveRequestSet->find(RemoveRequestSet::value_type(this, propDef)),
		e  = g_RemoveRequestSet->end();

	if (it == e)
		return;

	RemoveRequestSet::iterator
		itCopy = it;

	if (it == g_RemoveRequestSet->begin() || (--itCopy)->first != this)
	{
		itCopy = it;
		++itCopy;
		if (itCopy == e || itCopy->first != this)
		{
			ClearTSF(TSF_HasStoredProps); // no other RemoveRequests exists for this except it, which will be erased
		}
	}
	g_RemoveRequestSet->erase(it);
	if (!g_RemoveRequestSet->size())
		g_RemoveRequestSet.reset();
}

//----------------------------------------------------------------------
// struct CopyPropsContext
//----------------------------------------------------------------------

CopyPropsContext::CopyPropsContext(TreeItem*  dst, const TreeItem* src, 
		cpy_mode minCpyMode, bool doClearDest
	)
:	m_Src(src), m_Dst(dst)
,	m_MinCpyMode(minCpyMode)
,	m_DoClearDest(doClearDest)
{
	dms_assert(m_Src && m_Dst && m_Src != m_Dst);
	dms_assert(minCpyMode > cpy_mode::none);
}

CopyPropsContext::~CopyPropsContext()
{}

inline bool CopyPropsContext::MustCopy(AbstrPropDef* propDef)
{
	bool result = propDef->HasNonDefaultValue(m_Src);
	if (result == m_DoClearDest)
		return result;
	
	return propDef->HasNonDefaultValue(m_Src) == m_DoClearDest;
}

void CopyPropsContext::Apply()
{
	if (m_DoClearDest && m_Dst->GetTSF(TSF_HasStoredProps))
		RemoveStoredPropValues(m_Dst, m_MinCpyMode); // oops; only those to copy
	if (m_Src->GetTSF(TSF_HasStoredProps))
	{
		RemoveRequestSet::iterator iSrc = g_RemoveRequestSet->lower_bound(RemoveRequestSet::value_type(m_Src, nullptr));
		RemoveRequestSet::iterator e = g_RemoveRequestSet->end();

		if (!m_DoClearDest && m_Dst->GetTSF(TSF_HasStoredProps))
		{
			dms_assert(!m_DoClearDest); // stored props of m_Dst are already removed
			RemoveRequestSet::iterator iDst = g_RemoveRequestSet->lower_bound(RemoveRequestSet::value_type(m_Dst, nullptr));

			dms_assert(iDst != e && iDst->first == m_Dst); // else TSF_HasStoredProps wouldn't have been set
			while (iSrc != e && iSrc->first == m_Src)
			{
				AbstrPropDef* propDef = iSrc->second;
				if (propDef->GetCpyMode() >= m_MinCpyMode)
				{
					while (iDst != e && iDst->first == m_Dst && iDst->second < propDef)
						++iDst;
					if (iDst == e || iDst->first != m_Dst)
						break; // now also copy the rest
					if (iDst->second > propDef) // do not copy when dest still has (non default) prop values
						propDef->CopyValue(m_Src, m_Dst);
				}
				++iSrc;
			}
		}
		while (iSrc != e && iSrc->first == m_Src)
		{
			AbstrPropDef* propDef = iSrc->second;
			if (propDef->GetCpyMode() >= m_MinCpyMode)
				propDef->CopyValue(m_Src, m_Dst);
			++iSrc;
		}
	}

	CopyExternalProps(m_Src->GetDynamicClass());
}

void CopyPropsContext::CopyExternalProps(const Class* cls)
{
	assert(cls);

	// skip propless classes
	AbstrPropDef* propDef;
	while (true)
	{
		propDef = cls->GetLastCopyablePropDef();
		if (propDef)
			break;
		cls = cls->GetBaseClass();
		if (!cls)
			return;
	}

	// process propDefs of all baseClasses first
	const Class* base = cls->GetBaseClass();
	if (base)
		CopyExternalProps(base);

	// dump props of cls
	while (propDef) 
	{
		if (propDef->GetCpyMode() >= m_MinCpyMode && MustCopy(propDef) )
		{
			assert(propDef->GetSetMode() > set_mode::construction); // set mode obligated or optional, not construction or none
			propDef->CopyValue(m_Src, m_Dst);
		}
		propDef = propDef->GetPrevCopyablePropDef();
	}
}


//----------------------------------------------------------------------
// struct CopyPropsContext
//----------------------------------------------------------------------
#include "AbstrCalculator.h"
#include "act/ActorVisitor.h"


ActorVisitState VisitImplSupplFromIndirectProps(SupplierVisitFlag svf, const ActorVisitor& visitor, const TreeItem* curr)
{
	if (AbstrCalculator::VisitImplSuppl(svf, visitor, curr, curr->GetExpr(), CalcRole::Other) == AVS_SuspendedOrFailed)
		return AVS_SuspendedOrFailed;

	if (curr->GetTSF(TSF_HasStoredProps))
	{
		RemoveRequestSet::iterator iSrc = g_RemoveRequestSet->lower_bound(RemoveRequestSet::value_type(curr, nullptr));
		RemoveRequestSet::iterator e = g_RemoveRequestSet->end();

		while (iSrc != e && iSrc->first == curr)
		{
			AbstrPropDef* propDef = iSrc->second;
			if (propDef->AddImplicitSuppl())
			{
				SharedStr value = propDef->GetValueAsSharedStr(curr);
				if (AbstrCalculator::VisitImplSuppl(svf, visitor, curr, value, CalcRole::Other) == AVS_SuspendedOrFailed)
					return AVS_SuspendedOrFailed;
			}
			++iSrc;
		}
	}
	return AVS_Ready;
}
