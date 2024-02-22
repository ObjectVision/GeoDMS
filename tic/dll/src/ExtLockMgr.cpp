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

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------
#include "TicInterface.h"

#include "act/InterestRetainContext.h"
#include "act/TriggerOperator.h"
#include "dbg/debug.h"
#include "dbg/DmsCatch.h"

#include "TreeItemClass.h"
#include "TreeItemContextHandle.h"

//----------------------------------------------------------------------
// DMS interface functions
//----------------------------------------------------------------------


//----------------------------------------------------------------------
// local TreeItemMultiSet manager for debugging resource leaks
//----------------------------------------------------------------------


#if defined(MG_DEBUG)

#include "set/QuickContainers.h"

namespace {
	typedef std::multiset<const TreeItem*> TreeItemMultiSetType;

	struct ItemCountAdm : OwningPtr<TreeItemMultiSetType>
	{
		ItemCountAdm(CharPtr objName)
			:	OwningPtr(new TreeItemMultiSetType)
			,	m_ObjName(objName)
		{}

		~ItemCountAdm()
		{
			dms_assert(has_ptr());

			UInt32 n = get_ref().size();
			if (!n) 
				return;

			TreeItemMultiSetType::iterator i = get_ref().begin();
			TreeItemMultiSetType::iterator e = get_ref().end();
			while (i!=e)
			{
				const TreeItem* ti = *i++;
				reportF(MsgCategory::memory, SeverityTypeID::ST_MajorTrace, "%s Leak: %s (%d,%d) %s",
					m_ObjName,
					ti->GetDynamicClass()->GetName(), 
					ti->GetRefCount(), 
					ti->IsCacheItem(), 
					ti->GetFullName().c_str());
			}

			reportF(SeverityTypeID::ST_Error, "%s Leak of %d TreeItems. See EventLog for details.",
				m_ObjName,
				n
			);
		}

	private:
		CharPtr              m_ObjName;
	};

	ItemCountAdm 
		refCountAdm     ("RefCount"),
		interestCountAdm("Interest");

}	// anonymous namespace

#endif // defined(MG_DEBUG)

//----------------------------------------------------------------------
// C style Interface functions to AddRef & Release
//----------------------------------------------------------------------

TIC_CALL void DMS_CONV DMS_TreeItem_AddRef(TreeItem* self)
{
	DMS_CALL_BEGIN

		DBG_START("DMS_TreeItem", "AddRef", false);
		TreeItemContextHandle checkPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_AddRef");
		DBG_TRACE(("self = %s", self->GetName().c_str()));

#if defined(MG_DEBUG)
		dms_assert(refCountAdm);
		refCountAdm->insert(self);
#endif
		self->IncRef();

	DMS_CALL_END
}

TIC_CALL void DMS_CONV DMS_TreeItem_Release(TreeItem* self)
{
	DMS_CALL_BEGIN

		DBG_START("DMS_TreeItem", "Release", false);
		TreeItemContextHandle checkPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_Release");
		DBG_TRACE(("self = %s", self->GetName().c_str()));

#if defined(MG_DEBUG)
		dms_assert(refCountAdm);
		TreeItemMultiSetType::iterator p = refCountAdm->find(self);
		dms_assert(p != refCountAdm->end());
		refCountAdm->erase(p);
#endif
		self->Release();

	DMS_CALL_END
}

/********** InterestCount and managed actors **********/


//----------------------------------------------------------------------
// C style Interface functions for InterestCounting
//----------------------------------------------------------------------

void DMS_CONV TreeItem_IncInterestCountImpl(const TreeItem* self)
{
	DMS_CALL_BEGIN

		SuspendTrigger::Resume();

		TreeItemContextHandle checkPtr(self, 0, "DMS_TreeItem_IncInterestCount");

		SilentInterestRetainContext irc;

		self->IncInterestCount(); // can throw

#if defined(MG_DEBUG)
		dms_assert(interestCountAdm);
		interestCountAdm->insert(self); // if this throws we have a leaked Interest, but this is only in the debug build.
#endif // defined(MG_DEBUG)

		self->IncRef(); // doesn't throw

	DMS_CALL_END
}


TIC_CALL void DMS_CONV DMS_TreeItem_IncInterestCount(const TreeItem* self)
{
	DMS_SE_CALL_BEGIN

		MG_CHECK2(self, "invalid null pointer in DMS_TreeItem_IncInterestCount");
		TreeItem_IncInterestCountImpl(self);

	DMS_SE_CALL_END
}

TIC_CALL void DMS_CONV DMS_TreeItem_DecInterestCount(const TreeItem* self)
{
	DMS_CALL_BEGIN

		SuspendTrigger::Resume();

		TreeItemContextHandle checkPtr(self, 0, "DMS_TreeItem_DecInterestCount");

#if defined(MG_DEBUG)
		dms_assert(interestCountAdm);
		TreeItemMultiSetType::iterator p = interestCountAdm->find(self);
		dms_assert(p != interestCountAdm->end());
		interestCountAdm->erase(p);
#endif // defined(MG_DEBUG)

		self->DecInterestCount();
		self->Release();

	DMS_CALL_END
}

TIC_CALL UInt32 DMS_CONV DMS_TreeItem_GetInterestCount(const TreeItem* self)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_GetInteresetCount");
		return self->GetInterestCount();

	DMS_CALL_END
	return -1;
}


//----------------------------------------------------------------------
// C style Interface functions for Dynamic Stored PropDefs
//----------------------------------------------------------------------

#include "StoredPropDef.h"
#include "mci/PropdefEnums.h"

TIC_CALL AbstrPropDef* DMS_CONV DMS_StoredStringPropDef_Create(CharPtr name)
{
	DMS_CALL_BEGIN

		return new StoredPropDef<TreeItem, SharedStr>(name, set_mode::optional, xml_mode::element, cpy_mode::all, false);

	DMS_CALL_END
	return 0;
}

TIC_CALL void         DMS_CONV DMS_StoredStringPropDef_Destroy(AbstrPropDef* apd)
{
	DMS_CALL_BEGIN

		ObjectContextHandle checkPtr(apd, AbstrPropDef::GetStaticClass(), "DMS_StoredStringPropDef_Destroy");
		delete apd;

	DMS_CALL_END
}

