// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

// *****************************************************************************
//
// storage_read_table / storage_read_value (#587)
//
// The read of a stored item as an operator application. A table unit that is read from a storage
// gets the calculator
//
//     storage_read_table(spec, '/full/name', uint32, 'attr1', vu1, 'attr2', vu2, ...)
//
// and each of its stored attributes subitem(<that key>, 'attr1') through the cache-root merge
// (TreeItemMetaInfo.cpp, TreeItem_InstallStorageReadCalculator); a stored parameter gets
//
//     storage_read_value(spec, '/full/name', vu)
//
// where spec = union_data(uint2, do(ES..., storageName), storageType, sqlString, tableName) is the
// storage spec of decision 2 of doc/development/storage-read-operators.md, and the full name is the
// transitional argument that finds the configured item and its storage holder until S4 makes the spec
// self-contained. Both operators follow PhaseContainer: CreateResultCaller builds the result skeleton
// (the unit and one member per stored attribute), PreCalcUpdate collects the members that carry
// interest and are not read yet into the root's m_ReadAssets, and CalcResult reads exactly those under
// the storage manager's critical section, taken at the scheduling gate through
// GetRequiredStorageManager (#933). A member that gains interest after the read completed re-enters the
// operator for that member alone (oper_policy::members_on_demand, the #1167 re-entry).
//
// *****************************************************************************

#include "act/any.h"
#include "dbg/Diagnostics.h"
#include "dbg/SeverityType.h"
#include "mci/ValueComposition.h"
#include "mci/ValueWrap.h"
#include "utl/FileSystem.h" // IsFileOrDirAccessible: the mapping arm
#include "utl/scoped_exit.h"
#include "utl/splitPath.h"  // DelimitedConcat
#include "utl/StrFormat.h"
#include "xct/DmsException.h"

#include "AbstrDataItem.h"
#include "AbstrUnit.h"
#include "DataArray.h"
#include "DataArrayValue.h"
#include "DataItemClass.h"
#include "DataLocks.h"
#include "ItemLocks.h"
#include "LispTreeType.h"
#include "MoreDataControllers.h"
#include "OperationContext.h"
#include "OperGroups.h"
#include "Operator.h"
#include "SessionData.h"
#include "TreeItemClass.h"
#include "Unit.h"
#include "UnitClass.h"
#include "stg/AbstrStorageManager.h"

#include <optional>

namespace {

// *****************************************************************************
// the state of one read
// *****************************************************************************

// One member of a read: the cache item that receives the data, the configured item whose storage meta
// info says where it comes from, and the interest that keeps the member wanted between PreCalcUpdate
// and CalcResult.
struct storage_read_member
{
	SharedTreeItemInterestPtr m_Keep;
	TreeItem*                 m_CacheItem = nullptr; // owned by the result root
	SharedTreeItem            m_ConfigItem;
	StorageMetaInfoPtr        m_MetaInfo;
};

// Kept in the result root's m_ReadAssets from PreCalcUpdate to the end of CalcResult, as PhaseContainer
// keeps its phase_resource; TSF_ReadAssetsInterestScoped releases it when the read is abandoned.
struct storage_read_request
{
	SharedTreeItem                       m_ConfigItem; // the configured table unit, attribute or parameter
	SharedTreeItem                       m_Holder;     // its storage holder
	SharedPtr<AbstrStorageManager>       m_SM;
	SharedPtr<NonmappableStorageManager> m_NSM;        // m_SM when it reads through StorageReadHandle; null for a memory-mapped store, whose members are mapped
	std::vector<storage_read_member>     m_Pending;    // in read order: the root first when it is to be read

	bool IsPending(const TreeItem* cacheItem) const
	{
		for (auto& m : m_Pending)
			if (m.m_CacheItem == cacheItem)
				return true;
		return false;
	}
};

SharedStr ArgString(const ArgRefs& args, arg_index i)
{
	return GetTheCurrValue<SharedStr>(GetItem(args[i]));
}

// arg 1: the full name of the configured item, resolved against the configuration root
SharedTreeItem ResolveConfigItem(const ArgRefs& args)
{
	auto fullName = ArgString(args, 1);
	auto sd = SessionData::Curr();
	MG_CHECK(sd);
	auto configRoot = sd->GetConfigRoot();
	MG_CHECK(configRoot);
	auto path = fullName.AsRange();
	while (!path.empty() && path.first[0] == '/')
		++path.first;
	auto item = configRoot->ResolveItemPath(path);
	if (!item)
		throwDmsErrF("storage read: configured item {} not found", fullName);
	return item;
}

storage_read_request& GetOrCreateRequest(TreeItem* root, const ArgRefs& args)
{
	root->UpdateMetaInfo(); // a passor: marks it MetaInfo-ready, which the DataWriteLock of a member asserts on a worker thread
	if (!root->m_ReadAssets.has_value())
	{
		root->m_ReadAssets.emplace<storage_read_request>();
		root->SetTSF(TSF_ReadAssetsInterestScoped); // abandoned before CalcResult completes: StopInterest releases the request and the interest it holds
	}
	MG_CHECK(root->m_ReadAssets.is_a<storage_read_request>());
	auto& req = root->m_ReadAssets.Get<storage_read_request>();
	if (!req.m_SM)
	{
		req.m_ConfigItem = ResolveConfigItem(args);
		req.m_Holder = req.m_ConfigItem->GetStorageParent(false);
		if (!req.m_Holder)
			req.m_ConfigItem->throwItemError("storage read: the item has no storage to read from");
		auto sm = req.m_Holder->GetStorageManager();
		MG_CHECK(sm);
		req.m_SM = sm;
		req.m_NSM = dynamic_cast<NonmappableStorageManager*>(sm);
	}
	return req;
}

// Collect cacheItem when it is wanted and not read yet: the storage meta info is that of its configured
// counterpart, with the read redirected to the cache item.
void CollectMember(storage_read_request& req, TreeItem* cacheItem, const TreeItem* configItem)
{
	auto keep = cacheItem->GetInterestPtrOrNull();
	if (!keep)
		return; // nobody asked for it: not read (the point of #587)
	if (IsDataReady(cacheItem) || req.IsPending(cacheItem))
		return;
	MG_CHECK(configItem);
	StorageMetaInfoPtr smi;
	if (req.m_NSM) // a memory-mapped store needs none: MapMember names the file by the described item
	{
		smi = req.m_NSM->GetMetaInfo(req.m_Holder.get(), const_cast<TreeItem*>(configItem), StorageAction::read);
		MG_CHECK(smi);
		smi->SetDataTarget(cacheItem);
	}
	req.m_Pending.emplace_back(storage_read_member{ std::move(keep), cacheItem, make_shared_tree(configItem, existing_obj{}), std::move(smi) });
}

// the configured counterpart of a member of the result root: the same relative path, through raw links
const TreeItem* FindConfigMember(const TreeItem* configRoot, const TreeItem* cacheRoot, const TreeItem* cacheItem)
{
	auto relName = cacheItem->GetRelativeName(cacheRoot);
	auto path = relName.AsRange();
	auto curr = configRoot;
	while (curr && !path.empty())
	{
		auto sep = std::find(path.first, path.second, '/');
		TokenID id = GetTokenID_mt(path.first, sep);
		const TreeItem* found = nullptr;
		for (auto sub = curr->_GetFirstSubItem(); sub; sub = sub->GetNextItem())
			if (sub->GetNameID() == id)
			{
				found = sub;
				break;
			}
		curr = found;
		path.first = (sep == path.second) ? sep : sep + 1;
	}
	return curr;
}

// The read of a member of a memory-mapped store (MMD): its file is mapped and becomes the member's data
// object, as PrepareDataUsageImpl did for the configured item before #587. No storage handle: the
// manager has no ReadDataItem; opening the store once establishes its existence and its lock file.
void MapMember(storage_read_request& req, storage_read_member& m)
{
	auto sm = req.m_SM.get();
	MG_CHECK(IsDataItem(m.m_CacheItem));
	auto cacheItem = AsDataItem(m.m_CacheItem);

	auto relName = m.m_ConfigItem->GetRelativeName(req.m_Holder.get());
	if (relName.empty())
		relName = SharedStr("@main");
	auto fileName = DelimitedConcat(sm->GetNameStr().AsRange(), relName.AsRange());
	if (!IsFileOrDirAccessible(fileName))
	{
		cacheItem->Fail("Data not found in .MMD storage folder", FailType::Data);
		return;
	}
	if (!sm->IsOpen())
	{
		// open and close under the section, as the configured item's read did: the meta info's
		// destructor closes the store, and CloseStorage requires the section to be held
		AbstrStorageManager::lock_t lock(sm->m_CriticalSection);
		if (!sm->IsOpen())
		{
			StorageMetaInfo smi(req.m_Holder.get(), m.m_ConfigItem.get());
			sm->OpenForRead(smi);
		}
	}
	auto avu = AbstrValuesUnit(cacheItem);
	auto fh = OpenFileData(cacheItem, avu ? avu->GetTiledRangeData().get() : nullptr, fileName);
	if (!fh)
	{
		cacheItem->Fail("Cannot open data in .MMD storage folder", FailType::Data);
		return;
	}
	cacheItem->m_DataObject.reset(fh.release());
}

// CalcResult of the operators: read the pending members, one storage handle each, under the storage
// manager's critical section, which the scheduling gate acquired when this operation named the manager
// through GetRequiredStorageManager (#933) and which is taken here otherwise. A memory-mapped store
// has no handle and no gate: its members are mapped.
bool ReadPendingMembers(TreeItem* root)
{
	MG_CHECK(root->m_ReadAssets.is_a<storage_read_request>());
	auto& req = root->m_ReadAssets.Get<storage_read_request>();
	MG_CHECK(req.m_SM);

	// The request goes with this pass, whichever way it ends: it holds interest in the members and
	// their meta info, which a failed read must not keep alive (a dangling interest keeps the keys,
	// and their string literals, alive up to the teardown of the token registry).
	auto releaseRequest = [root, &req]() noexcept
	{
		req.m_Pending.clear(); // meta infos first, while the storage section below is still held
		root->m_ReadAssets.Clear();
		root->ClearTSF(TSF_ReadAssetsInterestScoped);
	};

	if (!req.m_NSM)
	{
		auto release = make_scoped_exit([&releaseRequest] { releaseRequest(); });
		for (auto& m : req.m_Pending)
		{
			if (m.m_CacheItem == root && req.m_Pending.size() > 1)
				root->throwItemError("storage read: a memory-mapped store maps attributes and parameters, not tables");
			auto progressMsg = mySSPrintF("Map {} from {}", m.m_ConfigItem->GetFullName(), req.m_SM->GetNameStr());
			reportD(MsgCategory::storage_read, SeverityTypeID::ST_MajorTrace, progressMsg.c_str());
			MapMember(req, m);
		}
		return true;
	}

	auto sm = req.m_NSM;
	std::optional<AbstrStorageManager::lock_t> csLock;
	if (auto oc = CancelableFrame::CurrActive(); oc && oc->m_StorageLockHeld && oc->m_RequiredStorageManager.get() == sm.get())
	{
		oc->m_StorageLockHeld = false; // this frame releases it, when the last handle is gone
		csLock.emplace(sm->m_CriticalSection, adopt_storage_lock);
	}
	else
		csLock.emplace(sm->m_CriticalSection);

	auto release = make_scoped_exit([&releaseRequest] { releaseRequest(); }); // after csLock: destroyed before it, so the meta infos close the storage under the section

	auto storageName = sm->GetNameStr();
	for (auto& m : req.m_Pending)
	{
		auto cacheItem = m.m_CacheItem;
		auto progressMsg = mySSPrintF("Read {} from {}", m.m_ConfigItem->GetFullName(), storageName);
		reportD(MsgCategory::storage_read, SeverityTypeID::ST_MajorTrace, progressMsg.c_str());
		try {
			StorageReadHandle srh(sm.get(), std::move(m.m_MetaInfo), no_storage_lock);
			bool ok = sm->IsOpen();
			if (ok)
			{
				if (IsUnit(cacheItem))
				{
					// the table's range; not through AbstrUnit::DoReadItem, which is for a configured unit
					// (it asserts that storage is not disabled, which it is on every result unit)
					ok = sm->ReadUnitRange(*srh.MetaInfo());
					MG_CHECK(!ok || AsUnit(cacheItem)->HasTiledRangeData());
				}
				else
					ok = srh.Read();
			}
			if (!ok)
				m.m_ConfigItem->throwItemErrorF("Reading from {} failed", storageName);
		}
		catch (const DmsException& x)
		{
			if (cacheItem == root)
				throw; // the table's range or the parameter itself: nothing is left to deliver
			if (!cacheItem->WasFailed(FailType::Data))
				cacheItem->DoFailCaller(x.AsErrMsg(), FailType::Data); // this member fails; the others are still read
		}
	}
	return true;
}

SharedPtr<NonmappableStorageManager> RequiredStorageManager(const TreeItemDualRef& resultHolder)
{
	auto root = resultHolder.GetNew();
	if (root && root->m_ReadAssets.is_a<storage_read_request>())
		return root->m_ReadAssets.Get<storage_read_request>().m_NSM; // null for a memory-mapped store: mapping needs no gate
	return {};
}

// 'geometry:poly' -> 'geometry', Polygon (decision 9: no suffix for Single)
std::pair<SharedStr, ValueComposition> SplitCompositionSuffix(const SharedStr& memberSpec)
{
	auto r = memberSpec.AsRange();
	auto colon = std::find(r.first, r.second, ':');
	if (colon == r.second)
		return { memberSpec, ValueComposition::Single };
	SharedStr suffix(CharPtrRange(colon + 1, r.second));
	auto vc = DetermineValueComposition(suffix.c_str());
	if (vc == ValueComposition::Unknown || vc == ValueComposition::Single)
		throwDmsErrF("storage_read_table: unknown value composition '{}' in member spec '{}'", suffix, memberSpec);
	return { SharedStr(CharPtrRange(r.first, colon)), vc };
}

// *****************************************************************************
// storage_read_table(spec, configName, domainType, [memberName, valuesUnit]*) -> unit with members
// *****************************************************************************

CommonOperGroup cog_storage_read_table("storage_read_table", oper_policy::allow_extra_args | oper_policy::dynamic_result_class | oper_policy::members_on_demand);

struct StorageReadTableOperator : TernaryOperator
{
	StorageReadTableOperator(AbstrOperGroup& og)
		: TernaryOperator(&og, AbstrUnit::GetStaticClass()
			, DataArray<SharedStr>::GetStaticClass() // spec
			, DataArray<SharedStr>::GetStaticClass() // the configured table's full name
			, AbstrUnit::GetStaticClass()            // the domain's value type, as a unit
		)
	{}

	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, LispPtr) const override
	{
		if (resultHolder && !resultHolder.IsTmp())
			return;
		MG_CHECK(IsMetaThread());
		MG_CHECK(args.size() >= 3 && (args.size() - 3) % 2 == 0);

		auto domainType = AsUnit(GetItem(args[2]));
		MG_CHECK(domainType);
		auto root = domainType->GetUnitClass()->CreateResultUnit(nullptr);
		MG_CHECK(root);

		for (arg_index i = 3; i < args.size(); i += 2)
		{
			auto [name, vc] = SplitCompositionSuffix(ArgString(args, i));
			auto vu = AsUnit(GetItem(args[i + 1]));
			MG_CHECK(vu);
			CreateDataItemFromPath(root.get(), name.c_str(), root.get(), vu, vc);
		}
		resultHolder = SharedMutableTreeItem(root);
	}

	bool PreCalcUpdate(TreeItemDualRef& resultHolder, ArgRefs& args) const override
	{
		MG_CHECK(IsMetaThread());
		auto root = resultHolder.GetNew();
		MG_CHECK(root);
		auto& req = GetOrCreateRequest(root, args);

		CollectMember(req, root, req.m_ConfigItem.get()); // the table's range, once

		for (auto member = root->WalkConstSubTree(root); member; member = root->WalkConstSubTree(member))
		{
			if (!IsDataItem(member))
				continue;
			auto configMember = FindConfigMember(req.m_ConfigItem.get(), root, member);
			if (!configMember)
				const_cast<TreeItem*>(member)->Fail(mySSPrintF("storage_read_table: no configured counterpart of member {} under {}", member->GetRelativeName(root), req.m_ConfigItem->GetFullName()).c_str(), FailType::Data);
			else
				CollectMember(req, const_cast<TreeItem*>(member), configMember);
		}
		return true;
	}

	auto GetRequiredStorageManager(TreeItemDualRef& resultHolder, const ArgRefs& args) const -> SharedPtr<NonmappableStorageManager> override
	{
		return RequiredStorageManager(resultHolder);
	}

	bool CalcResult(TreeItemDualRef& resultHolder, const ArgRefs& args, std::vector<ItemReadLock> readLocks, Explain::Context* context) const override
	{
		auto root = resultHolder.GetNew();
		MG_CHECK(root);
		return ReadPendingMembers(root);
	}
};

// *****************************************************************************
// storage_read_value(spec, configName, valuesUnit) -> parameter
// *****************************************************************************

CommonOperGroup cog_storage_read_value("storage_read_value", oper_policy::dynamic_result_class);

struct StorageReadValueOperator : TernaryOperator
{
	StorageReadValueOperator(AbstrOperGroup& og)
		: TernaryOperator(&og, AbstrDataItem::GetStaticClass()
			, DataArray<SharedStr>::GetStaticClass() // spec
			, DataArray<SharedStr>::GetStaticClass() // the configured parameter's full name
			, AbstrUnit::GetStaticClass()            // its values unit
		)
	{}

	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, LispPtr) const override
	{
		if (resultHolder && !resultHolder.IsTmp())
			return;
		MG_CHECK(IsMetaThread());
		MG_CHECK(args.size() == 3);

		auto vu = AsUnit(GetItem(args[2]));
		MG_CHECK(vu);
		resultHolder = SharedMutableTreeItem(CreateCacheDataItem(Unit<Void>::GetStaticClass()->CreateDefault(), vu, ValueComposition::Single));
	}

	bool PreCalcUpdate(TreeItemDualRef& resultHolder, ArgRefs& args) const override
	{
		MG_CHECK(IsMetaThread());
		auto root = resultHolder.GetNew();
		MG_CHECK(root);
		auto& req = GetOrCreateRequest(root, args);
		CollectMember(req, root, req.m_ConfigItem.get());
		return true;
	}

	auto GetRequiredStorageManager(TreeItemDualRef& resultHolder, const ArgRefs& args) const -> SharedPtr<NonmappableStorageManager> override
	{
		return RequiredStorageManager(resultHolder);
	}

	bool CalcResult(TreeItemDualRef& resultHolder, const ArgRefs& args, std::vector<ItemReadLock> readLocks, Explain::Context* context) const override
	{
		auto root = resultHolder.GetNew();
		MG_CHECK(root);
		return ReadPendingMembers(root);
	}
};

// *****************************************************************************
// storage_read_attr(spec, configName, domain, memberSpec, valuesUnit, extras...) -> attribute
//
// One stored attribute over a domain that is not read with it: a grid, a stream item (FSS, cfs), a
// strfiles attribute, an attribute of a memory-mapped store. The extras are further arguments the
// storage manager wants calculated before the read (strfiles: its FileName attribute).
// *****************************************************************************

CommonOperGroup cog_storage_read_attr("storage_read_attr", oper_policy::allow_extra_args | oper_policy::dynamic_result_class);

struct StorageReadAttrOperator : QuinaryOperator
{
	StorageReadAttrOperator(AbstrOperGroup& og)
		: QuinaryOperator(&og, AbstrDataItem::GetStaticClass()
			, DataArray<SharedStr>::GetStaticClass() // spec
			, DataArray<SharedStr>::GetStaticClass() // the configured attribute's full name
			, AbstrUnit::GetStaticClass()            // its domain
			, DataArray<SharedStr>::GetStaticClass() // 'name[:composition]'
			, AbstrUnit::GetStaticClass()            // its values unit
		)
	{}

	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, LispPtr) const override
	{
		if (resultHolder && !resultHolder.IsTmp())
			return;
		MG_CHECK(IsMetaThread());
		MG_CHECK(args.size() >= 5);

		auto domain = AsUnit(GetItem(args[2]));
		MG_CHECK(domain);
		auto [name, vc] = SplitCompositionSuffix(ArgString(args, 3));
		auto vu = AsUnit(GetItem(args[4]));
		MG_CHECK(vu);
		resultHolder = SharedMutableTreeItem(CreateCacheDataItem(domain, vu, vc));
	}

	bool PreCalcUpdate(TreeItemDualRef& resultHolder, ArgRefs& args) const override
	{
		MG_CHECK(IsMetaThread());
		auto root = resultHolder.GetNew();
		MG_CHECK(root);
		auto& req = GetOrCreateRequest(root, args);
		CollectMember(req, root, req.m_ConfigItem.get());
		return true;
	}

	auto GetRequiredStorageManager(TreeItemDualRef& resultHolder, const ArgRefs& args) const -> SharedPtr<NonmappableStorageManager> override
	{
		return RequiredStorageManager(resultHolder);
	}

	bool CalcResult(TreeItemDualRef& resultHolder, const ArgRefs& args, std::vector<ItemReadLock> readLocks, Explain::Context* context) const override
	{
		auto root = resultHolder.GetNew();
		MG_CHECK(root);
		return ReadPendingMembers(root);
	}
};

// *****************************************************************************
// instantiation
// *****************************************************************************

StorageReadTableOperator sro_table(cog_storage_read_table);
StorageReadValueOperator sro_value(cog_storage_read_value);
StorageReadAttrOperator  sro_attr (cog_storage_read_attr);

} // anonymous namespace
