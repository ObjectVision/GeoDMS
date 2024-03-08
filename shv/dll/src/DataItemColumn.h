// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__SHV_DIC_H)
#define __SHV_DIC_H

#include "DisplayValue.h"
#include "FontRole.h"
#include "MovableObject.h"
#include "TextControl.h"
#include "ThemeSet.h"

struct FontIndexCache;
struct FontArray;

enum class AggrMethod {
	undefined = 0,
	sum,
	min,
	max,
	first,
	last,
	asItemList,
	union_polygon,
	mean,
	sd,
	count,
	modus,
	modus_count,
	unique_count,
	bounding_box,
	diversity,
	entropy,
	any,
	all,
	frequency_table,
	nr_methods = 18
};

//----------------------------------------------------------------------
// class  : DataItemColumn
//----------------------------------------------------------------------

class DataItemColumn : public MovableObject, public ThemeSet, public AbstrTextEditControl
{
	typedef MovableObject base_type;
public:
	DataItemColumn(
		TableControl* owner,
		const AbstrDataItem* adi    = nullptr,
		AspectNrSet possibleAspects = ASE_LabelText, 
		AspectNr    activeTheme     = AN_LabelText
	);
	DataItemColumn(const DataItemColumn& src);

	GraphicClassFlags GetGraphicClassFlags() const override { return GraphicClassFlags::ClipExtents; };

	~DataItemColumn(); // hide call to dtor of SharedPtr<FontIndexCache>

	void SetAggrMethod(AggrMethod am)
	{
		m_AggrMethod = am;
		UpdateTheme();
	}
	const AbstrDataItem* GetActiveTextAttr() const;
	const AbstrDataItem* GetSrcAttr() const;

	SharedStr Caption() const;
	void SetElemWidth(UInt16 width);
	void SetElemSize(WPoint size);
	void SetElemBorder(bool hasBorder) { m_State.Set(DIC_HasElemBorder, hasBorder); }
	bool HasElemBorder() const         { return m_State.Get(DIC_HasElemBorder); }

	UInt32 ColumnNr() const    { dms_assert(IsDefined(m_ColumnNr)); return m_ColumnNr; }
	void SetColumnNr(SizeT nr) { dms_assert(IsDefined(nr)); m_ColumnNr = nr; }

	WPoint ElemSize()     const { return m_ElemSize; }
	UInt32 RowSepHeight() const;
	void   DrawElement(GraphDrawer& d, SizeT recNo, GRect absElemDeviceRect, GuiReadLockPair& locks) const;

	void  SetActiveRow(SizeT row);
	SizeT GetActiveRow() const { return m_ActiveRow; }
	void  MakeVisibleRow();
	void  SelectCol();

	void GotoRow(SizeT row);
	void GotoClipboardRow();
	void FindNextClipboardValue();
	void FindNextValue(SharedStr searchText);

	std::weak_ptr<TableControl> GetTableControl();
	std::weak_ptr<const TableControl> GetTableControl() const;

	void StartResize(MouseEventDispatcher& med);

protected:
//	override Actor callbacks
	void DoInvalidate() const override;
	ActorVisitState VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const override;

//	override GraphicObject virtuals
  	void SetActive(bool newState) override;
	void DoUpdateView() override;
	void DrawBackground(const GraphDrawer& d) const override;
	void Sync(TreeItem* viewContext, ShvSyncMode sm) override;
	GraphVisitState InviteGraphVistor(class AbstrVisitor& gv) override;
	COLORREF GetBkColor() const override;
	void SelectColor(AspectNr a);
	void SelectPenColor() { SelectColor(AN_LabelTextColor); }
	void SelectBrushColor() { SelectColor(AN_LabelBackColor); }

	void SetTransparentColor(AspectNr a);
	void SetTransparentPenColor() { SetTransparentColor(AN_LabelTextColor); }
	void SetTransparentBrushColor() { SetTransparentColor(AN_LabelBackColor); }

public:
	bool OnKeyDown(UInt32 nVirtKey) override;
	void FillMenu(MouseEventDispatcher& med) override;
	bool MouseEvent(MouseEventDispatcher& med) override;
	bool OnCommand(ToolButtonID id) override;

protected:
	void GenerateValueInfo();
	void SortAsc();
	void SortDesc();
	void Remove();
	void Ramp();
	void ToggleRelativeDisplay();

	bool    IsNumeric     () const;
	Float64 GetColumnTotal() const;
private:
	void InvalidateRelRect(CrdRect rect);

//	override AbstrTextEditControl callbacks
	void InvalidateDrawnActiveElement()              override;
	bool IsEditable(AspectNr)                  const override;
	void SetOrgText (SizeT recNo, CharPtr textData)  override;
	void SetRevBorder(bool revBorder)                override;
	std::weak_ptr<DataView> GetDataView()      const override { return base_type::GetDataView(); } // redirects AbstrTextEditControl::GetDataView()
	SharedStr GetOrgText (SizeT recNo, GuiReadLock& lock) const override;

private:
	friend class TextEditController;
	friend class TableControl;
	friend struct SelChangeInvalidatorBase;

	void UpdateTheme();

	CrdRect GetElemFullRelLogicalRect( SizeT rowNr) const;
	void  SetFocusRect();
	DataItemColumn* GetPrevControl();

	COLORREF  GetColor   (SizeT recNo, AspectNr a) const;
	DmsColor  GetOrgColor(SizeT recNo, AspectNr a) const;
	TextInfo  GetText    (SizeT recNo, SizeT maxLen, GuiReadLockPair& locks) const;
	WCHAR     GetSymbol  (SizeT recNo) const;
	HFONT     GetFont    (SizeT recNo, FontRole fr, Float64 subPixelFactor) const;

	void SetOrgColor(SizeT recNo, AspectNr a, DmsColor color);
	void RampColors(AbstrDataObject* ado, SizeT firstRow, SizeT lastRow);
	void RampValues(AbstrDataObject* ado, SizeT firstRow, SizeT lastRow);

	mutable OwningPtr<FontIndexCache> m_FontIndexCache;
	mutable OwningPtr<FontArray>      m_FontArray;

	SharedDataItemInterestPtr m_FutureSrcAttr, m_FutureAggrAttr;

	WPoint m_ElemSize;
	SizeT m_ColumnNr;
	SizeT m_ActiveRow;

	gr_elem_index m_GroupByIndex = -1;
	AggrMethod m_AggrMethod = AggrMethod::undefined;

	mutable Float64 m_ColumnTotal = 0.0;
	mutable SharedDataItemInterestPtrTuple m_DisplayInterest;

	DECL_RTTI(SHV_CALL, ShvClass);
};

#endif // __SHV_DIC_H
