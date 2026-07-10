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

#include <set> // s_ExtPins (std::multiset) is used unconditionally; <set> otherwise only reached via the MG_DEBUG path

//----------------------------------------------------------------------
// DMS interface functions
//----------------------------------------------------------------------


//----------------------------------------------------------------------
// local TreeItemMultiSet manager for debugging resource leaks
//----------------------------------------------------------------------


#if defined(MG_DEBUG)

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
				reportF(MsgCategory::memory, SeverityTypeID::ST_MajorTrace, "{} Leak: {} ({},{}) {}",
					m_ObjName,
					ti->GetDynamicClass()->GetName(), 
					ti->weak_from_this().use_count(), 
					ti->IsCacheItem(), 
					ti->GetFullName().c_str());
			}

			reportF(SeverityTypeID::ST_Error, "{} Leak of {} TreeItems. See EventLog for details.",
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

// External clients pin a TreeItem's lifetime through this C API. The intrusive refcount is gone (TreeItem is
// std::shared_ptr-managed), so a pin is now an OWNING shared_tree_ptr held in this registry; Release drops one.
static std::mutex                      s_ExtPinMutex;
static std::multiset<SharedTreeItem>   s_ExtPins; // ordered by stored pointer (std::shared_ptr operator<)

TIC_CALL void DMS_CONV DMS_TreeItem_AddRef(TreeItem* self)
{
	DMS_CALL_BEGIN

		DBG_START("DMS_TreeItem", "AddRef", false);
		TreeItemContextHandle checkPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_AddRef");
		DBG_TRACE(("self = {}", self->GetName().c_str()));

#if defined(MG_DEBUG)
		dms_assert(refCountAdm);
		refCountAdm->insert(self);
#endif
		{
			std::lock_guard lock(s_ExtPinMutex);
			s_ExtPins.insert(make_shared_tree(self, existing_obj{})); // owning pin
		}

	DMS_CALL_END
}

TIC_CALL void DMS_CONV DMS_TreeItem_Release(TreeItem* self)
{
	DMS_CALL_BEGIN

		DBG_START("DMS_TreeItem", "Release", false);
		TreeItemContextHandle checkPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_Release");
		DBG_TRACE(("self = {}", self->GetName().c_str()));

#if defined(MG_DEBUG)
		dms_assert(refCountAdm);
		TreeItemMultiSetType::iterator p = refCountAdm->find(self);
		dms_assert(p != refCountAdm->end());
		refCountAdm->erase(p);
#endif
		{
			std::lock_guard lock(s_ExtPinMutex);
			auto it = s_ExtPins.find(make_shared_tree(self, existing_obj{}));
			if (it != s_ExtPins.end())
				s_ExtPins.erase(it); // drop one owning pin
		}

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

