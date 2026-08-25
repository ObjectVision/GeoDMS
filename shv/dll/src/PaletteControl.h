// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __SHV_PALETTECONTROL_H
#define __SHV_PALETTECONTROL_H

#include "TableControl.h"
#include "LayerInfo.h"
#include "ptr/SharedTreePtr.h"
class Theme;

//----------------------------------------------------------------------
// class  : PaletteControl
//----------------------------------------------------------------------

class PaletteControl: public TableControl
{
	typedef TableControl base_type;
public:
	PaletteControl(MovableObject* owner, GraphicLayer* layer, bool hideCount);
	PaletteControl(MovableObject* owner, AbstrDataItem* classAttr, const AbstrDataItem* themeAttr, const AbstrUnit* themeUnit, DataView* dv);
	void Init();

	const AbstrDataItem* GetLabelTextAttr() const;
	const AbstrDataItem* GetBreakAttr    () const;
	const AbstrUnit*     GetPaletteDomain() const;
	const AbstrDataItem* GetThemeAttr    () const;
	const AbstrDataItem* GetPaletteAttr  () const { return m_PaletteAttr; }  // the aspect value per class, when this legend describes a layer
	      GraphicLayer*  GetLayer        () const { return m_Layer.get(); }  // null when the palette editor was opened on a class-break attribute

//	override virtuals of GraphicObject
	void Sync(TreeItem* viewContext, ShvSyncMode sm) override;

//	override virtual of TableControl
	bool ShowOriginTextColors() const override { return false; } // legend rows describe classes, not configured items

	void CreateSelCountColumn();
	bool m_HasTriedToAddSelCountColumn = false;

protected:
	std::shared_ptr<Theme> GetActiveTheme() const;

//	override virtuals of GraphicObject
	void DoUpdateView() override;

private:
	void CreateColumns();
	void CreateColumnsImpl();
	void CreateSymbolColumnFromLayer();
	void CreateSymbolColumnFromAttr();
	void CreateColorColumn();
	void CreateLabelTextColumn();
	void CreateAreaOrLengthColumn(TreeItem* container, SharedStr exprStr);

	std::shared_ptr<GraphicLayer>  m_Layer;
	SharedDataItemInterestPtr  m_LabelTextAttr;

	SharedDataItemInterestPtr  m_ThemeAttr;
	SharedDataItemInterestPtr  m_BreakAttr;
	SharedDataItemInterestPtr  m_PaletteAttr;
	SharedDataItemInterestPtr  m_CountAttr, m_SelCountAttr;
	SharedDataItemInterestPtr  m_AreaOrLengthAttr;
	std::shared_ptr<const AbstrUnit> m_PaletteDomain;
	SharedUnitInterestPtr      m_ThemeUnit;

	DECL_RTTI(, Class)
};


#endif // __SHV_PALETTECONTROL_H

