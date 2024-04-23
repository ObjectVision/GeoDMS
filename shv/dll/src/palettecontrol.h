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

//	override virtuals of GraphicObject
	void Sync(TreeItem* viewContext, ShvSyncMode sm) override;

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

	std::shared_ptr<GraphicLayer>  m_Layer;
	SharedDataItemInterestPtr  m_LabelTextAttr;

	SharedDataItemInterestPtr  m_ThemeAttr;
	SharedDataItemInterestPtr  m_BreakAttr;
	SharedDataItemInterestPtr  m_PaletteAttr;
	SharedDataItemInterestPtr  m_CountAttr, m_SelCountAttr;
	SharedPtr<const AbstrUnit> m_PaletteDomain;
	SharedUnitInterestPtr      m_ThemeUnit;

	DECL_RTTI(SHV_CALL, Class);
};


#endif // __SHV_PALETTECONTROL_H

