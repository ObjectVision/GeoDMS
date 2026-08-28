// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// TreeItem name resolution: sub-item lookup by id and by path, best-match reporting for
// unresolved names, class-checked casts, item creation, and the search that follows
// template instantiations back to their source.

#include "TreeItem.h"
#include "TreeItemFunctionSpec.h"
//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "RtcInterface.h"
#include "mci/ValueClass.h"
#include "mci/ValueComposition.h"
#include "act/ActorLock.h"
#include "act/ActorVisitor.h"
#include "act/InterestRetainContext.h"
#include "act/SupplierVisitFlag.h"
#include "act/TriggerOperator.h"
#include "act/UpdateMark.h"
#include "act/Waiter.h"
#include "dbg/debug.h"
#include "dbg/DmsCatch.h"
#include "mci/PropDef.h"
#include "stg/AbstrStorageManager.h"
#include "utl/Encodes.h"
#include "utl/Environment.h"
#include "utl/FileSystem.h"
#include "utl/IncrementalLock.h"
#include "utl/StrFormat.h"
#include "utl/splitPath.h"
#include "utl/scoped_exit.h"
#include "utl/SourceLocation.h"
#include "xct/DmsException.h"

#include "LispList.h"

#include "AbstrCalculator.h"
#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "DataLockContainers.h"
#include "AbstrUnit.h"
#include "CopyTreeContext.h"
#include "DataArray.h"
#include "DataController.h"
#include "DataLocks.h"
#include "LispTreeType.h"
#include "OperationContext.h"
#include "OperGroups.h"
#include "PropFuncs.h"
#include "SessionData.h"
#include "SupplCache.h"
#include "StateChangeNotification.h"
#include "TreeItemClass.h"
#include "TreeItemSet.h"
#include "TreeItemUtils.h"
#include "TicInterface.h"
#include "TicPropDefConst.h"
#include "TreeItemProps.h"
#include "TreeItemContextHandle.h"
#include "UsingCache.h"
#include "stg/MemoryMappedDataStorageManager.h"

#include <unordered_set>

//----------------------------------------------------------------------
// TreeItem Find Functions
//----------------------------------------------------------------------

SharedTreeItem TreeItem::GetConstSubTreeItemByID(TokenID subItemID) const
{
	// Qualified descent searches only this item and its referred-item chain.
	// In particular, it never ascends to the parent of a referred item.
	const TreeItem* subItem = GetFirstSubItem(); // calls UpdateMetaInfo
	while (true)
	{
		if (!subItem)
		{
			if (auto refItem = mc_RefItem.lock())
			{
				assert(refItem.get() != this);
				return refItem->GetConstSubTreeItemByID(subItemID);
			}
			return {};
		}

		if	(subItem->GetID() == subItemID)
			return make_shared_tree(subItem, existing_obj{});
		subItem = subItem->GetNextItem();
	}
}

SharedTreeItem TreeItem::GetCurrSubTreeItemByID(TokenID subItemID) const
{
	auto subItem = GetCurrFirstSubItem(); // requires UpdateMetaInfo to have been called
	while (true)
	{
		if (!subItem)
		{
			if (auto refItem = mc_RefItem.lock())
			{
				assert(refItem.get() != this);
				return refItem->GetCurrSubTreeItemByID(subItemID);
			}
			return {};
		}

		if (subItem->GetID() == subItemID)
			return make_shared_tree(subItem, existing_obj{});
		subItem = subItem->GetNextItem();
	}
}

TreeItem* TreeItem::GetSubTreeItemByID(TokenID subItemID) // does not UpdateMetaInfo
{
	TreeItem* subItem = _GetFirstSubItem(); // doesn't call UpdateMetaInfo (non const)

	while (subItem && subItem->GetID() != subItemID)
		subItem = subItem->GetNextItem();

	return subItem;
}

TreeItem* TreeItem::GetItem(CharPtrRange subItemNames)
{
	if (subItemNames.empty())
		return this;

	auto ids = NameTreeReg_GetParentAndBranchID(subItemNames);
	if (ids.first.empty()) // subItemNames is an atomic token or parent = root
	{
		if (ids.second.first != subItemNames.first || (ids.second.size() && ids.second.first[0] == '.'))
			return nullptr;
		return GetSubTreeItemByID(GetTokenID(ids.second));
	}
	TreeItem* parent = GetItem(ids.first);
	return (parent) ? parent->GetSubTreeItemByID(GetTokenID(ids.second)) : nullptr;
}

TreeItem* TreeItem::GetBestItem(CharPtrRange subItemNames)
{
	if (subItemNames.empty())
		return this;

	auto ids = NameTreeReg_GetParentAndBranchID(subItemNames);
	if (ids.first.empty()) // subItemNames is an atomic token or parent = root
	{
		if (ids.second.first != subItemNames.first || (ids.second.size() && ids.second.first[0] == '.'))
			return nullptr;
		auto result = GetSubTreeItemByID(GetTokenID(ids.second));
		return result ? result : this;
	}
	TreeItem* parent = GetItem(ids.first);
	if (!parent)
		return nullptr;
	auto result = parent->GetSubTreeItemByID(GetTokenID(ids.second));
	return result ? result : parent;
}

SharedTreeItem TreeItem::GetCurrItem(CharPtrRange subItemNames) const
{
	if (subItemNames.empty())
		return make_shared_tree(this, existing_obj{});

	auto ids = NameTreeReg_GetParentAndBranchID(subItemNames);
	if (ids.first.empty()) // subItemNames is an atomic token or parent = root
	{
		if (ids.second.first != subItemNames.first || (ids.second.size() && ids.second.first[0] == '.'))
			throwItemError("GetCurrItem is not allowed to look outside the accessible search context");
		return GetCurrSubTreeItemByID(GetTokenID(ids.second));
	}
	auto parent = GetCurrItem(ids.first);
	return parent ? parent->GetCurrSubTreeItemByID(GetTokenID(ids.second)) : SharedTreeItem{};
}


SharedTreeItem TreeItem::ResolveItemPath(CharPtrRange subItemNames) const
{
	assert(IsMetaThread());

	if (subItemNames.empty())
		return make_shared_tree(this, existing_obj{});

	auto ids = NameTreeReg_GetParentAndBranchID(subItemNames);
	assert(ids.first.first == subItemNames.first);
	assert(ids.second.second == subItemNames.second);
	if (ids.second.first == subItemNames.first) // subItemNames is an atomic token
	{	
		assert(!ids.second.empty());
		if (ids.second.first[0] == '.')
			return make_shared_tree(FollowDots(ids.second), existing_obj{});

		UpdateMetaInfoIfNotAlready();

		TokenID existingToken = GetExistingTokenID<mt_tag>(ids.second); //to be found token was already created if asserts hold
		if (!IsDefined(existingToken))
			return {};
		return make_shared_tree(FindTreeItemByID(this, existingToken), existing_obj{});
	}
	SharedTreeItem parent = {};
	if (ids.first.empty()) // We start at root.
	{
		MG_CHECK(!IsCacheItem());
		parent = make_shared_tree(static_cast<const TreeItem*>(GetRoot()), existing_obj{});
	}
	else
		parent = ResolveItemPath(ids.first);

	if (!parent)
		return {};
	parent->UpdateMetaInfoIfNotAlready();
//	if (parent->WasFailed(FailType::MetaInfo))
//		parent->ThrowFail();
	return parent->GetConstSubTreeItemByID(GetExistingTokenID(ids.second));
}

auto TreeItem::FindAndVisitItem(CharPtrRange subItemNames, SupplierVisitFlag svf, const ActorVisitor& visitor) const->std::optional<SharedTreeItem>  // directly referred persistent object.
{
	assert(IsMetaThread());
	assert(Test(svf, SupplierVisitFlag::ImplSuppliers));

	if (subItemNames.empty())
		return {};

	auto ids = NameTreeReg_GetParentAndBranchID(subItemNames);
	assert(ids.first.first == subItemNames.first);
	assert(ids.second.second == subItemNames.second);
	if (ids.second.first == subItemNames.first) // subItemNames is an atomic token
	{
		assert(!ids.second.empty());
		if (ids.second.first[0] == '.')
		{
			auto item = FollowDots(ids.second);
			if (visitor.Visit(item) == AVS_SuspendedOrFailed)
				return {};
			return make_shared_tree(item, existing_obj{});
		}

		UpdateMetaInfo();
		TokenID existingToken = GetExistingTokenID<mt_tag>(ids.second); //to be found token was already created if asserts hold
		if (!IsDefined(existingToken))
			return {};
		auto item = FindTreeItemByID(this, existingToken);
		if (visitor.Visit(item) == AVS_SuspendedOrFailed)
			return {};
		return make_shared_tree(item, existing_obj{});
	}
	SharedTreeItem parent;
	if (ids.first.empty()) // We start at root.
	{
		parent = SessionData::Curr()->GetConfigRoot();
		if (visitor.Visit(parent.get()) == AVS_SuspendedOrFailed)
			return {};
	}
	else
	{
		auto  optionalParent = FindAndVisitItem(ids.first, svf, visitor);
		if (!optionalParent)
			return {};

		parent = optionalParent.value();
	}
	if (!parent)
		return {};
	parent->UpdateMetaInfo();

	auto result = parent->GetConstSubTreeItemByID(GetExistingTokenID(ids.second));
	if (visitor.Visit(result.get()) == AVS_SuspendedOrFailed)
		return {};
	return result;
}

static auto FollowBestDots(const TreeItem* self, CharPtrRange dots) noexcept -> BestItemRef
{
	dms_assert(self);
	dms_assert(dots.size());
	while (true)
	{
		if (*dots.first != '.')
			return { make_shared_tree(self, existing_obj{}), SharedStr(dots.first MG_DEBUG_ALLOCATOR_SRC("FollowBestDots")) };
		dots.first++;
		if (dots.first == dots.second)
			return { make_shared_tree(self, existing_obj{}), SharedStr(dots.first MG_DEBUG_ALLOCATOR_SRC("FollowBestDots")) };

		auto parent = self->GetTreeParent();
		if (!parent)
			return { make_shared_tree(self, existing_obj{}), SharedStr(dots.first - 1 MG_DEBUG_ALLOCATOR_SRC("FollowBestDots")) };
		self = parent.get();
	}
}


auto TreeItem::FindBestItem(CharPtrRange subItemNames) const -> BestItemRef
{
	if (subItemNames.empty())
		return { make_shared_tree(this, existing_obj{}), {} };

	auto ids = NameTreeReg_GetParentAndBranchID(subItemNames);
	dms_assert(ids.first.first == subItemNames.first);
	dms_assert(ids.second.second == subItemNames.second);
	if (ids.second.first == subItemNames.first) 
	{
		// subItemNames is an atomic token
		dms_assert(!ids.second.empty());
		if (ids.second.first[0] == '.')
			return FollowBestDots(this, ids.second);
		UpdateMetaInfo();
		TokenID t = GetExistingTokenID(ids.second);
		if (IsDefined(t)) {
			auto result = FindTreeItemByID(this, t);
			if (result)
				return { make_shared_tree(result, existing_obj{}), {} };
		}
		return { make_shared_tree(this, existing_obj{}), SharedStr(ids.second) };
	}

	if (ids.first.empty()) 
	{
		// We start at root, first characted was '/'
		assert(subItemNames[0] == DELIMITER_CHAR);
		assert(ids.second.first = subItemNames.first + 1);

		auto configRoot = SessionData::Curr()->GetConfigRoot();
		if (configRoot)
		{
			configRoot->UpdateMetaInfo();
			auto t = GetExistingTokenID(ids.second);
			if (IsDefined(t))
			{
				auto result = configRoot->GetConstSubTreeItemByID(t);
				if (result)
					return { result, {} };
			}
		}
		return { configRoot, SharedStr(ids.second) };
	}

	// ===== we have first and second here, so we'd have to recurse and combine
	assert(!ids.first.empty());
	//dms_assert(!ids.second.empty()); Wrong if path contains '//' 

	auto parentRef = FindBestItem(ids.first);
	if (!parentRef.first)
		return { make_shared_tree(this, existing_obj{}), SharedStr(subItemNames) };

	if (!parentRef.second.empty())
		return { parentRef.first, parentRef.second + SharedStr(CharPtrRange(ids.first.second, ids.second.second)) };

	parentRef.first->UpdateMetaInfo();
	auto result = parentRef.first->GetConstSubTreeItemByID(GetExistingTokenID(ids.second));
	if (!result)
		return { parentRef.first, SharedStr(ids.second) };
	return { result, {} };
}

const TreeItem* TreeItem_CheckObjCls(const TreeItem* self, const Class* requiredClass)
{
	dms_assert(requiredClass);
	if (!self)
		return nullptr;
	const Class* thisClass = requiredClass->IsDataObjType()
			?	self->GetDynamicObjClass()
			:	self->GetDynamicClass();

	if	(!	thisClass->IsDerivedFrom(requiredClass))
		self->throwItemErrorF("Cannot cast to the requested type: {}", 
			requiredClass->GetName()
		);
	return self;
}

const TreeItem* TreeItem::CheckObjCls(const Class* requiredClass) const
{
	return TreeItem_CheckObjCls(this, requiredClass);
}

TreeItem* TreeItem_CheckCls(TreeItem* self, const Class* requiredClass)
{
	if (!self)
		return nullptr;
	dms_assert(requiredClass);

	const Class* thisClass = requiredClass->IsDataObjType()
			?	self->GetCurrentObjClass()
			:	self->GetDynamicClass();

	if (!	thisClass->IsDerivedFrom(requiredClass))
		self->throwItemErrorF(
			"Cannot cast to the requested type: {}", 
			requiredClass->GetName()
		);
	return self;
}

TreeItem* TreeItem::CheckCls(const Class* requiredClass)
{
	return TreeItem_CheckCls(this, requiredClass);
}

const TreeItem* TreeItem::FollowDots(CharPtrRange dots) const
{
	assert(dots.size());
	const TreeItem* result = this;
	while (true)
	{
		if (*dots.first++ != '.')
			throwItemError("FollowDots: '/' or '.' expected");
		if (dots.first == dots.second)
			return result;

		result = result->GetTreeParent().get();
		if (!result)
			throwItemError("FollowDots: relative pathname ascended above root");
	}
}

auto TreeItem::GetScriptName(const TreeItem* context) const -> SharedStr
{
	assert(*GetName().c_str());
	assert(context);
	assert(context->GetTreeParent());

	return context->GetTreeParent()->GetFindableName(this);
}

TreeItem* CheckedAs(TreeItem* self, const Class* requiredClass)
{
	// check on type of this and return
	if (requiredClass && !self->IsKindOf(requiredClass) )
		self->throwItemErrorF("CreateItem('{}') failed since it is already created as '{}'",
			requiredClass->GetName(), self->GetDynamicClass()->GetName());
	return self; 
}

auto CreateAndInitItem(TreeItem* self, TokenID id, const Class* requiredClass) -> SharedMutableTreeItem
{
	assert(requiredClass);

	// TreeItem-family classes are created through std::make_shared (CreateSharedObj); the raw
	// CreateObj() path remains as a fallback for any class without a shared creator registered.
	SharedMutableTreeItem newSubItem;
	if (requiredClass->HasSharedCreator())
		newSubItem = std::static_pointer_cast<TreeItem>(requiredClass->CreateSharedObj());
	else
		newSubItem = make_shared_tree(debug_cast<TreeItem*>(requiredClass->CreateObj()), newly_obj{});
	assert(newSubItem);

	// Pass a co-owning copy: InitTreeItem moves its copy into the parent's sub-item list (or drops it
	// for a root); `newSubItem` retains a share so the node survives and is returned to the caller.
	InitTreeItem(self, newSubItem, id);

	return newSubItem;
}

auto TreeItem_CreateItem(TreeItem* self, TokenID id, const Class* requiredClass) -> SharedMutableTreeItem
{
	assert(!requiredClass || requiredClass->IsDerivedFrom(TreeItem::GetStaticClass()));

	if (self)
	{
		if (!id)
			return make_shared_tree(CheckedAs(self, requiredClass), existing_obj{}); // borrow an owning share of the existing item

		// find foundSubItem according to firstSubItemName
		TreeItem* foundSubItem = self->GetSubTreeItemByID(id);
		if (foundSubItem)
			return make_shared_tree(CheckedAs(foundSubItem, requiredClass), existing_obj{});
	}

	// create something
	return CreateAndInitItem(self, id, (requiredClass) ? requiredClass : TreeItem::GetStaticClass());
}

auto TreeItem::CreateItem(TokenID id, const Class* requiredClass) -> SharedMutableTreeItem
{
	return TreeItem_CreateItem(this, id, requiredClass);
}

auto TreeItem_CreateItemFromPath(TreeItem* self, CharPtr subItemNames, const Class* requiredClass) -> SharedMutableTreeItem
{
	if (!requiredClass)
		requiredClass = TreeItem::GetStaticClass();

	assert(requiredClass->IsDerivedFrom(TreeItem::GetStaticClass()));
	assert(subItemNames);

	if (*subItemNames == 0) // all subItemNames are processed ??
	{
		if (self)
			return make_shared_tree(CheckedAs(self, requiredClass), existing_obj{});
		else
			return CreateAndInitItem(self, TokenID(), requiredClass);
	}


	// parsing the subItemNames recursively by calling this method on subItemNames parts

	// OPTIMIZE: Reverse order to make use of parent token tables
	CharPtr   restSubItemNames; // new subItemNames after parse
	SharedStr firstSubItemName = splitPathBase(subItemNames, &restSubItemNames); // firstSubItemName of storage after parse
	bool      hasRestSubItems = (*restSubItemNames) != 0;

	if (firstSubItemName.empty() || firstSubItemName[0] == '.')
		// subItemNames started with a '/': traversing an absolute path is not allowed for locating a new object
		// traversing outside the specified namespace is not allowed for locating a new object
		throwItemErrorF(self, "CreateItemFromPath({}): Cannot create new items outside creation context", subItemNames);

	TokenID   firstSubItemID = GetTokenID_mt(firstSubItemName.c_str());
	dms_assert(!firstSubItemID.empty());
	TreeItem* foundSubItem   = nullptr;
	if (self)
		foundSubItem = self->GetSubTreeItemByID(firstSubItemID); // find foundSubItem according to firstSubItemName

	SharedMutableTreeItem createdHolder; // keeps a freshly-created item alive across the recursion when no parent owns it (self==nullptr)
	if (!foundSubItem) // create something
	{
		createdHolder = CreateAndInitItem(self, firstSubItemID, (hasRestSubItems || !requiredClass) ? TreeItem::GetStaticClass() : requiredClass);
		foundSubItem  = createdHolder.get();
		if (!hasRestSubItems)
			return createdHolder;
	}
	assert(foundSubItem);
	return TreeItem_CreateItemFromPath(foundSubItem, restSubItemNames, requiredClass);
}

auto TreeItem::CreateItemFromPath(CharPtr subItemNames, const Class* requiredClass) -> SharedMutableTreeItem
{
	return TreeItem_CreateItemFromPath(this, subItemNames, requiredClass);
}

SharedMutableTreeItem TreeItem::CreateConfigRoot(TokenID id) // static
{
	dms_assert(!s_MakeEndoLockCount);
	SharedMutableTreeItem result = make_shared_tree(new TreeItem, newly_obj{}); // sole owner from birth (fresh std control block, wires shared_from_this)
	InitTreeItem(nullptr, result, id);
	result->SetFreeDataState(true);
	return result;
}
SharedMutableTreeItem TreeItem::CreateCacheRoot() // static
{
	dms_assert(s_MakeEndoLockCount);
	SharedMutableTreeItem result = make_shared_tree(new TreeItem, newly_obj{}); // sole owner from birth (fresh std control block, wires shared_from_this)
	InitTreeItem(nullptr, result, TokenID::GetEmptyID());
	result->SetPassor();
	return result;
}

using template_set = std::set<SharedTreeItem>;

bool TreeItem_IsTemplateInstantiaton(const TreeItem* item)	
{
	assert(item);
	if (!item->HasCalculator())
		return false;

	auto calculator = item->GetCalculator();
	if (!calculator)
		return false;
	if (calculator->HasTemplSource())
		return true;
	return calculator->IsForEachTemplHolder();
}

auto TreeItem_GetTemplateSource(const TreeItem* item) -> SharedTreeItem
{
	assert(item);
	assert(item->HasCalculator());

	auto calculator = item->GetCalculator();
	assert(calculator);
	if (calculator->HasTemplSource())
		return make_shared_tree(calculator->GetTemplSource(), existing_obj{});
	// IsForEachTemplHolder() is true only when applyItem==nullptr,
	// but GetForEachTemplSource() asserts applyItem!=nullptr -- mutually exclusive.
	// Skip holders that have not yet been instantiated.
	if (calculator->IsForEachTemplHolder())
		return {};
	return calculator->GetForEachTemplSource();
}

auto TreeItem_SearchItem_impl(template_set& visitedSet, const TreeItem* searchLoc, TokenID id, const TreeItem* blockedSubItem = nullptr, bool findNextMode = false) -> SharedTreeItem
{
//	if (searchLoc->GetID() == id)
//		return searchLoc;
	if (TreeItem_IsTemplateInstantiaton(searchLoc))
	{
		if (auto templateSource = TreeItem_GetTemplateSource(searchLoc))
		{
			if (visitedSet.find(templateSource) != visitedSet.end())
				return {};

			visitedSet.insert(templateSource);
			const TreeItem* templItem = nullptr;
			while (true)
			{
				templItem = templateSource->WalkConstSubTree(templItem);
				if (!templItem)
					break;

				if (templItem->GetID() == id)
					return make_shared_tree(templItem, existing_obj{});
			}
		}
	}
	for (auto subItem = searchLoc->_GetFirstSubItem(); subItem; subItem = subItem->GetNextItem())
	{
		if (subItem == blockedSubItem)
			findNextMode = false; // start deep searching after this subItem
		else if (!findNextMode)
		{
			if (subItem->GetID() == id)
				return make_shared_tree(subItem, existing_obj{});

			if (auto result = TreeItem_SearchItem_impl(visitedSet, subItem, id))
				return result;
		}
	}

	return {};
}

TIC_CALL auto TreeItem_SearchItem(const TreeItem* searchLoc, TokenID id) -> SharedTreeItem
{
	if (!searchLoc || searchLoc->IsCacheItem())
		return {};
	bool findNextMode = searchLoc->GetID() == id;
	if (!findNextMode) // else we're to do the FindNext 
	{
		if (auto cache = searchLoc->m_UsingCache.get())
		{
			auto result = cache->FindItem(id);
			if (result)
				return result;
		}
	}
	
	template_set alreadyVisited;
	if (auto result = TreeItem_SearchItem_impl(alreadyVisited, searchLoc, id, nullptr, false))
		return result;	

	while (auto parent = searchLoc->GetTreeParent().get())
	{
		if (!findNextMode && parent->GetID() == id)
			return make_shared_tree(parent, existing_obj{});

		if (auto result = TreeItem_SearchItem_impl(alreadyVisited, parent, id, searchLoc, findNextMode))
			return result;
		searchLoc = parent;
	}
	return {};
}
