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

#ifndef __SHV_GRAPHICRECT_H
#define __SHV_GRAPHICRECT_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "ScalableObject.h"
#include "DcHandle.h"

//#define DMS_SUPPORT_WINNT40
#if defined(DMS_SUPPORT_WINNT40)
#	define SHV_ALPHABLEND_NOT_SUPPORTED
#endif

//----------------------------------------------------------------------
// class  : RoiTracker
//----------------------------------------------------------------------

struct RoiTracker
{
	virtual void AdjustTargetVieport() =0;
};

//----------------------------------------------------------------------
// class  : GraphicRect
//----------------------------------------------------------------------

class GraphicRect : public ScalableObject, RoiTracker
{
private:
	typedef ScalableObject base_type;
public:
//	Constructor
	GraphicRect(ScalableObject* owner);
	~GraphicRect();

	GraphicClassFlags GetGraphicClassFlags() const override { dms_assert(!base_type::GetGraphicClassFlags()); return GCF_ClipExtents; };

	void SetROI(const CrdRect& roi);

//	override GraphicObject virtuals for size & display of GraphicObjects
	void UpdateExtents();
	void    DoUpdateView() override;
	GRect   GetBorderPixelExtents(CrdType subPixelFactor) const override;
	bool Draw(GraphDrawer& d) const override;

	bool MouseEvent(MouseEventDispatcher& med) override;
	
	void MoveWorldRect(const CrdPoint& point);

protected: // override virtuals of Actor
	ActorVisitState VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const override;

private:
	std::weak_ptr<MapControl> GetMapControl();
	std::weak_ptr<ViewPort> GetSourceVP  ();
	std::weak_ptr<ViewPort> GetTargetVP  ();

	void AdjustTargetVieport() override;
	bool DrawRect(GraphDrawer& d, const CrdRect& wr, const CrdRect& cr, GRect clientRect) const;

private: friend class LayerSet;
	GdiHandle<HPEN>                m_Pen;
	CrdRect                        m_ROI;
	mutable std::weak_ptr<ViewPort> m_SourceVP;
	mutable std::weak_ptr<ViewPort> m_TargetVP;

#if defined(SHV_ALPHABLEND_NOT_SUPPORTED)
	GdiHandle<HBRUSH>              m_Brush;
#else
	GdiHandle<HBITMAP>             m_BlendBitmap;
#endif


	DECL_RTTI(SHV_CALL, ShvClass);
};

#endif // __SHV_GRAPHICRECT_H

