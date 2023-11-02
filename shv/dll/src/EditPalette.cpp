#include "ShvDllPch.h"

#include "EditPalette.h"

#include "mci/ValueClass.h"
#include "mci/ValueComposition.h"
#include "mci/CompositeCast.h"
#include "ser/AsString.h"
#include "utl/IncrementalLock.h"
#include "utl/mySPrintF.h"

#include "AbstrUnit.h"
#include "DataArray.h"
#include "DataController.h"
#include "TreeItemClass.h"
#include "PropFuncs.h"

#include "StgBase.h"

#include "DataView.h"
#include "DataItemColumn.h"
#include "KeyFlags.h"
#include "LayerControl.h"
#include "MenuData.h"
#include "MouseEventDispatcher.h"
#include "PaletteControl.h"
#include "ScrollPort.h"
#include "Theme.h"

//----------------------------------------------------------------------
// class  : NumericEditControl
//----------------------------------------------------------------------

NumericEditControl::NumericEditControl(MovableObject* owner)
	:	EditableTextControl(owner, true, DEF_TEXT_PIX_WIDTH / 2)
	,	m_Value(0)
{}

void   NumericEditControl::SetOrgText (SizeT recNo, CharPtr textData)
{
	dms_assert(recNo == 0);
	AssignValueFromCharPtr(m_Value, textData);
	SetText(GetAsText());
}

SharedStr NumericEditControl::GetAsText() const
{
	return AsDataStr(m_Value);
}

SharedStr NumericEditControl::GetOrgText (SizeT recNo, GuiReadLock& lock) const
{
	dms_assert(recNo == 0);
	return GetAsText();
}

bool NumericEditControl::OnKeyDown(UInt32 nVirtKey)
{
	if ( KeyInfo::IsChar(nVirtKey) )
	{
		char ch = nVirtKey;
		if (ch >= 0x20 && !(ch >= '0' && ch <= '9') && ch != '.' && ch != 'e' && ch != 'E' && ch != '+' && ch != '-')
		{
			MessageBeep(-1);
			return true;
		}
	}
	return base_type::OnKeyDown(nVirtKey);
}

void NumericEditControl::SetValue(Float64 v)
{
	if (m_Value == v)
		return;
	m_Value = v;
	SetText( GetAsText() );
}

Float64 NumericEditControl::GetValue() const
{
	return m_Value;
}

//----------------------------------------------------------------------
// class  : PaletteButton
//----------------------------------------------------------------------


const TPoint editPaletteButtonClientSize = shp2dms_order<TType>(DEF_TEXT_PIX_WIDTH*2, DEF_TEXT_PIX_HEIGHT);

PaletteButton::PaletteButton(MovableObject* owner,const AbstrUnit* paletteDomain)
	:	TextControl(owner, editPaletteButtonClientSize.X(), editPaletteButtonClientSize.Y(),  "")
	,	m_PaletteDomain(nullptr)
{
	if(paletteDomain)
		OnSetDomain(paletteDomain);
}

void PaletteButton::OnSetDomain(const AbstrUnit* newDomain)
{
	m_PaletteDomain = newDomain;
	SetText(m_PaletteDomain->GetDisplayName());
}

//----------------------------------------------------------------------
// class  : EditPaletteControl
//----------------------------------------------------------------------

EditPaletteControl::EditPaletteControl(DataView* dv, const AbstrDataItem* themeAttr)
	:	ViewControl     (dv)
	,	m_ThemeAttr     (themeAttr)
{}

void EditPaletteControl::Init(DataView* dv, GraphicLayer* layer)
{
	m_TableView = make_shared_gr<TableViewControl>(dv)(dv, "PaletteData");
	m_PaletteControl = make_shared_gr<PaletteControl>(m_TableView->GetTableScrollPort(), layer, false)();
	Init();
}

EditPaletteControl::EditPaletteControl(DataView* dv)
	:	ViewControl(dv)
{}

void EditPaletteControl::Init(AbstrDataItem* classAttr, const AbstrDataItem* themeAttr, const AbstrUnit* themeUnit)
{
	AddBreakColumn(classAttr, themeAttr, themeUnit);
}

void EditPaletteControl::AddBreakColumn(AbstrDataItem* classAttr, const AbstrDataItem* themeAttr, const AbstrUnit* themeUnit)
{
	dms_assert(!m_TableView     );
	dms_assert(!m_PaletteControl);

	auto dv = GetDataView().lock(); if (!dv) return;

	m_ThemeAttr     = themeAttr;
	m_TableView     = make_shared_gr<TableViewControl>(dv.get())(dv.get(), "PaletteData");
	m_PaletteControl= make_shared_gr<PaletteControl>  (m_TableView->GetTableScrollPort(), classAttr, themeAttr, themeUnit, dv.get())();

	Init();

}
SharedStr EditPaletteControl::GetCaption() const
{
	return mySSPrintF("PaletteEditor for %s (%s)"
		,	m_ThemeAttr ? m_ThemeAttr->GetDisplayName() : SharedStr()
		,	m_PaletteControl ? m_PaletteControl->GetCaption() : SharedStr()
		);
}

bool EditPaletteControl::CanContain(const AbstrDataItem* attr) const
{
	return false; // edit palette control can never accept another dataitem

	if (!attr)
		return false;
	if (m_PaletteControl)
		return m_PaletteControl->CanContain(attr);
	else
		return attr->IsEditable();
}

void EditPaletteControl::Init()
{
	m_txtDomain    = std::make_shared<TextControl>(this, DEF_TEXT_PIX_WIDTH / 2);
	m_txtNrClasses = std::make_shared<TextControl>(this, DEF_TEXT_PIX_WIDTH / 2);
	m_numNrClasses = std::make_shared<NumericEditControl>(this);

	m_Line1 = std::make_shared<TextControl>(this, 0,0);
	m_Line2 = std::make_shared<TextControl>(this, 0,0);

	m_TableView->SetOwner(this);
	m_TableView->SetTableControl(m_PaletteControl.get());

	m_PaletteButton = std::make_shared<PaletteButton>(this, m_PaletteControl->GetPaletteDomain());

	m_txtDomain   ->SetText(SharedStr("Domain:"));
	m_txtNrClasses->SetText(SharedStr("#Classes"));
	m_PaletteButton->SetBorder(true);
	m_numNrClasses ->SetBorder(true);

	m_Line1->SetBorder(true);
	m_Line2->SetBorder(true);

	InsertEntry(m_txtDomain.get());
	InsertEntry(m_PaletteButton.get());
	InsertEntry(m_txtNrClasses.get());
	InsertEntry(m_numNrClasses.get());
	InsertEntry(m_Line1.get());
	InsertEntry(m_TableView.get());
	InsertEntry(m_Line2.get());

/*
	auto dv = GetDataView().lock();
	if (dv)
		SetClientSize(TPoint(dv->ViewLogicalRect().Size()));
*/

}

void EditPaletteControl::ProcessSize(CrdPoint viewClientSize)
{
	if (!m_PaletteButton)
		return;

	auto clientRect = CrdRect(Point<CrdType>(0,0), viewClientSize);

	m_txtDomain    ->MoveTo(shp2dms_order<CrdType>(0, BORDERSIZE));
	m_PaletteButton->MoveTo(shp2dms_order<CrdType>(BORDERSIZE, 0) + TopRight(m_txtDomain    ->GetCurrClientRelLogicalRect())); m_PaletteButton->SetClientSize(editPaletteButtonClientSize);
	m_txtNrClasses ->MoveTo(shp2dms_order<CrdType>(BORDERSIZE, 0) + TopRight(m_PaletteButton->GetCurrClientRelLogicalRect()));
	m_numNrClasses ->MoveTo(shp2dms_order<CrdType>(BORDERSIZE, 0) + TopRight(m_txtNrClasses ->GetCurrClientRelLogicalRect()));


	clientRect.first .Y() += editPaletteButtonClientSize.Y() + 3*BORDERSIZE;
	clientRect.second.Y() -= editPaletteButtonClientSize.Y() + 3*BORDERSIZE; 

	MakeMin(clientRect.second.Y(), viewClientSize.Y());
	MakeMin(clientRect.first.Y(), clientRect.second.Y());

	if (clientRect.first.X() < clientRect.second.X()) ++clientRect.first .X();
	if (clientRect.first.X() < clientRect.second.X()) --clientRect.second.X();

	m_Line1->SetClientRect(CrdRect(TopLeft   (clientRect), TopRight   (clientRect)));
	m_Line2->SetClientRect(CrdRect(BottomLeft(clientRect), BottomRight(clientRect)));

	clientRect.first.Y() += BORDERSIZE;
	clientRect.second.Y() -= BORDERSIZE;

	MakeMin(clientRect.second.Y(), viewClientSize.Y());
	MakeMin(clientRect.first .Y(), clientRect.second.Y());

	if (clientRect.first .X() > 0                 ) --clientRect.first.X();
	if (clientRect.second.X() < viewClientSize.X()) ++clientRect.second.X();

	m_TableView->SetClientRect(clientRect);
}

void EditPaletteControl::DoUpdateView() 
{
	UpdateNrClasses();
}

EditPaletteControl::~EditPaletteControl()
{}

void EditPaletteControl::DoInvalidate () const
{
	base_type::DoInvalidate();

	dms_assert(!m_State.HasInvalidationBlock());

	m_SortedUniqueValueCache = {};

	m_State.Clear(PCF_CountsUpdated);

	dms_assert(DoesHaveSupplInterest() || !GetInterestCount());
}

bool EditPaletteControl::OnCommand(ToolButtonID id)
{
	return base_type::OnCommand(id) || m_TableView->OnCommand(id);
}

void EditPaletteControl::UpdateCounts()
{
	if (m_State.Get(PCF_CountsUpdated))
		return;
	dms_assert(m_SortedUniqueValueCache.first.empty());

	const AbstrDataItem* adi = m_ThemeAttr;
	if (adi && adi->GetAbstrValuesUnit()->GetValueType()->IsNumeric() && adi->GetValueComposition() == ValueComposition::Single)
		m_SortedUniqueValueCache = PrepareCounts(adi, MAX_PAIR_COUNT);
	else
		m_SortedUniqueValueCache = {};
	m_State.Set(PCF_CountsUpdated);
}

void EditPaletteControl::FillMenu(MouseEventDispatcher& med)
{
	UpdateCounts();

	SizeT k = GetNrRequestedClasses();
	SizeT m = m_SortedUniqueValueCache.first.size();
	SizeT maxK = GetDomain()->GetValueType()->GetMaxValueAsFloat64();

	if (m && m_PaletteControl->GetBreakAttr())
	{
		SubMenu subMenu(med.m_MenuData, "Classify "+m_ThemeAttr->GetDisplayName()); // SUBMENU
		med.m_MenuData.push_back( MenuItem(SharedStr("Unique Values"),          make_MembFuncCmd(&EditPaletteControl::ClassifyUniqueValues ), this, (m<=maxK)   ?0:MFS_GRAYED) );
		med.m_MenuData.push_back( MenuItem(SharedStr("Equal Counts"),           make_MembFuncCmd(&EditPaletteControl::ClassifyEqualCount   ), this, (m>1)       ?0:MFS_GRAYED) );
		med.m_MenuData.push_back( MenuItem(SharedStr("Equal Interval"),         make_MembFuncCmd(&EditPaletteControl::ClassifyEqualInterval), this, (m>1 && k>1)?0:MFS_GRAYED) );
		med.m_MenuData.push_back( MenuItem(SharedStr("JenksFisher non-zero"),   make_MembFuncCmd(&EditPaletteControl::ClassifyNZJenksFisher), this, (m > k && k > 1) ? 0 : MFS_GRAYED));
		med.m_MenuData.push_back( MenuItem(SharedStr("JenksFisher"),            make_MembFuncCmd(&EditPaletteControl::ClassifyCRJenksFisher), this, (m>k && k>1)?0:MFS_GRAYED) );
		med.m_MenuData.push_back( MenuItem(SharedStr("Logarithminc Intervals"), make_MembFuncCmd(&EditPaletteControl::ClassifyLogInterval  ), this, (m>1 && k>1 && m_SortedUniqueValueCache.first[0].first >= 0)?0:MFS_GRAYED) );
	}
	auto dic = med.m_MenuData.m_DIC.lock();
	if (dic)
	{		
		const AbstrUnit* du = GetDomain();
		if (du && !du->IsDerivable())
		{
			SubMenu subMenu(med.m_MenuData, SharedStr("Classes")); // SUBMENU
			med.m_MenuData.push_back(MenuItem(SharedStr("Split"), make_MembFuncCmd(&EditPaletteControl::ClassSplit), this, m_PaletteControl->m_Rows.IsDefined() ? 0 : MFS_GRAYED));
			med.m_MenuData.push_back(MenuItem(SharedStr("Merge"), make_MembFuncCmd(&EditPaletteControl::ClassMerge), this, m_PaletteControl->m_Rows.IsDefined() ? 0 : MFS_GRAYED));
		}
		if (dic->GetActiveAttr() && dic->GetActiveAttr() == m_PaletteControl->GetLabelTextAttr())
			med.m_MenuData.push_back( MenuItem(SharedStr("ReLabel"), make_MembFuncCmd(&EditPaletteControl::ReLabelRanges), this, 0) );
	}
	base_type::FillMenu(med);
}

void EditPaletteControl::Sync(TreeItem* viewContext, ShvSyncMode sm)
{
	if (sm == SM_Load)
	{
		dms_assert(!m_TableView);
		AddBreakColumn(0, 0, 0);
		dms_assert(m_TableView);
	}
	if (m_PaletteControl)
	{
		m_PaletteControl->Sync(viewContext, sm);
		if (sm == SM_Load)
		{
			m_PaletteButton->OnSetDomain(m_PaletteControl->GetEntity());
			m_ThemeAttr = m_PaletteControl->GetThemeAttr();
		}
	}
}

void EditPaletteControl::ClassSplit() 
{
	m_PaletteControl->ClassSplit();
	UpdateNrClasses();
}

void EditPaletteControl::ClassMerge() 
{
	m_PaletteControl->ClassMerge();
	UpdateNrClasses();
}

CrdPoint EditPaletteControl::CalcMaxSize() const
{
	CrdPoint buttonSize    = ConcatHorizontal(m_txtDomain   ->CalcMaxSize(), m_PaletteButton->CalcMaxSize());
	CrdPoint nrClassesSize = ConcatHorizontal(m_txtNrClasses->CalcMaxSize(), m_numNrClasses     ->CalcMaxSize());
	buttonSize = ConcatHorizontal(buttonSize, nrClassesSize);
	buttonSize.Y() *= 2;
	buttonSize.Y() += 4*BORDERSIZE;
	return ConcatVertical(
		buttonSize,
		m_TableView->CalcMaxSize()
	);
}

UInt32 EditPaletteControl::GetNrRequestedClasses() const
{
	return m_numNrClasses->GetValue();
}

SizeT EditPaletteControl::GetNrElements() const
{
	dms_assert(m_State.Get(PCF_CountsUpdated));
	return m_SortedUniqueValueCache.first.m_Total;
}

void EditPaletteControl::ClassifyUniqueValues ()
{
	m_PaletteControl->CheckCardinalityChangeRights(true);
	UpdateCounts();

	SizeT m    = m_SortedUniqueValueCache.first.size();
	SizeT maxK = GetDomain()->GetValueType()->GetMaxValueAsFloat64();

	dms_assert(m); // PRECONDITION FOR MenuItem
	if (m && (m-1) > maxK)
		GetDomain()->throwItemErrorF(
			"#Unique values = %d, which is too many for the palette domain of type %s which allows a maximum of %d classes",
			m, 
			GetDomain()->GetValueType()->GetName(), 
			maxK
		);

	if (m != GetDomain()->GetCount())
	{
		m_PaletteControl->CheckCardinalityChangeRights(true);
		const_cast<AbstrUnit*>(GetDomain())->SetCount(m);
	}

	::ClassifyUniqueValues(const_cast<AbstrDataItem*>(m_PaletteControl->GetBreakAttr()), m_SortedUniqueValueCache.first, m_SortedUniqueValueCache.second);

	ReLabelValues();
	UpdateNrClasses();
	FillStdColors();
}

void EditPaletteControl::ClassifyEqualCount()
{
	UpdateCounts();

	UInt32 k = GetNrRequestedClasses();
	SizeT  n = GetNrElements();
	SizeT  m = m_SortedUniqueValueCache.first.size();
	dms_assert(m>1);
	MakeMin(k, m);
	if (k != GetDomain()->GetCount())
	{
		m_PaletteControl->CheckCardinalityChangeRights(true);
		const_cast<AbstrUnit*>(GetDomain())->SetCount(k);
	}
	dms_assert(k<=m); // follows from previous if+assignment
	dms_assert(m<=n); // #unique value (X) <= #X
	dms_assert(k<=n); // follows from previous asserts

	AbstrDataItem*   breakAttr = const_cast<AbstrDataItem*>(m_PaletteControl->GetBreakAttr());
	dms_assert(breakAttr);
	dms_assert(breakAttr->HasInterest());

	auto ba = ::ClassifyEqualCount(breakAttr, m_SortedUniqueValueCache.first, m_SortedUniqueValueCache.second);

	ReLabelRanges();
	UpdateNrClasses();
	FillRampColors(begin_ptr(ba), end_ptr(ba));
}

void EditPaletteControl::ClassifyEqualInterval()
{
	UpdateCounts();

	UInt32 k = GetNrRequestedClasses();

	if (k != GetDomain()->GetCount())
	{
		m_PaletteControl->CheckCardinalityChangeRights(true);
		const_cast<AbstrUnit*>(GetDomain())->SetCount(k);
	}

	AbstrDataItem* breakAttr = const_cast<AbstrDataItem*>(m_PaletteControl->GetBreakAttr());
	assert(breakAttr);
	assert(breakAttr->HasInterest());

	auto ba = ::ClassifyEqualInterval(breakAttr, m_SortedUniqueValueCache.first, m_SortedUniqueValueCache.second);

	ReLabelRanges();
	UpdateNrClasses();
	FillRampColors(begin_ptr(ba), end_ptr(ba));
}

void EditPaletteControl::ClassifyLogInterval  ()
{
	UpdateCounts();

	UInt32 k = GetNrRequestedClasses();
	SizeT  m = m_SortedUniqueValueCache.first.size();
	dms_assert(m>1); // PRECONDITION: Data available
	dms_assert(k>1); // PRECONDITION

	break_array ba; 

	::ClassifyLogInterval(ba, k, m_SortedUniqueValueCache.first);
	ApplyLimits(begin_ptr(ba), end_ptr(ba));
	UpdateNrClasses();
}

void EditPaletteControl::ClassifyJenksFisher(bool separateZero)
{
	UpdateCounts();

	UInt32 k = GetNrRequestedClasses();
	SizeT  n = GetNrElements();
	SizeT  m = m_SortedUniqueValueCache.first.size();
	dms_assert(m>1);
	MG_CHECK(m > k); // PRECONDITION

	if (k != GetDomain()->GetCount())
	{
		m_PaletteControl->CheckCardinalityChangeRights(true);
		const_cast<AbstrUnit*>(GetDomain())->SetCount(k);
	}
	dms_assert(k< m); // follows from previous if+assignment
	dms_assert(m<=n); // #unique value (X) <= #X
	dms_assert(k< n); // follows from previous asserts

	AbstrDataItem*   breakAttr = const_cast<AbstrDataItem*>(m_PaletteControl->GetBreakAttr());
	dms_assert(breakAttr);
	dms_assert(breakAttr->HasInterest());

	auto ba = ::ClassifyJenksFisher(breakAttr, m_SortedUniqueValueCache.first, m_SortedUniqueValueCache.second, separateZero);

	ReLabelRanges();
	UpdateNrClasses();
	FillRampColors(begin_ptr(ba), end_ptr(ba));
}

void EditPaletteControl::ApplyLimits(Float64* first, Float64* last)
{
	UInt32 k = last-first;
	if (k != GetDomain()->GetCount())
	{
		m_PaletteControl->CheckCardinalityChangeRights(true);
		const_cast<AbstrUnit*>(GetDomain())->SetCount(k);
	}

	AbstrDataItem* breakAttr = const_cast<AbstrDataItem*>(m_PaletteControl->GetBreakAttr());
	dms_assert(breakAttr);
	dms_assert(breakAttr->HasInterest());

	DataWriteLock breakLock(breakAttr);
	AbstrDataObject* breakObj = breakLock.get();
	breakObj->SetValuesAsFloat64Array(tile_loc(0, 0), k, first);
	breakLock.Commit();

	FillRampColors(first, last);
	ReLabelRanges();
}

void EditPaletteControl::ReLabelValues()
{
	const AbstrDataItem* breakAttr = m_PaletteControl->GetBreakAttr();
	AbstrDataItem* labelAttr = const_cast<AbstrDataItem*>(m_PaletteControl->GetLabelTextAttr());

	SizeT k = GetDomain()->GetCount();

	PreparedDataReadLock  breakLock(breakAttr);
	DataWriteLock labelLock(labelAttr);
	const AbstrDataObject* breakObj = breakAttr->GetRefObj();
	GuiReadLock lockHolder;
	for (SizeT j =0; j != k; ++j)
		labelLock->SetValue<SharedStr>(j, breakObj->AsString(j, lockHolder));
	labelLock.Commit();
}

void EditPaletteControl::ReLabelRanges()
{
	AbstrDataItem* labelAttr = const_cast<AbstrDataItem*>(m_PaletteControl->GetLabelTextAttr());
	if (!labelAttr)
		return;

	const AbstrDataItem* breakAttr = m_PaletteControl->GetBreakAttr();

	UInt32 k = GetDomain()->GetCount();
	if (!k)
		return;

	auto breakIndex = m_PaletteControl->CreateIndex(breakAttr);
	

	PreparedDataReadLock  breakLock(breakAttr);
	DataReadLock  breakIndexLock(AsDataItem(GetItem(breakIndex)));
	DataWriteLock labelLock(labelAttr);

	SharedStr prevBreakStr;
	UInt32 prevJJ;
	GuiReadLock lock;
	for (UInt32 j =0; j != k; ++j)
	{
		UInt32 jj = (breakIndex)
			? breakIndexLock->GetValue<UInt32>(j)
			:	j;

		SharedStr breakStr = breakLock->AsString(jj, lock);
		if (j>0)
			labelLock->SetValue<SharedStr>(prevJJ, prevBreakStr + " ... " + breakStr);
		prevBreakStr = breakStr;
		prevJJ       = jj;
	}
	labelLock->SetValue<SharedStr>(prevJJ, ">= " + prevBreakStr);

	labelLock.Commit();
}


void EditPaletteControl::FillStdColors()
{
	const AbstrUnit* domain = GetDomain();
	UInt32 n = domain->GetCount();
	UInt32 k = domain->GetNrDataItemsOut();
	while (k)
	{
		AbstrDataItem* colorAttr = const_cast<AbstrDataItem*>(domain->GetDataItemOut(--k));
		if (IsColorAspectNameID(TreeItem_GetDialogType(colorAttr)))
		{
			DataWriteLock dwl(colorAttr);
			DataArray<UInt32>* colorData = mutable_array_cast<UInt32>(dwl);
			UInt32* colorArray = colorData->GetDataWrite().begin();
			for (UInt32 i = 0; i !=n; ++i)
				colorArray[i] = STG_Bmp_GetDefaultColor(i & 0xFF);
			dwl.Commit();
		}
	}
}

void EditPaletteControl::FillRampColors(const Float64* first, const Float64* last)
{
	auto dv = GetDataView().lock();
	if (dv)
		CreatePaletteData(dv.get(), GetDomain(), AN_BrushColor, true, true, first, last);
}

void EditPaletteControl::UpdateNrClasses()
{
	if(!m_numNrClasses)
		return;

	const AbstrUnit* domain = GetDomain();
	if (!domain)
		return;
	if (!PrepareDataOrUpdateViewLater(domain))
		return;
	assert(IsDataReady(domain->GetUltimateItem()) || domain->WasFailed() || domain->GetUltimateItem()->WasFailed());
	if (!IsDataReady(domain->GetUltimateItem()))
		return;

	m_numNrClasses->SetValue( domain->GetCount() );
}

const AbstrUnit* EditPaletteControl::GetDomain() const
{
	if (!m_PaletteControl)
		return nullptr;
	return m_PaletteControl->GetEntity(); 
}

//----------------------------------------------------------------------
// class  : EditPaletteView
//----------------------------------------------------------------------

EditPaletteView::EditPaletteView(TreeItem* viewContents, GraphicLayer* layer, const AbstrDataItem* themeAttr)
	:	DataView(viewContents)
{}

EditPaletteView::EditPaletteView(TreeItem* viewContents, AbstrDataItem* classAttr, const AbstrDataItem* themeAttr, const AbstrUnit* themeUnit, ShvSyncMode sm)
	:	DataView(viewContents)
{}

EditPaletteView::EditPaletteView(TreeItem* viewContents, ShvSyncMode sm)
	:	DataView(viewContents)
{}

void EditPaletteView::AddLayer(const TreeItem* ti, bool isDropped)
{ 
//	dms_assert(CanContain(ti)); MUTABLE
	dms_assert(IsDataItem(ti)); // implied by CanContain
	if (AsDataItem(ti)->HasConfigData())
		GetEditPaletteControl()->AddBreakColumn(const_cast<AbstrDataItem*>(AsDataItem(ti)), 0, 0);
} 

bool EditPaletteView::CanContain(const TreeItem* viewCandidate) const
{
	dms_assert(GetEditPaletteControl());
	return GetEditPaletteControl()->CanContain(AsDynamicDataItem(viewCandidate));
}

IMPL_RTTI_CLASS(EditPaletteView);

//----------------------------------------------------------------------

void CreateEditPaletteMdiChild(GraphicLayer* layer, const AbstrDataItem* themeAttr)
{
	dms_assert(themeAttr);
	dms_assert(layer);

	auto dv = layer->GetDataView().lock(); if (!dv) return;
	std::weak_ptr<GraphicLayer> layer_wp = layer->shared_from_base<GraphicLayer>();
	SharedPtr< const AbstrDataItem> themeAttrSPtr = themeAttr;
	dv->AddGuiOper([layer_wp, themeAttrSPtr]() {
		std::shared_ptr<GraphicLayer> layer = layer_wp.lock(); if (!layer) return;
		auto dv = layer->GetDataView().lock(); if (!dv) return;
		TreeItem* desktopItem = dv->GetDesktopContext();
		TreeItem* viewContext = desktopItem->CreateItem(UniqueName(desktopItem, EditPaletteView::GetStaticClass()));
		dms_assert(viewContext);

		auto editPaletteView = std::make_shared<EditPaletteView>(viewContext, layer.get(), themeAttrSPtr);
		editPaletteView->SetContents(make_shared_gr<EditPaletteControl>(editPaletteView.get(), themeAttrSPtr)(editPaletteView.get(), layer.get()), SM_Save);

		SharedStr caption = "PaletteEditor for " + themeAttrSPtr->GetDisplayName();

		if (editPaletteView->CreateMdiChild(tvsPaletteEdit, caption.c_str()))
			Keep(editPaletteView);
		}
	);
}

#include "ShvDllInterface.h"
#include "dbg/DmsCatch.h"
#include "dbg/DebugContext.h"

const AbstrDataItem* DMS_CONV SHV_EditPaletteView_Create(TreeItem* desktopItem, TreeItem* classItem, const TreeItem* themeItem)
{
	DMS_CALL_BEGIN

		CDebugContextHandle hnd("EditPaletteView", "Create", false);

		AbstrDataItem*       classAttr = AsDynamicDataItem(classItem);
		const AbstrDataItem* themeAttr = AsDynamicDataItem(themeItem);
		const AbstrUnit*     themeUnit = AsDynamicUnit    (themeItem);

		if (themeAttr && !themeUnit) 
			themeUnit = themeAttr->GetAbstrValuesUnit();
		if (classAttr && !themeUnit) 
			themeUnit = classAttr->GetAbstrValuesUnit();
		if (!themeItem && themeUnit)
			themeItem = themeUnit;
		if (!classAttr && themeUnit && HasCdfProp(themeUnit) )
			classAttr = const_cast<AbstrDataItem*>(GetCdfAttr(themeUnit));

		dms_assert(themeItem);

		if (classItem && !classAttr) classItem->throwItemError("ClassItem is not a DataItem");
		if (themeItem && !themeAttr && !themeUnit) themeItem->throwItemError("ThemeItem is not a DataItem");

		dms_assert(desktopItem);
		dms_assert(themeAttr || classAttr || themeUnit);

		TreeItem* viewContext = desktopItem->CreateItem(UniqueName(desktopItem, EditPaletteView::GetStaticClass()));
		dms_assert(viewContext);

		std::shared_ptr<EditPaletteView> editPaletteView = std::make_shared<EditPaletteView>(viewContext, classAttr, themeAttr, themeUnit, SM_Save);

		SharedStr caption ="PaletteEditor for " + themeItem->GetDisplayName();

		if (editPaletteView->CreateMdiChild(tvsPaletteEdit, caption.c_str()) )
		{
			editPaletteView->SetContents(make_shared_gr<EditPaletteControl>(editPaletteView.get())(classAttr, themeAttr, themeUnit), SM_Save);
			const AbstrDataItem* adi = editPaletteView->GetEditPaletteControl()->GetPaletteControl()->GetBreakAttr();
			Keep(editPaletteView);
			return adi;
		}

	DMS_CALL_END

	return nullptr;
}
