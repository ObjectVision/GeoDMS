// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __SHV_LAYERCONTROL_H
#define __SHV_LAYERCONTROL_H

#include "MovableContainer.h"
#include "TableControl.h"
#include "TextControl.h"
#include "DisplayValue.h"

class Theme;
class GraphicLayer;
class LayerControl;
class PaletteControl;

//----------------------------------------------------------------------
// class  : LayerHeaderControl
//----------------------------------------------------------------------

class LayerHeaderControl : public TextControl
{
public:
	LayerHeaderControl(MovableObject* owner);

	bool MouseEvent(MouseEventDispatcher& med) override;
};

//----------------------------------------------------------------------
// class  : LayerInfoControl
//----------------------------------------------------------------------

class LayerInfoControl : public TextControl
{
public:
	LayerInfoControl(MovableObject* owner);
	bool MouseEvent(MouseEventDispatcher& med) override;
	void SetExplainableValue(WeakStr text, SharedPtr<const AbstrDataItem> themeAttr, SizeT focusID);

	void ExplainValue();

	SharedPtr<const AbstrDataItem> m_ThemeAttr;
	SizeT                          m_FocusID = 0;
};

//----------------------------------------------------------------------
// class  : LayerControlBase
//----------------------------------------------------------------------

class LayerControlBase : public GraphicVarRows
{
	typedef GraphicVarRows base_type;
public:
	LayerControlBase(MovableObject* owner, ScalableObject* layerSetElem);
	void Init();

	ScalableObject* GetLayerSetElem() const;

	virtual void SetFontSizeCategory(FontSizeCategory fid);
	FontSizeCategory GetFontSizeCategory() const override { return m_FID; }

//	override virtuals of GraphicObject
  	void SetActive(bool newState) override;
	void ToggleVisibilityAndMakeActiveIfNeeded();

	void FillMenu(MouseEventDispatcher& med) override;
	bool MouseEvent(MouseEventDispatcher& med) override;

protected:
//	override virtuals of Actor
	ActorVisitState VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const override;
	void OnLayerVisibilityChanged();

protected:
	void SetHeaderCaption(CharPtr caption);
	LayerHeaderControl* GetHeaderControl() { return m_HeaderControl.get(); }
	SharedStr GetCaption() const override { return m_HeaderControl->GetCaption(); }

private:
	virtual void OnDetailsVisibilityChanged() = 0;

private:
	ScalableObject*           m_LayerElem; // ownership must be guarded by derived class that sees the complete type
	FontSizeCategory          m_FID = FontSizeCategory::MEDIUM;

	std::shared_ptr<LayerHeaderControl>  m_HeaderControl;

	ScopedConnection m_connVisibilityChanged;
	ScopedConnection m_connDetailsVisibilityChanged;
};

//----------------------------------------------------------------------
// class  : LayerControl
//----------------------------------------------------------------------

class LayerControl : public LayerControlBase
{
	typedef LayerControlBase base_type;
public:
	LayerControl(MovableObject* owner, GraphicLayer* layer);
	~LayerControl();
	void Init();

	void TogglePaletteIsVisible();

	GraphicLayer* GetLayer() const { return m_Layer.get(); }

//	override virtuals of GraphicObject
	COLORREF GetBkColor() const override;
	void FillMenu(MouseEventDispatcher& med) override;
  	GraphVisitState InviteGraphVistor(AbstrVisitor&) override;
	void DoUpdateView() override;

//	override virtuals of Actor
	ActorVisitState VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const override;
	ActorVisitState DoUpdate(ProgressState ps) override;

	const AbstrUnit* GetPaletteDomain() const;

protected:
	void SetPaletteControl();
	void EditPalette();

private:
	void OnFocusElemChanged(SizeT selectedID, SizeT oldSelectedID);
	void OnDetailsVisibilityChanged() override;
	void SetFontSizeCategory(FontSizeCategory fid) override;

private:
	std::shared_ptr<GraphicLayer> m_Layer;

	std::shared_ptr<LayerInfoControl>     m_InfoControl;
	std::shared_ptr<PaletteControl>       m_PaletteControl;

	mutable SharedDataItemInterestPtrTuple m_LabelLocks;

	ScopedConnection                      m_connFocusElemChanged;

	DECL_RTTI(SHV_CALL, Class);
};

//----------------------------------------------------------------------
// class  : LayerControlSet
//----------------------------------------------------------------------

class LayerControlSet : public GraphicVarRows
{
	typedef GraphicVarRows base_type;
public:
	LayerControlSet(MovableObject* owner, LayerSet* layerSet);

	GraphicClassFlags GetGraphicClassFlags() const override { return GraphicClassFlags::PushVisibility| GraphicClassFlags::ClipExtents; }

//	override virtuals of GraphicObject
  	GraphVisitState InviteGraphVistor(AbstrVisitor&) override;
	void FillMenu(MouseEventDispatcher& med) override;

	LayerSet* GetLayerSet() { return m_LayerSet.get(); }
	bool HasHiddenControls() const;
	void ShowHiddenControls();

protected:
	void DoUpdateView() override;

private:
	void OnElemSetChanged();

	std::shared_ptr<LayerSet> m_LayerSet;

	ScopedConnection    m_connElemSetChanged;

	DECL_RTTI(SHV_CALL, Class);
};

//----------------------------------------------------------------------
// class  : LayerControlGroup
//----------------------------------------------------------------------

class LayerControlGroup : public LayerControlBase
{
	typedef LayerControlBase base_type;
public:
	LayerControlGroup(MovableObject* owner, LayerSet* layerSet);
	~LayerControlGroup();
	void Init(LayerSet* layerSet, CharPtr caption);

	GraphicClassFlags GetGraphicClassFlags() const override { return GraphicClassFlags::ClipExtents; }

//	override virtuals of GraphicObject
	void FillMenu(MouseEventDispatcher& med) override;
	GraphVisitState InviteGraphVistor(AbstrVisitor& av) override;

private:
	void OnDetailsVisibilityChanged() override;
	void SetFontSizeCategory(FontSizeCategory fid) override;

private:
	const LayerControlSet* GetConstControlSet() const;
	      LayerControlSet* GetControlSet();

	std::shared_ptr<LayerSet>        m_LayerSet;
	std::shared_ptr<LayerControlSet> m_Contents;

	ScopedConnection           m_connVisibilityChanged;

	DECL_RTTI(SHV_CALL, Class);
};


#endif // __SHV_LAYERCONTROL_H

