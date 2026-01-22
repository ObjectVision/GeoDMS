// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

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
	using TreeItemMultiSetType = std::multiset<const TreeItem*>;

	struct ItemCountAdm : std::unique_ptr<TreeItemMultiSetType>
	{
		ItemCountAdm(CharPtr objName)
			:	std::unique_ptr<TreeItemMultiSetType>(new TreeItemMultiSetType)
			,	m_ObjName(objName)
		{}

		~ItemCountAdm()
		{
			assert(get());

			UInt32 n = get()->size();
			if (!n) 
				return;

			TreeItemMultiSetType::iterator i = get()->begin();
			TreeItemMultiSetType::iterator e = get()->end();
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

