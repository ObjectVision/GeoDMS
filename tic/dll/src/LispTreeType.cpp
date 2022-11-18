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
#pragma hdrstop

#include "LispTreeType.h"

#include "LispList.h"

#include "act/MainThread.h"
#include "dbg/SeverityType.h"
#include "mci/ValueClass.h"
#include "mci/ValueComposition.h"
#include "ser/AsString.h"

#include "AbstrCalculator.h"
#include "AbstrDataItem.h"
#include "AbstrUnit.h"
#include "TicInterface.h"
#include "TreeItemProps.h"

#include "stg/AbstrStorageManager.h"

//----------------------------------------------------------------------
// Definition of private helper funcs
//----------------------------------------------------------------------


LispRef slSubItemCall(LispPtr baseExpr, CharPtrRange relPath)
{
	static LispRef subItemSymb = LispRef(token::subitem);
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

static TokenID readSqlID = GetTokenID_st("readSql");
static TokenID readID = GetTokenID_st("read");

namespace token {
	TIC_CALL TokenID add = GetTokenID_st("add");
	TIC_CALL TokenID sub = GetTokenID_st("sub");
	TIC_CALL TokenID mul = GetTokenID_st("mul");
	TIC_CALL TokenID div = GetTokenID_st("div");
	TIC_CALL TokenID mod = GetTokenID_st("mod");

	TIC_CALL TokenID neg = GetTokenID_st("neg");

	TIC_CALL TokenID eq = GetTokenID_st("eq");
	TIC_CALL TokenID ne = GetTokenID_st("ne");
	TIC_CALL TokenID lt = GetTokenID_st("lt");
	TIC_CALL TokenID le = GetTokenID_st("le");
	TIC_CALL TokenID gt = GetTokenID_st("gt");
	TIC_CALL TokenID ge = GetTokenID_st("ge");

	TIC_CALL TokenID id = GetTokenID_st("id");

	TIC_CALL TokenID and_ = GetTokenID_st("and");
	TIC_CALL TokenID or_ = GetTokenID_st("or");
	TIC_CALL TokenID not_ = GetTokenID_st("not");
	TIC_CALL TokenID iif = GetTokenID_st("iif");

	TIC_CALL TokenID true_ = GetTokenID_st("true");
	TIC_CALL TokenID false_ = GetTokenID_st("false");

	TIC_CALL TokenID arrow = GetTokenID_st("arrow");
	TIC_CALL TokenID lookup = GetTokenID_st("lookup");
	TIC_CALL TokenID convert = GetTokenID_st("convert");
	TIC_CALL TokenID eval = GetTokenID_st("eval");
	TIC_CALL TokenID scope = GetTokenID_st("scope");

	TIC_CALL TokenID subitem = GetTokenID_st("SubItem");
	TIC_CALL TokenID NrOfRows = GetTokenID_st("NrOfRows");
	TIC_CALL TokenID range = GetTokenID_st("range");
	TIC_CALL TokenID TiledUnit = GetTokenID_st("TiledUnit");

	TIC_CALL TokenID point = GetTokenID_st("point");
	TIC_CALL TokenID union_data = GetTokenID_st("union_data");
	TIC_CALL TokenID sourceDescr = GetTokenID_st("SourceDescr");
	TIC_CALL TokenID container = GetTokenID_st("container");
	TIC_CALL TokenID nr_OrgEntity = GetTokenID_st("nr_OrgEntity");
	TIC_CALL TokenID org_rel = GetTokenID_st("org_rel");
	TIC_CALL TokenID first_rel = GetTokenID_st("first_rel");
	TIC_CALL TokenID second_rel = GetTokenID_st("second_rel");
	TIC_CALL TokenID BaseUnit = GetTokenID_st("BaseUnit");
	TIC_CALL TokenID UInt32 = GetTokenID_st("UInt32");
	TIC_CALL TokenID left = GetTokenID_st("left");
	TIC_CALL TokenID right = GetTokenID_st("right");
	TIC_CALL TokenID DomainUnit = GetTokenID_st("DomainUnit");
	TIC_CALL TokenID ValuesUnit = GetTokenID_st("ValuesUnit");
	TIC_CALL TokenID integrity_check = GetTokenID_st("IntegrityCheck");

	TIC_CALL TokenID map = GetTokenID_st("map");
	TIC_CALL TokenID geometry = GetTokenID_st("geometry");
}

LispRef CreateStorageSpec(const TreeItem* src)
{
	dms_assert(src);
//	dbg_assert(!src->HasCalculatorImpl()); // PRECONDITION
	dms_assert(src->IsLoadable());         // PRECONDITION

	const TreeItem* storageParent = src->GetStorageParent(false);
	if (!storageParent)
		return LispRef();
	auto storageManager = storageParent->GetStorageManager();
	SharedStr storageName = storageManager ? storageManager->GetNameStr() : TreeItemPropertyValue(storageParent, storageNamePropDefPtr);
	TokenID   storageType = storageManager ? storageManager->GetDynamicClass()->GetID() : storageTypePropDefPtr->GetValue(storageParent);
	const TreeItem* sqlStringParent = src;
	while (true)
	{
		if (sqlStringPropDefPtr->HasNonDefaultValue(sqlStringParent))
		{
			auto sqlString = TreeItemPropertyValue(sqlStringParent, sqlStringPropDefPtr);
			return List6<LispRef>(
				LispRef(readSqlID),
				LispRef(storageName.c_str()),
				LispRef(storageType),
				LispRef(src->GetRelativeName(storageParent).c_str()),
				LispRef(sqlString.c_str()),
				LispRef(src->GetRelativeName(sqlStringParent).c_str())
			);
		}
		if (sqlStringParent == storageParent) 
			break;
		sqlStringParent = sqlStringParent->GetTreeParent();
		dms_assert(sqlStringParent);
	}
	dms_assert(sqlStringParent == storageParent);
	return List4<LispRef>(
		LispRef(readID),
		LispRef(storageName.c_str()),
		LispRef(storageType),
		LispRef(src->GetRelativeName(storageParent).c_str())
	);
}

static TokenID paramID = GetTokenID_st("param");
static TokenID attrID = GetTokenID_st("attr");
static TokenID unitID = GetTokenID_st("unit");
static TokenID itemID = GetTokenID_st("item");

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
		return List2<LispRef>(LispRef("Error"), LispRef(msg.begin(), msg.send()));
	}
}


// REMOVELispComponent s_UseLispAdm;
static auto lspTrue = ExprList(token::true_);
static auto lspFalse = ExprList(token::false_);

auto AsLispRef(Bool v, LispPtr valuesUnitKeyExpr) -> LispRef
{
	return v ? lspTrue : lspFalse;
}


LispRef CreateLispSubTree(const TreeItem* self, bool inclSubTree)
{
	LispRef result;
	try {
		self->DetermineState();
		if (inclSubTree)
			for (const TreeItem* subItem = self->GetFirstSubItem(); subItem; subItem = subItem->GetNextItem())
			{
				result = LispRef(CreateLispSubTree(subItem, true), result);
			}

		if (self->IsLoadable())
			result = LispRef(CreateStorageSpec(self), result);
		else
			result = LispRef(LispRef(GetTokenID("SubItems")), result);
	}
	catch (const DmsException& x)
	{
		SharedStr msg = x.AsErrMsg()->Why();
		result = LispRef(LispRef(msg.begin(), msg.send()), result);
		result = LispRef(LispRef(GetTokenID("Error")), result); // TOOD G8: move to token::
	};
	result = LispRef(CreateLispSign(self), result);
	result = LispRef(LispRef(GetTokenID("SigAndSub")), result); // TOOD G8: move to token::

#if defined(MG_DEBUG_LISP_TREE)
	reportF(SeverityTypeID::ST_MinorTrace,"CreateLispSubTree %d %s",  inclSubTree, AsFLispSharedStr(result).c_str());
	dms_assert(IsExpr(result));
#endif

	return result;
}

LispRef CreateLispTree(const TreeItem* self, bool inclSubTree)
{
	auto result = ExprList(token::sourceDescr
	,	LispRef(TokenID(self->GetFullName()))
	,	CreateLispSubTree(self, inclSubTree)
	);
#if defined(MG_DEBUG_LISP_TREE)
	reportF(SeverityTypeID::ST_MinorTrace, "CreateLispTree: %d %s", inclSubTree, AsFLispSharedStr(result).c_str());
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
	auto unionRange = AsLispRef(Range<UInt32>(0, sz), std::move(uint32Ref));

	return LispRef(LispRef(token::union_data), LispRef(unionRange, valueList));
}


