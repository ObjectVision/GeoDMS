// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "xml/XmlTreeOut.h"

#include "RtcInterface.h"
#include "act/TriggerOperator.h"
#include "dbg/DmsCatch.h"
#include "mci/ValueClass.h"
#include "ser/AsString.h"
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
	assert(unit->GetValueType()->IsCountable());
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
	assert(unit);
	assert(!SuspendTrigger::DidSuspend()); // PRECONDITION
	assert(IsMainThread());

	xmlTable.NameValueRow("ValueType", unit->GetValueType()->GetName().c_str());

	if (unit->InTemplate())
		return false;
	if (unit->GetUnitClass() == Unit<Void>::GetStaticClass())
		return true;

	if (unit->GetTSF(TSF_Categorical))
		xmlTable.NameValueRow("Categorical", "Yes");

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
				xmlTable.NameErrRow(METRIC_NAME, *err, unit);
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
		auto nrTiles = trd->GetNrTiles();
		xmlTable.NameValueRow("Nr Tiles", AsString(nrTiles).c_str());
		if (nrTiles)
			xmlTable.NameValueRow("Max Tile size", AsString(trd->GetMaxTileSize()).c_str());
	}
	else if (allTileInfo)
	{
		XML_Table::Row row(xmlTable);
		row.ValueCell("Tile Sizes");
		XML_Table::Row::Cell cell(row);

		XML_OutElement details(xmlTable.OutStream(), "details");
		{
			XML_OutElement summary(xmlTable.OutStream(), "summary");
			xmlTable.OutStream() << "DataSize " << AsString(trd->GetDataSize()).c_str();
		}
		for (tile_id t = 0, tn = trd->GetNrTiles(); t != tn; ++t)
		{
			NewLine(xmlTable.OutStream());
			xmlTable.OutStream() 
				<< mySSPrintF("Tile %d", t).c_str()
				<< GetTileStrRange(currRangeUnit.get_ptr(), t).c_str()
				<< " = "
				<< GetTileStrCount(currRangeUnit.get_ptr(), t).c_str()
				<< " elements.";
		}
	}
	return true;
}

bool WriteUnitInfo(XML_Table& xmlTable, CharPtr role, const AbstrUnit* unit)
{
	assert(unit);

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

XML_ItemBody::XML_ItemBody(OutStreamBase& out, CharPtr caption, CharPtr subText, const TreeItem* item, bool showFullName)
	: XML_OutElement(out, "BODY")
{
	out.WriteAttr("bgcolor", CLR_BODY);

	{
		XML_OutElement page_title_table(out, "TABLE");
		out.WriteAttr("width", "100%");
		XML_OutElement table_row(out, "TR");
		out.WriteAttr("bgcolor", "#89CFF0");
		XML_OutElement table_col(out, "TD");
		{
			XML_OutElement font_size(out, "font");
			out.WriteAttr("size", "4");
			out << caption;
		}
	}

	{
	XML_OutElement page_title_table(out, "I");
	if (subText && *subText)
		out << subText;
    }

	/*XML_Table current_item_table(out);
	XML_OutElement table_row0(out, "TR");
	XML_Table::Row row = XML_Table::Row(current_item_table);
	auto cell = XML_Table::Row::Cell(row);
	row.ValueCell("Active Item");
	auto cell2 = XML_Table::Row::Cell(row);
	row.ItemCell(item, true);
	XML_OutElement table_row2(out, "TR");*/
	NewLine(out);
	NewLine(out);
	{
		XML_OutElement bold(out, "B");
		{
			XML_hRef supplRef(out, ItemUrl(item->GetFullName().c_str()));
			out << item->GetFullName().c_str();
		}
	}
	NewLine(out);

}

// ********** XML_Table                                             *********

void XML_Table::NameErrRow(CharPtr propName, const ErrMsg& err, const TreeItem* self)
{
	{
		Row row(*this);
		row.ValueCell(propName);
		auto cell = Row::Cell(row);
		OutStream() << err;
	}
	auto errSrc = TreeItem_GetErrorSource(self, false);
	if (errSrc.first && errSrc.first != self)
	{
		Row row(*this);
		{
			Row::Cell xmlElemTD(row);
			OutStream().WriteTrimmed("Also failed");
			NewLine(OutStream());
			OutStream().WriteTrimmed("(F2 target)");
		}
//		row.ValueCell("Also Failed (F2 target)");
		row.ItemCell(errSrc.first);
	}
}

// ********** XML_Table::Row                                        *********

void XML_Table::Row::ClickableCell(CharPtr value, CharPtr hRef, bool bold /* = false */)
{
	Cell xmlElemTD(*this);
	if (bold)
	{
		auto boldElement = XML_OutElement(OutStream(), "B");
		hRefWithText(OutStream(), value, hRef);
	}
	else
		hRefWithText(OutStream(), value, hRef);
}

void XML_Table::Row::EditablePropCell(CharPtr propName, CharPtr propLabel /*= ""*/, const TreeItem* item /* = nullptr */)
{
	assert(propName);
	assert(propLabel);

	if (!*propLabel)
		propLabel = propName;
	if (item && item->IsCacheItem())
		ValueCell(propLabel);
	else
	{
		/*SharedStr editUrl = (item)
			?	mySSPrintF("dms:edit!%s:%s", propName, item->GetFullName().c_str())
			:	mySSPrintF("dms:edit!%s", propName);*/

		//ClickableCell(propLabel, editUrl.c_str());

		ValueCell(propLabel);
	}
}

// *****************************************************************************
//											COMPONENT HELPER FUNCS
// *****************************************************************************
void TabIndentation(OutStreamBase& out, UInt8 number_of_tabs)
{
	for (int i = 0; i < number_of_tabs; i++)
	{
		out << "    ";
	}
}

void NewLine(OutStreamBase& out)
{
	XML_OutElement br(out, "BR", "", ClosePolicy::nonPairedElement);
}

void WriteLispRefExpr(OutStreamBase& stream, LispPtr lispExpr)
{
	stream << AsFLispSharedStr(lispExpr, FormattingFlags::ThousandSeparator).c_str();
}

TIC_CALL void(*s_AnnotateExprFunc)(OutStreamBase& outStream, const TreeItem* searchContext, SharedStr expr);

const TreeItem* WriteExprOrSourceDescrAndReturnSourceItem(OutStreamBase& stream, const TreeItem* ti)
{
	SharedStr exprStr = ti->GetExpr();
	if (!exprStr.empty())
	{
		assert(s_AnnotateExprFunc);
		s_AnnotateExprFunc(stream, AbstrCalculator::GetSearchContext(ti, CalcRole::Calculator), exprStr);
		if (AbstrCalculator::MustEvaluate(exprStr.begin()))
		{
			NewLine(stream);
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
		if (storageParent && (IsUnit(ti) || IsDataItem(ti)))
		{
			const AbstrStorageManager* sm = storageParent->GetStorageManager();
			assert(sm); // because of POSTCONDITION of GetStorageParant
			stream << "Read from " << sm->GetNameStr().c_str();
		}
		else
			stream << "<none>";
		return nullptr;
	}
	const TreeItem* si = ti->GetSourceItem();
	if (si)
	{
		assert(si != ti);
		assert(!si->IsCacheItem());
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
	{
		if (calc->IsDataBlock())
			stream << "[ ... ]";
		else
			stream << calc->GetAsFLispExprOrg(FormattingFlags::ThousandSeparator).c_str();
	}
	return nullptr;
}

void WriteExprOrSourceDescr(OutStreamBase& stream, const TreeItem* self)
{
	XML_OutElement details(stream, "details");

	auto si = self;
	{
		XML_OutElement summary(stream, "summary");

		si = WriteExprOrSourceDescrAndReturnSourceItem(stream, self);
	}

	while (si)
	{
		assert(si != self);
		assert(!si->IsCacheItem());
		NewLine(stream);
		stream << "which refers to";
		{
			XML_hRef sourceRef(stream, ItemUrl(si).c_str());
			stream << si->GetFullName().c_str();
		}
		self = si;
		si = si->GetSourceItem();
	}

	if (self->HasCalculator() && !self->WasFailed(FR_MetaInfo))
	{
		auto c = self->GetCalculator();
		if (c)
		{
			NewLine(stream);
			stream << "ParseResult: " << AsFLispSharedStr(c->GetLispExprOrg(), FormattingFlags::ThousandSeparator).c_str();
		}
	}
	if (!self->WasFailed(FR_MetaInfo))
	{
		auto metaInfo = self->GetCurrMetaInfo({});
		auto calcExpr = GetAsLispRef(metaInfo);

		NewLine(stream);
		stream << "CalcExpr: " << AsFLispSharedStr(calcExpr, FormattingFlags::ThousandSeparator).c_str();

		if (metaInfo.index() == 1 || metaInfo.index() == 0 && std::get<MetaFuncCurry>(metaInfo).fullLispExpr.EndP())
		{
			auto keyExpr = self->GetCheckedKeyExpr();
			if (keyExpr != calcExpr)
			{
				NewLine(stream);
				stream << "CheckedKeyExpr: " << AsFLispSharedStr(keyExpr, FormattingFlags::ThousandSeparator).c_str();
			}
		}
	}
}

void WriteIntegrityCheck(OutStreamBase& stream, AbstrCalculatorRef icCalc)
{
	icCalc->WriteHtmlExpr(stream);
}

void WriteExprOrSourceDescrRow(XML_Table& xmlTable, const TreeItem* ti)
{
	assert(ti);
	{
		XML_Table::Row exprRow(xmlTable);
		exprRow.OutStream().WriteAttr("bgcolor", CLR_HROW);
		XML_Table::Row::Cell xmlExprElemTD(exprRow);
		exprRow.OutStream().WriteTrimmed("CalculationRule");

		//exprRow.ClickableCell("CalculationRule", "dms:Edit Definition");
		XML_Table::Row::Cell xmlElemTD1(exprRow);
		WriteExprOrSourceDescr(xmlTable.OutStream(), ti);
	}
	if (ti->HasIntegrityChecker())
	{
		auto icCalc = ti->GetIntegrityChecker();

		XML_Table::Row icRow(xmlTable);
		icRow.OutStream().WriteAttr("bgcolor", CLR_HROW);
		XML_Table::Row::Cell xmlicElemTD(icRow);
		icRow.OutStream().WriteTrimmed("IntegrityCheck");

		//exprRow.ClickableCell("CalculationRule", "dms:Edit Definition");
		XML_Table::Row::Cell xmlElemTD1(icRow);
		WriteIntegrityCheck(xmlTable.OutStream(), icCalc);
	}
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

		assert(xmlOutStr);
		try {
			self->XML_Dump(xmlOutStr);
		}
		catch (...)
		{
			*xmlOutStr << catchException(false)->Why().c_str();
		}

	DMS_CALL_END
}

bool TreeItem_XML_DumpGeneralBody(const TreeItem* self, OutStreamBase* xmlOutStrPtr)
{
	XML_Table xmlTable(*xmlOutStrPtr);
	//xmlTable.EditableNameValueRow("FullName", self->GetFullName().c_str());

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
			xmlTable.NameErrRow("FailReason", *fr, self);
		}
	}
	if (self->InTemplate())
	{
		xmlTable.NamedItemRow("PartOfTemplate", GetTemplRoot(self));
	}
	SharedStr result = self->GetDescr();
	if (!result.empty()) {

		xmlTable.SingleCellRow(result.c_str(), "#ACE1AF");
		xmlTable.EmptyRow();
	}
		

	result = TreeItemPropertyValue(self, labelPropDefPtr); 
	if (!result.empty())
		xmlTable.EditableNameValueRow(LABEL_NAME, result.c_str());

	ValueComposition vc = ValueComposition::Unknown;
	if (IsDataItem(self))
	{
		const AbstrDataItem* di = AsDataItem(self);

		if (auto avu = di->GetAbstrValuesUnit())
			if (auto vt = avu->GetValueType())
				xmlTable.NameValueRow("ValuesType", vt->GetName().c_str());
		vc = di->GetValueComposition();
		if (vc != ValueComposition::Single)
			xmlTable.NameValueRow("ValueComposition", GetValueCompositionID(vc).GetStr().c_str());
		if (di->GetTSF(TSF_Categorical))
			xmlTable.NameValueRow("Categorical", "Yes");

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
	WriteExprOrSourceDescrRow(xmlTable, self);

	// ==================== Explicit Suppliers
	if (self->HasSupplCache())
	{
		XML_Table::Row exprRow(xmlTable);
		exprRow.OutStream().WriteAttr("bgcolor", CLR_HROW);
		exprRow.ValueCell("ExplicitSuppliers");
		XML_Table::Row::Cell xmlElemTD(exprRow);

		auto& out  = xmlTable.OutStream();
		XML_OutElement details(out, "details");
		{
			XML_OutElement summary(out, "summary");
			out << explicitSupplPropDefPtr->GetValueAsSharedStr(self).c_str();
		}

		try {
			auto n = self->GetSupplCache()->GetNrConfigured(self); // only ConfigSuppliers, Implied suppliers come after this, Calculator & StorageManager have added them
			for (decltype(n) i = 0; i < n; ++i)
			{
				const Actor* supplier = self->GetSupplCache()->begin(self)[i];
				auto supplTI = debug_cast<const TreeItem*>(supplier);
				if (supplTI)
				{
					NewLine(out);
					out << AsString(i).c_str();
					hRefWithText(out, supplTI->GetFullName().c_str(), ItemUrl(supplTI).c_str());
				}
			}
		}
		catch (...)
		{
			auto err = catchException(true);
			if (err)
				xmlTable.NameErrRow("ExplicitSuppliers", *err, self);
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

			xmlTable.NameValueRow("AccessType", readOnly ? "ReadOnly" : "ReadWrite");

			result = TreeItemPropertyValue(self, sqlStringPropDefPtr);
			if (!result.empty())
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

TIC_CALL bool TreeItem_XML_DumpGeneral(const TreeItem* self, OutStreamBase* xmlOutStrPtr)
{
	assert(xmlOutStrPtr);
	SuspendTrigger::Resume();
	XML_ItemBody xmlItemBody(*xmlOutStrPtr, "Generic properties", "Item definition and determination of primary data", self);

	try {
		if (!TreeItem_XML_DumpGeneralBody(self, xmlOutStrPtr))
			return false;
	} catch (...)
	{
		auto err = catchException(true);
		if (!err)
			*xmlOutStrPtr << "unrecognized error";
		else
			*xmlOutStrPtr << *err;
	}

	return true;
}
#include "TreeItemProps.h"
TIC_CALL void TreeItem_XML_DumpSourceDescription(const TreeItem* self, SourceDescrMode mode, OutStreamBase* xmlOutStrPtr)
{
	assert(xmlOutStrPtr);
	SuspendTrigger::Resume();
	SharedStr source_description_subtitle = {};
	switch (mode) {
	case SourceDescrMode::Configured:  source_description_subtitle = "Configured Source Descriptions\n"; break;
	case SourceDescrMode::ReadOnly:    source_description_subtitle = "Read Only Storage Managers\n"; break;
	case SourceDescrMode::WriteOnly:   source_description_subtitle = "Non-Read Only Storage Managers\n"; break;
	case SourceDescrMode::All:         source_description_subtitle = "Utilized Storage Managers\n"; break;
	case SourceDescrMode::DatasetInfo: source_description_subtitle = "Dataset metainfo and properties\n"; break;
	}

	XML_ItemBody xmlItemBody(*xmlOutStrPtr, "Source Description", source_description_subtitle.c_str(), self);
	TreeItem_DumpSourceCalculator(self, mode, true, xmlOutStrPtr);
}

TIC_CALL void TreeItem_XML_ConvertAndDumpDatasetProperties(const TreeItem* self, const prop_tables& dataset_properties, OutStreamBase* xmlOutStrPtr)
{
	XML_ItemBody xmlItemBody(*xmlOutStrPtr, "Source Description", "Dataset metainfo and properties", self);
	for (auto& property : dataset_properties)
	{
		auto level = property.first;
		auto name = property.second.first;
		auto value = property.second.second;
		{
			XML_OutElement br(*xmlOutStrPtr, "P", "", ClosePolicy::pairedButWithoutSeparator);
			{
				auto indentation_level_str = SharedStr("margin-left: " + AsString(level * 15) + "px");
				xmlOutStrPtr->WriteAttr("style", indentation_level_str.c_str());
				xmlOutStrPtr->WriteValue(""); // Close attr list
				xmlOutStrPtr->FormattingStream() << name.GetStr().c_str() << " : " << value.c_str();
			}
		}
	}
}

TIC_CALL bool XML_MetaInfoRef(const TreeItem* self, OutStreamBase* xmlOutStrPtr)
{
	assert(xmlOutStrPtr);
	assert(self);
	SuspendTrigger::Resume();

	XML_ItemBody xmlItemBody(*xmlOutStrPtr, "Meta information reference", "a link to description or documentation", self);
	try {
		XML_Table table(*xmlOutStrPtr);
		for (auto cursor=self; cursor; cursor = cursor->GetTreeParent())
		{
			auto url = TreeItemPropertyValue(cursor, urlPropDefPtr);
			if (!url.empty())
			{
				XML_Table::Row row(table);
				row.ItemCell(cursor);
				auto context = cursor;
				if (url[0] == '#')
				{
					context = self;
					url = SharedStr(url.begin() + 1, url.send());
				}

				auto expandedUrl = AbstrStorageManager::GetFullStorageName(context, url);
				row.ClickableCell(expandedUrl.c_str(), expandedUrl.c_str());
			}
		}
	}
	catch (...)
	{
		auto err = catchException(true);
		if (!err)
			*xmlOutStrPtr << "unrecognized error";
		else
			*xmlOutStrPtr << *err;
	}

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
			if (pd->IsDepreciated())
				continue;
			if (!showAll && !pd->HasNonDefaultValue(self))
				continue;
			if (SuspendTrigger::DidSuspend())
				return;
			result = pd->GetValueAsSharedStr(self);
			if (SuspendTrigger::DidSuspend())
				return;
		}
		catch (...)
		{
			auto err = catchException(true);
			if (err)
				xmlTable.NameErrRow(pd->GetName().c_str(), *err, self);
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
						xmlTable.NameErrRow("EvaluationErr", *err, self);
					break;
				}
			}
		}
	}
}

TIC_CALL bool DMS_CONV DMS_TreeItem_XML_DumpAllProps(const TreeItem* self, OutStreamBase* xmlOutStrPtr, bool showAll)
{
	DMS_CALL_BEGIN

		assert(xmlOutStrPtr);
		assert(!SuspendTrigger::DidSuspend());

		CharPtr caption = showAll
			? "All properties"
			: "Properties with non-default values";

		XML_ItemBody xmlItemBody(*xmlOutStrPtr, caption, "ordered by specificity and then property-name", self);
		XML_Table    xmlTable   (*xmlOutStrPtr);

		const Class* cls = self->GetDynamicClass();
		while (cls)
		{
			WritePropValueRows(xmlTable, self, cls, showAll);
			if (SuspendTrigger::DidSuspend())
				return false;

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

void TreeItem_XML_DumpExploreThisAndParents_impl(const TreeItem* self, OutStreamBase* xmlOutStrPtr
	, bool viewHidden, TreeItemSetType& doneItems, const TreeItem* calledBy, CharPtr callingRole)
{
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
			TreeItem_XML_DumpExploreThisAndParents_impl(us, xmlOutStrPtr, viewHidden, doneItems, self, role);
		}
	}
	else
	{
		const TreeItem* parent = self->GetTreeParent();
		if (parent)
			TreeItem_XML_DumpExploreThisAndParents_impl(parent, xmlOutStrPtr, viewHidden, doneItems, self, "is parent of");
	}
	return;

omit_repetition:
	// here we are outside the scope of xmlTable
	(*xmlOutStrPtr) << "(repetition of sub items omitted)";
}

void TreeItem_XML_DumpExploreThisAndParents(const TreeItem* self, OutStreamBase* xmlOutStrPtr, bool viewHidden, const TreeItem* calledBy, CharPtr callingRole)
{
	assert(self);
	assert(self);

	TreeItemSetType doneItems;
	XML_ItemBody xmlItemBody(*xmlOutStrPtr, "Explore accessible namespaces", "in search order.", self, true);

	TreeItem_XML_DumpExploreThisAndParents_impl(self, xmlOutStrPtr, viewHidden, doneItems, calledBy, callingRole);
}

TIC_CALL void DMS_CONV DMS_TreeItem_XML_DumpExplore(const TreeItem* self, OutStreamBase* xmlOutStrPtr, bool viewHidden)
{
	DMS_CALL_BEGIN

		TreeItem_XML_DumpExploreThisAndParents(self, xmlOutStrPtr, viewHidden, nullptr, "");

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

	OwningPtr<OutStreamBase> xmlOutStr = XML_OutStream_Create(&fileOut, syntax, "DMS", calcRulePropDefPtr);
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
		auto sfwa = DSM::GetSafeFileWriterArray();
		if (!sfwa)
			return false;
		ItemSave(self, fileNameStr.c_str(), sfwa.get(), isRoot);
		return true;

	DMS_CALL_END
	return false;
}
