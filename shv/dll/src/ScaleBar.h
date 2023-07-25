#if !defined(__SHV_SCALEBAR_H)
#define __SHV_SCALEBAR_H

#include "ShvBase.h"
#include "MovableObject.h"

//----------------------------------------------------------------------
// class  : ScaleBarBase interface
//----------------------------------------------------------------------

class ScaleBarBase
{
public:
	ScaleBarBase(const ViewPort* vp);
	~ScaleBarBase();

	bool MustUpdateView() const;
	bool DoUpdateViewImpl(CrdPoint scaleFactor);
	bool Draw(HDC dc, const GRect& clientAbsRect) const;

	const ViewPort* GetViewPort() const { return m_ViewPort; }

	TPoint GetLogicalSize() const;
	GRect DetermineBoundingBox(const MovableObject* owner, CrdPoint subPixelFactors) const;

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

	void DetermineAndSetLogicalBoundingBox(TPoint currTL, TPoint currPageSize);

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
	const ViewPort* GetViewPort() const { return m_Impl.GetViewPort(); }

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
