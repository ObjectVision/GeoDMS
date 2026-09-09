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

#include "act/ActorVisitor.h"      // #1264: MakeDerivedBoolVisitor over the rule's named suppliers
#include "act/SupplierVisitFlag.h" // #1264: SupplierVisitFlag::NamedSuppliers
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

#include "AbstrCalculator.h" // #1264: the rule's suppliers and IsDataBlock/IsStorageRead
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
// #1264: a rule the reader can re-apply is stored as a rule, not as bytes
//
// #1247 made the store materialise every attribute it declares. Where an attribute's rule can be
// re-applied by the reader WITHOUT the writer's configuration, the store can carry the rule instead
// and save the bytes. The criterion is syntactic, on the formal identifiers the rule NAMES rather
// than on what it resolves to: `y := x + 1` with x in the store qualifies, `y := f(src) + 1` does
// not, even where the two compute the same values. Testing the resolved key instead would call the
// second one store-local and then have to turn that key back into something meaningful in the
// reader's context, which is the part that does not work.
//
// The rule text goes into the dictionary verbatim: a reader merges the dictionary as a subtree
// under its own holder and the relative shape is preserved, so a store-relative identifier resolves
// to the same place there as here. That is why an identifier naming a store item by an ABSOLUTE
// path disqualifies the item -- it is store-local but would resolve outside the store in the
// reader, where the store sits somewhere else entirely.
//
// This saves DISK, not compute: an item in interest is still calculated, its array simply is not
// mapped into the store (DataWriteLock's MMD arm skips the write-through on TSF_MmdRuleOnly).
//////////////////////////////////////////////////////////////////////

namespace {

	// An identifier that names an item by an absolute path, i.e. a '/' that starts a token rather
	// than separating two name parts. Conservative by construction: anything this cannot read as
	// clearly relative disqualifies the item, and materialising is always correct, merely larger.
	bool Mmd_RuleHasAbsolutePath(WeakStr expr)
	{
		for (auto p = expr.begin(), e = expr.send(); p != e; ++p)
		{
			if (*p != '/')
				continue;
			if (p == expr.begin())
				return true;
			char prev = p[-1];
			// a relative path has a name character before the separator ('a/b', '../a/b');
			// anything else -- '(', ',', an operator, a space -- starts a new token with '/'
			if (!isalnum(UChar(prev)) && prev != '_' && prev != '.' && prev != '/')
				return true;
		}
		return false;
	}

} // anonymous namespace

// Declared at its use in TreeItemDataUsage.cpp, as LedgerHasRoomForDeferral is: this is private to
// the MMD write side and the header reaches most of Clc.
bool Mmd_QualifiesAsRuleOnly(const TreeItem* storageHolder, const TreeItem* item)
{
	assert(IsMetaThread());
	assert(storageHolder && item);

	if (!IsDataItem(item))
		return false; // a unit contributes its Range to the dictionary, which is metadata, not bytes
	if (item->IsDisabledStorage() || item->IsMergedFromRefItem())
		return false; // not part of the store to begin with

	// KeepData says the modeller wants this data kept, and a store is a place where it is kept.
	// The bound is the PROPAGATED flag (the user's decision): SetKeepDataState pushes the value
	// down to sub-items and pushes False down too, so this answers "has it, or inherited it with no
	// intermediate False". It also carries a KeepData set ABOVE the storage holder, which the rule
	// as #1264 states it excludes -- so a KeepData anywhere above a store materialises that whole
	// store. That is the accepted reading, not an oversight.
	if (item->GetKeepDataState())
		return false;

	if (!item->HasConfiguredCalcRule())
		return false; // nothing to write in place of the bytes
	auto calc = item->GetCalculator();
	if (!calc || calc->IsDataBlock() || calc->IsStorageRead())
		return false; // literal data, or an engine-installed read (#587): neither is a rule to re-apply

	if (Mmd_RuleHasAbsolutePath(calcRulePropDefPtr->GetRawValue(item)))
		return false;

	// every identifier the rule names must be inside this store, so the reader can resolve it
	bool allInside = true;
	auto visitor = MakeDerivedBoolVisitor(
		[storageHolder, &allInside](const Actor* a) -> ActorVisitState
		{
			auto ti = dynamic_cast<const TreeItem*>(a);
			if (!ti || !storageHolder->DoesContain(ti))
			{
				allInside = false;
				return AVS_SuspendedOrFailed; // stop the walk; one outsider settles it
			}
			return AVS_Ready;
		});
	calc->VisitSuppliers(SupplierVisitFlag::NamedSuppliers, visitor);
	return allInside;
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

} // anonymous namespace

void MmdStorageManager::DoWriteTree(const TreeItem* storageHolder)
{
	if (!storageHolder)
		return;

	ExportMetaInfo(storageHolder, storageHolder);

	Mmd_RefuseDisabledStorage(storageHolder, GetNameStr());

	auto dictFileName = GetFullFileName("0Dictionary.dms");

	auto osb = VectorOutStreamBuff();
	auto out = OutStream_DMS(&osb, calcRulePropDefPtr);

	// #1154: let XML_Dump synthesize the external-unit restrictions at this root
	t_MmdDictionaryRoot = storageHolder;
	auto resetRoot = make_scoped_exit([] { t_MmdDictionaryRoot = nullptr; });

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

// #1247: see the declaration for why a stored item can end up without a file of its own.
void MmdStorageManager::MaterializeSharedContent(const TreeItem* storageHolder, const AbstrDataItem* adi)
{
	assert(IsMetaThread()); // what lets the lock be dropped for the copy: only this thread writes m_MaterializedAt
	assert(storageHolder);
	assert(adi);

	// Whose file holds this content? The produced array was filed under the back reference of the
	// shared cache item (see DataWriteLock in DataLocks.cpp), so when that is another item, this
	// item has no file and the dictionary would declare data that is not there.
	auto ultimate = adi->GetCurrUltimateItem();
	if (!ultimate || !ultimate->IsCacheItem())
		return; // nothing shared: this item produced its own array, write-through already wrote it
	auto owner = ultimate->GetBackRef();
	if (!owner || owner.get() == adi)
		return; // this item IS the one the array was filed under

	auto stamp = adi->GetLastChangeTS();

	auto relName = adi->GetRelativeName(storageHolder);
	if (relName.empty())
		relName = SharedStr("@main"); // the same name DataWriteLock gives the holder itself
	auto fileName = DelimitedConcat(GetNameStr().c_str(), relName.c_str());

	{
		// m_CriticalSection is a binary_semaphore, not recursive, and OpenForWrite asserts that the
		// caller already holds it. Held for the guard check and the open only: a worker running a
		// storage_read_* CalcResult of this store holds the same semaphore, so keeping it across a
		// copy that can be gigabytes would block every read of the store for that whole time.
		// Releasing it for the copy costs no safety, because the ordinary write-through path holds
		// no such lock at all -- an operator fills its FileTileArray with this section untaken.
		auto lock = lock_t(m_CriticalSection);
		if (auto i = m_MaterializedAt.find(adi); i != m_MaterializedAt.end() && i->second == stamp)
			return; // already copied, and neither this item nor a supplier has changed since

		// The store may never have been opened: TreeItem::PrepareDataUsage takes its
		// CheckCalculatingOrReady short cut before the arm that calls OpenForWrite, so a store whose
		// every item is already ready produced no folder and no dictionary at all, silently and with
		// exit 0. Opening it here rather than there keeps that hot path untouched.
		if (!IsOpenForWrite())
		{
			auto smi = StorageMetaInfo(storageHolder, const_cast<AbstrDataItem*>(adi));
			OpenForWrite(smi);
		}
	}

	{
		DataReadLock readLock(adi); // nests with the one CommitDataChanges holds: lock_shared only counts
		auto adu = adi->GetAbstrDomainUnit();
		assert(adu && adu->HasInterest()); // CreateFileTileArray needs a defined, interested domain;
		                                   // both hold here: the domain IS the ready source's

		// An AbstrDataObject is a SharedObj, counted intrusively, so it must be held in a
		// SharedPtr and never in the bare unique_ptr the factory hands back: CopyData takes a
		// counted reference of its own, and letting that be the FIRST one drives the count 0->1->0,
		// which destroys the object while the unique_ptr still owns it and deletes it again. This
		// release-into-a-SharedPtr is the same handoff DataWriteLock makes (DataLocks.cpp).
		SharedPtr<AbstrDataObject> target;
		target.reset(CreateFileTileArray(adi, nullptr, dms_rw_mode::write_only_all, fileName, false).release());
		MG_CHECK(target);
		CopyData(adi->GetRefObj().get(), target.get());
	} // mapping closed here, before the caller's UpdateDictionary declares the file

	auto lock = lock_t(m_CriticalSection);
	m_MaterializedAt[adi] = stamp; // only after the copy returned: a throw or a suspension leaves no mark
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
