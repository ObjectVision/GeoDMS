// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

// MmdStorageManager.cpp: implementation of the MmdStorageManager class.
//
//////////////////////////////////////////////////////////////////////

#include "stg/MemoryMappedDataStorageManager.h"

#include <algorithm>

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
#include "DataStoreManagerCaller.h"
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

	// The unit's name as the reader will resolve it: the RAW configured token, the same spelling
	// the attribute signatures dump, resolving by up-scope search from the read holder.
	SharedStr Mmd_UnitRefStr(TokenID nameToken)
	{
		if (!IsDefined(nameToken) || nameToken == TokenID::GetEmptyID())
			return {};
		return SharedStr(GetTokenStr(nameToken).c_str()); // materialize: TokenStr holds the token-registry lock
	}

	void Mmd_AddUnitRestriction(SharedStr& expr, std::vector<const AbstrUnit*>& seen
		, const TreeItem* dictRoot, const AbstrUnit* u, TokenID nameToken, bool isDomainRole)
	{
		if (!u || u->IsDefaultUnit() || dictRoot->DoesContain(u))
			return;
		if (std::find(seen.begin(), seen.end(), u) != seen.end())
			return;
		auto name = Mmd_UnitRefStr(nameToken);
		if (name.empty() || name == ".")
			return;
		seen.push_back(u);

		auto vc = u->GetValueType();
		assert(vc);
		if (!expr.empty())
			expr += " && ";
		expr += mySSPrintF("PropValue({}, 'ValueType') == '{}'", name, vc->GetName());

		// The extent matters for a domain: the stored per-element files are only readable against
		// the count they were written with. A values unit needs no bounds -- and a base unit's
		// range would assert the full value-type range, which restricts nothing.
		if (!isDomainRole)
			return;
		auto rangeItem = u->GetCurrRangeItem();
		if (!rangeItem || !IsCalculatingOrReady(rangeItem.get()))
			return; // not known yet; a later re-emission (see UpdateDictionary) refreshes the dictionary
		InterestPtr<const TreeItem*> holder(u); // the same guarded access the Range subtag emission uses
		u->PrepareDataUsage(DrlType::Certain);

		auto nrDims = vc->GetNrDims();
		if (nrDims == 1 && vc->IsNumeric())
		{
			// 64-bit integral bounds do not round-trip through the Float64 accessor; the ValueType
			// restriction still holds and the count mismatch surfaces at the file-size guards.
			auto vcid = vc->GetValueClassID();
			if (vcid == ValueClassID::VT_UInt64 || vcid == ValueClassID::VT_Int64)
				return;
			auto [b, e] = u->GetRangeAsFloat64();
			if (!IsDefined(b) || !IsDefined(e) || b > e)
				return;
			// cast-constructor literals: <vt>(<plain number>) needs no per-type literal suffix
			expr += mySSPrintF(" && LowerBound({0}) == {1}({2}) && UpperBound({0}) == {1}({3})"
				, name, vc->GetName()
				, AsString(b, FormattingFlags::None), AsString(e, FormattingFlags::None));
		}
		else if (nrDims == 2)
		{
			auto [from, to_] = u->GetRangeAsDRect();
			if (!IsLowerBound(from, to_))
				return;
			auto crdName = vc->GetScalarClass()->GetName();
			expr += mySSPrintF(" && LowerBound({0}) == point_xy({1}({2}), {1}({3}))"
				" && UpperBound({0}) == point_xy({1}({4}), {1}({5}))"
				, name, crdName
				, AsString(from.Col(), FormattingFlags::None), AsString(from.Row(), FormattingFlags::None)
				, AsString(to_.Col(), FormattingFlags::None), AsString(to_.Row(), FormattingFlags::None));
		}
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
		Mmd_AddUnitRestriction(expr, seen, dictRoot, adi->GetAbstrDomainUnit(), adi->DomainUnitToken(), true);
		Mmd_AddUnitRestriction(expr, seen, dictRoot, adi->GetAbstrValuesUnit(), adi->ValuesUnitToken(), false);
	}
	return expr;
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
	if (DoesExist(storageHolder)) // TODO: lock deze file vanaf hier.
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
	if (curr->HasCalculator()) // don't read schema info if the item has a calculator; this is the production case
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

void MmdStorageManager::DoWriteTree(const TreeItem* storageHolder)
{
	if (!storageHolder)
		return;

	ExportMetaInfo(storageHolder, storageHolder);

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
