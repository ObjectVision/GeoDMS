// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// TreeItem serialization to an OutStreamBase: the XML / DMS-syntax dump of an item and
// its sub-items, including the rendering of a function item as 'function f<vars>(params) -> result'.

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
#include "TreeItemInternal.h"
#include "UsingCache.h"
#include "stg/MemoryMappedDataStorageManager.h"

#include <unordered_set>

//----------------------------------------------------------------------
// Dumping to OutStreamBase
//----------------------------------------------------------------------

#include "xml/XMLOut.h"
#include "Xml/XmlTreeOut.h"
#include <time.h>

bool IsDumpingToFolder();

// =============================================================================
// DMS-syntax serialization of FUNCTION items as 'function name<tvs>(params) -> result'
// declarations (rather than the generic 'container ...: IsTemplate' form). All helpers
// hand-write raw text via operator<< onto the (stateful) OutStream_DMS: params must be
// ';'-separated (the attr machinery would emit ','), and the branch never calls
// WriteAttr/DumpPropList/DumpSubTags. Source fidelity: data-item types are built from the
// SOURCE tokens (ValuesUnitToken/DomainUnitToken), not the resolve-then-GetScriptName prop
// path. See doc/development/typed-hof-language-design.md and the plan.
// =============================================================================
namespace {

// Robust per-sub-item dump: a throw while rendering one child must not corrupt the enclosing
// brace nesting; emit an inline (parse-neutral) '//' comment and continue so EndSubItems runs.
void TreeItem_XML_DumpSubItemSafe(const TreeItem* subItem, OutStreamBase* out, bool notWritingDictionary)
{
	try
	{
		subItem->XML_Dump(out, notWritingDictionary);
	}
	catch (...)
	{
		auto err = catchException(false);
		out->NewLine();
		*out << "// ERROR dumping ";
		*out << SharedStr(subItem->GetName()).c_str();
		if (err)
		{
			SharedStr why(err->Why());
			std::string oneLine;
			for (CharPtr p = why.begin(), e = why.send(); p != e; ++p)
				oneLine += (*p == '\n' || *p == '\r') ? ' ' : *p;
			*out << ": ";
			*out << oneLine.c_str();
		}
		out->NewLine();
	}
}

void DMS_WriteTypeVars(OutStreamBase& out, const TreeItem* fn)
{
	auto tvs = TreeItem_GetFunctionTypeVars(fn);
	if (!tvs || tvs->empty())
		return;
	out << "<";
	bool first = true;
	for (const auto& tv : *tvs)
	{
		if (!first) out << ", ";
		first = false;
		out << SharedStr(tv.first).c_str();
		if (tv.second) { out << ": "; out << SharedStr(tv.second).c_str(); }
	}
	out << ">";
}

void DMS_WriteTypeArgs(OutStreamBase& out, const std::vector<TokenID>* typeArgs)
{
	if (!typeArgs || typeArgs->empty())
		return;
	out << "<";
	bool first = true;
	for (TokenID t : *typeArgs)
	{
		if (!first) out << ", ";
		first = false;
		out << SharedStr(t).c_str();
	}
	out << ">";
}

// The values-type PREFIX (no name, no domain): 'attribute<V>' / 'parameter<V>' /
// 'unit<vt>' / 'function' / 'container', from SOURCE tokens for data items.
void DMS_WriteValuesPrefix(OutStreamBase& out, const TreeItem* item)
{
	if (item->IsFunctionItem()) { out << "function"; return; }
	if (IsDataItem(item))
	{
		auto adi = AsDataItem(item);
		SharedStr vt(adi->ValuesUnitToken());
		// a void-domain item is a 'parameter<V>'; anything else is an 'attribute<V>'
		out << (adi->HasVoidDomainGuarantee() ? "parameter<" : "attribute<"); out << vt.c_str(); out << ">";
		return;
	}
	if (IsUnit(item)) { out << SharedStr(item->GetSignature()).c_str(); return; } // 'unit<vt>' -- source-faithful
	out << "container";
}

// The domain SUFFIX ' (dTok[, comp])' -- placed AFTER the item name (attribute grammar);
// nothing for a void/domain-less item or a non-data item. '.' (self) domains are omitted:
// they are the parser's default for a domain-less function param/result and re-derive on reload.
void DMS_WriteDomainSuffix(OutStreamBase& out, const TreeItem* item)
{
	if (!IsDataItem(item))
		return;
	auto adi = AsDataItem(item);
	if (adi->HasVoidDomainGuarantee())
		return; // void domain -> 'parameter<V>', no suffix
	auto vc = adi->GetValueComposition();
	SharedStr dt(adi->DomainUnitToken());
	bool selfDomain = (dt.ssize() == 1 && dt.begin()[0] == '.');
	if (selfDomain && vc == ValueComposition::Single)
		return; // '.' self-domain, Single -> implicit (the parser's default for a domain-less item)
	if (dt.empty())
	{
		if (vc == ValueComposition::Single)
		{
			// source token absent (e.g. some geometry/sequence attributes): fall back to the
			// resolved domain's script name so the domain is not silently dropped
			auto adu = adi->GetAbstrDomainUnit();
			if (!adu)
				return; // truly unresolved (an in-template generic domain) -> implicit
			dt = adu->GetScriptName(item);
			if (dt.empty())
				return;
		}
		else
			dt = SharedStr("."); // no domain token but a non-Single composition -> keep '.' so the composition survives
	}
	out << " ("; out << dt.c_str();
	if (vc != ValueComposition::Single)
	{
		SharedStr vcName(GetValueCompositionID(vc));
		if (!vcName.empty()) { out << ", "; out << vcName.c_str(); }
	}
	out << ")";
}

// A full typed item declaration '<values-prefix> name <domain-suffix>' (params, members).
void DMS_WriteTypedItem(OutStreamBase& out, const TreeItem* item)
{
	DMS_WriteValuesPrefix(out, item);
	out << " "; out << SharedStr(item->GetName()).c_str();
	DMS_WriteDomainSuffix(out, item);
}

void DMS_WriteFunctionParam(OutStreamBase& out, const TreeItem* fn, const TreeItem* param, UInt32 idx, UInt32 nrParams)
{
	SharedStr pname(param->GetName());
	if (idx + 1 == nrParams && TreeItem_HasFunctionRestParam(fn)) { out << "..."; out << pname.c_str(); return; }
	if (TreeItem_IsFunctionMetaRefParam(fn, idx)) { out << "item "; out << pname.c_str(); return; }
	if (auto sig = TreeItem_GetFunctionParamSignature(fn, idx))
	{
		out << pname.c_str(); out << ": ";
		out << SharedStr(sig->GetScriptName(fn)).c_str();
		DMS_WriteTypeArgs(out, TreeItem_GetFunctionParamSigTypeArgs(fn, idx));
		return;
	}
	DMS_WriteTypedItem(out, param); // '<values-prefix> name <domain-suffix>'
	// unit param carrying a member block: 'unit<uint32> Road { attribute<float64> flow; }'
	if (IsUnit(param) && param->_GetFirstSubItem())
	{
		out << " { ";
		for (const TreeItem* m = param->_GetFirstSubItem(); m; m = m->GetNextItem())
		{
			DMS_WriteTypedItem(out, m);
			if (IsDataItem(m) && !m->GetExpr().empty()) { out << " := "; out << SharedStr(m->GetExpr()).c_str(); }
			out << "; ";
		}
		out << "}";
	}
}

void DMS_WriteFunctionUsings(OutStreamBase& out, const TreeItem* fn)
{
	UInt32 n = fn->GetNrNamespaceUsages();
	for (UInt32 i = 0; i != n; ++i)
	{
		auto ns = fn->GetNamespaceUsage(i);
		if (ns && !ns->DoesContain(fn))
		{
			SharedStr nsName(ns->GetScriptName(fn));
			if (!nsName.empty()) { out << ", using = "; out << nsName.c_str(); }
		}
	}
}

void DMS_WriteResultType(OutStreamBase& out, const TreeItem* fn, const TreeItem* resultChild)
{
	if (TreeItem_IsFunctionResultFunction(fn) || (resultChild && resultChild->IsFunctionItem()))
	{
		if (auto rsig = TreeItem_GetFunctionResultSig(fn))
		{
			out << SharedStr(rsig->GetScriptName(fn)).c_str();
			DMS_WriteTypeArgs(out, TreeItem_GetFunctionResultSigTypeArgs(fn));
		}
		else
			out << "function";
		return;
	}
	if (resultChild) { DMS_WriteValuesPrefix(out, resultChild); DMS_WriteDomainSuffix(out, resultChild); return; } // '<prefix> (domain)'
	out << "container"; // defensive
}

} // anonymous namespace

void TreeItem::XML_DumpFunctionDecl(OutStreamBase* out, bool notWritingDictionary) const
{
	bool isVariantSet = TreeItem_IsFunctionVariantSet(this);
	bool isVariant    = GetTreeParent() && GetTreeParent()->IsFunctionItem() && TreeItem_IsFunctionVariantSet(GetTreeParent().get());
	bool isSigOnly    = TreeItem_IsFunctionSignatureOnly(this);
	SharedStr nameStr(GetName());
	bool mustDumpEndogenous = !IsDumpingToFolder();
	auto passes = [&](const TreeItem* s) {
		return (notWritingDictionary || !s->IsDisabledStorage())
			&& (mustDumpEndogenous || !s->IsEndogenous());
	};

	// --- variant SET: 'function name { variant v(...) ...; ... }'
	if (isVariantSet)
	{
		XML_OutElement elem(*out, "function", nameStr.c_str());
		elem.SetHasSubItems();
		out->BeginSubItems();
		for (const TreeItem* c = _GetFirstSubItem(); c; c = c->GetNextItem())
			if (passes(c))
				TreeItem_XML_DumpSubItemSafe(c, out, notWritingDictionary);
		out->EndSubItems();
		return;
	}

	UInt32 nrParams = TreeItem_GetFunctionParamCount(this);
	TokenID resultNameTok = TreeItem_GetFunctionResultName(this);
	const TreeItem* resultChild = nullptr;
	for (const TreeItem* c = _GetFirstSubItem(); c; c = c->GetNextItem())
		if (c->GetID() == resultNameTok) { resultChild = c; break; }

	// header: 'function name<tvs>(params), using = ns -> result'  (variants: keyword 'variant';
	// signature alias: 'name = function<tvs>(params) -> type;' -- name precedes the keyword)
	XML_OutElement elem(*out,
		isSigOnly ? nameStr.c_str() : (isVariant ? "variant" : "function"),
		isSigOnly ? "= function" : nameStr.c_str());

	DMS_WriteTypeVars(*out, this);
	*out << "(";
	const TreeItem* p = _GetFirstSubItem();
	for (UInt32 i = 0; i != nrParams && p; ++i, p = p->GetNextItem())
	{
		if (i) *out << "; ";
		DMS_WriteFunctionParam(*out, this, p, i, nrParams);
	}
	*out << ")";
	DMS_WriteFunctionUsings(*out, this);
	*out << " -> ";
	DMS_WriteResultType(*out, this, resultChild);

	if (isSigOnly)
		return; // 'nuf = function<...>(...) -> type;' -- no designation, no body; the dtor emits ';'

	if (resultChild) { *out << " := "; *out << SharedStr(resultChild->GetName()).c_str(); }

	// body block: all non-param children (the designated result child renders here too)
	const TreeItem* firstBody = _GetFirstSubItem();
	for (UInt32 k = nrParams; k && firstBody; --k) firstBody = firstBody->GetNextItem();
	const TreeItem* scan = firstBody;
	while (scan && !passes(scan)) scan = scan->GetNextItem();
	if (!scan)
		return; // no dumpable body item -> no block; dtor emits ';'
	elem.SetHasSubItems();
	out->BeginSubItems();
	for (const TreeItem* s = firstBody; s; s = s->GetNextItem())
		if (passes(s))
			TreeItem_XML_DumpSubItemSafe(s, out, notWritingDictionary);
	out->EndSubItems();
}

void TreeItem::XML_Dump(OutStreamBase* xmlOutStr, bool notWritingDictionary) const
{
	// write #include <filename> if configStore defined
	if (xmlOutStr->GetLevel() > 0 && IsDumpingToFolder())
	{
		SharedStr dirName = SharedStr( configStorePropDefPtr->GetValue(this) );
		if (!dirName.empty())
		{
			if (!*getFileNameExtension(dirName.c_str()))
			{
				if (xmlOutStr->GetSyntaxType() == OutStreamBase::ST_DMS)
					dirName += ".dms";
				else
					dirName += ".xml";
			}
			xmlOutStr->WriteInclude(dirName.c_str());
			if (xmlOutStr->HasFileName())
				IncludeFileSave(this, dirName.c_str());
			return;
		}
	}

	// A function item serializes as a 'function name<tvs>(params) -> result' declaration in
	// DMS syntax rather than the generic 'container ...: IsTemplate' form (which is misleading
	// and unpastable). Other stream types (XML/HTM detail pages) keep the generic rendering.
	if (xmlOutStr->GetSyntaxType() == OutStreamBase::ST_DMS && IsFunctionItem())
	{
		XML_DumpFunctionDecl(xmlOutStr, notWritingDictionary);
		return;
	}

	// Copy of code from Object because xmlElem must live after subItems
	SharedStr tagName = SharedStr((xmlOutStr->GetSyntaxType() != OutStreamBase::ST_DMS) ? SharedStr(GetXmlClassName()) : GetSignature());

	XML_OutElement xmlElem(*xmlOutStr, tagName.c_str(), GetName().c_str());

	xmlOutStr->DumpPropList(this);

	if (notWritingDictionary)
		xmlOutStr->DumpSubTags(this);
	else if (this == t_MmdDictionaryRoot)
	{
		// #1154: record what the stored bytes were written against for every unit declared
		// OUTSIDE this dictionary. Merged onto the read holder, #1180 folds these restrictions
		// into every sub-item read through it.
		auto restrictions = Mmd_SynthesizeExternalUnitRestrictions(this);
		if (!restrictions.empty())
			xmlOutStr->DumpSubTag(ICHECK_NAME, restrictions.c_str(), false);
	}
	// end of Copy

	if (IsDataItem(this))
	{
		bool isDataBlock = GetCalculatorMember() && GetCalculatorMember()->IsDataBlock();
		if (isDataBlock || HasConfigData())
		{
			xmlOutStr->DumpSubTagDelim();
			if (isDataBlock)
				*xmlOutStr << GetCalculatorMember()->GetExpr().c_str();
			else
			{
				TreeItemInterestPtr holder(this);
				XML_DumpData(xmlOutStr);
			}
		}
	}
	else if (IsUnit(this) && !notWritingDictionary)
	{
		auto au = AsUnit(this);
		if (au->HasVarRange() && IsCalculatingOrReady(au->GetCurrRangeItem().get()))
		{
			// when the dictionary is written at OpenForWrite time, the range of this unit may not have been
			// calculated yet (issue #1130: we can be inside PrepareDataUsage of this very unit);
			// then skip the Range subtag rather than tripping the IsCalculatingOrReady invariant in WaitReady
			TreeItemInterestPtr xholder(this);
			this->PrepareDataUsage(DrlType::Certain);

			xmlOutStr->DumpSubTag("Range", au->GetRangeAsStr(FormattingFlags::None).c_str(), false);
		}
	}

	// check if any non endogenous subitems exist
	const TreeItem* subItem = _GetFirstSubItem(); // we don't want UpdateMetaInfo
	if (!subItem)
		return;
	bool mustDumpEndogenousSubItems = !IsDumpingToFolder();
	while (true)
	{
		if (notWritingDictionary || !subItem->IsDisabledStorage()) // disabled storage items are not dumped in MMD dictionary
			if (mustDumpEndogenousSubItems || !subItem->IsEndogenous())
				break; // found one
		subItem = subItem->GetNextItem();
		if (!subItem)
			return; // no non endogenous subitems, so we don't dump subitems
	}

	// output all non endogenous subitems
	xmlElem.SetHasSubItems();
	xmlOutStr->BeginSubItems();

	subItem = _GetFirstSubItem(); // we don't want UpdateMetaInfo
	while (subItem)
	{
		if (notWritingDictionary || !subItem->IsDisabledStorage()) // disabled storage items are not dumped in MMD dictionary
			if (mustDumpEndogenousSubItems || !subItem->IsEndogenous())
				TreeItem_XML_DumpSubItemSafe(subItem, xmlOutStr, notWritingDictionary);
		subItem = subItem->GetNextItem();
	}
	xmlOutStr->EndSubItems();
}


