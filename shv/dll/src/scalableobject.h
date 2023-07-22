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
#pragma once

#ifndef __SHV_SCALABLEOBJECT_H
#define __SHV_SCALABLEOBJECT_H

#include "GraphicObject.h"

#include "ShvSignal.h"
#include "Region.h"

class LayerControlBase;

//----------------------------------------------------------------------
// class: ScalableObject
//----------------------------------------------------------------------

typedef std::shared_ptr<ScalableObject> sharedPtrSO;
typedef std::shared_ptr<const ScalableObject> sharedPtrCSO;

class ScalableObject : public GraphicObject
{
	typedef GraphicObject base_type;
public:
	ScalableObject(GraphicObject* owner);

	bool DetailsVisible() const { return  m_State.Get(SOF_DetailsVisible);   }
	bool DetailsTooLong() const { return  m_State.Get(SOF_DetailsTooLong);   }
	void ToggleDetailsVisibility();
	void SetDetailsTooLong(bool v) const { m_State.Set(SOF_DetailsTooLong, v); }

	void SetTopographic()       { m_State.Set(SOF_Topographic); m_State.Clear(GOF_Exclusive);  }
	bool IsTopographic () const { return m_State.Get(SOF_Topographic); }
	bool HasNoExtent() const { return IsTopographic();  }
	bool HasDefinedExtent() const override { return !HasNoExtent(); }

//	override GraphicObject: Size and Position
	TRect CalcFullAbsLogicalRect  (const GraphVisitor& v) const override;
	GRect GetCurrFullAbsDeviceRect(const GraphVisitor& v) const override;

	CrdRect GetCurrWorldClientRect() const;
	CrdRect CalcWorldClientRect   () const; 

	void InvalidateWorldRect(const CrdRect& rect, const TRect* borderExtentsPtr) const;

//	accessing related-entries
	const ViewPort* GetViewPort() const;
	ViewPort* GetViewPort() { return const_cast<ViewPort*>(const_cast<const ScalableObject*>(this)->GetViewPort()); }

//	override GraphicObject for accessing related entries by supplying covariant return types
	ScalableObject* GetEntry(SizeT i) override;
	const ScalableObject* GetConstEntry(SizeT i) const { return const_cast<ScalableObject*>(this)->GetEntry(i); }

//	override Actor callbacks
	void DoInvalidate() const override;

//	override GraphicObject
	void Sync(TreeItem* viewContext, ShvSyncMode sm) override;
	void OnVisibilityChanged() override;

//	define new virtuals for size and extents of GraphicObjects
	virtual TRect GetBorderLogicalExtents() const;


//	interface definition for layer control
	virtual std::shared_ptr<LayerControlBase> CreateControl(MovableObject* owner);
	virtual SharedStr         GetCaption() const;
	virtual void              FillLcMenu(MenuData& menuData);

protected:
	void SetWorldClientRect(const CrdRect& worldClientRect); // calls OnSizeChanged if extents changed

public:
	CmdSignal m_cmdDetailsVisibilityChanged;
	CmdSignal m_cmdVisibilityChanged;
private:
	CrdRect   m_WorldClientRect;

	DECL_ABSTR(SHV_CALL, Class);
};

#endif // __SHV_SCALABLEOBJECT_H

