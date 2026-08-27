// Copyright (C) 1998-2026 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "TicPCH.h"
#include "act/UpdateMark.h" // UpdateMarker

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "LispTreeType.h"

#include "LispList.h"

#include "act/MainThread.h"
#include "mci/ValueClass.h"
#include "mci/ValueComposition.h"
#include "utl/SourceLocation.h"
#include "xct/DmsException.h"

#include "AbstrDataItem.h"
#include "AbstrUnit.h"
#include "TreeItemProps.h"

#include "stg/AbstrStorageManager.h"

//----------------------------------------------------------------------
// Definition of private helper funcs
//----------------------------------------------------------------------


TIC_CALL LispRef slSubItemCall(LispPtr baseExpr, CharPtrRange relPath)
{
	static LispRef subItemSymb = LispRef(token::subitem);
	assert(!baseExpr.EndP());
	return List3<LispRef>(subItemSymb, baseExpr, LispRef(relPath.begin(), relPath.end()));
}

// *****************************************************************************
// Section:     ParseResult Implementation of public interface
// *****************************************************************************

// tree ::= [sign [srcSpec subItems]] == (tiSign props subItem1 ...)
// subItems ::= (tree ... )

// srcSpec ::= calcRule
// srcSpec ::= read   ( storageName, storageType, relPath )
// srcSpec ::= readSql( storageName, storageType, relPath, sqlString, relPathToSqlString)
// srcSpec ::= EndP

// tree ::= tree(tiSign, storageSpec, calcRule,  subItem1, subItem2, ...)

// tiSign ::= (attr  name domainSpec valuesSpec)
// tiSign ::= (param name valuesSpec)
// tiSign ::= (unit  name valueTypeName coordTypeName)

// domainSpec ::= unitSpec
// valuesSpec ::= unitSpec

static StaticTokenID readSqlID("readSql");
static StaticTokenID readID("read");

namespace token {
	TIC_CALL StaticTokenID add("add");
	TIC_CALL StaticTokenID sub("sub");
	TIC_CALL StaticTokenID mul("mul");
	TIC_CALL StaticTokenID div("div");
	TIC_CALL StaticTokenID mod("mod");

	TIC_CALL StaticTokenID neg("neg");

	TIC_CALL StaticTokenID eq("eq");
	TIC_CALL StaticTokenID ne("ne");
	TIC_CALL StaticTokenID lt("lt");
	TIC_CALL StaticTokenID le("le");
	TIC_CALL StaticTokenID gt("gt");
	TIC_CALL StaticTokenID ge("ge");

	TIC_CALL StaticTokenID id("id");

	TIC_CALL StaticTokenID and_("and");
	TIC_CALL StaticTokenID or_("or");
	TIC_CALL StaticTokenID not_("not");
	TIC_CALL StaticTokenID iif("iif");

	TIC_CALL StaticTokenID const_("const");;

	TIC_CALL StaticTokenID true_("true");
	TIC_CALL StaticTokenID false_("false");
	TIC_CALL StaticTokenID pi("?");

	TIC_CALL StaticTokenID null_b("null_b");
	TIC_CALL StaticTokenID null_w("null_w");
	TIC_CALL StaticTokenID null_u("null_u");
	TIC_CALL StaticTokenID null_u64("null_u64");
	TIC_CALL StaticTokenID null_c("null_c");
	TIC_CALL StaticTokenID null_s("null_s");
	TIC_CALL StaticTokenID null_i("null_i");
	TIC_CALL StaticTokenID null_i64("null_i64");
	TIC_CALL StaticTokenID null_f("null_f");
	TIC_CALL StaticTokenID null_d("null_d");
	TIC_CALL StaticTokenID null_sp("null_sp");
	TIC_CALL StaticTokenID null_wp("null_wp");
	TIC_CALL StaticTokenID null_ip("null_ip");
	TIC_CALL StaticTokenID null_up("null_up");
	TIC_CALL StaticTokenID null_fp("null_fp");
	TIC_CALL StaticTokenID null_dp("null_dp");
	TIC_CALL StaticTokenID null_str("null_str");

	bool isConst(TokenID t)
	{ 
		assert(true_.GetNr(TokenID::TokenKey()) + 19 == null_str.GetNr(TokenID::TokenKey()));
		return t >= true_ && t <= null_str;
	}

	TIC_CALL StaticTokenID arrow("arrow");
	TIC_CALL StaticTokenID lookup("lookup");
	TIC_CALL StaticTokenID convert("convert");
	TIC_CALL StaticTokenID rounded_convert("rounded_convert");
	TIC_CALL StaticTokenID eval("eval");
	TIC_CALL StaticTokenID scope("scope");

	TIC_CALL StaticTokenID subitem("SubItem");
	TIC_CALL StaticTokenID NrOfRows("NrOfRows");
	TIC_CALL StaticTokenID range("range");
	TIC_CALL StaticTokenID cat_range("cat_range");
	TIC_CALL StaticTokenID TiledUnit("TiledUnit");

	TIC_CALL StaticTokenID point("point");
	StaticTokenID point_xy("point_xy");
	TIC_CALL StaticTokenID union_data("union_data");
	TIC_CALL StaticTokenID ordered_union_data("ordered_union_data");
	StaticTokenID sourceDescr("SourceDescr");
	TIC_CALL StaticTokenID container("container");
	TIC_CALL StaticTokenID classify("classify");

//	SELECT section BEGIN
	TIC_CALL StaticTokenID select("select");
	TIC_CALL StaticTokenID select_uint8("select_uint8");
	TIC_CALL StaticTokenID select_uint16("select_uint16");
	TIC_CALL StaticTokenID select_uint32("select_uint32");
	TIC_CALL StaticTokenID select_uint64("select_uint64");

	TIC_CALL StaticTokenID select_with_org_rel("select_with_org_rel");
	TIC_CALL StaticTokenID select_uint8_with_org_rel("select_uint8_with_org_rel");
	TIC_CALL StaticTokenID select_uint16_with_org_rel("select_uint16_with_org_rel");
	TIC_CALL StaticTokenID select_uint32_with_org_rel("select_uint32_with_org_rel");
	TIC_CALL StaticTokenID select_uint64_with_org_rel("select_uint64_with_org_rel");

	TIC_CALL StaticTokenID select_with_attr_by_cond("select_with_attr_by_cond");
	TIC_CALL StaticTokenID select_uint8_with_attr_by_cond("select_uint8_with_attr_by_cond");
	TIC_CALL StaticTokenID select_uint16_with_attr_by_cond("select_uint16_with_attr_by_cond");
	TIC_CALL StaticTokenID select_uint32_with_attr_by_cond("select_uint32_with_attr_by_cond");
	TIC_CALL StaticTokenID select_uint64_with_attr_by_cond("select_uint64_with_attr_by_cond");

	TIC_CALL StaticTokenID select_with_org_rel_with_attr_by_cond("select_with_org_rel_with_attr_by_cond");
	TIC_CALL StaticTokenID select_uint8_with_org_rel_with_attr_by_cond("select_uint8_with_org_rel_with_attr_by_cond");
	TIC_CALL StaticTokenID select_uint16_with_org_rel_with_attr_by_cond("select_uint16_with_org_rel_with_attr_by_cond");
	TIC_CALL StaticTokenID select_uint32_with_org_rel_with_attr_by_cond("select_uint32_with_org_rel_with_attr_by_cond");
	TIC_CALL StaticTokenID select_uint64_with_org_rel_with_attr_by_cond("select_uint64_with_org_rel_with_attr_by_cond");

	TIC_CALL StaticTokenID select_with_attr_by_org_rel("select_with_attr_by_org_rel");
	TIC_CALL StaticTokenID select_uint8_with_attr_by_org_rel("select_uint8_with_attr_by_org_rel");
	TIC_CALL StaticTokenID select_uint16_with_attr_by_org_rel("select_uint16_with_attr_by_org_rel");
	TIC_CALL StaticTokenID select_uint32_with_attr_by_org_rel("select_uint32_with_attr_by_org_rel");
	TIC_CALL StaticTokenID select_uint64_with_attr_by_org_rel("select_uint64_with_attr_by_org_rel");

	TIC_CALL StaticTokenID collect_by_cond("collect_by_cond");
	TIC_CALL StaticTokenID collect_by_org_rel("collect_by_org_rel"); // synonimous with lookup, arrow-operator, and (reversed) array-index operator

	TIC_CALL StaticTokenID collect_attr_by_cond("collect_attr_by_cond");
	TIC_CALL StaticTokenID collect_attr_by_org_rel("collect_attr_by_org_rel");

	// #337: the spec forms, whose first argument carries the configuration

	TIC_CALL StaticTokenID select_spec("select_spec");
	TIC_CALL StaticTokenID collect_spec("collect_spec");
	TIC_CALL StaticTokenID table_spec("table_spec");

	TIC_CALL StaticTokenID recollect_by_cond("recollect_by_cond");
	StaticTokenID recollect_by_org_rel("recollect_by_org_rel");

//	SELECT section END

	TIC_CALL StaticTokenID nr_OrgEntity("nr_OrgEntity");
	TIC_CALL StaticTokenID polygon_rel("polygon_rel");
	TIC_CALL StaticTokenID part_rel("part_rel");
	TIC_CALL StaticTokenID arc_rel("arc_rel");;
	TIC_CALL StaticTokenID sequence_rel("sequence_rel");;
	TIC_CALL StaticTokenID org_rel("org_rel");
	TIC_CALL StaticTokenID first_rel("first_rel");
	TIC_CALL StaticTokenID second_rel("second_rel");
	TIC_CALL StaticTokenID ordinal("ordinal");
	StaticTokenID BaseUnit("BaseUnit");
	StaticTokenID CrsUnit("CrsUnit"); // see doc/development/crs-metric-decoupling.md
	StaticTokenID UInt32("UInt32");
	TIC_CALL StaticTokenID left("left");
	TIC_CALL StaticTokenID right("right");
	TIC_CALL StaticTokenID DomainUnit("DomainUnit");
	StaticTokenID ValuesUnit("ValuesUnit");
	TIC_CALL StaticTokenID integrity_check("IntegrityCheck");

	StaticTokenID map("map");
	TIC_CALL StaticTokenID spatial_reference("spatial_reference");
	TIC_CALL StaticTokenID geometry("geometry");
	TIC_CALL StaticTokenID geometry_z("geometry_z");
	TIC_CALL StaticTokenID geometry_m("geometry_m");
	TIC_CALL StaticTokenID PhaseContainer("PhaseContainer");

	StaticTokenID SubItems("SubItems");
	StaticTokenID Error("Error");
	StaticTokenID SigAndSub("SigAndSub");

	TIC_CALL StaticTokenID direct_index("direct_index");
	TIC_CALL StaticTokenID index("index");
	TIC_CALL StaticTokenID subindex("subindex");
}

LispRef CreateStorageSpec(const TreeItem* src)
{
	dms_assert(src);
//	dbg_assert(!src->HasCalculatorImpl()); // PRECONDITION
	dms_assert(src->IsLoadable());         // PRECONDITION

	auto storageParent = src->GetStorageParent(false);
	if (!storageParent)
		return LispRef();
	auto storageManager = storageParent->GetStorageManager();
	SharedStr storageName = storageManager ? storageManager->GetNameStr() : TreeItemPropertyValue(storageParent.get(), storageNamePropDefPtr);
	TokenID   storageType = storageManager ? storageManager->GetDynamicClass()->GetID() : storageTypePropDefPtr->GetValue(storageParent.get());
	SharedTreeItem sqlStringParent = make_shared_tree(src, existing_obj{});
	while (true)
	{
		if (sqlStringPropDefPtr->HasNonDefaultValue(sqlStringParent.get()))
		{
			auto sqlString = TreeItemPropertyValue(sqlStringParent.get(), sqlStringPropDefPtr);
			return List6<LispRef>(
				LispRef(readSqlID),
				LispRef(storageName.c_str()),
				LispRef(storageType),
				LispRef(src->GetRelativeName(storageParent.get()).c_str()),
				LispRef(sqlString.c_str()),
				LispRef(src->GetRelativeName(sqlStringParent.get()).c_str())
			);
		}
		if (sqlStringParent.get() == storageParent.get())
			break;
		sqlStringParent = sqlStringParent->GetTreeParent();
		assert(sqlStringParent);
	}
	assert(sqlStringParent == storageParent);
	return List4<LispRef>(
		LispRef(readID),
		LispRef(storageName.c_str()),
		LispRef(storageType),
		LispRef(src->GetRelativeName(storageParent.get()).c_str())
	);
}

static StaticTokenID paramID("Param");
static StaticTokenID attrID("Attr");
static StaticTokenID unitID("Unit");
static StaticTokenID itemID("Item");

LispRef CreateLispSubTree(const TreeItem* self, bool inclSubTree);

LispRef CreateLispSign(const TreeItem* self)
{
	dbg_assert(IsMetaThread() && IsUnit(self) //&& HasConfigData()
		|| self->m_LastGetStateTS == UpdateMarker::GetLastTS() || self->InTemplate() || self->IsPassor()); // suppliers have been scanned, thus mc_Calculator and m_SupplCache have been determined.

	try {
		const AbstrDataItem* sourceData = AsDynamicDataItem(self);
		const AbstrUnit*     sourceUnit = AsDynamicUnit(self);

		if (sourceData)
		{
			if (sourceData->HasVoidDomainGuarantee())
			{
				return ExprList(paramID, LispRef(self->GetID())
					, CreateLispSubTree(sourceData->GetAbstrValuesUnit(), false)
					, LispRef(GetValueCompositionID(sourceData->GetValueComposition()))
					);
			}
			return ExprList(attrID, LispRef(self->GetID())
				, CreateLispSubTree(sourceData->GetAbstrDomainUnit(), false)
				, CreateLispSubTree(sourceData->GetAbstrValuesUnit(), false)
				, LispRef(GetValueCompositionID(sourceData->GetValueComposition()))
				);
		}
		if (sourceUnit)
		{
			return ExprList(unitID, LispRef(self->GetID()), LispRef(sourceUnit->GetValueType()->GetID()));
		}
		return ExprList(itemID, LispRef(self->GetID()));
	}
	catch (const DmsException& x)
	{
		SharedStr msg = x.AsErrMsg()->Why();
		return List2<LispRef>(LispRef(token::Error), LispRef(msg.begin(), msg.send()));
	}
}


// REMOVELispComponent s_UseLispAdm;
static auto lspTrue = ExprList(token::true_);
static auto lspFalse = ExprList(token::false_);

auto AsLispRef(Bool v, LispPtr valuesUnitKeyExpr) -> LispRef
{
	return v ? lspTrue.AsLispPtr() : lspFalse.AsLispPtr();
}


LispRef CreateLispSubTree(const TreeItem* self, bool inclSubTree)
{
	LispRef result;
	try {
		if (!self)
			return {};

		if (inclSubTree)
			for (const TreeItem* subItem = self->GetFirstSubItem(); subItem; subItem = subItem->GetNextItem())
			{
				result = LispRef(CreateLispSubTree(subItem, true), result);
			}

		if (self->IsLoadable())
			result = LispRef(CreateStorageSpec(self), result);
		else
			result = LispRef(LispRef(token::SubItems), result);
	}
	catch (const DmsException& x)
	{
		SharedStr msg = x.AsErrMsg()->Why();
		result = LispRef(LispRef(msg.begin(), msg.send()), result);
		result = LispRef(LispRef(token::Error), result); // TOOD G8: move to token::
	};
	result = LispRef(CreateLispSign(self), result);
	result = LispRef(LispRef(token::SigAndSub), result); // TOOD G8: move to token::

#if defined(MG_DEBUG_LISP_TREE)
	reportF(SeverityTypeID::ST_MinorTrace,"CreateLispSubTree {} {}",  inclSubTree, AsFLispSharedStr(result, FormattingFlags::ThousandSeparator).c_str());
	dms_assert(IsExpr(result));
#endif

	return result;
}

LispRef CreateLispTree(const TreeItem* self, bool inclSubTree)
{
	assert(self);
	UInt32 loadNumber = 0;
	auto location = self->GetLocation();
	if (location)
		loadNumber = location->m_ConfigFileDescr->m_LoadNumber;

	auto result = ExprList(token::sourceDescr
	,	LispRef(TokenID(self->GetFullName()))
	,	LispRef(loadNumber)
	,	CreateLispSubTree(self, inclSubTree)
	);
#if defined(MG_DEBUG_LISP_TREE)
	reportF(SeverityTypeID::ST_MinorTrace, "CreateLispTree: {} {}", inclSubTree, AsFLispSharedStr(result, FormattingFlags::ThousandSeparator).c_str());
	dms_assert(IsExpr(result));
#endif
	return result;
}


LispRef slConvertedLispExpr(LispPtr result, LispPtr vu)
{
	dms_assert(token::convert);
	static auto convertSymb = LispRef(token::convert);

	return List(convertSymb, result, vu);
}

LispRef slUnionDataLispExpr(LispPtr valueList, SizeT sz)
{
	if (sz == 1)
	{
		dms_assert(valueList.IsRealList());
		dms_assert(valueList.Right().EndP());
		return valueList.Left();
	}
	static auto uint32Ref = ExprList(ValueWrap<UInt32>::GetStaticClass()->GetID());
	auto unionRange = AsLispRef(Range<UInt32>(0, sz), std::move(uint32Ref), true);

	return LispRef(LispRef(token::union_data), LispRef(unionRange, valueList));
}


