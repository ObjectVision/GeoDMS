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


#if !defined(__SHV_SCALEBAR_H)
#define __SHV_SCALEBAR_H

#include "ShvBase.h"
#include "MovableObject.h"

//----------------------------------------------------------------------
// class  : ScaleBarBase interface
//----------------------------------------------------------------------

class ScaleBarBase
{
//	typedef MovableObject base_type;
public:
	ScaleBarBase(const ViewPort* vp);
	~ScaleBarBase();

	bool MustUpdateView() const;
	bool DoUpdateViewImpl(CrdPoint scaleFactor);
	bool Draw(HDC dc, const GRect& clientAbsRect) const;

	const ViewPort* GetViewPort() const { return m_ViewPort; }

	TPoint GetSize(Float64 subPixelFactor) const;
	TRect DetermineBoundingBox(const MovableObject* owner, CrdPoint subPixelFactors) const;

protected: 
	const ViewPort*  m_ViewPort = nullptr;
	const AbstrUnit* m_CrdUnit  = nullptr;
	CrdType          m_Factor = 0, m_MeasureSize = 0;
	Float64          m_MeasureValue = 0;
	UInt32           m_MeasureGroupCount = 0;
	SharedStr        m_UnitLabel;
};

//----------------------------------------------------------------------
// class  : ScaleBarObj interface
//----------------------------------------------------------------------

class ScaleBarObj : public MovableObject
{
	typedef MovableObject base_type;
public:
	ScaleBarObj(MovableObject* owner, const ViewPort* vp);
	~ScaleBarObj();

	void DetermineAndSetBoundingBox(TPoint currTL, TPoint currPageSize, Float64 subPixelFactor);

protected:
//	override Actor interface
	ActorVisitState VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const override;

//	override GraphicObject
	void DoUpdateView() override;
	bool Draw(GraphDrawer& d) const override;

private:
	ScaleBarBase m_Impl;
};

//----------------------------------------------------------------------
// class  : ScaleBarCaret interface
//----------------------------------------------------------------------

#include "AbstrCaret.h"

class ScaleBarCaret : public AbstrCaret
{
	typedef AbstrCaret base_type;
public:
	ScaleBarCaret(const ViewPort* vp);
	~ScaleBarCaret();

	bool DoUpdateViewImpl(CrdPoint scaleFactor) { return m_Impl.DoUpdateViewImpl(scaleFactor); }

//	override AbstrCaret
	void Move(const AbstrCaretOperator& caretOper, HDC dc) override;
	void Reverse(HDC dc, bool newVisibleState) override;

	GRect GetCurrBoundingBox() const;
	void  DetermineAndSetBoundingBox(CrdPoint scaleFactor);

private:
	ScaleBarBase m_Impl;
	GRect        m_Rect;
};

#include "CaretOperators.h"

class ScaleBarCaretOperator : public AbstrCaretOperator
{
  	void operator()(AbstrCaret*) const override;
};

#endif // __SHV_SCALEBAR_H
