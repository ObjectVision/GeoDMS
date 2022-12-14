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


#include "TableControl.h"

#include "act/ActorVisitor.h"
#include "mci/Class.h"
#include "mci/ValueClass.h"
#include "mci/ValueWrap.h"
#include "ser/FileStreamBuff.h"
#include "ser/FormattedStream.h"
#include "ser/MoreStreamBuff.h"
#include "ser/StringStream.h"
#include "utl/MySPrintF.h"

#include "LispList.h"

#include "AbstrDataItem.h"
#include "DataArray.h"
#include "DataItemClass.h"
#include "DataLockContainers.h"
#include "DataStoreManagerCaller.h"
#include "LispTreeType.h"
#include "OperationContext.h"
#include "TicInterface.h"
#include "Unit.h"
#include "UnitClass.h"

#include "Clipboard.h"
#include "DataItemColumn.h"
#include "DataView.h"
#include "FocusElemProvider.h"
#include "KeyFlags.h"
#include "TableViewControl.h"
#include "Theme.h"
#include "ThemeReadLocks.h"

#include "../res/resource.h"

//----------------------------------------------------------------------
// class  : SelChangeInvalidatorBase
//----------------------------------------------------------------------

MG_DEBUGCODE(
	UInt32 sd_SelChangeInvalidatorCount = 0;
)

SelChangeInvalidatorBase::SelChangeInvalidatorBase(TableControl* tableControl)
	:	m_TableControl(tableControl)
	,	m_OldSelRect(GetSelRect())
	,	m_OldRowRange(tableControl->m_Rows)
	,	m_OldColRange(tableControl->m_Cols)
{
	MG_DEBUGCODE( ++sd_SelChangeInvalidatorCount );
	dbg_assert(sd_SelChangeInvalidatorCount == 1);
}

SelChangeInvalidatorBase::~SelChangeInvalidatorBase()
{
	if (m_TableControl)
	{
		m_TableControl->m_Rows = m_OldRowRange;
		m_TableControl->m_Cols = m_OldColRange;
	}

	dbg_assert(sd_SelChangeInvalidatorCount == 1);
	MG_DEBUGCODE( --sd_SelChangeInvalidatorCount );
}

void SelChangeInvalidatorBase::ProcessChange(bool mustSetFocusElemIndex)
{
	if (!m_TableControl) return;
	auto dv = m_TableControl->GetDataView().lock(); if (!dv) return;

	bool equalColRange = EqualRange(m_OldColRange, m_TableControl->m_Cols);
	bool equalRowRange = EqualRange(m_OldRowRange, m_TableControl->m_Rows);
	if (equalColRange && equalRowRange)
		return;

	if (m_TableControl->IsDrawn())
	{
		TRect newSelRect = GetSelRect();
		if (m_OldSelRect != newSelRect)
		{
			Region selChange( TRect2GRect(m_OldSelRect) );
			selChange ^= Region( TRect2GRect(newSelRect) );
			dv->InvalidateRgn(selChange);
		}
	}
	m_TableControl->UpdateActiveCell(mustSetFocusElemIndex);

	if (!equalColRange)
		m_TableControl->m_cmdElemSetChanged();
	
	m_OldRowRange = m_TableControl->m_Rows;
	m_OldColRange = m_TableControl->m_Cols;

	MG_DEBUGCODE( tmp_swapper<UInt32> zzz(sd_SelChangeInvalidatorCount, 0);	);

	if (mustSetFocusElemIndex && m_TableControl->m_FocusElemProvider)
		m_TableControl->m_FocusElemProvider->SetIndex(m_TableControl->GetRecNo( m_TableControl->GetActiveRow() ) );
	m_TableControl = nullptr;
}

TRect SelChangeInvalidatorBase::GetSelRect() const
{
	dms_assert(m_TableControl);
	dms_assert(m_TableControl->m_Rows.IsDefined() == m_TableControl->m_Cols.IsDefined());
	if (!m_TableControl->IsDrawn() || !m_TableControl->m_Rows.IsDefined())
		return TRect(0, 0, 0, 0);

	// topleft corner
	const DataItemColumn* dic = m_TableControl->GetConstColumn(m_TableControl->m_Cols.m_Begin);
	TRect 
		result  = dic->GetElemFullRelRect(m_TableControl->m_Rows.m_Begin) + dic->GetCurrClientRelPos();

	// expand to bottom right corner if any direction is a strict range (aka open)
	if (!m_TableControl->m_Cols.IsClosed() || !m_TableControl->m_Rows.IsClosed())
	{
		auto DEBUG = m_TableControl->m_Cols.m_End;
		gr_elem_index DEBUG2 = DEBUG;

		dic = m_TableControl->GetConstColumn(m_TableControl->m_Cols.m_End  );
		result |= TRect(dic->GetElemFullRelRect(m_TableControl->m_Rows.m_End  ) + dic->GetCurrClientRelPos());
	}

	// clip
	result &= TRect(TPoint(0,0), m_TableControl->GetCurrClientSize());
	result += m_TableControl->GetCurrClientAbsPos();
	result &= TRect(m_TableControl->GetDrawnFullAbsRect());
	if (result.empty())
		return TRect(0, 0, 0, 0);
	
	return result;
}

//----------------------------------------------------------------------
// class  : SelChangeInvalidator
//----------------------------------------------------------------------

SelChangeInvalidator::SelChangeInvalidator(TableControl* tableControl)
	:	SelChangeInvalidatorBase(tableControl) 
	,	m_ChangeLock(tableControl)
{}


void SelChangeInvalidator::ProcessChange(bool mustSetFocusElemIndex)
{
	SelChangeInvalidatorBase::ProcessChange(mustSetFocusElemIndex);
	m_ChangeLock.ProcessChange();
}

//----------------------------------------------------------------------
// class  : TableControl
//----------------------------------------------------------------------

TableControl::TableControl(MovableObject* owner)
	:	base_type(owner)
	,	m_TableView(0)
{}

TableControl::~TableControl()
{
	if (m_FocusElemProvider)
		m_FocusElemProvider->DelTableControl(this);
}

DataItemColumn* TableControl::GetColumn(gr_elem_index i)
{
	return debug_cast<DataItemColumn*>(GetEntry(i));
}

const DataItemColumn* TableControl::GetConstColumn(gr_elem_index i) const
{
	return debug_cast<const DataItemColumn*>(GetConstEntry(i));
}

gr_elem_index TableControl::FindColumn(const AbstrDataItem* adi)
{
	for (gr_elem_index i = 0, n = NrEntries(); i != n; ++i)
	{
		DataItemColumn* dic = GetColumn(i);
		if (dic)
		{
			auto theme = dic->GetActiveTheme();
			if (theme && theme->GetActiveAttr() == adi)
				return i;
		}
	}
	return UNDEFINED_VALUE(gr_elem_index);
}

void TableControl::InsertColumn(DataItemColumn* dic)
{
	ProcessDIC(dic);
	InsertEntry(dic);
}

void TableControl::InsertColumnAt(DataItemColumn* dic, SizeT pos)
{
	ProcessDIC(dic);
	InsertEntryAt(dic, pos);
}

void TableControl::ProcessDIC(DataItemColumn* dic)
{
	dms_assert(dic);
	dic->UpdateTheme();
	auto dv = GetDataView().lock(); if (!dv) return;
	dic->ConnectSelectionsTheme(dv.get());
	const AbstrUnit* entity = dic->GetActiveEntity();
	if (m_Entity) {
		if (entity)
			m_Entity->UnifyDomain(entity, "Table Entity", "Domain of attribute", UM_Throw, 0);
	}
	else
		SetEntity( entity );
	if (dic->IsActive())
		dic->SetActive(false);
}

const AbstrUnit* TableControl::GetRowEntity() const
{
	if (m_GroupByEntity)
		return m_GroupByEntity;
	if (m_SelEntity)
		return m_SelEntity;

	dms_assert(m_Entity == nullptr);
	return nullptr;
}

SizeT TableControl::NrRows() const
{
	dms_assert(!SuspendTrigger::DidSuspend());
	auto rowEntity = GetRowEntity();
	if (!rowEntity)
		return 8;

	dbg_assert(!SuspendTrigger::DidSuspend());
	return const_cast<TableControl*>(this)->PrepareDataOrUpdateViewLater(rowEntity) ? rowEntity->GetCount() : UNDEFINED_VALUE(SizeT);
}

SizeT TableControl::GetRecNo(SizeT rowNr) const
{
	if (!IsDefined(rowNr) || rowNr >= NrRows())
		return UNDEFINED_VALUE(SizeT);

	if (m_State.Get(TCF_FlipSortOrder))
		rowNr = (NrRows() - (rowNr+1));
	if (!m_SelIndexAttr)
		return rowNr;

	if (!const_cast<TableControl*>(this)->PrepareDataOrUpdateViewLater(m_SelIndexAttr.get_ptr()))
		return UNDEFINED_VALUE(SizeT);
	PreparedDataReadLock lck(m_SelIndexAttr);
	return m_SelIndexAttr->GetRefObj()->GetValueAsUInt32(rowNr);
}

SizeT TableControl::GetRowNr(SizeT recNo) const
{
	if (!IsDefined(recNo))
		return recNo;

	if (m_SelIndexAttr)
	{
		PreparedDataReadLock lck(m_SelIndexAttr);
		if (GetRecNo(GetActiveRow())==recNo) // best guess
			return m_Rows.m_Curr;
		recNo = m_SelIndexAttr->GetRefObj()->FindPosOfSizeT(recNo);
		if (!IsDefined(recNo))
			return recNo;
	}
	if (m_State.Get(TCF_FlipSortOrder))
		recNo = (NrRows() - (recNo+1));
	return recNo;
}

SizeT TableControl::GetActiveRow() const
{
	SizeT col = GetActiveCol();
	if (!IsDefined(col))
		return UNDEFINED_VALUE(SizeT);
	return GetConstColumn(col)->GetActiveRow();
}

gr_elem_index TableControl::GetActiveCol() const
{
//	dms_assert(IsDefined(m_Cols.m_Curr));
	return m_Cols.m_Curr;
}

DataItemColumn* TableControl::GetActiveDIC()
{
	if (!m_Cols.IsDefined())
		return nullptr;
	return debug_cast<DataItemColumn*>(GetEntry(GetActiveCol()));
}

const DataItemColumn* TableControl::GetActiveDIC() const
{
	if (!m_Cols.IsDefined())
		return nullptr;
	return debug_cast<const DataItemColumn*>(GetConstEntry(GetActiveCol()));
}

void TableControl::ShowActiveCell()
{
	SizeT colNr = GetActiveCol();
	if (IsDefined(colNr))
		GetColumn( colNr )->MakeVisibleRow(); //is called by Activate
}


bool TableControl::ShowSelectedOnlyEnabled() const // override
{
	return	HasSortOptions() // disabled by PaletteEditors PaletteControl which implies full show
		&&	NrEntries()
		&&	GetConstColumn(0)->m_Themes[AN_Selections]; // first make a selection before non selected can be hidden
}


/*
index(s):    br->i
sel(e):      i ->bool
SubSet(sel): j ->i

index[SubSet(sel[index]).org]
= lookup(SubSet(sel[index]).org, index)
= lookup(SubSet(lookup(index, sel)), index)
als index=<> dan = SubSet(sel).org

sr->i
*/

void TableControl::UpdateShowSelOnly()
{
	dms_assert(
			(!ShowSelectedOnly()) 
		||	(	(NrEntries() > 0) 
			&&	GetColumn(0)->m_Themes[AN_Selections]
			)
	);

	UpdateShowSelOnlyImpl( this, 
		m_Entity.get_ptr(), m_IndexAttr,
		m_SelEntity, m_SelIndexAttr,
		(NrEntries()) ? GetColumn(0)->m_Themes[AN_Selections].get() : nullptr);

	NotifyCaptionChange();
	if (m_FocusElemProvider)
	{
		// post to separate GUI action as it can block on ItemWriteLock of domain
		m_FocusElemProvider->GetIndexParam()->PrepareDataUsage(DrlType::Certain);
		auto focusElemSetFunctor = [thisWPtr = weak_from_base<TableControl>()]() {
			auto thisSPtr = thisWPtr.lock();
			if (!thisSPtr)
				return;
			auto index = thisSPtr->m_FocusElemProvider->GetIndex();
			if (!IsDefined(index))
				return;
			auto dvSPtr = thisSPtr->GetDataView().lock();
			if (!dvSPtr)
				return;
			dvSPtr->AddGuiOper([thisWPtr, index]() {
				auto thisSPtr = thisWPtr.lock();
				if (!thisSPtr)
					return;
				thisSPtr->OnFocusElemChanged(index, UNDEFINED_VALUE(SizeT));
				}
			);
		};
		auto t = dms_task(focusElemSetFunctor);
}

}

void TableControl::GoTo(SizeT row, gr_elem_index col)
{
	dbg_assert(sd_SelChangeInvalidatorCount == 1);

	SizeT nrRows = NrRows();
	SizeT nrCols = NrEntries();

	dms_assert(row < nrRows);
	dms_assert(col < nrCols);

	MakeMin(row, nrRows-1);
	MakeMin(col, nrCols-1);

	dms_assert(!GetColumn(col)->IgnoreActivation());

	if (row == GetActiveRow() && col == GetActiveCol() && m_Rows.IsClosed() && m_Cols.IsClosed())
		return;

	m_Rows.CloseAt( row );
	m_Cols.CloseAt (col );
}

void TableControl::OnFocusElemChanged(SizeT newFE, SizeT oldFE)
{
	newFE = GetRowNr(newFE);
	if (newFE != GetActiveRow())
	{
		dbg_assert(sd_SelChangeInvalidatorCount == 0);
		if (IsDefined(newFE) && m_Cols.IsDefined())
		{
			SelChangeInvalidator sci(this);
			GoTo(newFE, m_Cols.m_Curr);
			sci.ProcessChange(false);
		}
		else
			GoRow(newFE, false);
	}
}

SharedStr TableControl::GetCaption() const
{
	const AbstrUnit* domain = GetEntity();
	if (!domain)
		return SharedStr("<UNDEFINED DOMAIN>");

	SizeT nrRows = NrRows();
	SizeT nrRecs = const_cast<TableControl*>(this)->PrepareDataOrUpdateViewLater(domain) ? domain->GetCount() : UNDEFINED_VALUE(SizeT);
	if (m_GroupByEntity)
		return mgFormat2SharedStr("%d recs in %s grouped in %d rows by %s ", nrRecs, domain->GetName(), nrRows, m_GroupByEntity->GetExpr());
	if (nrRows == nrRecs)
		return mgFormat2SharedStr("%s recs in %s", AsString(nrRecs), domain->GetName());
	return mgFormat2SharedStr("%s/%s recs in %s", AsString(nrRows), AsString(nrRecs), domain->GetName());
}

void TableControl::NotifyCaptionChange()
{
	auto dv = GetDataView().lock();
	if (dv)
		dv->OnCaptionChanged();
}

void TableControl::DoUpdateView()
{
	base_type::DoUpdateView();

	if (m_FocusElemProvider)
	{
		SizeT focusIndex = m_FocusElemProvider->GetIndex();
		SizeT activeRow  = GetActiveRow();
		if (GetRecNo(activeRow) != focusIndex)
		{
			SizeT row = GetRowNr( focusIndex );
			if (row != activeRow)
				GoRow(row, false);
		}
	}
	else
		if (!IsDefined(GetActiveRow()) && NrRows())
			GoRow(0, true);
	NotifyCaptionChange();
}

void TableControl::UpdateActiveCell(bool mustSetFocusElemIndex)
{
	if (!NrEntries())
		return;
	DataItemColumn* dic = nullptr;
	if (m_Rows.IsDefined() && m_Cols.IsDefined())
	{
		dic = GetColumn( m_Cols.m_Curr );
		dms_assert(dic);
		dic->SetActiveRow(m_Rows.m_Curr);
	}

	auto dv = GetDataView().lock(); if (!dv) return;

	if (dic)
	{
		if (IsActive())
			dv->Activate( dic );
	}
	else
		if (IsActive())
			dv->Activate( this );
}

void TableControl::GoUp(bool shift, UInt32 c)
{
	if (!m_Rows.IsDefined()) return;
	dms_assert(m_Rows.IsDefined() && m_Cols.IsDefined());

	SelChangeInvalidator sci(this);
	m_Rows.GoLeft(shift, c);
	if (!shift) m_Cols.Close();
	sci.ProcessChange(true);
}

void TableControl::GoDn(bool shift, UInt32 c)
{
	if (!m_Rows.IsDefined()) 
		return;
	dms_assert(m_Rows.IsDefined() && m_Cols.IsDefined());

	SizeT nrRows = NrRows();
	if (!nrRows || !IsDefined(nrRows))
		return;

	SelChangeInvalidator sci(this);
	m_Rows.GoRight(shift, c, nrRows-1);
	if (!shift) m_Cols.Close();
	sci.ProcessChange(true);
}

void TableControl::GoLeft(bool shift)
{
	if (!m_Rows.IsDefined()) return;
	if (!NrEntries()) return;
	dms_assert(m_Rows.IsDefined() && m_Cols.IsDefined());

	SelChangeInvalidator sci(this);
	SizeT row = GetActiveRow();
	SizeT col = GetActiveCol();
redo:
	if (col)
	{
		if (GetColumn(--col)->IgnoreActivation())
			goto redo;
		m_Cols.GoLeft(shift, GetActiveCol() - col);
		if (!shift)
			m_Rows.CloseAt(row);
	}
	else
	{
		if (shift || !row--)
			return;
		col = NrEntries();
		while (GetColumn(--col)->IgnoreActivation())
			if (!col)
				return;
		GoTo(row, col);
	}
	sci.ProcessChange(true);
}

void TableControl::GoRight(bool shift)
{
	if (!m_Rows.IsDefined()) return;
	if (!NrEntries()) return;

	dms_assert(m_Rows.IsDefined() && m_Cols.IsDefined());

	SelChangeInvalidator sci(this);
	UInt32 row = GetActiveRow();
	UInt32 col = GetActiveCol();
redo:
	if (++col < NrEntries())
	{
		if (GetColumn(col)->IgnoreActivation())
			goto redo;
		m_Cols.GoRight(shift, col - GetActiveCol(), NrEntries()-1);
		if (!shift) m_Rows.CloseAt(row);
	}
	else
	{
		if (shift || ++row >= NrRows())
			return;
		col = 0;
		while (GetColumn(col)->IgnoreActivation())
			if (++col >= NrEntries())
				return;
		GoTo(row, col);
	}
	sci.ProcessChange(true);
}

void TableControl::GoHome(bool shift, bool firstActiveCol)
{
	dms_assert(m_Rows.IsDefined() == m_Cols.IsDefined());

	if (!m_Rows.IsDefined())
	{
		shift = false;
		firstActiveCol = true;
	}

	SelChangeInvalidator sci(this);
	auto activeCol = FirstActiveCol();
	if (activeCol < NrEntries())
	{
		if (firstActiveCol)
			m_Cols.Go(shift,  activeCol);
		m_Rows.GoHome(shift);
	}
	sci.ProcessChange(true);
}

void TableControl::GoEnd(bool shift)
{
	dms_assert(m_Rows.IsDefined() == m_Cols.IsDefined());

	if (!m_Rows.IsDefined())
		shift = false;

	SelChangeInvalidator sci(this);
	m_Rows.GoEnd(shift, NrRows()-1);
	m_Cols.GoEnd(shift, NrEntries()-1);
	sci.ProcessChange(true);
}

SizeT TableControl::FirstActiveCol() const
{
	SizeT col = 0, nrCols = NrEntries();

	while (col < nrCols && const_cast<TableControl*>(this)->GetColumn(col)->IgnoreActivation())
		++col;

	return col;
}

void TableControl::GoRow(SizeT row, bool mustSetFocusElemIndex)
{
	dms_assert(m_Rows.IsDefined() == m_Cols.IsDefined());

	SelChangeInvalidator sci(this);
	m_Rows.CloseAt(row);
	if (IsDefined(row))
	{
		auto c = FirstActiveCol();
		auto n = NrEntries();
		if (c < n) {
			m_Cols.m_Begin = 0;
			m_Cols.m_Curr = c;
			m_Cols.m_End = n - 1;
			goto ready;
		}
		m_Rows.CloseAt(UNDEFINED_VALUE(SizeT));
	}
	m_Cols.CloseAt(UNDEFINED_VALUE(SizeT));

ready:
	sci.ProcessChange(mustSetFocusElemIndex);
}

FormattedOutStream& operator << (FormattedOutStream& fos, const SelRange& sr)
{
	fos << sr.m_Begin << ";" << sr.m_Curr << ";" << sr.m_End;
	return fos;
}

#include "ser/FormattedStream.h"

FormattedInpStream& operator >> (FormattedInpStream& fis, SelRange& sr)
{
	fis >> sr.m_Begin >> ";" >> sr.m_Curr >> ";" >> sr.m_End;
	return fis;
}

//	override GraphicObject virtuals for size & display of GraphicObjects
void TableControl::Sync(TreeItem* viewContext, ShvSyncMode sm)
{
	dms_assert(viewContext);
	base_type::Sync(viewContext, sm);
	if (sm == SM_Load)
	{
		SizeT n = NrEntries(); 
		while (n)
			ProcessDIC(GetColumn(--n));
	}
	if (sm==SM_Save)
	{
		VectorOutStreamBuff mosb;
		FormattedOutStream outStream(&mosb, FormattingFlags::None);
		outStream << m_Rows << ";" << m_Cols << '\0';
		
		SetViewData(viewContext, mosb.GetData());
	}
	else
	{
		dms_assert(sm==SM_Load);
		SyncIndex(sm);
		SyncGroupBy(sm);
		UpdateShowSelOnly(); // followup on base_type::Sync call to SyncShowSelOnly

		SharedStr viewData = GetViewData(viewContext);
		if (!viewData.empty())
		{
			SelChangeInvalidator sci(this);
			MemoInpStreamBuff misb(viewData.begin(), viewData.send());
			FormattedInpStream inpStream(&misb);
			inpStream >> m_Rows >> ";" >> m_Cols;
			sci.ProcessChange(true);
		}
	}
}

const UInt32 PAGE_SIZE = 10;

bool TableControl::OnKeyDown(UInt32 virtKey)
{
	if (KeyInfo::IsSpec(virtKey))
	{
		bool shift = GetKeyState(VK_SHIFT) & 0x8000;
		switch (KeyInfo::CharOf(virtKey)) {
			case VK_RIGHT:  GoRight(shift);                                return true;
			case VK_LEFT:   GoLeft(shift);                                 return true;
			case VK_TAB:    if (shift) GoLeft(false); else GoRight(false); return true;

			case VK_UP:     GoUp(shift, 1);                                return true;
			case VK_DOWN:   GoDn(shift, 1);                                return true;
			case VK_HOME:   GoHome(shift, true);                           return true;
			case VK_END:    GoEnd(shift);                                  return true;
			case VK_PRIOR:  GoUp(shift, PAGE_SIZE);                        return true;
			case VK_NEXT:   GoDn(shift, PAGE_SIZE);                        return true;
			case VK_ESCAPE: GoRow(UNDEFINED_VALUE(SizeT), true);           return true; // non-virtual key: Escape
		}
	} else if (KeyInfo::IsCtrl(virtKey))
	{
		switch (KeyInfo::CharOf(virtKey)) {
			case 'N': return OnCommand(TB_Export);
		}
	}
	return base_type::OnKeyDown(virtKey);
}

bool TableControl::OnCommand(ToolButtonID id)
{
	switch (id)
	{
		case TB_SelectAll :      SelectAllRows( true); return true;
		case TB_SelectNone:      SelectAllRows(false); return true;
		case TB_SelectRows:      SelectRows();         return true;
		case TB_ZoomSelectedObj: GoToFirstSelected();  return true;
		case TB_Export:          Export();             return true;
		case TB_TableCopy:       TableCopy();          return true;

		case TB_Info:
			return GetActiveDIC() && GetActiveDIC()->OnCommand(id);

		case TB_ShowSelOnlyOff:
			SetShowSelectedOnly(false);
			return true;
		case TB_ShowSelOnlyOn:
			if (ShowSelectedOnlyEnabled())
				SetShowSelectedOnly(true);
			id = TB_Neutral;
			[[fallthrough]];

		case TB_TableGroupBy:
		case TB_Neutral:
		{
			CreateTableGroupBy(id == TB_TableGroupBy);
			SetViewPortCursor(LoadCursor(g_ShvDllInstance, IDC_ARROW));

			//setCommand:
			auto dv = GetDataView().lock();
			if (dv) dv->m_ControllerID = id;
			return true;
		}
	}
	return base_type::OnCommand(id);
}

void TableControl::RemoveEntry(MovableObject* g)
{
	SizeT i = GetEntryPos(g);
	dms_assert(i < m_Array.size());
	base_type::RemoveEntry(g);

	if (m_Cols.m_End < i)
		return;

	if (m_Cols.IsClosed())
	{
		m_Cols = SelRange();
		m_Rows = SelRange();
		return;
	}
	dms_assert(m_Cols.m_Begin < m_Cols.m_End);
	--m_Cols.m_End;
	if (i < m_Cols.m_Begin) --m_Cols.m_Begin;
	if (i < m_Cols.m_Curr ) --m_Cols.m_Curr;
}

void TableControl::ProcessCollectionChange()
{
	base_type::ProcessCollectionChange();
	SizeT n = NrEntries(); 
	while (n)
	{
		--n;
		GetColumn(n)->SetColumnNr(n);
	}
}

void TableControl::SelectAllRows(bool v)
{
	if (NrEntries() == 0) // || !m_Rows.IsDefined())
		return;
	auto selTheme = GetColumn(0)->CreateSelectionsTheme();
	dms_assert(selTheme);
	AbstrDataItem* selThemeAttr = const_cast<AbstrDataItem*>(selTheme->GetThemeAttr());
	dms_assert(selThemeAttr);

	DataWriteLock writeLock(selThemeAttr);

	auto selData = mutable_array_cast<SelectionID>(writeLock)->GetDataWrite();

	DataArray<SelectionID>::iterator
		b = selData.begin(),
		e = selData.end();

	if (v)
		fast_fill(b, e, true);
	else
		fast_zero(b, e);
	writeLock.Commit();

	NotifyCaptionChange();
}

void TableControl::SelectRows()
{
	if (NrEntries() == 0 || !m_Rows.IsDefined())
		return;
	auto selTheme = GetColumn(0)->CreateSelectionsTheme();
	if (!selTheme)
		return;

	dms_assert(selTheme);
	AbstrDataItem* selThemeAttr = const_cast<AbstrDataItem*>(selTheme->GetThemeAttr());
	dms_assert(selThemeAttr);

	bool ctrlPressed = (GetKeyState(VK_CONTROL) & 0x8000);
	bool shftPressed = (GetKeyState(VK_SHIFT  ) & 0x8000);

	DataWriteLock writeLock(selThemeAttr, DmsRwChangeType(!(shftPressed || ctrlPressed)));

	PreparedDataReadLock  indexLock(m_IndexAttr); // lock is required in GetRecNo in inner-loop

	auto selData = mutable_array_cast<SelectionID>(writeLock)->GetDataWrite();

	DataArray<SelectionID>::iterator b = selData.begin();
	bool isSelected = !ctrlPressed;
	for (SizeT rowNr = m_Rows.m_Begin, e = m_Rows.m_End+1; rowNr!=e; ++rowNr)
		b[GetRecNo(rowNr)] = isSelected;
	writeLock.Commit();

	NotifyCaptionChange();
}

void TableControl::GoToFirstSelected()
{
	if (NrEntries() == 0)
		return;

	if (ShowSelectedOnly())
	{
		GoHome(false, true);
		return;
	}

	auto selTheme = GetColumn(0)->GetTheme(AN_Selections);
	if (!selTheme)
		return;

	const AbstrDataItem* selThemeAttr = selTheme->GetThemeAttr();
	dms_assert(selThemeAttr);
	PreparedDataReadLock selLock  (selThemeAttr);
	PreparedDataReadLock indexLock(m_SelIndexAttr);

	auto selData = const_array_cast<SelectionID>(selThemeAttr)->GetDataRead();
	auto b = selData.begin();

	for (SizeT i = 0, n = NrRows(); i!=n; ++i)
		if (SelectionID(b[GetRecNo(i)]))
		{
			SelChangeInvalidator sci(this);
			m_Rows.CloseAt(i);
			if (!m_Cols.IsDefined())
				m_Cols.m_Curr = 0;
			m_Cols.m_Begin = 0;
			m_Cols.m_End   = NrEntries()-1;
			sci.ProcessChange(true);
			break;
		}
}

void TableControl::SaveTo(OutStreamBuff* buffPtr) const
{
	SizeT k1, k2;
	SizeT n1, n2;
	if (m_Cols.IsClosed() && m_Rows.IsClosed())
	{
		n1 = 0;
		n2 = NrRows();

		k1 = 0;
		k2 = NrEntries();
	}
	else
	{
		k1 = m_Cols.m_Begin;
		k2 = m_Cols.m_End + 1;

		n1 = m_Rows.m_Begin;
		n2 = m_Rows.m_End + 1;
	}

	SizeT pk = k2 - 1;

	std::vector<const AbstrDataItem*>   itemArray; itemArray.reserve(k2 - k1);
	for (SizeT j = k1; j != k2; ++j)
	{
		const DataItemColumn* dic = GetConstColumn(j);
		if (!dic) continue;
		const AbstrDataItem* adi = dic->GetActiveAttr();
		if (adi)
			itemArray.emplace_back(adi);
	}
	std::vector<SizeT> recNos; recNos.reserve(n2 - n1);
	for (SizeT i = n1; i != n2; ++i)
		recNos.emplace_back(GetRecNo(i));
	Table_Dump(buffPtr, begin_ptr(itemArray), end_ptr(itemArray), begin_ptr(recNos), end_ptr(recNos));
}

void TableControl::TableCopy() const
{
	VectorOutStreamBuff buff;

	SaveTo(&buff);

	ClipBoard board;
	board.SetText(buff.GetData(), buff.GetDataEnd());
}

ExportInfo TableControl::GetExportInfo()
{
	ExportInfo result;
	result.m_SubPixelFactor = 1;
	result.m_ROI = CrdRect();
	if (m_Cols.IsClosed() && m_Rows.IsClosed())
	{
		result.m_DotsPerTile.Row() = NrRows();
		result.m_DotsPerTile.Col() = NrEntries();
	}
	else
	{
		result.m_DotsPerTile.Row() = m_Rows.Size() + 1;
		result.m_DotsPerTile.Col() = m_Cols.Size() + 1;
	}
	result.m_SubDotsPerDot.Row() = 1;
	result.m_SubDotsPerDot.Col() = 1;
	result.m_FullFileNameBase = GetFullFileNameBase(GetContext());
	result.m_NrTiles.Col() = 0;
	result.m_NrTiles.Row() = 1;

	return result;
}

void TableControl::Export()
{
	ExportInfo info = GetExportInfo();
	
	SharedStr fileName = mySSPrintF("%s.csv", info.m_FullFileNameBase.c_str());

	FileOutStreamBuff buff(fileName, DSM::GetSafeFileWriterArray(), true);
	SaveTo(&buff);
}

void TableControl::SetRowHeight(UInt32 height)
{
	SizeT n = NrEntries(); 
	while (n)
	{
		DataItemColumn* dic = GetColumn(--n);

		UInt32 elemHeight = height;
		if (dic->HasElemBorder())
			elemHeight -= 2*BORDERSIZE;

		GPoint elemSize(elemHeight, elemHeight);
		if (dic->GetTheme(AN_LabelText))
			elemSize.x= dic->ElemSize().x;
		dic->SetElemSize(elemSize);
	}
}

void TableControl::AddLayer(const TreeItem* viewCandidate, bool isDropped)
{
//	dms_assert(CanContain(viewCandidate)); MUTABLE
	bool hasAdminMode = HasAdminMode();

	if (!m_Entity)
		SetEntity( SHV_DataContainer_GetDomain(viewCandidate, 1, hasAdminMode) );
	dms_assert(m_Entity);

	if (!isDropped && IsDataItem(viewCandidate))
	{
		const AbstrDataItem* adi = AsDataItem(viewCandidate);
		gr_elem_index i = FindColumn(adi);
		if (IsDefined(i))
		{
			GetColumn(i)->SelectCol();
			return;
		}
	}
	for(SizeT nrCols = SHV_DataContainer_GetItemCount(viewCandidate, m_Entity, 1, hasAdminMode), colNr=0; colNr != nrCols; ++colNr)
		InsertColumn(
			make_shared_gr<DataItemColumn>(this,  SHV_DataContainer_GetItem(viewCandidate, m_Entity, colNr, 1, hasAdminMode) )().get()
		);
}

static TokenID idID = GetTokenID_st("id");

SharedPtr<AbstrDataItem> TableControl::CreateIdAttr(const AbstrUnit* domain, const AbstrDataItem* exampleAttr)
{
	dms_assert(HasSortOptions());

	if (!domain || domain->GetValueType() == ValueWrap<Void>::GetStaticClass())
		return nullptr;

	domain = GetUltimateSourceItem(domain);

	auto dv = GetDataView().lock(); if (!dv) return nullptr;

	SharedPtr<AbstrDataItem> idAttr = CreateDataItem(
		CreateDesktopContainer(dv->GetDesktopContext(), domain)
	,	idID
	,	domain 
	,	domain
	);
	idAttr->SetKeepDataState(false);
	idAttr->DisableStorage(true);


	if (domain->IsCacheItem())
	{
		dms_assert(exampleAttr);
		auto keyExpr = ExprList(token::id, ExprList(token::DomainUnit, exampleAttr->GetCheckedKeyExpr()));
		idAttr->SetDC(GetOrCreateDataController(keyExpr));
	}
	else
		idAttr->SetExpr( mySSPrintF("id(%s)", domain->GetScriptName(idAttr).c_str() ) );
	return idAttr;
}

void TableControl::AddIdColumn(const AbstrUnit* domain, const AbstrDataItem* exampleAttr)
{
	SharedPtr<AbstrDataItem> idAttr = CreateIdAttr(domain, exampleAttr);
	if ( idAttr )
	{
		idAttr->UpdateMetaInfo();
		auto column = std::make_shared<DataItemColumn>(this,  idAttr );
		column->m_State.Set(GOF_IgnoreActivation);
		InsertColumn(column.get());
	}
}

bool TableControl::CanContain(const TreeItem* viewCandidate) const
{
	bool hasAdminMode = HasAdminMode();
	const AbstrUnit* domain = m_Entity;
	if (!domain)
		domain = SHV_DataContainer_GetDomain(viewCandidate, 1, hasAdminMode);
	return domain && SHV_DataContainer_GetItemCount(viewCandidate, domain, 1, hasAdminMode);
}

auto TableControl::CreateIndex(const AbstrDataItem* attr) -> FutureData
{
	auto dv = GetDataView().lock(); if (!dv) return nullptr;
	if (attr)
		attr = GetUltimateSourceItem(attr);
	if (!attr)
		return nullptr;
/*
	SharedPtr<AbstrDataItem> indexAttr = CreateDataItem(
		CreateDesktopContainer(dv->GetDesktopContext(), attr)
		, GetTokenID_mt("Index")
		, attr->GetAbstrDomainUnit()
		, Unit<UInt32>::GetStaticClass()->CreateDefault()
	);
	indexAttr->SetKeepDataState(true);
	indexAttr->DisableStorage(true);
	indexAttr->SetDC( ExprList(GetTokenID("direct_index"), attr->GetAsLispRef() ) );
	*/
//	dms_assert(attr->mc_DC);
	auto dc = GetOrCreateDataController(ExprList(GetTokenID("direct_index"), attr->GetCheckedKeyExpr() ));
	return dc->CalcResult();
}

void TableControl::CreateTableIndex(DataItemColumn* dic, SortOrder so)
{
	dms_assert(m_Entity);

	m_IndexColumn = dic->shared_from_base<const DataItemColumn>();
	m_State.Set(TCF_FlipSortOrder, so == SO_Descending);

	UpdateTableIndex();
}

void TableControl::UpdateTableIndex()
{
	auto dic = m_IndexColumn.lock();
	auto attr = dic ? dic->GetActiveTextAttr() : nullptr;
	MG_CHECK(!attr || attr->GetAbstrDomainUnit()->UnifyDomain(GetRowEntity()));
	if (attr)
	{
		m_fd_Index = CreateIndex(attr);
		if (m_fd_Index)
			m_IndexAttr = AsDataItem(m_fd_Index->GetOld());
		else
			m_IndexAttr = {};
	} else {
		m_fd_Index = {};
		m_IndexAttr = {};
	}
	SyncIndex(SM_Save);
	InvalidateDraw();
	UpdateShowSelOnly();
}

void TableControl::CreateTableGroupBy(bool activate)
{
	auto dv = GetDataView().lock(); if (!dv) return;

	if (!m_Cols.IsDefined())
		activate = false;

	if (!activate && !m_GroupByEntity && !m_GroupByRel)
		return;

	GoHome(false, false);

	for (SizeT i = 0; i != NrEntries(); ++i)
		GetColumn(i)->m_GroupByIndex = -1;

	auto oldGroupByEntity = std::move(m_GroupByEntity); m_GroupByEntity = nullptr;
	auto oldGroupByRel    = std::move(m_GroupByRel);    m_GroupByRel= nullptr;
	if (oldGroupByEntity) oldGroupByEntity->RemoveFromConfig();
	if (oldGroupByRel) oldGroupByRel->RemoveFromConfig();

	if (activate) {
		auto dic = GetColumn(m_Cols.m_Begin);
		dic->m_GroupByIndex = 0;
		SharedStr expr = dic->m_FutureSrcAttr->GetFullName();
		if (m_Cols.m_Begin < m_Cols.m_End)
		{
			expr = mgFormat2SharedStr("String(%s)", expr);
			for (SizeT i = m_Cols.m_Begin + 1; i <= m_Cols.m_End; ++i)
			{
				auto dic2 = GetColumn(i);
				dic2->m_GroupByIndex = i - m_Cols.m_Begin;
				expr = mgFormat2SharedStr("%s + '_' + String(%s)"
					, expr
					, dic2->m_FutureSrcAttr->GetFullName()
				);
			}
		}
		SharedPtr<AbstrUnit> groupByEntity = Unit<UInt32>::GetStaticClass()->CreateUnit(GetContext(), GetTokenID_mt("GroupBy"));
		groupByEntity->DisableStorage();
		groupByEntity->SetExpr(mgFormat2SharedStr("unique(%s)", expr));
		m_GroupByEntity = groupByEntity.get_ptr();

		SharedPtr<AbstrDataItem> groupByRel = CreateDataItem(groupByEntity, GetTokenID_mt("per_Row"), GetEntity(), m_GroupByEntity);
		groupByRel->DisableStorage();
		groupByRel->SetExpr(mgFormat2SharedStr("rlookup(%s, values)", expr));
		m_GroupByRel = groupByRel.get_ptr();
	}

	for (SizeT i = 0; i != NrEntries(); ++i)
	{
		auto dic = GetColumn(i);
		try {	
			if (dic->m_State.Get(GOF_IgnoreActivation))
				dic->m_AggrMethod = AggrMethod::count;
			dic->UpdateTheme();
			dic->InvalidateView();
		}
		catch (...) {}
	}

	SyncGroupBy(SM_Save);
	UpdateTableIndex();
	InvalidateDraw();
	UpdateShowSelOnly();
}

void TableControl::SetEntity(const AbstrUnit* newDomain)
{
	if (newDomain == m_Entity)
		return;

	if (m_FocusElemProvider)
		m_FocusElemProvider->DelTableControl(this);
	m_Entity = newDomain;
	auto dv = GetDataView().lock();
	if (m_Entity && dv)
	{
		m_FocusElemProvider = GetFocusElemProvider(dv.get(), m_Entity);
		if (m_FocusElemProvider)
			m_FocusElemProvider->AddTableControl(this);
		m_LabelAttr = m_Entity->GetLabelAttr();
		UpdateShowSelOnly();
	}
	else
		m_FocusElemProvider = nullptr;

	NotifyCaptionChange();
}

void TableControl::SyncIndex(ShvSyncMode sm)
{
	SyncRef(m_IndexAttr, GetContext(), GetTokenID_mt("Index"), sm);
	SyncState(this, GetContext(), GetTokenID_mt("FlipOrder"), TCF_FlipSortOrder, false, sm);
}

void TableControl::SyncGroupBy(ShvSyncMode sm)
{
//	SyncRef(m_GroupByEntity, GetContext(), GetTokenID_mt("GroupBy"), sm);
//	SyncRef(m_GroupByRel, GetContext(), GetTokenID_mt("GroupBy_rel"), sm);
}

//	insert / delete rows
bool TableControl::CheckCardinalityChangeRights(bool doThrow)
{
	dms_assert(m_Entity);
	if (!m_Entity->IsDerivable())
		return true;
	if (doThrow)
		m_Entity->throwItemError("Cannot change a derived cardinality");
	return false;
}

void TableControl::ClassSplit()
{
	CheckCardinalityChangeRights(true);

	AbstrUnit* domain = const_cast<AbstrUnit*>(GetEntity());
	SizeT currNrFocusRows = 1 + m_Rows.m_End - m_Rows.m_Begin;
	domain->Split(m_Rows.m_Begin, currNrFocusRows);
	SelChangeInvalidator sci(this);
	m_Rows.m_End = m_Rows.m_Begin + 2*currNrFocusRows - 1;
	if (m_Rows.m_Curr != m_Rows.m_Begin)
		m_Rows.m_Curr = m_Rows.m_End;
	sci.ProcessChange(m_Rows.m_Curr != m_Rows.m_Begin);
}

void TableControl::ClassMerge()
{
	CheckCardinalityChangeRights(true);
	if (m_IndexAttr)
		m_IndexAttr->throwItemError("Cannot Merge classes of a sorted table");

	AbstrUnit* domain = const_cast<AbstrUnit*>(GetEntity());
	SelChangeInvalidatorBase sci(this);
	domain->Merge( GetRecNo(m_Rows.m_Begin+1), m_Rows.m_End - m_Rows.m_Begin);
	m_Rows.CloseAt(m_Rows.m_Begin);
	sci.ProcessChange(true);
}

ActorVisitState TableControl::VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const
{
	if (	(visitor.Visit(m_Entity.get_ptr()) == AVS_SuspendedOrFailed)
		||	(m_Entity && visitor.Visit(m_LabelAttr.get_ptr()) == AVS_SuspendedOrFailed)
		||	(visitor.Visit(m_SelEntity.get_ptr()) == AVS_SuspendedOrFailed)
		||	(visitor.Visit(m_IndexAttr.get_ptr()) == AVS_SuspendedOrFailed)
		||	(visitor.Visit(m_SelIndexAttr.get_ptr()) == AVS_SuspendedOrFailed)
		||	(visitor.Visit(m_LabelAttr.get_ptr()) == AVS_SuspendedOrFailed)
		)
		return AVS_SuspendedOrFailed;

	return base_type::VisitSuppliers(svf, visitor);
}

#include "MouseEventDispatcher.h"

bool TableControl::MouseEvent(MouseEventDispatcher& med)
{
	if ((med.GetEventInfo().m_EventID & EID_LBUTTONDOWN)  && med.m_FoundObject.get() ==  this)
	{
		GType curX = med.GetEventInfo().m_Point.x;
		// find child that is left of position
		for (SizeT i=0, n=NrEntries(); i!=n; ++i)
		{
			MovableObject* chc = GetEntry(i);
			TType x = chc->GetCurrFullAbsRect().Right();
			if ((x <= curX) && (curX < x + TType(ColSepWidth())))
			{
				debug_cast<DataItemColumn*>(chc)->StartResize(med);
				break;
			}
		}
	}
	return base_type::MouseEvent(med);
}

IMPL_RTTI_CLASS(TableControl)
