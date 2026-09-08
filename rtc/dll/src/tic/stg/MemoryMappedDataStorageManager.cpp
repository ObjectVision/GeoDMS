// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "stg/MemoryMappedDataStorageManager.h"

#include <algorithm>
#include <map>

#include "act/TriggerOperator.h"
#include "dbg/debug.h"
#include "dbg/SeverityType.h"
#include "mci/ValueClass.h"
#include "mci/ValueClassID.h"
#include "ptr/InterestHolders.h"
#include "ser/AsString.h"
#include "ser/FileStreamBuff.h"
#include "utl/Environment.h"
#include "utl/FileSystem.h"
#include "utl/StrFormat.h"
#include "utl/scoped_exit.h"
#include "utl/splitPath.h"
#include "xml/XMLOut.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "AbstrUnit.h"
#include "DataLocks.h" // DrlType
#include "ItemLocks.h"
#include "TreeItem.h"
#include "TicInterface.h"
#include "TreeItemProps.h"

#include "stg/StorageClass.h"

TIC_CALL AppendTreeFromConfigurationFuncPtr s_AppendTreeFromConfigurationPtr = nullptr;

thread_local const TreeItem* t_MmdDictionaryRoot = nullptr;

//////////////////////////////////////////////////////////////////////
// #1154: restrictions on units external to the dictionary
//
// The dictionary describes what is INSIDE the storage; the value type and range of a unit
// declared OUTSIDE it only survive as a name in the attribute signatures, which re-resolves
// against whatever the reading configuration declares under that name. When that declaration
// changed since the write -- the fpoint->dpoint case of #1154 -- the reader silently binds the
// new type to the old bytes. These restrictions record what the bytes were written against, as
// an IntegrityCheck on the dictionary root: merged onto the read holder, #1180 folds them into
// every sub-item read through it, so a mismatch fails every consumer instead of delivering
// reinterpreted data. Units inside the dictionary are self-describing and need none of this.
//////////////////////////////////////////////////////////////////////

namespace {

	// How the reader must spell this unit: the very text the dictionary's own attribute declaration
	// carries for it -- the raw DomainUnit / ValuesUnit property, which is what DumpPropList writes
	// into the '(domain)' suffix and the 'attribute<values>' prefix. Anything else can resolve
	// differently than the declaration next to it: #1195 took the RAW configured token instead, so a
	// domain configured as '../RegioUnit' produced 'PropValue(../RegioUnit, ..)' beside a declaration
	// that said '(/BaseData/../RegioUnit)'. The reader merges the dictionary elsewhere in the tree,
	// where '../RegioUnit' is not resolvable, and the store could not be read at all. Deriving both
	// from one property keeps check and declaration in step by construction.
	SharedStr Mmd_UnitRefStr(const AbstrDataItem* adi, bool isDomainRole)
	{
		auto* propDef = isDomainRole ? s_DomainUnitPropDefPtr : s_ValuesUnitPropDefPtr;
		return propDef->GetRawValue(adi);
	}

	// The extent of a domain: the stored per-element files are only readable against the count they
	// were written with. Empty when it cannot be established (see the early returns), and empty for
	// a values unit, which needs no bounds -- a base unit's range would assert the full value-type
	// range, which restricts nothing.
	SharedStr Mmd_UnitBoundsRestriction(const AbstrUnit* u, WeakStr name, bool isDomainRole)
	{
		if (!isDomainRole)
			return {};
		auto rangeItem = u->GetCurrRangeItem();
		if (!rangeItem || !IsCalculatingOrReady(rangeItem.get()))
			return {}; // not known yet; a later re-emission (see UpdateDictionary) refreshes the dictionary
		InterestPtr<const TreeItem*> holder(u); // the same guarded access the Range subtag emission uses
		u->PrepareDataUsage(DrlType::Certain);

		auto vc = u->GetValueType();
		auto nrDims = vc->GetNrDims();
		if (nrDims == 1 && vc->IsNumeric())
		{
			// 64-bit integral bounds do not round-trip through the Float64 accessor; the ValueType
			// restriction still holds and the count mismatch surfaces at the file-size guards.
			auto vcid = vc->GetValueClassID();
			if (vcid == ValueClassID::VT_UInt64 || vcid == ValueClassID::VT_Int64)
				return {};
			auto [b, e] = u->GetRangeAsFloat64();
			if (!IsDefined(b) || !IsDefined(e) || b > e)
				return {};
			// cast-constructor literals: <vt>(<plain number>) needs no per-type literal suffix
			return mySSPrintF("LowerBound({0}) == {1}({2}) && UpperBound({0}) == {1}({3})"
				, name, vc->GetNameID()
				, AsString(b, FormattingFlags::None), AsString(e, FormattingFlags::None));
		}
		if (nrDims == 2)
		{
			auto [from, to_] = u->GetRangeAsDRect();
			if (!IsLowerBound(from, to_))
				return {};
			auto crdName = vc->GetScalarClass()->GetName();
			return mySSPrintF("LowerBound({0}) == point_xy({1}({2}), {1}({3}))"
				" && UpperBound({0}) == point_xy({1}({4}), {1}({5}))"
				, name, crdName
				, AsString(from.Col(), FormattingFlags::None), AsString(from.Row(), FormattingFlags::None)
				, AsString(to_.Col(), FormattingFlags::None), AsString(to_.Row(), FormattingFlags::None));
		}
		return {};
	}

	void Mmd_AddUnitRestriction(SharedStr& expr, std::vector<const AbstrUnit*>& seen
		, const TreeItem* dictRoot, const AbstrDataItem* adi, bool isDomainRole)
	{
		auto u = isDomainRole ? adi->GetAbstrDomainUnit() : adi->GetAbstrValuesUnit();
		if (!u || u->IsDefaultUnit() || dictRoot->DoesContain(u))
			return;
		if (std::find(seen.begin(), seen.end(), u) != seen.end())
			return;
		auto name = Mmd_UnitRefStr(adi, isDomainRole);
		if (name.empty() || name == ".")
			return;
		seen.push_back(u);

		assert(u->GetValueType());
		auto bounds = Mmd_UnitBoundsRestriction(u, name, isDomainRole);

		if (!expr.empty())
			expr += " && ";

		// A typed bound literal pins the value type as well: with 'LowerBound(u) == uint32(0)' there
		// is no eq operator left once u is declared int32 ("Cannot find operator for these
		// arguments"), so the read fails on that term alone. A PropValue(u,'ValueType') term beside
		// it would only restate that, so the ValueType restriction is emitted only where no bounds
		// could be: a values unit, an unknown range, a 64-bit integral or a non-numeric 1-d domain.
		expr += bounds.empty()
			? mySSPrintF("PropValue({}, 'ValueType') == '{}'", name, u->GetValueType()->GetNameID())
			: bounds;
	}

} // anonymous namespace

auto Mmd_SynthesizeExternalUnitRestrictions(const TreeItem* dictRoot) -> SharedStr
{
	SharedStr expr;
	std::vector<const AbstrUnit*> seen; // first-encounter order keeps the dictionary text deterministic

	std::vector<const TreeItem*> stack{ dictRoot };
	while (!stack.empty())
	{
		auto ti = stack.back();
		stack.pop_back();
		for (auto sub = ti->_GetFirstSubItem(); sub; sub = sub->GetNextItem())
			if (!sub->IsDisabledStorage()) // mirrors what the dictionary dump includes
				stack.push_back(sub);
		if (!IsDataItem(ti))
			continue;
		auto adi = AsDataItem(ti);
		Mmd_AddUnitRestriction(expr, seen, dictRoot, adi, true);
		Mmd_AddUnitRestriction(expr, seen, dictRoot, adi, false);
	}
	return expr;
}

//////////////////////////////////////////////////////////////////////
// #1247: two items of one store that are the same content
//
// A stored reference does not keep its config item as referred item: TreeItem::SetReferredItem
// swaps in a DataController over 'convert(<source key>, <values unit key>)', so that the produced
// array can be mapped into the store, and GetOrCreateDataController interns those by key
// expression. Two store items that resolve to the same source therefore share ONE cache item; its
// back reference is claimed by whichever item got there first, and DataWriteLock names the store
// file from exactly that back reference. So the content was written once while the dictionary
// declared every claimant, and a reader failed on first use of one that never got a file, with
// "Data not found in .MMD storage folder" -- while the write itself had reported nothing.
//
// The dump now declares such an item as a reference to the store-local item that does carry the
// content, which is a rule the reader can re-evaluate against suppliers inside the store. UNITS are
// mapped as well as attributes: the unit alias is what lets a reader unify an aliased attribute
// with its own declared domain, since AbstrUnit::UnifyDomain compares GetCurrUltimateItem() and
// then the DataController of GetCheckedKeyExpr(), on both of which two separately declared units of
// equal range differ ("Domain mismatch ... (different CheckedKeyExpr)").
//
// This is deliberately only the case that identity decides. An item whose rule merely MENTIONS
// something outside the store keeps a file of its own, as before; rewriting such a rule in terms of
// store-local suppliers is the open question of #1247 and is not attempted here.
//
// The map is thread_local and lives for one dump, like t_MmdDictionaryRoot above. It stays out of
// the header on purpose: that header is included by clc/dll/include/CastedUnaryAttrOper.h and so
// reaches most of Clc, while this is private to the dictionary dump. TreeItem::XML_Dump declares
// the one accessor it needs, as it already declares IsDumpingToFolder.
//////////////////////////////////////////////////////////////////////

namespace {

	using MmdAliasMap = std::map<const TreeItem*, SharedTreeItem>;
	thread_local const MmdAliasMap* t_MmdAliasMap = nullptr;

	// What an item's content IS, as an identity to compare on: for a data item the shared cache
	// result whose bytes the store file holds, for a unit the item it refers to. An item that
	// refers to nothing is its own content and can never be the alias.
	auto Mmd_ContentIdentity(const TreeItem* ti) -> const TreeItem*
	{
		auto ultimate = ti->GetCurrUltimateItem();
		return (ultimate && ultimate.get() != ti) ? ultimate.get() : nullptr;
	}

	// Pre-order, first sub-item first: the order TreeItem::XML_Dump writes the dictionary in, with
	// the same skip of the engine's own shadows (#1245). "The first one declared" is then a
	// statement about the dictionary and not about the walk.
	void Mmd_CollectByIdentity(const TreeItem* ti, std::map<const TreeItem*, std::vector<SharedTreeItem>>& byIdentity)
	{
		for (auto sub = ti->_GetFirstSubItem(); sub; sub = sub->GetNextItem())
		{
			if (sub->IsDisabledStorage())
				continue;
			if (IsDataItem(sub) || IsUnit(sub))
				if (auto identity = Mmd_ContentIdentity(sub))
					byIdentity[identity].push_back(make_shared_tree(sub, existing_obj{}));
			Mmd_CollectByIdentity(sub, byIdentity);
		}
	}

	auto Mmd_BuildAliasMap(const TreeItem* dictRoot) -> MmdAliasMap
	{
		std::map<const TreeItem*, std::vector<SharedTreeItem>> byIdentity;
		Mmd_CollectByIdentity(dictRoot, byIdentity);

		MmdAliasMap result;
		for (const auto& [identity, sharers] : byIdentity)
		{
			if (sharers.size() < 2)
				continue;

			// Which sharer keeps a stored declaration: the item the produced array was filed
			// under, so that the declaration and the file agree by construction rather than by
			// both happening to follow the same order. DataWriteLock takes the file name from the
			// cache item's back reference; a unit has no file and no back reference, and there the
			// first item the dictionary declares carries it.
			SharedTreeItem owner;
			if (identity->IsCacheItem())
				owner = identity->GetBackRef();
			if (!owner || std::find(sharers.begin(), sharers.end(), owner) == sharers.end())
				owner = sharers.front();

			for (const auto& sharer : sharers)
				if (sharer != owner)
					result.emplace(sharer.get(), owner);
		}
		return result;
	}

} // anonymous namespace

// Defined here, declared at its one use in TreeItemXmlDump.cpp, as IsDumpingToFolder is.
auto Mmd_DictionaryAliasOf(const TreeItem* item) -> SharedTreeItem
{
	if (!t_MmdAliasMap)
		return {};
	auto i = t_MmdAliasMap->find(item);
	return (i == t_MmdAliasMap->end()) ? SharedTreeItem() : i->second;
}

//////////////////////////////////////////////////////////////////////
// MmdStorageManager implementation
//////////////////////////////////////////////////////////////////////

SharedStr MmdStorageManager::GetFullFileName(CharPtr name) const
{
	return DelimitedConcat(GetNameStr().c_str(), MakeFileName(name).c_str());
}

FileDateTime MmdStorageManager::GetLastChangeDateTime(const TreeItem* storageHolder, CharPtr path) const
{
	if (DoesExist(storageHolder)) // TODO: lock this file from here on.
	{
		m_FileTime = GetFileOrDirDateTime(GetFullFileName(path));
	}
	return m_FileTime; 
}

bool MmdStorageManager::DoCheckExistence(const TreeItem* storageHolder, const TreeItem* storageItem) const
{
	if (!storageItem)
		return AbstrStorageManager::DoCheckExistence(storageHolder, storageItem);

	auto relName = storageItem->GetRelativeName(storageHolder);
	return IsFileOrDirAccessible(GetFullFileName(relName.c_str()));
}

void MmdStorageManager::DoUpdateTree(const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const
{
	if (curr != storageHolder) // only update the root item
		return;
	if (curr->HasConfiguredCalcRule()) // don't read schema info if the item has a calculation rule; this is the production case (#587: a read installed by the engine is not one)
		return;

	if (storageReadOnlyPropDefPtr->GetValue(storageHolder))
	{
		// #1154/#1179 usage contract for a read holder: the reader declares ONLY the holder --
		// StorageName plus StorageReadOnly -- and everything below it comes from the dictionary.
		// Anything else is refused loudly rather than merged over: a reader-declared sub-item
		// would collide with its dictionary namesake, and a reader-declared IntegrityCheck on
		// the holder would be silently replaced by the restrictions the dictionary carries
		// (which, conversely, guard all merged sub-items since #1180).
		if (m_MergedReadHolders.contains(curr))
			return; // this holder's dictionary is already merged; its sub-items are the dictionary's
		if (curr->_GetFirstSubItem())
			curr->throwItemErrorF(
				"a read-only MMD storage holder must not declare sub-items; "
				"they are defined by the dictionary of {}", GetNameStr());
		if (integrityCheckPropDefPtr->HasNonDefaultValue(curr))
			curr->throwItemErrorF(
				"an IntegrityCheck on a read-only MMD storage holder is not supported; "
				"the restrictions of {} come from its dictionary", GetNameStr());
	}
	else if (curr->_GetFirstSubItem())
		return;

	auto dictFileName = GetFullFileName("0Dictionary.dms");

	if (!IsFileOrDirAccessible(dictFileName))
		return;
	if (!s_AppendTreeFromConfigurationPtr)
		throwErrorD("MmdStorageManager::DoUpdateTree", "s_AppendTreeFromConfigurationPtr is not set");

	s_AppendTreeFromConfigurationPtr(dictFileName.c_str(), curr);
	m_MergedReadHolders.insert(curr);
}

namespace {

	// #1245: DisableStorage below an MMD holder is refused, not skipped. Everything below the holder
	// is the store: skipping an item wrote the attributes declared on it against a domain the
	// dictionary does not carry, and where the flag had been lost -- a template's case parameters
	// lost it on instantiation -- the parameter unit was written as a stored domain of its own,
	// which the reader could not combine with the store it was derived from. Either way the store
	// was written without complaint and failed much later, in the reader. Only the engine's own
	// shadows of a referred cache root's sub-items (TSF_MergedFromRefItem) are still skipped, as
	// XML_Dump does: no configuration declared those. The shadow's own subtree is not visited
	// either, mirroring the dump.
	void Mmd_RefuseDisabledStorage(const TreeItem* storageHolder, WeakStr storageName)
	{
		std::vector<const TreeItem*> stack{ storageHolder };
		while (!stack.empty())
		{
			auto ti = stack.back();
			stack.pop_back();
			for (auto sub = ti->_GetFirstSubItem(); sub; sub = sub->GetNextItem())
			{
				if (!sub->IsDisabledStorage())
				{
					stack.push_back(sub);
					continue;
				}
				if (sub->IsMergedFromRefItem())
					continue;
				sub->throwItemErrorF(
					"DisableStorage is not supported below the MMD storage {} that is being written: "
					"every item below its holder {} is part of the store. "
					"Move this item outside the holder or drop the property."
					, storageName, storageHolder->GetFullName());
			}
		}
	}

	// #1247: the content of this item was filed under an item OUTSIDE this store -- another MMD
	// store that got there first, or an item elsewhere in the configuration whose expression
	// interned to the same key. Its data went there, so this store would declare an item it does
	// not hold, and no store-local rule can point at the twin. Two items that are the same content
	// can share ONE store, where the second is declared as a reference to the first; they cannot be
	// split over two. Refuse, as #1247 asked: the writer knows it here, the reader learns it much
	// later and elsewhere.
	void Mmd_RefuseForeignlyOwnedContent(const TreeItem* storageHolder, WeakStr storageName)
	{
		std::vector<const TreeItem*> stack{ storageHolder };
		while (!stack.empty())
		{
			auto ti = stack.back();
			stack.pop_back();
			for (auto sub = ti->_GetFirstSubItem(); sub; sub = sub->GetNextItem())
			{
				if (sub->IsDisabledStorage())
					continue;
				stack.push_back(sub);
				if (!IsDataItem(sub))
					continue; // a unit has no file of its own; its range is in the dictionary
				auto ultimate = sub->GetCurrUltimateItem();
				if (!ultimate || !ultimate->IsCacheItem())
					continue;
				auto owner = ultimate->GetBackRef();
				if (!owner || owner.get() == sub || storageHolder->DoesContain(owner.get()))
					continue;
				sub->throwItemErrorF(
					"this item shares its data with {}, which lies outside the MMD storage {} that is being written, "
					"so the data is written there and this store would declare an item it does not hold. "
					"Two items that resolve to the same source can share one store, where the second is written "
					"as a reference to the first, but they cannot be written to two stores. "
					"Give this item a calculation rule of its own, or write it to only one store."
					, owner->GetFullName(), storageName);
			}
		}
	}

} // anonymous namespace

void MmdStorageManager::DoWriteTree(const TreeItem* storageHolder)
{
	if (!storageHolder)
		return;

	ExportMetaInfo(storageHolder, storageHolder);

	Mmd_RefuseDisabledStorage(storageHolder, GetNameStr());
	Mmd_RefuseForeignlyOwnedContent(storageHolder, GetNameStr()); // #1247

	auto dictFileName = GetFullFileName("0Dictionary.dms");

	auto osb = VectorOutStreamBuff();
	auto out = OutStream_DMS(&osb, calcRulePropDefPtr);

	// #1154: let XML_Dump synthesize the external-unit restrictions at this root
	t_MmdDictionaryRoot = storageHolder;
	auto resetRoot = make_scoped_exit([] { t_MmdDictionaryRoot = nullptr; });

	// #1247: which items of this dictionary are declared as references to the twin that carries
	// their content. Built once, here, from the state the dump is about to describe.
	auto aliasMap = Mmd_BuildAliasMap(storageHolder);
	t_MmdAliasMap = &aliasMap;
	auto resetAliases = make_scoped_exit([] { t_MmdAliasMap = nullptr; });

	TreeItem_XML_DumpOrThrow(storageHolder, &out, false);

	auto fsb = FileOutStreamBuff(dictFileName, true);
	fsb.WriteBytes(osb.GetData(), osb.CurrPos());
}

void MmdStorageManager::UpdateDictionary(const TreeItem* storageHolder)
{
	// #1155: called when a unit under this storage commits, i.e. when its range has just
	// become ready. The dictionary emitted at OpenForWrite time skipped the Range subtag
	// of units that were not calculated yet (see the var-range branch of TreeItem::XML_Dump,
	// #1130); re-emitting it here completes the dictionary before the write session ends.
	auto lock = lock_t(m_CriticalSection);
	// Not `m_IsOpen && m_IsOpenedForWrite`: the storage is already CLOSED when the last stored
	// attribute finishes committing, and that is exactly the moment the extent of a domain
	// declared outside this storage first becomes readable (#1154). The dictionary is a separate
	// text file, so refreshing it needs the write SESSION, not the mapped storage. Before
	// OpenForWrite has run there is nothing to refresh, which m_IsOpenedForWrite still states.
	if (!m_IsOpenedForWrite)
		return; // nothing emitted yet: OpenForWrite will dump the dictionary with the now-ready range

	SuspendTrigger::FencedBlocker blockSuspension("MmdStorageManager::UpdateDictionary");
	DoWriteTree(storageHolder);
}

// #587: an attribute of the store is read by mapping its file (storage_read_attr / storage_read_value,
// the mapping arm of StorageReadOperators.cpp); the units keep the ranges the dictionary gave them and
// are not read.
ReadCallSpec MmdStorageManager::DescribeReadCall(const TreeItem* storageHolder, const TreeItem* item) const
{
	if (!IsDataItem(item))
		return {};
	return DescribeAttrRead(storageHolder, AsDataItem(item));
}

bool IsInMMD(const AbstrDataItem* cacheItem)
{
	auto configItem = (!cacheItem->m_BackRef.expired() && IsDataItem(cacheItem->m_BackRef.lock().get())) ? AsDataItem(cacheItem->m_BackRef.lock().get()) : cacheItem;
	if (auto sp = configItem->GetCurrStorageParent(true))
	{
		auto sm = sp->GetStorageManager();
		assert(sm);
		if (auto mmd = dynamic_cast<MmdStorageManager*>(sm))
			return true;
	}
	return false;
}

//----------------------------------------------------------------------
// instantiation and registration
//----------------------------------------------------------------------

IMPL_DYNC_STORAGECLASS(MmdStorageManager, "MMD")
