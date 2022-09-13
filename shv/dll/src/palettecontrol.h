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
	SharedDataItemInterestPtr  m_CountAttr;
	SharedPtr<const AbstrUnit> m_PaletteDomain;
	SharedUnitInterestPtr      m_ThemeUnit;

	DECL_RTTI(SHV_CALL, Class);
};


#endif // __SHV_PALETTECONTROL_H

