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

#include "ShvDllPch.h"


#include "DataItemColumn.h"

#include "StgBase.h"

#include "act/ActorVisitor.h"
#include "dbg/Debug.h"
#include "mci/AbstrValue.h"
#include "mci/ValueClass.h"
#include "mci/ValueClassID.h"
#include "ptr/SharedStr.h"
#include "ser/AsString.h"
#include "ser/PointStream.h"
#include "utl/mySPrintF.h"
#include "act/MainThread.h"

#include "AbstrDataItem.h"
#include "AbstrUnit.h"
#include "DataArray.h"
#include "DataItemClass.h"
#include "DataController.h"
#include "DisplayValue.h"
#include "TreeItemProps.h"
#include "Unit.h"
#include "UnitClass.h"

#include "RtcTypeLists.h"
#include "UnitCreators.h"
#include "UnitProcessor.h"

#include "Clipboard.h"
#include "DataController.h"
#include "DataView.h"
#include "DcHandle.h"
#include "FontIndexCache.h"
#include "IdleTimer.h"
#include "KeyFlags.h"
#include "MouseEventDispatcher.h"
#include "ScrollPort.h"
#include "TableControl.h"
#include "Theme.h"
#include "ThemeReadLocks.h"
#include "ThemeValueGetter.h"

//----------------------------------------------------------------------
// class  : DataItemColumn
//----------------------------------------------------------------------

UInt32 GetDefaultColumnWidth(const AbstrDataItem* adi)
{
//	if (adi && adi->GetAbstrValuesUnit()->GetValueType()->IsNumeric())
//	{
//		dms_assert(adi->GetAbstrValuesUnit()->GetValueType()->GetValueComposition() == ValueComposition::Single);
//		return DEF_TEXT_PIX_WIDTH / 2;
//	}
	return DEF_TEXT_PIX_WIDTH / 2;
}

DataItemColumn::DataItemColumn(
	TableControl* owner,
	const AbstrDataItem* adi,
	AspectNrSet possibleAspects, 
	AspectNr    activeTheme
)	:	MovableObject(owner)
	,	ThemeSet(possibleAspects, activeTheme)
	,	m_FutureSrcAttr(adi)
	,	m_ElemSize(GetDefaultColumnWidth(adi), DEF_TEXT_PIX_HEIGHT)
	,	m_ColumnNr(UNDEFINED_VALUE(UInt32))	
	,	m_ActiveRow(0)
{
	dms_assert(!GetActiveTheme());
}

DataItemColumn::DataItemColumn(const DataItemColumn& src)
	:	MovableObject(src)
	,	ThemeSet(src)
	,	m_ElemSize(src.m_ElemSize)
	,	m_ColumnNr(UNDEFINED_VALUE(UInt32))
	,	m_ActiveRow(src.m_ActiveRow)
{
	if (src.HasElemBorder())
		SetElemBorder(true);
}

DataItemColumn::~DataItemColumn()
{}

static TokenID aggrID = GetTokenID_st("Aggr");

bool Allowed(const AbstrDataItem* adi, AggrMethod am)
{
	const AbstrUnit* avu = adi->GetAbstrValuesUnit();
	ValueComposition vcm = adi->GetValueComposition();
	const ValueClass* vc = avu->GetValueType();
	switch (am) {
		case AggrMethod::first:
		case AggrMethod::last:
			return true;

		case AggrMethod::count:
			return vc->GetValueClassID() == VT_UInt32 && vcm == ValueComposition::Single; // we dont want to have to change the ValuesUnit type

		case AggrMethod::union_polygon:
			return vc->GetNrDims() == 2 && vcm == ValueComposition::Polygon && vc->IsIntegral();

		case AggrMethod::modus:
			if (!vc->IsIntegral())
				return false;
			[[fallthrough]];
		case AggrMethod::sum:
		case AggrMethod::mean:
		case AggrMethod::sd:
			if (!vc->IsNumeric())
				return false;
			[[fallthrough]];
		case AggrMethod::min:
		case AggrMethod::max:
		case AggrMethod::asItemList:
			return vcm == ValueComposition::Single;
	}
	return false;
}

ConstUnitRef ValuesUnit(const AbstrDataItem* adi, AggrMethod am)
{
	dms_assert(Allowed(adi, am));

	const AbstrUnit* avu = adi->GetAbstrValuesUnit();
	const ValueClass* vc = avu->GetValueType();
	switch (am) {
		case AggrMethod::count:
			return count_unit_creator(adi);

		case AggrMethod::asItemList:
			return (vc->GetValueClassID() == VT_String) ? avu : Unit<SharedStr>::GetStaticClass()->CreateDefault();

		default:
			return avu;
	}
	return nullptr;
}

CharPtr OperName(const AbstrDataItem* adi, AggrMethod am)
{
	dms_assert(Allowed(adi, am));

	switch(am) {
		case AggrMethod::sum: return "sum";
		case AggrMethod::count: return "count";
		case AggrMethod::union_polygon: return "partitioned_union_polygon";
		case AggrMethod::asItemList:
			if (adi->GetAbstrValuesUnit()->GetValueType()->GetValueClassID() == VT_String)
				return "asItemList";
			else
				return "asExprList";
		case AggrMethod::min:  return "min";
		case AggrMethod::max:  return "max";
		case AggrMethod::first:return "first";
		case AggrMethod::last: return "last";
		case AggrMethod::mean: return "mean";
		case AggrMethod::sd  : return "sd";
		case AggrMethod::modus: return "modus";
	}
	return "unknown";
}

CharPtr OperExprFormat(const AbstrDataItem* adi, AggrMethod am)
{
	switch (am)
	{
	case AggrMethod::asItemList:
		if (adi->GetValueComposition() != ValueComposition::Single || adi->GetAbstrValuesUnit()->GetValueType()->GetNrDims() > 1)
			return "%1%(AsDataString(%2%), %3%)"; // fits for most cases
		break;
	case AggrMethod::first:
	case AggrMethod::last:
		if (adi->GetValueComposition() != ValueComposition::Single)
			return "%2%[%1%(ID(DomainUnit(%3%)), %3%)]";
		break;
	}
	return "%1%(%2%, %3%)"; // fits for most cases
}

SharedStr OperExpr(const AbstrDataItem* adi, AggrMethod am)
{
	CharPtr groupByItemName = "../GroupBy/per_Row";
	return mgFormat2SharedStr(OperExprFormat(adi, am), OperName(adi, am) , adi->GetFullName(), groupByItemName);
}

const AbstrDataItem* DataItemColumn::GetActiveTextAttr() const
{
	if (m_FutureAggrAttr)
		return m_FutureAggrAttr.get_ptr();
	return GetSrcAttr();
}

const AbstrDataItem* DataItemColumn::GetSrcAttr() const
{
	return m_FutureSrcAttr.get_ptr();
}

SharedStr DataItemColumn::Caption() const
{
	SharedStr name = GetThemeDisplayNameInclMetric(this);
	auto tc = GetTableControl().lock(); if (!tc) return name;
	if (!tc->m_GroupByEntity || IsDefined(m_GroupByIndex))
		return name;

	return mgFormat2SharedStr("%1%(%2%)", OperName(GetSrcAttr(), m_AggrMethod), name);
}

void DataItemColumn::UpdateTheme()
{
	auto tc = GetTableControl().lock(); if (!tc) return;
	auto attr = GetSrcAttr();

	if (!attr) return;

	auto oldAggrAttr = GetContext()->GetSubTreeItemByID(aggrID);
	if (oldAggrAttr)
		oldAggrAttr->RemoveFromConfig();

	if (tc->m_GroupByEntity)
	{
		auto aggrMethod = IsDefined(m_GroupByIndex) ? AggrMethod::first : m_AggrMethod;
		while (!Allowed(GetSrcAttr(), aggrMethod))
		{
			dms_assert(aggrMethod != AggrMethod::first); // must always be allowed.
			aggrMethod = AggrMethod(int(aggrMethod) + 1);
			dms_assert(aggrMethod < AggrMethod::nr_methods); // there will be an allowed method.
			m_AggrMethod = aggrMethod;
		}

		SharedPtr<AbstrDataItem> aggrAttr = CreateDataItem(GetContext(), aggrID, tc->m_GroupByEntity, ValuesUnit(GetSrcAttr(), aggrMethod), GetSrcAttr()->GetValueComposition());
		aggrAttr->SetKeepDataState(false);
		aggrAttr->DisableStorage(true);
		aggrAttr->SetExpr(OperExpr(GetSrcAttr(), aggrMethod));

		m_FutureAggrAttr = aggrAttr.get_ptr(); m_FutureAggrAttr->PrepareDataUsage(DrlType::Certain); // can throw when Expr is invalid, m_FutureAggrAttr will then not be updated
		attr = aggrAttr;
	}
	else
	{
		if (m_FutureAggrAttr) 
			m_FutureAggrAttr->RemoveFromConfig();
		m_FutureAggrAttr = nullptr;
	}
	auto newTheme = std::make_shared<Theme>(m_ActiveTheme, GetSrcAttr(), nullptr, nullptr);
	//		newTheme->SetThemeAttr(adi);
	SetTheme(newTheme.get(), GetContext());
	attr->UpdateMetaInfo();
	tc->m_cmdOnCaptionChange();
	InvalidateDraw();
}

void DataItemColumn::SetElemSize(const GPoint& size)
{
	if (m_ElemSize == size)
		return;

	m_ElemSize = size;

	m_FontArray.reset();
	m_FontIndexCache.reset();

//	OnSizeChanged();
	InvalidateView();
	InvalidateDraw();
}

void DataItemColumn::SetElemWidth(GType width)
{
	if (width == m_ElemSize.x)
		return;

	GType colWidth = width; if (HasElemBorder()) colWidth += (2*BORDERSIZE);

	if (GetEnabledTheme(AN_SymbolIndex))
		InvalidateDraw();

	TType currClientWidth = GetCurrClientSize().x();
	assert(m_ElemSize.x + (colWidth-width) == currClientWidth);
	GType relPosX = m_ElemSize.x; if (HasElemBorder()) relPosX += BORDERSIZE;

	MakeMin(m_ElemSize.x, width);
	GrowHor(colWidth - currClientWidth, relPosX, 0);
	MakeMax(m_ElemSize.x, width);

	dms_assert(GetCurrClientSize().x() == colWidth);
	dms_assert(m_ElemSize.x            == width);
}

void DataItemColumn::SetActiveRow(SizeT row)
{
	auto dv = GetDataView().lock();
	if (row == GetActiveRow())
		return;
	if (IsActive())
	{
		dv->m_TextEditController.CloseCurr();
		InvalidateDrawnActiveElement();
	}
	dbg_assert( row != UNDEFINED_VALUE(UInt32) ); // DEBUG, REMOVE
	m_ActiveRow = row;

	InvalidateDrawnActiveElement();
	MakeVisibleRow();
}

void DataItemColumn::MakeVisibleRow()
{
	dms_assert(AllVisible());
	DBG_START("DataItemColumn", "MakeVisibleRow", MG_DEBUG_SCROLL);

	UpdateView(); // make sure that this DIC has the appropiate size

	auto tc = GetTableControl().lock(); if (!tc) return;

	TRect elemRect = GetElemFullRelRect(GetActiveRow());
	TPoint border  = TPoint(tc->ColSepWidth(), RowSepHeight());
	std::shared_ptr<MovableObject> obj = shared_from_this();
	do
	{
		DBG_TRACE(("RelElemRect: %s", AsString(elemRect).c_str()));

		ScrollPort* sp = dynamic_cast< ScrollPort* > ( obj.get());
		if (sp)
		{
			sp->MakeVisible(elemRect, border);
			break;
		}
		elemRect += obj->GetCurrClientRelPos();
		dms_assert( IsIncluding(obj->GetCurrClientRelRect(), elemRect));

		obj = obj->GetOwner().lock();
	} while (obj);
	SetFocusRect();
}

void DataItemColumn::Sync(TreeItem* viewContext, ShvSyncMode sm)
{
	dms_assert(viewContext);
	base_type::Sync(viewContext, sm);

	ThemeSet::SyncThemes(viewContext, sm);

	if (sm== SM_Load)
	{
		SPoint elemSize;
		SyncValue<SPoint>(viewContext, GetTokenID_mt("ElemSize"), elemSize, Convert<SPoint>(ElemSize()), SM_Load);
		SetElemSize(Convert<GPoint>(elemSize));
	}
	else
	{
		dms_assert(sm==SM_Save);
		SaveValue<SPoint>(viewContext, GetTokenID_mt("ElemSize"), Convert<SPoint>(ElemSize()));
	}
	Bool elemBorder = HasElemBorder();
	SyncValue<Bool>(viewContext, GetTokenID_mt
	("ElemBorder"), elemBorder, Bool(false), sm);
	SetElemBorder(elemBorder);
}

void DataItemColumn::DoInvalidate() const
{
	dms_assert(!m_State.HasInvalidationBlock());

	base_type::DoInvalidate();
	const_cast<DataItemColumn*>(this)->InvalidateDraw();

	m_FontArray.reset();
	m_FontIndexCache.reset();
	m_State.Clear(DIC_TotalReady);

	dms_assert(DoesHaveSupplInterest() || !GetInterestCount());
}

Float64 DataItemColumn::GetColumnTotal() const
{
	if (!m_State.Get(DIC_TotalReady))
	{
		m_ColumnTotal = GetSrcAttr()->GetRefObj()->GetSumAsFloat64();
		m_State.Set(DIC_TotalReady);
	}
	return m_ColumnTotal;
}

ActorVisitState DataItemColumn::VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const
{
	if (ThemeSet::VisitSuppliers(svf, visitor, IsActive()) == AVS_SuspendedOrFailed)
		return AVS_SuspendedOrFailed;
	if (m_FutureAggrAttr && m_FutureAggrAttr->VisitSuppliers(svf, visitor) == AVS_SuspendedOrFailed)
		return AVS_SuspendedOrFailed;
	return base_type::VisitSuppliers(svf, visitor);
}

void DataItemColumn::DoUpdateView()
{
	dbg_assert(!SuspendTrigger::DidSuspend());

	auto tc = GetTableControl().lock(); if (!tc) return;

	SizeT n = tc->NrRows();
	if (!IsDefined(n))
	{
		n = 8;
		PrepareDataOrUpdateViewLater(tc->GetRowEntity());
	}
	TPoint size( m_ElemSize.x, m_ElemSize.y );

	if (HasElemBorder())
	{
		size.x() += DOUBLE_BORDERSIZE;
		size.y() += DOUBLE_BORDERSIZE;
	}
	UInt32 rowSepHeight = RowSepHeight();
	size.y() += rowSepHeight;
	MakeMin<SizeT>(n, MaxValue<TType>() / size.y());
	size.y() *= n;
	MakeMin<TType>(size.y(), MaxValue<TType>() - rowSepHeight);
	size.y() += rowSepHeight;

	SetClientSize(size);

	dbg_assert(!SuspendTrigger::DidSuspend());
}

void DataItemColumn::DrawBackground(const GraphDrawer& d) const
{
	dms_assert(d.DoDrawBackground()); // PRECONDITION
	dms_assert(d.GetDC()); // implied by prev
	dms_assert(IsMainThread());
	base_type::DrawBackground(d);

	GType rowSep = RowSepHeight();
	if (!rowSep)
		return;

	auto tc = GetTableControl().lock(); if (!tc) return;
	SizeT n = tc->NrRows(); if (!IsDefined(n)) n = 8;

	auto penTheme = GetEnabledTheme(AN_PenColor);

	GdiHandle<HBRUSH> br( CreateSolidBrush( DmsColor2COLORREF(0) ) );

	GRect  absFullRect = GetClippedCurrFullAbsRect(d);
	GType  rowDelta    = ElemSize().y + rowSep;
	if (HasElemBorder())
		rowDelta += 2*BORDERSIZE;

	TType clientOffsetRow = d.GetClientOffset().y();
	GType pageClipRectRow = d.GetAbsClipRect().Top();
	SizeT recNo = (pageClipRectRow > clientOffsetRow)
		?	(pageClipRectRow - clientOffsetRow) / rowDelta
		:	0;

	if (recNo > n)
		return;

	if (penTheme)
	{
		ThemeReadLocks trl;
		SuspendTrigger::BlockerBase block;
		trl.push_back(penTheme.get(), DrlType::Certain);

		br = GdiHandle<HBRUSH>(CreateSolidBrush(DmsColor2COLORREF(penTheme->GetValueGetter()->GetColorValue(Min<SizeT>(recNo, n-1)))));
	}
	TType currRow    = TType(recNo) * rowDelta + d.GetClientOffset().y();
	GType clipEndRow = d.GetAbsClipRect().Bottom();

	// draw horizontal borders
	while ( recNo < n)
	{
		if (currRow >= clipEndRow)
			return;
		absFullRect.Top   () = currRow;
		absFullRect.Bottom() = currRow+rowSep;
		FillRect(d.GetDC(), &absFullRect, br);

		++recNo;
		currRow += rowDelta;
	}
	dms_assert(recNo == n);

	absFullRect.Top() = currRow;
	if (absFullRect.Top() >= clipEndRow)
		return;
	absFullRect.Bottom() = absFullRect.Top() + rowSep;
	FillRect(d.GetDC(), &absFullRect, br);
}


TRect DataItemColumn::GetElemFullRelRect( SizeT rowNr) const
{
	if (!IsDefined(rowNr))
		return GetCurrFullRelRect();

	GPoint size = m_ElemSize;
	if (HasElemBorder())
	{
		size.x += 2*BORDERSIZE;
		size.y += 2*BORDERSIZE;
	}

	UInt32 rowSepHeight = RowSepHeight();

	TType startRow = (TType(size.y) + rowSepHeight) * rowNr + rowSepHeight;

	return TRect(TPoint(0, startRow), TPoint(size.x, startRow + size.y));
}

void DataItemColumn::InvalidateRelRect(TRect rect)
{
	auto dv = GetDataView().lock(); if (!dv) return;
	GRect screenRect = TRect2GRect( rect + GetCurrClientAbsPos () );
	screenRect &= GetDrawnClientAbsRect();
	if (!screenRect.empty())
		dv->InvalidateRect( screenRect );
}

void DataItemColumn::InvalidateDrawnActiveElement()
{
	if (!IsDrawn())
		return;

	InvalidateRelRect( GetElemFullRelRect(m_ActiveRow) );
}

void DataItemColumn::DrawElement(GraphDrawer& d, SizeT rowNr, GRect elemExtents, GuiReadLockPair& locks) const
{
	dms_assert(!SuspendTrigger::DidSuspend());
	dms_assert(d.DoDrawData()); // PRECONDITION
	dms_assert(d.GetDC()); // implied by prev

// TODO: Set scaled Font size, Set TextAlignMode
//	CrdPoint base = d.GetTransformation().GetOffset();
//	dms_assert(base == d.GetTransformation().Apply(CrdPoint(0, 0) ) );

//	GRect elemExtents = absElemRect; //TRect( d.GetClientOffset(), d.GetClientOffset() + TPoint(elemSize) );

	if (HasElemBorder())
		DrawButtonBorder(d.GetDC(), elemExtents);

	bool isSymbol = GetEnabledTheme(AN_SymbolIndex).get();
	bool isActive = IsActive() && rowNr == GetActiveRow();
	DmsColor bkClr;

	auto tc = GetTableControl().lock(); if (!tc) return;
	SizeT recNo = tc->GetRecNo(rowNr); if (!IsDefined(recNo)) return;

	SelectionID selValue;
	auto selTheme = GetEnabledTheme(AN_Selections);
	if	(	selTheme 
		&&	(! tc->ShowSelectedOnly() )
		&& (selValue = Convert<Bool>(selTheme->GetValueGetter()->GetClassIndex(recNo)), selValue)
	)
		bkClr = GetSelectedClr(selValue);
	else if (GetEnabledTheme(AN_LabelBackColor))
		bkClr = GetEnabledTheme(AN_LabelBackColor)->GetValueGetter()->GetColorValue(recNo);
	else if (GetEnabledTheme(AN_BrushColor))
		bkClr = GetEnabledTheme(AN_BrushColor)->GetValueGetter()->GetColorValue(recNo);
	else
		bkClr = (WasFailed())
			?	DmsRed
			:	UndefinedValue(TYPEID(DmsColor));             // Transparent

	if (isSymbol || !GetEnabledTheme(AN_LabelText))
	{
		HFONT hFont = isSymbol ? GetFont (recNo, FR_Symbol, d.GetSubPixelFactor()) : 0;
		WCHAR wSymb = isSymbol ? GetSymbol(recNo) : UNDEFINED_WCHAR;
		if (isActive) 
			bkClr = GetFocusClr();
		DrawSymbol(
			d.GetDC(), 
			elemExtents,
			hFont,
			GetColor(recNo, AN_LabelTextColor),
			bkClr,
			wSymb
		);
	}
	else
	{
		auto textInfo = GetText(recNo, MAX_TEXTOUT_SIZE, locks);
		DrawEditText(
			d.GetDC(),
			elemExtents,
			GetFont(recNo, FR_Label, d.GetSubPixelFactor()),
			textInfo.m_Grayed ? RGB(100, 100, 100) : GetColor(recNo, AN_LabelTextColor),
			bkClr,
			textInfo.m_Text.c_str(),
			isActive
		);
	}
	if (isActive)
	{
		if (HasElemBorder())
		{
			elemExtents.Expand(1); DrawFocusRect(d.GetDC(), &elemExtents ); 
			elemExtents.Expand(1); DrawFocusRect(d.GetDC(), &elemExtents );
		}
	}
	else
	{
		if (tc->InSelRange(rowNr, m_ColumnNr) )
		{
			InvertRect(d.GetDC(), &elemExtents );
			if (HasElemBorder())
			{
				elemExtents.Expand(BORDERSIZE);
				InvertRect(d.GetDC(), &elemExtents );
			}
		}
	}
}

COLORREF DataItemColumn::GetBkColor() const
{
//	auto bckTheme = GetTheme(AN_LabelBackColor);
//	if (bckTheme)
//		return bckTheme->GetColorAspectValue();

	bool isSymbol = GetEnabledTheme(AN_SymbolIndex).get();
	switch (m_GroupByIndex) {
		case -1: break;
		case 0: return DmsColor2COLORREF(CombineRGB(128, 255, 128));  // Transparent
		case 1: return DmsColor2COLORREF(CombineRGB(128 / 2 + 64, 255 / 2 + 64, 128/2 + 64));
		case 2: return DmsColor2COLORREF(CombineRGB(128 / 4 + 64, 255 / 4 + 64, 128 / 4 + 64));
		case 3: return DmsColor2COLORREF(CombineRGB(128 / 8 + 64, 255 / 8 + 64, 128 / 8 + 64));
		case 4: return DmsColor2COLORREF(CombineRGB(128 /16 + 64, 255 /16 + 64, 128 /16 + 64));
		case 5: return DmsColor2COLORREF(CombineRGB(128 /32 + 64, 255 /32 + 64, 128 /32 + 64));
		default: return DmsColor2COLORREF(CombineRGB(128 / 32 + 64, 255 / 32 + 64, 128 / 32 + 64));
	}

	if (isSymbol || IsEditable(AN_LabelText))
		return DmsColor2COLORREF(DmsWhite);

	return DmsColor2COLORREF(UNDEFINED_VALUE(DmsColor));  // Transparent
}

bool DataItemColumn::IsEditable(AspectNr a) const 
{
	auto tc = GetTableControl().lock(); if (!tc) return false;
	if (tc->m_GroupByEntity) return false;

	auto theme = GetEnabledTheme(AN_LabelText);
	if (!theme)
		theme =  GetEnabledTheme(AN_LabelTextColor);
	if (!theme)
		return false;

	const AbstrDataItem* adi = theme->GetThemeAttrSource();
	return !adi || adi->IsEditable();
}

DmsColor DataItemColumn::GetOrgColor(SizeT recNo, AspectNr a) const
{
	auto theme = GetEnabledTheme(a);
	if (theme && !theme->IsFailed())
		return theme->GetValueGetter()->GetColorValue(recNo);

	return UNDEFINED_VALUE(DmsColor);
}

COLORREF DataItemColumn::GetColor(SizeT recNo, AspectNr a) const
{
	return DmsColor2COLORREF( GetOrgColor(recNo, a) );
}

void DataItemColumn::SetOrgColor(SizeT recNo, AspectNr a, DmsColor color)
{
	dms_assert(!SuspendTrigger::DidSuspend());
	auto theme = GetEnabledTheme(a);
	if (!theme)
		return;
	AbstrDataItem* adi = const_cast<AbstrDataItem*>(theme->GetThemeAttrSource());
	if (adi)
	{
		StaticStIncrementalLock<TreeItem::s_MakeEndoLockCount> makeEndoLock;

		DataWriteHandle hnd(adi, dms_rw_mode::read_write);
		hnd->SetValue<DmsColor>(adi->HasVoidDomainGuarantee() ? 0 : recNo, color);
		hnd.Commit();
		adi->MarkTS(UpdateMarker::GetFreshTS(MG_DEBUG_TS_SOURCE_CODE("SetOrgColor")));
	}
	else
	{
		dms_assert(theme->m_ValueGetterPtr);
		auto vg = dynamic_cast<ConstVarGetter<DmsColor>*>(theme->m_ValueGetterPtr.get_ptr());
		if (vg)
			vg->SetValue(color);
		else
			theme->m_ValueGetterPtr.assign(new ConstValueGetter<DmsColor>(color));
	}
	InvalidateDraw();
}

WCHAR DataItemColumn::GetSymbol(SizeT recNo) const
{
	auto theme = GetEnabledTheme(AN_SymbolIndex);
	dms_assert(theme);
	if (theme->IsFailed())
		return 0;
	return theme->GetValueGetter()->GetOrdinalValue(recNo);
}

TextInfo DataItemColumn::GetText(SizeT recNo, SizeT maxLen, GuiReadLockPair& locks) const
{
	SharedDataItem activeTextAttr = GetActiveTextAttr();
	if (!activeTextAttr)
	{
		auto theme = GetEnabledTheme(AN_LabelText);
		dms_assert(theme);
		if (theme->IsFailed())
		{
			auto fr = theme->GetFailReason(); if (!fr) return TextInfo({}, true);
			return TextInfo{ fr->GetAsText(), true };
		}
		GuiReadLock dummy;
		return theme->GetValueGetter()->GetTextInfo(recNo, dummy);
	}
	if (activeTextAttr->WasFailed(FR_Data))
		return TextInfo{ activeTextAttr->GetFailReason()->Why(), true };

	auto  refObj = activeTextAttr->GetRefObj();
	if (refObj->IsNull(recNo))
	{
		static SharedStr undefinedStr(UNDEFINED_VALUE_STRING);
		return TextInfo(undefinedStr, true);
	}
	if (m_State.Get(DIC_RelativeDisplay))
	{
		dms_assert(IsNumeric());
		Float64 total = GetColumnTotal();
		if (total != 0)
		{
			Float64 relValue = refObj->GetValueAsFloat64(recNo) /  total;
			return TextInfo{ mySSPrintF("%lg %%", 100.0 * relValue), false };
		}
	}
	return TextInfo{ DisplayValue(activeTextAttr, recNo, false, m_DisplayInterest, maxLen, locks), false };
}

SharedStr DataItemColumn::GetOrgText(SizeT recNo, GuiReadLock& lock) const
{
	auto theme = GetTheme(AN_LabelText);
	if (!theme || theme->IsFailed(FR_MetaInfo))
		return SharedStr();

	dms_assert(GetActiveTextAttr() && GetActiveTextAttr()->GetInterestCount() > 0);
	DataReadLock readHandle(GetActiveTextAttr());
	dms_assert(GetActiveTextAttr() && GetActiveTextAttr()->GetCurrUltimateItem()->m_ItemCount > 0);
	return readHandle->AsString(recNo, lock);
}

void DataItemColumn::SetOrgText  (SizeT recNo, CharPtr textData)
{
	auto theme = GetTheme(AN_LabelText);
	if (!theme)
		return;
	AbstrDataItem* adi = const_cast<AbstrDataItem*>(theme->GetThemeAttrSource());
	DataWriteLock dwl(adi, dms_rw_mode::read_write);
		AbstrDataObject* ado = dwl.get();
		OwningPtr<AbstrValue> av = ado->CreateAbstrValue();
		av->AssignFromCharPtr(textData);
		ado->SetAbstrValue(recNo, *av);
	dwl.Commit();
}

void DataItemColumn::SetRevBorder(bool revBorder)
{
	base_type::SetRevBorder(revBorder);
}

HFONT DataItemColumn::GetFont(SizeT recNo, FontRole fr, Float64 subPixelFactor) const
{
	auto fontTheme = GetTheme(fontNameAspect[fr]);

	if (!fontTheme && !*(defFontNames[fr]))
		return 0;

	dms_assert(m_FontIndexCache || !m_FontArray); // FontArray is only avaiable when FontIndexCache is available

	if (! m_FontArray || m_FontIndexCache->GetLastSubPixelFactor() != subPixelFactor)
	{
		UInt32 cellHeight = m_ElemSize.y;
		if (HasBorder())
			cellHeight -= 2*BORDERSIZE;

		if (!m_FontIndexCache)
			m_FontIndexCache.assign(
				new FontIndexCache(
					0, 0, fontTheme.get(), 0
				,	fontTheme ? fontTheme->GetThemeEntityUnit() : Unit<Void>::GetStaticClass()->CreateDefault() // theme domain entity
				,	cellHeight, 0, GetTokenID_mt(defFontNames[fr]), 0
				)
			);
		m_FontIndexCache->UpdateForZoomLevel(1.0, subPixelFactor);
		m_FontArray.assign(new FontArray(m_FontIndexCache, true) );
	}
	dms_assert(m_FontArray);

	if (m_FontArray->IsSingleton())
		recNo = 0;
	else
		recNo = m_FontIndexCache->GetKeyIndex(recNo);

	return m_FontArray->GetFontHandle(recNo);
}


GraphVisitState DataItemColumn::InviteGraphVistor(class AbstrVisitor& gv)
{
	return gv.DoDataItemColumn(this);
}

UInt32 DataItemColumn::RowSepHeight() const
{
	auto tc = GetTableControl().lock(); if (!tc) return 0;
	return tc->ColSepWidth();
}

std::weak_ptr<TableControl> DataItemColumn::GetTableControl()
{
	return std::static_pointer_cast<TableControl>(GetOwner().lock());
}

std::weak_ptr<const TableControl> DataItemColumn::GetTableControl() const
{
	return std::static_pointer_cast<const TableControl>(GetOwner().lock());
}

std::atomic<UInt32> s_ChooseColorDialogCount = 0;

bool ChooseColorDialog(DmsColor& rgb, DataView* dv)
{
	dms_assert(dv);

	HWND hParent = dv->GetHWnd();
	if (s_ChooseColorDialogCount)
		return false;
	StaticMtIncrementalLock<s_ChooseColorDialogCount> dialogLock;

	static_assert(nrPaletteColors >= 16);

	COLORREF custColors[16] = {}; // array with static initialization enables users to change the custom colors
	for (UInt32 i = 0; i != 16; ++i)
		custColors[i] = DmsColor2COLORREF(dv->m_ColorPalette[i]);

	CHOOSECOLOR colorData = { 
		/*lStructSize    : */ sizeof(CHOOSECOLOR),
		/*hwndOwner      : */ 0,
		/*hInstance      = */ 0, // ignored unless CC_ENABLETEMPLATEHANDLE or CC_ENABLETEMPLATE is set,
		/*rgbResult      = */ 0,
		/*lpCustColors   = */ custColors,
		/*Flags          = */ CC_ANYCOLOR|CC_RGBINIT|CC_FULLOPEN,
		/*lCustData      = */ 0, // ignored unless CC_ENABLEHOOK is set
		/*lpfnHook       = */ 0, // ignored unless CC_ENABLEHOOK is set
		/*lpTemplateName = */ 0, // ignored unless CC_ENABLETEMPLATE is set
	};
	colorData.hwndOwner = hParent;

	IdleTimer idleProcessingProvider;

	colorData.rgbResult = DmsColor2COLORREF(rgb);
	bool result = ChooseColor(&colorData);
	if (result)
		rgb = COLORREF2DmsColor(colorData.rgbResult);

	// store edited custom colors
	for (UInt32 i = 0; i != 16; ++i)
		dv->m_ColorPalette[i] = COLORREF2DmsColor(custColors[i]);
	return result;
}

void DataItemColumn::SetActive(bool newState)
{
	if (newState)
		MakeVisibleRow();
	else
	{
		dms_assert(IsActive());
		auto dv = GetDataView().lock();
		if (dv) {
			MG_DEBUGCODE(dv->m_TextEditController.CheckCurrTec(this); )
				dv->m_TextEditController.CloseCurr();
		}
	}
	if (newState != IsActive())
		InvalidateDrawnActiveElement();
	base_type::SetActive(newState);
	SetFocusRect();
}

void DataItemColumn::SelectCol()
{
	auto tc = GetTableControl().lock(); if (!tc) return;
	dms_assert(IsDefined(m_ColumnNr));
	dms_assert(tc->GetColumn(m_ColumnNr) == this);

	SelChangeInvalidator sci(tc.get());

	if (!tc->m_Rows.IsDefined()) tc->m_Rows.m_Curr = 0;
	tc->m_Rows.m_Begin = 0;
	tc->m_Rows.m_End   = tc->NrRows()-1;

	tc->m_Cols.CloseAt(m_ColumnNr);

	sci.ProcessChange(true);
}

void DataItemColumn::GotoRow(SizeT row)
{
	auto tc = GetTableControl().lock(); if (!tc) return;
	if (row >= tc->NrRows())
	{
		reportF(SeverityTypeID::ST_Warning, "ViewColumn.GotoRow: row %s not available", row);
		return;
	}
	SelChangeInvalidator sci(tc.get());
	tc->GoTo(row, m_ColumnNr);
	sci.ProcessChange(true);
}

void DataItemColumn::GotoClipboardRow()
{
	auto clipboardText = ClipBoard().GetText();
	SizeT row = ThrowingConvert<SizeT>(clipboardText);
	GotoRow(row);
}

void DataItemColumn::FindNextClipboardValue()
{
	auto searchText = ClipBoard(false).GetText();
	FindNextValue(searchText);
}

// TODO: Move to SharedStr.h or SequenceArray.h
namespace {

	template <typename T, typename U>
	bool equal_to(const T& lhs, const U& rhs)
	{
		return lhs == rhs;
	}

	bool equal_to(SA_ConstReference<char> lhs, const SharedStr& rhs)
	{
		return (lhs.IsDefined() == IsDefined(rhs)) 
			&& equal(lhs.begin(), lhs.end(), rhs.cbegin(), rhs.csend());
	}
}

void DataItemColumn::FindNextValue(SharedStr searchText)
{
	auto tc = GetTableControl().lock(); if (!tc) return;
	reportF(SeverityTypeID::ST_Warning, "ViewColumn.FindNext %.80s", searchText);
	auto aa = this->GetActiveAttr();
	DataReadLock lock(aa);
	visit<typelists::fields>(GetActiveTextAttr()->GetAbstrValuesUnit(),
		[searchText = std::move(searchText), aa, tc, this] <typename value_type> (const Unit<value_type>*) 
		{
			auto searchValue = ThrowingConvert<value_type>(searchText);
			SizeT nrRows = tc->NrRows();
			SizeT row = tc->GetActiveRow();
			SizeT currRow = row;
			auto searchData = const_array_cast<value_type>(aa)->GetLockedDataRead();
			do{
				++row;  if (row == nrRows) row = 0; // go to next row modulo nrRows
				auto recNo = tc->GetRecNo(row);
				dms_assert(recNo < searchData.size());
				if (equal_to(searchData[recNo], searchValue))
				{
					this->GotoRow(row);
					return;
				}
			} while (row != currRow);
			reportF(SeverityTypeID::ST_Warning, "ViewColumn.FindNext: failed finding value %.80s", AsString(searchValue));
		}
	);
}

void DataItemColumn::SetFocusRect()
{
	auto dv = GetDataView().lock();
	if (IsActive())
	{
		GRect elemAbsRect = TRect2GRect( TRect(GetElemFullRelRect(GetActiveRow()) + GetCurrClientAbsPos()).Expand(1) );
		dv->SetFocusRect( elemAbsRect );
	}
	else
		dv->SetFocusRect(GRect());
}

void DataItemColumn::SelectColor(AspectNr a)
{
	auto dv = GetDataView().lock(); if (!dv) return;

	dms_assert(!SuspendTrigger::DidSuspend());
	SizeT rowNr = GetActiveRow();
	DmsColor rgb;
	{
		ThemeReadLocks trl;
		if (!trl.push_back(GetEnabledTheme(AN_LabelTextColor).get(), DrlType::Suspendible) )
			return; 
		rgb = GetOrgColor(rowNr, a);
	}
	if (ChooseColorDialog(rgb, dv.get()))
		SetOrgColor(rowNr, a, rgb);
}

void DataItemColumn::SetTransparentColor(AspectNr a)
{
	SetOrgColor(GetActiveRow(), a, DmsTransparent);
}

bool DataItemColumn::MouseEvent(MouseEventDispatcher& med)
{
	auto tc = GetTableControl().lock(); if (!tc) return false;
	if (med.GetEventInfo().m_EventID & EID_MOUSEWHEEL )
	{
		bool shift = GetKeyState(VK_SHIFT) & 0x8000;
		int wheelDelta = GET_WHEEL_DELTA_WPARAM(med.r_EventInfo.m_wParam);
		if (wheelDelta > 0) tc->GoUp(shift, ((  wheelDelta -1) / WHEEL_DELTA) + 1);
		if (wheelDelta < 0) tc->GoDn(shift, (((-wheelDelta)-1) / WHEEL_DELTA) + 1);
		return true;
	}
	if (med.GetEventInfo().m_EventID & EID_LBUTTONDOWN)
	{
		switch( GetControlRegion(med.GetEventInfo().m_Point.x) ) 
		{
			case RG_LEFT:
				{
					DataItemColumn* prevHeader = GetPrevControl();
					if (prevHeader)
						prevHeader->StartResize(med);
					return true;
				}
			case RG_RIGHT: 
				{
					StartResize(med);
					return true;
				}
		}
	}

	if ((med.GetEventInfo().m_EventID & EID_SETCURSOR ))
	{
		if (GetControlRegion(med.GetEventInfo().m_Point.x) != RG_MIDDLE )
		{
			SetCursor(LoadCursor(NULL, IDC_SIZEWE));
			return true;
		}
			
		if (IsEditable(AN_LabelText))
		{
			SetCursor(LoadCursor(NULL, GetEnabledTheme(AN_LabelText) ? IDC_IBEAM : IDC_ARROW));
			return true;
		}
	}

	if ((med.GetEventInfo().m_EventID & (EID_LBUTTONDOWN|EID_LBUTTONDBLCLK) ) && !IgnoreActivation())
	{
		dms_assert(tc->GetColumn(m_ColumnNr) == this);

		TPoint relClientPos = TPoint(med.GetEventInfo().m_Point) - (med.GetClientOffset() + GetCurrClientRelPos());

		GType height = m_ElemSize.y + RowSepHeight();
		if (HasElemBorder()) height += (2*BORDERSIZE);
		SizeT rowNr = relClientPos.y() / height;
		if (rowNr >= tc->NrRows()) goto skip;

		relClientPos.y() %= height;
		if (HasElemBorder())
		{
			if (relClientPos.x() < BORDERSIZE) goto skip;
			if (relClientPos.y() < BORDERSIZE) goto skip;
			relClientPos.x() -= BORDERSIZE;
			relClientPos.y() -= BORDERSIZE;
		}
		if (!IsStrictlyLower(relClientPos, TPoint(m_ElemSize))) goto skip;

		{
			SelChangeInvalidator sci(tc.get());
			bool shift = GetKeyState(VK_SHIFT) & 0x8000;

			tc->m_Rows.Go( shift, rowNr );
			tc->m_Cols.Go( shift, m_ColumnNr);
			sci.ProcessChange(true);
		}

		if(med.GetEventInfo().m_EventID & EID_LBUTTONDBLCLK )
		{
			if ( GetEnabledTheme(AN_LabelBackColor) )
			{
				if (IsEditable(AN_LabelBackColor))
				{
					SelectBrushColor();
					return true;
				}
			}
			else if (GetEnabledTheme(AN_LabelTextColor))
			{
				if (IsEditable(AN_LabelTextColor))
				{
					SelectPenColor();
					return true;
				}
			}
			else if (GetEnabledTheme(AN_LabelText))
			{
				if (IsEditable(AN_LabelTextColor))
				{
					OnKeyDown(VK_F2);
					return true;
				}
			}
			if (!IgnoreActivation())
				GenerateValueInfo();
			else
				MessageBeep(-1);
		}
		return true;  // don't continue processing LBUTTONDBLCLK
	}
skip:
	return base_type::MouseEvent(med);
}

void DataItemColumn::GenerateValueInfo()
{
	auto tc = GetTableControl().lock(); if (!tc) return;
	if (tc->m_Rows.IsDefined())
		CreateViewValueAction(GetActiveAttr(), tc->GetRecNo(tc->m_Rows.m_Curr), true);
}


bool DataItemColumn::OnCommand(ToolButtonID id)
{
	switch (id)
	{
		case TB_Info: GenerateValueInfo();  return true;
		case TB_GotoClipboardLocation: GotoClipboardRow(); return true;
		case TB_FindClipboardLocation: FindNextClipboardValue(); return true;
	}
	return base_type::OnCommand(id);
}

bool DataItemColumn::OnKeyDown(UInt32 virtKey)
{
	auto dv = GetDataView().lock(); if (!dv) return true;
	auto at = GetActiveTextAttr();
	if (!at || at->m_ItemCount < 0)
		return false;

	if (dv->m_TextEditController.OnKeyDown(this, GetActiveRow(), virtKey))
		return true;
	if (KeyInfo::IsCtrl(virtKey))
	{
		switch (KeyInfo::CharOf(virtKey)) {
			case 'F': return OnCommand(TB_FindClipboardLocation);
			case 'G': return OnCommand(TB_GotoClipboardLocation);
		}
	}
	return base_type::OnKeyDown(virtKey);
}

void DataItemColumn::FillMenu(MouseEventDispatcher& med)
{
	med.m_MenuData.m_DIC = shared_from_base<const DataItemColumn>();
	SharedStr caption = GetThemeDisplayName(this);

	auto tc = GetTableControl().lock(); if (!tc) return;

	if (tc->HasSortOptions())
	{
		SubMenu subMenu(med.m_MenuData, "Sort on " + GetThemeDisplayName(this)); // SUBMENU

		med.m_MenuData.push_back( MenuItem(SharedStr("Ascending" ), new MembFuncCmd<DataItemColumn>(&DataItemColumn::SortAsc ), this) );
		med.m_MenuData.push_back( MenuItem(SharedStr("Descending"), new MembFuncCmd<DataItemColumn>(&DataItemColumn::SortDesc), this) );
	}
	if (tc->m_GroupByEntity && !IsDefined(m_GroupByIndex)) {
		SubMenu subMenu(med.m_MenuData, SharedStr("Aggregate by ")); // SUBMENU
		for (auto am = AggrMethod(0); am != AggrMethod::nr_methods; am = AggrMethod(int(am)+1))
			if (Allowed(GetSrcAttr(), am))
				med.m_MenuData.push_back(MenuItem(
					SharedStr(OperName(GetSrcAttr(), am)),
					new MembFuncCmd<DataItemColumn, AggrMethod>(&DataItemColumn::SetAggrMethod, am), 
					this,
					am == m_AggrMethod ? MF_CHECKED : 0
				));
	}

//	Display Relative
	if (IsNumeric())
		med.m_MenuData.push_back( 
			MenuItem(
				SharedStr("&Relative Display (as % of total)")
			,	new MembFuncCmd<DataItemColumn>(&DataItemColumn::ToggleRelativeDisplay)
			,	this
			,	m_State.Get(DIC_RelativeDisplay) ? MF_CHECKED : 0
			)
		);
//	Goto & Find
	med.m_MenuData.push_back(MenuItem(SharedStr("Goto (Ctrl-G): take Clipboard contents as row number and go there"), new MembFuncCmd<DataItemColumn>(&DataItemColumn::GotoClipboardRow), this));
	med.m_MenuData.push_back(MenuItem(SharedStr("FindNextValue (Ctrl-F): take Clipboard contents as value and search for it, starting after the current position"), new MembFuncCmd<DataItemColumn>(&DataItemColumn::FindNextClipboardValue), this));

//	Remove DIC
	med.m_MenuData.push_back( MenuItem(mySSPrintF("&Remove %s", caption.c_str()), new MembFuncCmd<DataItemColumn>(&DataItemColumn::Remove), this) );

//	Ramping
	SharedPtr<const AbstrDataItem> activeAttr = GetActiveAttr();
	if (activeAttr && activeAttr->GetAbstrValuesUnit()->GetValueType()->IsNumeric()
		&&	tc->m_Cols.IsDefined()
		&&	tc->GetActiveCol() == m_ColumnNr
	)
	{
		bool rangeSelected = 
				tc->m_Rows.IsDefined()
			&&	!tc->m_Rows.IsClosed()
			&&	tc->GetActiveCol() == m_ColumnNr;

		if (rangeSelected)
		{
			AbstrDataItem* adi = const_cast<AbstrDataItem*>(GetActiveAttr());

			bool rampingPossible = 
				adi->GetAbstrValuesUnit()->GetValueType()->IsNumeric()
			&&	!adi->IsDerivable();

			med.m_MenuData.push_back(
				MenuItem(
					SharedStr(GetEnabledTheme(AN_LabelTextColor) ? "Ramp Colors": "Ramp Values")
				,	new MembFuncCmd<DataItemColumn>(&DataItemColumn::Ramp)
				,	this
				,	rampingPossible ? 0 : MFS_GRAYED
				)
			);
		}
	}

	if ( GetEnabledTheme(AN_LabelBackColor) && IsEditable(AN_LabelBackColor) )
	{
		SubMenu subMenu(med.m_MenuData, SharedStr("Change Brush Color"));

		med.m_MenuData.push_back(
			MenuItem(
				SharedStr("Select from Palette")
			,	new MembFuncCmd<DataItemColumn>(&DataItemColumn::SelectBrushColor)
			,	this
			)
		);
		med.m_MenuData.push_back(
			MenuItem(
				SharedStr("Set to transparent")
			,	new MembFuncCmd<DataItemColumn>(&DataItemColumn::SetTransparentBrushColor)
			,	this
			)
		);
	}
	if (GetEnabledTheme(AN_LabelTextColor) && IsEditable(AN_LabelTextColor))
	{
		SubMenu subMenu(med.m_MenuData, SharedStr("Change Pen Color"));

		med.m_MenuData.push_back(
			MenuItem(
				SharedStr("Select from Palette")
				, new MembFuncCmd<DataItemColumn>(&DataItemColumn::SelectPenColor)
				, this
			)
		);
		med.m_MenuData.push_back(
			MenuItem(
				SharedStr("Set to transparent")
				, new MembFuncCmd<DataItemColumn>(&DataItemColumn::SetTransparentPenColor)
				, this
			)
		);
	}

	base_type::FillMenu(med);
}

void DataItemColumn::SortAsc()
{
	auto tc = GetTableControl().lock(); if (!tc) return;
	tc->CreateTableIndex(this, SO_Ascending);
}

void DataItemColumn::SortDesc()
{
	auto tc = GetTableControl().lock(); if (!tc) return;
	tc->CreateTableIndex(this, SO_Descending);
}

void DataItemColumn::Remove()
{
	auto tc = GetTableControl().lock(); if (!tc) return;
	tc->RemoveEntry(this);
}

void DataItemColumn::RampColors(AbstrDataObject* ado, SizeT firstRow, SizeT lastRow)
{
	auto tc = GetTableControl().lock(); if (!tc) return;
	UInt32 firstClr = ado->GetValue<DmsColor>(tc->GetRecNo(firstRow) );
	UInt32 lastClr  = ado->GetValue<DmsColor>(tc->GetRecNo( lastRow) );

	SizeT n = lastRow - firstRow;
	dms_assert(n > 0);

	for (SizeT i = firstRow+1; i < lastRow; ++i)
	{
		ado->SetValue<DmsColor>(
			tc->GetRecNo(i),
			InterpolateColor(firstClr, lastClr, n, i - firstRow)
		);
	}
}

void DataItemColumn::RampValues(AbstrDataObject* ado, SizeT firstRow, SizeT lastRow)
{
	auto tc = GetTableControl().lock(); if (!tc) return;
	Float64 firstValue = ado->GetValueAsFloat64(tc->GetRecNo(firstRow) );
	Float64 lastValue  = ado->GetValueAsFloat64(tc->GetRecNo( lastRow) );


	SizeT n = lastRow - firstRow;
	dms_assert(n > 0);

	for (row_id i = firstRow+1; i < lastRow; ++i)
	{
		ado->SetValueAsFloat64(
			tc->GetRecNo(i),
			InterpolateValue<Float64>(firstValue, lastValue, n, i - firstRow)
		);
	}
}

void DataItemColumn::Ramp()
{
	auto tc = GetTableControl().lock(); if (!tc) return;
	AbstrDataItem* adi = const_cast<AbstrDataItem*>(GetActiveAttr());
	// PRECONDITIONS
	dms_assert( adi->GetAbstrValuesUnit()->GetValueType()->IsNumeric());
	dms_assert( tc->GetActiveCol() == m_ColumnNr);
	dms_assert( tc->m_Rows.IsDefined());
	dms_assert(!tc->m_Rows.IsClosed());

	if (adi->IsDerivable())
		adi->throwItemError("Ramp: Cannot change derived data; try to copy the attribute and change the copied data");

	DataWriteLock lock(adi);

	auto colorTheme = GetEnabledTheme(AN_LabelTextColor);
	if (colorTheme)
	{
		dms_assert(colorTheme->GetThemeAttr() == adi);
		RampColors(lock.get(), tc->m_Rows.m_Begin, tc->m_Rows.m_End);
	}
	else
	{
		dms_assert(GetTheme(AN_LabelText)->GetThemeAttr() == adi);
		RampValues(lock.get(), tc->m_Rows.m_Begin, tc->m_Rows.m_End);
	}
	lock.Commit();
}

void DataItemColumn::ToggleRelativeDisplay()
{
	m_State.Toggle(DIC_RelativeDisplay);
	InvalidateDraw();
}

bool DataItemColumn::IsNumeric() const
{
	auto theme = GetTheme(AN_LabelText);
	if (!theme)
		return false;
	auto tvu = theme->GetThemeValuesUnit();
	if (!tvu)
		return false;
	return tvu->GetValueType()->IsNumeric();
}

//----------------------------------------------------------------------

#include "Carets.h"
#include "Controllers.h"

//----------------------------------------------------------------------
// class  : ColumnSizerDragger interface
//----------------------------------------------------------------------

class ColumnSizerDragger : public AbstrController
{
	typedef AbstrController base_type;
public:
	ColumnSizerDragger(DataView* owner, DataItemColumn* target);

protected:
	bool Exec(EventInfo& eventInfo) override;
};

//----------------------------------------------------------------------
// class  : ColumnSizerDragger implementation
//----------------------------------------------------------------------

ColumnSizerDragger::ColumnSizerDragger(DataView* owner, DataItemColumn* target)
	:	AbstrController(owner, target, 0, EID_MOUSEDRAG|EID_LBUTTONUP, EID_CLOSE_EVENTS & ~EID_SCROLLED)
{}

bool ColumnSizerDragger::Exec(EventInfo& eventInfo)
{
	auto to = GetTargetObject().lock(); if (!to) return true;
	DataItemColumn* target = debug_cast<DataItemColumn*>(to.get());
	dms_assert(target);
	TPoint clientPos = target->GetCurrClientAbsPos();
	TType newWidth = eventInfo.m_Point.x - clientPos.x();
	if (target->HasElemBorder())
		newWidth -= DOUBLE_BORDERSIZE;
	MakeMax(newWidth, 6);
	target->SetElemWidth(TType2GType(newWidth));
	return true;
}

//==========================================================================


DataItemColumn* DataItemColumn::GetPrevControl()
{
	UInt32 colNr = ColumnNr();
	if (!colNr--)
		return nullptr;
	auto tc = GetTableControl().lock(); if (!tc) return nullptr;
	return debug_cast<DataItemColumn*>(tc->GetEntry(colNr));
}

void DataItemColumn::StartResize(MouseEventDispatcher& med)
{
	auto dv = GetDataView().lock(); if (!dv) return;
	auto owner = GetOwner().lock(); if (!owner) return;
	auto medOwner = med.GetOwner().lock(); if (!medOwner) return;
	TRect   currAbsRect = GetCurrFullAbsRect();
	GPoint& mousePoint  = med.GetEventInfo().m_Point;

	mousePoint.x = currAbsRect.Right();
	dv->SetCursorPos(mousePoint);

	SelectCol();

	medOwner->InsertController(
		new TieCursorController(
			medOwner.get(),
			owner.get(),
			TRect2GRect(TRect(currAbsRect.Left()+6, mousePoint.y, MaxValue<TType>(), TType(mousePoint.y)+1)),
			EID_MOUSEDRAG, EID_CLOSE_EVENTS & ~EID_SCROLLED
		)
	);

	medOwner->InsertController(
		new DualPointCaretController(
			medOwner.get(),
			new MovableRectCaret( TRect(currAbsRect.Right()-4, currAbsRect.Top(), currAbsRect.Right()+5, currAbsRect.Bottom()) ),
			this,
			mousePoint,
			EID_MOUSEDRAG, 0, EID_CLOSE_EVENTS & ~EID_SCROLLED
		)
	);

	medOwner->InsertController(
		new DualPointCaretController(
			medOwner.get(),
			new MovableRectCaret( TRect(currAbsRect.Right()-2, currAbsRect.Top(), currAbsRect.Right()+3, currAbsRect.Bottom()) ),
			this,
			mousePoint,
			EID_MOUSEDRAG, 0, EID_CLOSE_EVENTS & ~EID_SCROLLED
		)
	);
	medOwner->InsertController(
		new ColumnSizerDragger(
			medOwner.get(),
			this
		)
	);
}

#include "LayerClass.h"

IMPL_DYNC_SHVCLASS(DataItemColumn, TableControl)
