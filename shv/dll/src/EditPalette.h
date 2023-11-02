// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__SHV_EDITPALETTE_H)
#define __SHV_EDITPALETTE_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "TableViewControl.h"
#include "TextControl.h"
#include "CalcClassBreaks.h"

//----------------------------------------------------------------------
// class  : NumericEditControl
//----------------------------------------------------------------------

class NumericEditControl : public EditableTextControl
{
	typedef EditableTextControl base_type;
public:
	NumericEditControl(MovableObject* owner);

	void SetValue(Float64 v);
	Float64 GetValue() const;

protected:
//	override AbstrTextEditControl callbacks
	void      SetOrgText (SizeT recNo, CharPtr textData) override;
	SharedStr GetOrgText (SizeT recNo, GuiReadLock& lock) const override;
	bool      OnKeyDown(UInt32 nVirtKey)                 override;
	SharedStr GetAsText() const;

private:
	Float64 m_Value;
};

//----------------------------------------------------------------------
// class  : PaletteButton
//----------------------------------------------------------------------

class PaletteButton : public TextControl
{
	typedef TextControl base_type;
public:
	PaletteButton(MovableObject* owner, const AbstrUnit* paletteDomain);

	void OnSetDomain(const AbstrUnit* newDomain);

protected:
	SharedStr GetAsText() const;

private:
	WeakPtr<const AbstrUnit> m_PaletteDomain;
};

//----------------------------------------------------------------------
// class  : EditPaletteControl
//----------------------------------------------------------------------

class EditPaletteControl : public ViewControl
{
	typedef ViewControl base_type;
public:
	EditPaletteControl(DataView* dv, const AbstrDataItem* themeAttr);
	void Init(DataView* dv, GraphicLayer* layer);

	EditPaletteControl(DataView* dv);
	void Init(AbstrDataItem* classAttr, const AbstrDataItem* themeAttr, const AbstrUnit* themeUnit);
	~EditPaletteControl();

	bool CanContain(const AbstrDataItem* attr) const;

	void AddBreakColumn(AbstrDataItem* classAttr, const AbstrDataItem* themeAttr, const AbstrUnit* themeUnit);

	      PaletteControl* GetPaletteControl()       { return m_PaletteControl.get(); }
	const PaletteControl* GetPaletteControl() const { return m_PaletteControl.get(); }
	SharedStr GetCaption() const override;

protected: // override GraphicObject virtuals

	void DoUpdateView() override;
	void FillMenu(MouseEventDispatcher& med) override;
	bool OnCommand(ToolButtonID id) override;
	void Sync(TreeItem* viewContext, ShvSyncMode sm) override;
	void ProcessSize(CrdPoint viewClientSize) override;

//	override Actor virtuals
	void DoInvalidate() const override;

	void ClassifyUniqueValues ();
	void ClassifyEqualCount   ();
	void ClassifyNZEqualCount ();
	void ClassifyEqualInterval();
	void ClassifyNZEqualInterval();
	void ClassifyJenksFisher  (bool separateZero);
	void ClassifyNZJenksFisher() { ClassifyJenksFisher(true); } // separate out zero
	void ClassifyCRJenksFisher() { ClassifyJenksFisher(false); } // Continuous Range without special treatment of zero

	void ClassifyLogInterval  ();

	void ClassSplit();
	void ClassMerge();

	void UpdateCounts();

private:
	void Init();

	const AbstrUnit* GetDomain() const;
	UInt32 GetNrRequestedClasses() const;
	SizeT GetNrElements() const;
	void ApplyLimits(Float64* first, Float64* last);
	void ReLabelValues();
	void ReLabelRanges();
	void FillStdColors  ();
	void FillRampColors(const Float64* first, const Float64* last);
	void UpdateNrClasses();

public:
	CrdPoint CalcMaxSize() const override;

private:
	SharedDataItemInterestPtr           m_ThemeAttr;

	std::shared_ptr<TableViewControl>   m_TableView;
	std::shared_ptr<PaletteControl>     m_PaletteControl;

	std::shared_ptr<TextControl>        m_txtDomain;
	std::shared_ptr<PaletteButton>      m_PaletteButton;

	std::shared_ptr<TextControl>        m_txtNrClasses;
	std::shared_ptr<NumericEditControl> m_numNrClasses;

	std::shared_ptr<TextControl>        m_Line1;

	std::shared_ptr<TextControl>        m_Line2;


	mutable CountsResultType            m_SortedUniqueValueCache;
	mutable UInt32                      m_NrValues = 0;
};

//----------------------------------------------------------------------
// class  : EditPaletteView
//----------------------------------------------------------------------

#include "DataView.h"

class EditPaletteView : public DataView
{
	typedef DataView base_type;
public:
	EditPaletteView(TreeItem* viewContents, GraphicLayer*  layer,     const AbstrDataItem* themeAttr);
	EditPaletteView(TreeItem* viewContents, AbstrDataItem* classAttr, const AbstrDataItem* themeAttr, const AbstrUnit* themeUnit, ShvSyncMode sm);
	EditPaletteView(TreeItem* viewContents, ShvSyncMode sm);

	auto GetViewType() const -> ViewStyle override { return ViewStyle::tvsPaletteEdit;  }

	      EditPaletteControl* GetEditPaletteControl()       { return debug_cast<      EditPaletteControl*>(GetContents().get()); }
	const EditPaletteControl* GetEditPaletteControl() const { return debug_cast<const EditPaletteControl*>(GetContents().get()); }

//	override DataView
	void AddLayer(const TreeItem*, bool isDropped) override;
	bool CanContain(const TreeItem* viewCandidate) const override;

	DECL_RTTI(SHV_CALL, Class);
};


#endif // !defined(__SHV_EDITPALETTE_H)
