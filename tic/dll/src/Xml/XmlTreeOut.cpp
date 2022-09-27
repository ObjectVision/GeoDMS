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

#include "xml/XmlTreeOut.h"

#include "RtcInterface.h"
#include "act/TriggerOperator.h"
#include "dbg/DmsCatch.h"
#include "mci/ValueClass.h"
#include "ser/FileStreamBuff.h"
#include "utl/Environment.h"
#include "utl/splitPath.h"
#include "xct/DmsException.h"
#include "xml/XmlTreeOut.h"

#include "stg/AbstrStorageManager.h"

#include "AbstrCalculator.h"
#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "DataLocks.h"
#include "DataStoreManagerCaller.h"
#include "xml/XmlTreeOut.h"
#include "StateChangeNotification.h"
#include "SupplCache.h"
#include "TicInterface.h"
#include "TicPropDefConst.h"
#include "TreeItemProps.h"
#include "TreeItemClass.h"
#include "TreeItemContextHandle.h"
#include "TreeItemProps.h"
#include "TreeItemSet.h"
#include "Unit.h"
#include "UnitClass.h"
#include "UsingCache.h"

// *****************************************************************************
//											LOCAL HELPER FUNCS
// *****************************************************************************

SharedStr GetStrRange(const AbstrUnit* unit)
{
	try 
	{
		bool singleDimensional = unit->GetNrDimensions() == 1;
		if (singleDimensional && !unit->GetValueType()->IsNumeric())
			return SharedStr("non numeric");
		if (!CheckDataReady(unit->GetCurrRangeItem()))
			goto notCalculated;
		if (singleDimensional)
		{
			auto [b, e] = unit->GetRangeAsFloat64();
			if (b > e)
				goto unbounded;
			return mySSPrintF("From %s to %s", AsString(b).c_str(), AsString(e).c_str());
		}
		dms_assert(unit->GetNrDimensions() == 2);

		auto [from, to_] = unit->GetRangeAsDRect();
		if (!IsLowerBound(from, to_))
			goto unbounded;
		return mySSPrintF("From %s to %s", AsString(DPoint(from.Row(), from.Col())).c_str(), AsString(DPoint(to_.Row(), to_.Col())).c_str());
	}
	catch (const DmsException& x)
	{
		return x.AsErrMsg()->Why();
	}
unbounded:
	return SharedStr("unbounded");
notCalculated:
	return SharedStr("Not Calculated");
}

SharedStr GetTileStrRange(const AbstrUnit* unit, tile_id t)
{
	try
	{
		bool singleDimensional = unit->GetNrDimensions() == 1;
		if (singleDimensional && !unit->GetValueType()->IsNumeric())
			return SharedStr("non numeric");
		if (!CheckDataReady(unit->GetCurrRangeItem()))
			goto notCalculated;
		if (singleDimensional)
		{
			auto [b, e] = unit->GetTileIndexRange(t);
			if (b > e)
				goto unbounded;
			return mySSPrintF("From %s to %s", AsString(b).c_str(), AsString(e).c_str());
		}
		dms_assert(unit->GetNrDimensions() == 2);

		auto [from, to_] = unit->GetTileRangeAsIRect(t);
		if (!IsLowerBound(from, to_))
			goto unbounded;
		return mySSPrintF("From %s to %s", AsString(DPoint(from.Row(), from.Col())).c_str(), AsString(DPoint(to_.Row(), to_.Col())).c_str());
	}
	catch (const DmsException& x)
	{
		return x.AsErrMsg()->Why();
	}
unbounded:
	return SharedStr("unbounded");
notCalculated:
	return SharedStr("Not Calculated");
}

SharedStr GetStrCount(const AbstrUnit* unit)
{
	dms_assert(unit->GetValueType()->IsCountable());
	try {	
		if (!CheckDataReady(unit->GetCurrRangeItem()))
			return SharedStr("Not Calculated");
		return AsString(unit->GetPreparedCount(false));
	}
	catch (const DmsException& x)
	{
		return x.AsErrMsg()->Why();
	}
}

SharedStr GetTileStrCount(const AbstrUnit* unit, tile_id t)
{
	dms_assert(t != no_tile);
	dms_assert(unit->GetValueType()->IsCountable());
	try {
		if (!CheckDataReady(unit->GetCurrRangeItem()))
			return SharedStr("Not Calculated");
		return AsString(unit->GetPreparedTileCount(t));
	}
	catch (const DmsException& x)
	{
		return x.AsErrMsg()->Why();
	}
}

void WriteCdf(XML_Table& xmlTable, const TreeItem* ti)
{
	SharedStr result = TreeItemPropertyValue(ti, cdfPropDefPtr);
	if (result.empty()) return;
	XML_Table::Row row(xmlTable);
	row.EditablePropCell("cdf", "ClassDiagram");
	row.ClickableCell(result.c_str(), ItemUrl(result.c_str()).c_str());
}

bool WriteUnitProps(XML_Table& xmlTable, const AbstrUnit* unit, bool allTileInfo)
{
	dms_assert(unit);
	dms_assert(!SuspendTrigger::DidSuspend()); // PRECONDITION
	dms_assert(IsMainThread());
	dms_assert(unit->GetInterestCount() || unit->WasFailed(FR_Data));

	xmlTable.NameValueRow("ElementType", unit->GetValueType()->GetName().c_str());

	if (unit->InTemplate())
		return false;
	if (unit->GetUnitClass() == Unit<Void>::GetStaticClass())
		return true;

	if (unit->GetNrDimensions() == 1)
	{
		if (unit->GetValueType()->IsNumeric())
		{
			try {
				SharedStr metricStr = unit->GetMetricStr(xmlTable.GetFormattingFlags());
				if (!metricStr.empty())
					xmlTable.EditableNameValueRow(METRIC_NAME, metricStr.c_str(), unit);
			}
			catch (...)
			{
				auto err = catchException(true);
				if (err)
				xmlTable.NameErrRow(METRIC_NAME, *err);
			}
		}
	}
	else
	{
		dms_assert(unit->GetNrDimensions() == 2);
		SharedStr projStr = unit->GetProjectionStr(xmlTable.GetFormattingFlags());
		if (!projStr.empty())
			xmlTable.EditableNameValueRow(PROJECTION_NAME, projStr.c_str(), unit);
	}

	InterestPtr<const AbstrUnit*> currRangeUnit = AsUnit(unit->GetCurrRangeItem());

	if (!CheckDataReady(currRangeUnit.get_ptr()))
		return false;

	ItemReadLock xx(currRangeUnit.get_ptr());

	if (unit->GetValueType()->IsNumeric() || unit->GetNrDimensions() == 2)
		xmlTable.EditableNameValueRow("Range", GetStrRange(currRangeUnit.get_ptr()).c_str(), currRangeUnit.get_ptr());

	if (!unit->GetValueType()->IsCountable())
		return true;

	WriteCdf(xmlTable, unit);


	xmlTable.NameValueRow("NrElements", GetStrCount(currRangeUnit.get_ptr()).c_str());

	if (!currRangeUnit->IsTiled())
		return true;
	auto trd = currRangeUnit->GetTiledRangeData();
	if (!trd)
		return true;

	xmlTable.NameValueRow("RangeSize", AsString(trd->GetRangeSize()).c_str());

	if (trd->IsCovered())
	{
		xmlTable.NameValueRow("Nr Tiles", AsString(trd->GetNrTiles()).c_str());
		xmlTable.NameValueRow("Max Tile size", AsString(trd->GetMaxTileSize()).c_str());
	}
	else if (allTileInfo)
	{
		xmlTable.NameValueRow("DataSize", AsString(trd->GetDataSize()).c_str());
		for (tile_id t = 0, tn = trd->GetNrTiles(); t != tn; ++t)
		{
			XML_Table::Row row(xmlTable);
			row.ValueCell(mySSPrintF("Tile %d", t).c_str());
			row.ValueCell((GetTileStrRange(currRangeUnit.get_ptr(), t) + " = " + GetTileStrCount(currRangeUnit.get_ptr(), t) + " elements.").c_str());
		}
	}
	return true;
}

bool WriteUnitInfo(XML_Table& xmlTable, CharPtr role, const AbstrUnit* unit)
{
	dms_assert(unit);

	dms_assert(unit->GetInterestCount() || unit->WasFailed(FR_Data));

	xmlTable.LinedRow();
	if (unit->IsDefaultUnit())
	{
		XML_Table::Row row(xmlTable);
		row.OutStream().WriteAttr("bgcolor", CLR_HROW);
			row.EditablePropCell(role);
			row.ValueCell(unit->GetValueType()->GetName().c_str());
	}
	else
		xmlTable.EditableNameItemRow(role, unit);
	return WriteUnitProps(xmlTable, unit, false);
}

// *****************************************************************************
// ********** XML structs                                             **********
// *****************************************************************************

// ********** XML_ItemBody                                             *********

XML_ItemBody::XML_ItemBody(OutStreamBase& out, const TreeItem* item, bool showFullName)
	:	XML_OutElement(out, "BODY")
{
	out.WriteAttr("bgcolor", CLR_BODY);

	XML_OutElement xmlElemH2(out, "H2");
	XML_OutElement xmlElemA (out, "A");
	out.WriteAttr("href", ItemUrl(item).c_str());
	if (showFullName)
	{
		if (item->GetTreeParent())
			out << item->GetFullName().c_str();
		else
			out << "(ROOT)";
	}
	else
		out << item->GetName().c_str();
}

// *****************************************************************************
//											COMPONENT HELPER FUNCS
// *****************************************************************************

void NewLine(OutStreamBase& out)
{
	XML_OutElement br(out, "BR", "", false);
}

CharPtr UpdateStateName(UInt32 nc)
{
	switch(nc) {
		case NC2_Invalidated: return "None";
		case NC2_MetaReady:   return "MetaInfoReady";
		case NC2_DataReady:   return "DataReady";
		case NC2_Validated:   return "Validated";
		case NC2_Committed:   return "Validated&Committed";
	}
	return "unrecognized";
}

CharPtr FailStateName(UInt32 fs)
{
	switch(fs) 
	{
		case FR_None     : return "None";
		case FR_Determine: return "DetermineState Failed";
		case FR_MetaInfo : return "MetaInfo Failed"; 
		case FR_Data     : return "Primary Data Derivation Failed"; 
		case FR_Validate : return "Validation (Integrity Check) Failed"; 
		case FR_Committed: return "Committing Data (writing to storage) Failed"; 
	}
	return "unrecognized";
}

void WriteLispRefExpr(OutStreamBase& stream, LispPtr lispExpr)
{
//	lispExpr.PrintAsFLisp(stream.FormattingStream(), 0); doesn't do HtmlEncode
	stream << AsFLispSharedStr(lispExpr).c_str();
}

TIC_CALL void(*s_AnnotateExprFunc)(OutStreamBase& outStream, const TreeItem* searchContext, SharedStr expr);

const TreeItem* GetExprOrSourceDescrAndReturnSourceItem(OutStreamBase& stream, const TreeItem* ti)
{
	SharedStr result = ti->GetExpr();
	if (!result.empty())
	{
		dms_assert(s_AnnotateExprFunc);
		s_AnnotateExprFunc(stream, AbstrCalculator::GetSearchContext(ti, CalcRole::Calculator), result);
		if (AbstrCalculator::MustEvaluate(result.begin()))
		{
			NewLine(stream);
			if (ti->InTemplate())
				stream << "which isn't evaluated here since this item is part of a template definition";
			else
			{
				stream << "which evaluates to:";
				NewLine(stream);
				auto calculator = ti->GetCalculator();
				if (!calculator)
					stream << "(no calculator)";
				else
					calculator->WriteHtmlExpr(stream);
			}
		}
		return ti->GetSourceItem();
	}
	if (!ti->HasCalculator())
	{
		const TreeItem* storageParent = ti->GetStorageParent(false);
		if (storageParent)
		{
			const AbstrStorageManager* sm = storageParent->GetStorageManager();
			dms_assert(sm); // because of POSTCONDITION of GetStorageParant
			stream << "Read from " << sm->GetNameStr().c_str();
		}
		else
			stream << "<none>";
		return nullptr;
	}
	const TreeItem* si = ti->GetSourceItem();
	if (si)
	{
		dms_assert(si != ti);
		dms_assert(!si->IsCacheItem());
		stream << "assigned by ";
		{
			XML_hRef parentRef(stream, ItemUrl(ti->GetTreeParent()).c_str());
			stream << "parent";
		}
		stream << " to ";
		{
			XML_hRef sourceRef(stream, ItemUrl(si).c_str());
			stream << si->GetFullName().c_str();
		}
		return si->GetSourceItem();
	}
	SharedPtr<const AbstrCalculator> calc = ti->GetCalculator();
	if (calc)
		stream << calc->GetAsFLispExprOrg().c_str();
	return nullptr;
}

void GetExprOrSourceDescr(OutStreamBase& stream, const TreeItem* ti)
{
	const TreeItem* si = GetExprOrSourceDescrAndReturnSourceItem(stream, ti);
	while (si)
	{
		dms_assert(si != ti);
		dms_assert(!si->IsCacheItem());
		NewLine(stream);
		stream << "which refers to";
		{
			XML_hRef sourceRef(stream, ItemUrl(si).c_str());
			stream << si->GetFullName().c_str();
		}
		ti = si;
		si = si->GetSourceItem();
	}
}

void GetExprOrSourceDescrRow(XML_Table& xmlTable, const TreeItem* ti)
{
	XML_Table::Row exprRow(xmlTable);
		exprRow.OutStream().WriteAttr("bgcolor", CLR_HROW);
		exprRow.ClickableCell("CalculationRule", "dms:Edit Definition");
		XML_Table::Row::Cell xmlElemTD(exprRow);
			GetExprOrSourceDescr(xmlTable.OutStream(), ti);
}

void GetLispRefRow(XML_Table& xmlTable, LispPtr lispExpr, CharPtr title)
{
	if (lispExpr.EndP())
		return;
	XML_Table::Row exprRow(xmlTable);
		exprRow.ValueCell(title);
		XML_Table::Row::Cell xmlElemTD(exprRow);
			WriteLispRefExpr(xmlTable.OutStream(), lispExpr);
}

const TreeItem* GetTemplRoot(const TreeItem* self)
{
	dms_assert(self);
	while (!self->IsTemplate())
	{
		dms_assert(self->InTemplate());
		self = self->GetTreeParent();
		dms_assert(self);
	}
	return self;
}

void TraceConfigSource(const TreeItem* self, XML_Table& xmlTable)
{
	if (!self)
		return;
	xmlTable.NamedItemRow("Instantiated from", self);
	TraceConfigSource(self->mc_OrgItem, xmlTable);
}

// *****************************************************************************
//											ITERFACE FUNCS
// *****************************************************************************

TIC_CALL void DMS_CONV DMS_TreeItem_XML_Dump(const TreeItem* self, OutStreamBase* xmlOutStr)
{
	DMS_CALL_BEGIN

		dms_assert(xmlOutStr);

		self->XML_Dump(xmlOutStr);

	DMS_CALL_END
}

bool TreeItem_XML_DumpGeneralBody(const TreeItem* self, OutStreamBase* xmlOutStrPtr, bool showAll)
{
	XML_Table xmlTable(*xmlOutStrPtr);
	xmlTable.EditableNameValueRow("FullName", self->GetFullName().c_str());

#if defined(MG_DEBUG)
	if (!self->InTemplate())
	{
		UInt32 nc = DMS_TreeItem_GetProgressState(self);
		xmlTable.NameValueRow(
			"ProgressState",
			mySSPrintF("%s at %d",
				UpdateStateName(nc),
				self->GetLastChangeTS()
			).c_str()
		);
	}
#endif

	if (self->IsFailed())
	{
		auto ft = self->GetFailType();
		auto fr = self->GetFailReason();
		if (fr)
		{
			xmlTable.NameValueRow("FailState", FailStateName(ft));
			xmlTable.NameErrRow("FailReason", *fr);
		}
	}
	if (self->InTemplate())
	{
		xmlTable.NamedItemRow("PartOfTemplate", GetTemplRoot(self));
	}
	SharedStr result = TreeItemPropertyValue(self, labelPropDefPtr);
	if (!result.empty())
		xmlTable.EditableNameValueRow(LABEL_NAME, result.c_str());

	result = self->GetDescr();
	if (!result.empty())
		xmlTable.EditableNameValueRow(DESCR_NAME, result.c_str());

	ValueComposition vc = ValueComposition::Unknown;
	if (IsDataItem(self))
	{
		const AbstrDataItem* di = AsDataItem(self);
		xmlTable.NameValueRow("ValuesType", di->GetAbstrValuesUnit()->GetValueType()->GetName().c_str());
		vc = di->GetValueComposition();
		if (vc != ValueComposition::Single)
			xmlTable.NameValueRow("ValueComposition", GetValueCompositionID(vc).GetStr().c_str());

		WriteCdf(xmlTable, di);
	}
	if (IsUnit(self))
	{
		WriteUnitProps(xmlTable, AsUnit(self), true);
		if (SuspendTrigger::DidSuspend())
			return false;
	}

	TraceConfigSource(self->mc_OrgItem, xmlTable);

	// ==================== Calculation rule and/or Storage description
	xmlTable.LinedRow();
	GetExprOrSourceDescrRow(xmlTable, self);
	if (self->HasCalculator() && !self->WasFailed(FR_MetaInfo))
	{
		auto c = self->GetCalculator();
		if (c)
			GetLispRefRow(xmlTable, c->GetLispExprOrg(), "ParseResult");
	}
	if (!self->WasFailed(FR_MetaInfo))
	{
		auto metaInfo = self->GetCurrMetaInfo({});
		auto calcExpr = GetAsLispRef(metaInfo);
		GetLispRefRow(xmlTable, calcExpr, "CalcExpr");
		if (metaInfo.index() == 1 || metaInfo.index() == 0 && std::get<MetaFuncCurry>(metaInfo).fullLispExpr.EndP())
		{
			auto keyExpr = self->GetCheckedKeyExpr();
			if (keyExpr != calcExpr)
				GetLispRefRow(xmlTable, keyExpr, "CheckedKeyExpr");
		}
	}

	// ==================== Explicit Suppliers
	if (self->HasSupplCache())
	{
		{
			XML_Table::Row exprRow(xmlTable);
			exprRow.OutStream().WriteAttr("bgcolor", CLR_HROW);
			exprRow.ValueCell("ExplicitSuppliers");
			exprRow.ValueCell(explicitSupplPropDefPtr->GetValueAsSharedStr(self).c_str());
		}

		UInt32 n = self->GetSupplCache()->GetNrConfigured(self); // only ConfigSuppliers, Implied suppliers come after this, Calculator & StorageManager have added them
		for (UInt32 i = 0; i < n; ++i)
		{
			const Actor* supplier = self->GetSupplCache()->begin(self)[i];
			auto supplTI = debug_cast<const TreeItem*>(supplier); 
			if (supplTI)
				xmlTable.NamedItemRow(AsString(i).c_str(), supplTI);
		}
	}

	const TreeItem* sp = self->GetStorageParent(false);
	if (sp)
	{
		AbstrStorageManager* sm = sp->GetStorageManager();
		dms_assert(sm);
		bool readOnly = sm->IsReadOnly();
		if (!readOnly || !self->HasCalculator())
		{
			xmlTable.EmptyRow();
			{
				XML_Table::Row row(xmlTable);
				xmlOutStrPtr->WriteAttr("bgcolor", CLR_HROW);
				row.ValueCell(IsUnit(self) || IsDataItem(self)
					? self->HasCalculator()
					? "DataTarget"
					: "DataSource"
					: "Storage");
				row.ClickableCell(sm->GetNameStr().c_str(), sm->GetUrl().c_str());
			}
			{
				XML_Table::Row row(xmlTable);
				row.ValueCell("assigned to");
				row.ItemCell(sp);
			}
			result = storageTypePropDefPtr->GetValue(sp);
			if (!result.empty())
				xmlTable.NameValueRow(STORAGETYPE_NAME, result.c_str());

			xmlTable.NameValueRow("ReadOnly", AsString(readOnly).c_str());

			result = TreeItemPropertyValue(self, sqlStringPropDefPtr);
			if (showAll || !result.empty())
				xmlTable.EditableNameValueRow(SQLSTRING_NAME, result.c_str());
		}
	}
	if (!IsDataItem(self) || self->WasFailed(FR_MetaInfo))
		return true;

	const AbstrUnit* prevUnit = nullptr;
	auto prevVC = vc;
	const AbstrDataItem* di = AsDataItem(self);
	CharPtr title = "ValuesUnit";
	do {
		auto avu = di->GetAbstrValuesUnit();
		vc = di->GetValueComposition();
		if (prevUnit != avu || prevVC != vc)
		{
			if (!WriteUnitInfo(xmlTable, title, avu))
				if (SuspendTrigger::DidSuspend())
					return false;
				else
					break;
			if (vc != ValueComposition::Single)
				xmlTable.NameValueRow("ValueComposition", GetValueCompositionID(vc).GetStr().c_str());

			prevUnit = avu;
			prevVC = vc;
		}
		WriteCdf(xmlTable, di->GetAbstrValuesUnit());

		di = AsDataItem(di->GetReferredItem());
		title = "Derived ValuesUnit";
	} while (di);

	title = "DomainUnit";
	di = AsDataItem(self);
	prevUnit = nullptr;
	do {
		auto adu = di->GetAbstrDomainUnit();
		if (prevUnit != adu)
		{
			if (!WriteUnitInfo(xmlTable, title, adu))
			{
				if (SuspendTrigger::DidSuspend())
					return false;
				else
					break;
			}
			prevUnit = adu;
		}
		di = AsDataItem(di->GetReferredItem());
		title = "Derived DomainUnit";
	} while (di);
	return true;
}

TIC_CALL bool DMS_CONV DMS_TreeItem_XML_DumpGeneral(const TreeItem* self, OutStreamBase* xmlOutStrPtr, bool showAll)
{
	DMS_CALL_BEGIN

		dms_assert(self->GetInterestCount() || self->WasFailed(FR_Data));
		dms_assert(xmlOutStrPtr);

		SuspendTrigger::Resume();

		XML_ItemBody xmlItemBody(*xmlOutStrPtr, self);
		try {
			if (!TreeItem_XML_DumpGeneralBody(self, xmlOutStrPtr, showAll))
				return false;
		} catch (...)
		{
			auto err = catchException(true);
			if (!err)
				*xmlOutStrPtr << "unrecognized error";
			else
				*xmlOutStrPtr << *err;
		}

	DMS_CALL_END_NOTHROW
	return true;
}

void WritePropValueRows(XML_Table& xmlTable, const TreeItem* self, const Class* cls, bool showAll)
{
	{
		XML_Table::Row exprRow(xmlTable);
			exprRow.OutStream().WriteAttr("bgcolor", CLR_HROW);
			exprRow.ValueCell("CLASS");
			exprRow.ValueCell(cls->GetName().c_str());
	}
	for (const AbstrPropDef* pd = cls->GetLastPropDef(); pd; pd = pd->GetPrevPropDef())
	{
		SharedStr result;
		bool firstValue = true;
		bool canBeIndirect = pd->CanBeIndirect();
		try {
			if (!showAll && !pd->HasNonDefaultValue(self))
				continue;
			result = pd->GetValueAsSharedStr(self);
		}
		catch (...)
		{
			auto err = catchException(true);
			if (err)
				xmlTable.NameErrRow(pd->GetName().c_str(), *err);
			canBeIndirect = false;
		}
		while (true)
		{
			if (result.empty() && !showAll)
				break;

			if (firstValue)
				xmlTable.EditableNameValueRow(pd->GetName().c_str(), result.c_str());
			else
				xmlTable.NameValueRow("which evaluates to:", result.c_str());

			if (!canBeIndirect)
				break;

			if (!AbstrCalculator::MustEvaluate(result.c_str()))
				break;

			if (self->InTemplate())
			{
				xmlTable.NameValueRow("", "which isn't evaluated here since this item is part of a template definition");
				break;
			}
			else
			{
				try {
					result = AbstrCalculator::EvaluatePossibleStringExpr(self, result, CalcRole::Other);
					firstValue = false;
				}
				catch (...)
				{
					auto err = catchException(true);
					if (err)
						xmlTable.NameErrRow("EvaluationErr", *err);
					break;
				}
			}
		}
	}
}

TIC_CALL bool DMS_CONV DMS_TreeItem_XML_DumpAllProps(const TreeItem* self, OutStreamBase* xmlOutStrPtr, bool showAll)
{
	DMS_CALL_BEGIN

		dms_assert(xmlOutStrPtr);

		XML_ItemBody xmlItemBody(*xmlOutStrPtr, self);
		XML_Table    xmlTable   (*xmlOutStrPtr);

		const Class* cls = self->GetDynamicClass();
		while (cls)
		{
			WritePropValueRows(xmlTable, self, cls, showAll);
			cls = cls->GetBaseClass();
		}

	DMS_CALL_END
	return true;
}


void ExploreCell(const TreeItem* subItem, XML_Table::Row& xmlRow, bool showFullPath = false)
{
	xmlRow.ClickableCell(
		showFullPath ? subItem->GetFullName().c_str() : subItem->GetName().c_str(),
		mySSPrintF("dms:dp.explore:%s", subItem->GetFullName().c_str()).c_str()
	);
}

void TreeItem_XML_DumpItem(const TreeItem* subItem, XML_Table& xmlTable, bool viewHidden)
{
	dms_assert(subItem);
	dms_assert(subItem->GetTreeParent());
	auto parent = subItem->GetTreeParent();
	if (viewHidden || !subItem->GetTSF(TSF_InHidden))
	{
		XML_Table::Row xmlRow(xmlTable);
			ExploreCell(subItem, xmlRow);
			if (parent->GetTreeParent())
				xmlRow.ValueCell( mySSPrintF("in %s", parent->GetFullName()).c_str());
			try {
				if (IsDataItem(subItem))
				{
					auto adi = AsDataItem(subItem);
					if (adi->HasVoidDomainGuarantee())
						xmlRow.ValueCell(
							mySSPrintF("param: %s %s"
								, adi->GetAbstrValuesUnit()->GetDisplayName()
								, adi->GetAbstrValuesUnit()->GetFormattedMetricStr()
							).c_str()
						);
					else
						xmlRow.ValueCell( 
							mySSPrintF("attr: %s -> %s %s"
								, adi->GetAbstrDomainUnit()->GetDisplayName()
								, adi->GetAbstrValuesUnit()->GetDisplayName()
								, adi->GetAbstrValuesUnit()->GetFormattedMetricStr()
							).c_str() 
						);
				}
				if (IsUnit(subItem))
				{
					auto au = AsUnit(subItem);
					xmlRow.ValueCell(
						mySSPrintF("unit<%s> %s"
							, au->GetValueType()->GetName()
							, au->GetFormattedMetricStr()
						).c_str()
					);
				}
			}
			catch (...) {}
	}
}

void TreeItem_XML_DumpExploreThisAndParents(const TreeItem* self, OutStreamBase* xmlOutStrPtr, bool viewHidden, TreeItemSetType& doneItems, const TreeItem* calledBy, CharPtr callingRole)
{
	dms_assert(self);
	XML_ItemBody xmlItemBody(*xmlOutStrPtr, self, true);
	dms_assert(xmlOutStrPtr);
	{
		XML_Table    xmlTable   (*xmlOutStrPtr);
		if (calledBy)
		{
			XML_Table::Row xmlRow(xmlTable);
				xmlRow.ValueCell( callingRole );
				ExploreCell(calledBy, xmlRow, true);
		}

		TreeItemSetType::iterator itemPtr = doneItems.lower_bound(self);
		if (itemPtr != doneItems.end() && *itemPtr == self)
		{
			dms_assert(calledBy);
			goto omit_repetition;
		}
		doneItems.insert(itemPtr, self);

		for (const TreeItem* subItem = self->GetFirstVisibleSubItem(); subItem; subItem = subItem->GetNextVisibleItem())
			TreeItem_XML_DumpItem(subItem, xmlTable, viewHidden);
	}
	if (self->CurrHasUsingCache())
	{
		auto uc = self->GetUsingCache();
		UInt32 i=uc->GetNrUsings();
		if(!i)
		{
			dms_assert(!self->GetTreeParent());
			return;
		}
		while (i)
		{
			auto us = uc->GetUsing(--i);
			if (!us)
				continue;
			CharPtr role = "is used by";
			if (us == self->GetTreeParent())
			{
				role = "is parent of";
				if (i)
					role = "is parent and used by";
			}
			TreeItem_XML_DumpExploreThisAndParents(us, xmlOutStrPtr, viewHidden, doneItems, self, role);
		}
	}
	else
	{
		const TreeItem* parent = self->GetTreeParent();
		if (parent)
			TreeItem_XML_DumpExploreThisAndParents(parent, xmlOutStrPtr, viewHidden, doneItems, self, "is parent of");
	}
	return;

omit_repetition:
	// here we are outside the scope of xmlTable
	(*xmlOutStrPtr) << "(repetition of sub items omitted)";
}

TIC_CALL void DMS_CONV DMS_TreeItem_XML_DumpExplore(const TreeItem* self, OutStreamBase* xmlOutStrPtr, bool viewHidden)
{
	DMS_CALL_BEGIN

		* xmlOutStrPtr << "items visible from here per namespace in search order";

		TreeItemSetType doneItems;
		TreeItem_XML_DumpExploreThisAndParents(self, xmlOutStrPtr, viewHidden, doneItems, nullptr, "");

	DMS_CALL_END
}


// *****************************************************************************
//											DMS_TreeItem_Dump
// *****************************************************************************

#include "utl/swapper.h"

void ReadCommentedHeader(FormattedInpStream& inp, FormattedOutStream& out)
{
	enum { sm_in_header, sm_slash_read, sm_in_linecomment, sm_in_blockcomment, sm_star_read } 
		state = sm_in_header;
	char ch;
	while (true)
	{
		inp >> ch;
		switch (ch)
		{
			case '/': 
				switch (state)
				{
					case sm_in_header:      state = sm_slash_read;                  break; // keep until we know it's part of the header
					case sm_slash_read:     state = sm_in_linecomment; out << "//"; break;

					case sm_star_read:      state = sm_in_header; 
						[[fallthrough]];
					default: out << ch;
				} 
				break;
			case '\n': // EOL
				switch (state)
				{
					case sm_slash_read:     out << '/'; return;

					case sm_star_read:      
					case sm_in_linecomment: state = sm_in_header;
						[[fallthrough]];
					default: out << ch;
				} 
				break;
			case '\t':
			case ' ': // isspace
				switch (state)
				{
					case sm_slash_read:   out << '/'; return;

					case sm_star_read:    state = sm_in_blockcomment; // back to previous state
						[[fallthrough]];
					default:              out << ch;
				}
				break;
			case '*':
				switch (state)
				{
					case sm_in_header:       return;
					case sm_slash_read:      state = sm_in_blockcomment; out << "/*"; break;

					case sm_in_blockcomment: state = sm_star_read;
						[[fallthrough]];
					default:                 out << ch;
				} 
				break;

			case '\0':
			case '\26': // EOF
				return;

			default: // any other character
				switch (state) 
				{
					case sm_slash_read:   out << '/'; 
						[[fallthrough]];
					case sm_in_header:
						return;

					default:              out << ch; break;
				}				
				break;
		}
	}
}

OutStreamBase::SyntaxType GetSyntaxTypeFromExt(CharPtr fileExt)
{
	if (!stricmp(fileExt, "dms" )) return OutStreamBase::ST_DMS;
	if (!stricmp(fileExt, "xml" )) return OutStreamBase::ST_XML;
	if (!stricmp(fileExt, "html")) return OutStreamBase::ST_HTM;

	return OutStreamBase::ST_Unknown;
}

static SharedStr s_gDumpFolder;

void ItemSave(const TreeItem* self, CharPtr fileName, SafeFileWriterArray* sfwa, bool copyDir)
{
	CharPtr fileExt = getFileNameExtension(fileName);
	OutStreamBase::SyntaxType syntax = GetSyntaxTypeFromExt(fileExt);

	if (syntax == OutStreamBase::ST_Unknown)
		throwErrorF("XML", "ItemSave(%s,%s): cannot write configuration data to the unknown format '%s'",
				self->GetName(), fileName, fileExt);

	SharedStr fileNameStr    = DelimitedConcat(s_gDumpFolder.c_str(), fileName);
	SharedStr fileDir        = DelimitedConcat(s_gDumpFolder.c_str(), getFileNameBase(fileName).c_str());
	SharedStr oldFileNameStr = AbstrStorageManager::GetFullStorageName(self, SharedStr()) + '.' + fileExt;

	if (copyDir)
	{
		SharedStr orgDir = getFileNameBase(oldFileNameStr.c_str()); 
		if ( IsFileOrDirAccessible(orgDir) )
		{
			SharedStr dstDir = getFileNameBase(   fileNameStr.c_str()); 
			CopyFileOrDir(orgDir.c_str(), dstDir.c_str(), false);
		}
	}

	tmp_swapper<SharedStr> fileDirHolder(s_gDumpFolder, fileDir);

	// Get Last header from existing file
	SharedStr commentedHeader;
	if (syntax == OutStreamBase::ST_DMS)
	{
		if (IsFileOrDirAccessible(oldFileNameStr))
		{
			FileInpStreamBuff fileInp(oldFileNameStr, sfwa, true);
			VectorOutStreamBuff vectOut;
			FormattedInpStream fin(&fileInp);
			FormattedOutStream fout(&vectOut, FormattingFlags::None);
			ReadCommentedHeader(fin, fout);
			commentedHeader = SharedStr(vectOut.GetData(), vectOut.GetDataEnd());
		}
		else
			commentedHeader = mySSPrintF("// DMS ConfigDataDump version %s\n", DMS_GetVersion());
	}

	FileOutStreamBuff fileOut(fileNameStr, sfwa, true);
	FormattedOutStream fout(&fileOut, FormattingFlags::None);
	fout << commentedHeader;

	OwningPtr<OutStreamBase> xmlOutStr = XML_OutStream_Create(&fileOut, syntax, "DMS", exprPropDefPtr);
	DMS_TreeItem_XML_Dump(self, xmlOutStr);
}

void IncludeFileSave(const TreeItem* self, CharPtr fileName, SafeFileWriterArray* sfwa)
{
	dms_assert(self->GetTreeParent());

	TreeItemContextHandle checkPtr(self, TreeItem::GetStaticClass(), "IncludedSave");

	ItemSave(self, fileName, sfwa, false);
}

TIC_CALL bool DMS_CONV DMS_TreeItem_Dump(const TreeItem* self, CharPtr fileName)
{
	DMS_CALL_BEGIN

		dms_assert(self);
		SharedStr fileNameStr(fileName);
		fileNameStr = ConvertDosFileName(fileNameStr);

		TreeItemContextHandle checkPtr(self, TreeItem::GetStaticClass(), "ConfigSave");

		bool isRoot = !(self->GetTreeParent());
		if (!isRoot)
			fileNameStr = DelimitedConcat(GetCaseDir(self->GetTreeParent()).c_str(), fileNameStr.c_str());
		else
		{
			dms_assert(s_gDumpFolder.empty() );
			fileNameStr = MakeAbsolutePath( fileNameStr.c_str() );
		}
		ItemSave(self, fileNameStr.c_str(), DSM::GetSafeFileWriterArray(self), isRoot);
		return true;

	DMS_CALL_END
	return false;
}
