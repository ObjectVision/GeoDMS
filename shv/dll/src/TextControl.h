// Copyright (C) 1998-2026 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#ifndef __TEXTCONTROL_H
#define __TEXTCONTROL_H

#include "MovableObject.h"
#include "geo/color.h"

class GraphDrawer;

//----------------------------------------------------------------------
// class  : TextControl enums
//----------------------------------------------------------------------

const UInt32 DEF_TEXT_PIX_WIDTH   = 120;
const UInt32 DEF_TEXT_PIX_HEIGHT  =  20;
const UInt32 COLOR_PIX_HEIGHT = DEF_TEXT_PIX_HEIGHT - BORDERSIZE * 2;
const UInt32 COLOR_PIX_WIDTH  = DEF_TEXT_PIX_HEIGHT - BORDERSIZE * 2;

//----------------------------------------------------------------------
// class  : TextControl
//----------------------------------------------------------------------

class TextControl: public MovableObject
{
	typedef MovableObject base_type;

public:
	TextControl(
		MovableObject* owner,
		UInt32 ctrlWidth   = DEF_TEXT_PIX_WIDTH,
		UInt32 ctrlHeight  = DEF_TEXT_PIX_HEIGHT,
		CharPtr caption    = "Uninitialized",
		COLORREF textColor = CLR_INVALID, 
		COLORREF bkColor   = CLR_INVALID
	);
	GraphicClassFlags GetGraphicClassFlags() const override { return GraphicClassFlags::ClipExtents; };

	void SetWidth (UInt32 width);
	void SetHeight(UInt32 height);

	void SetText     (WeakStr caption);
	void SetTextColor(COLORREF clr);
	void SetBkColor  (COLORREF clr);
	void SetIsInverted(bool v);

	SharedStr GetCaption()    const;
	COLORREF GetColor()   const;
	COLORREF GetBkColor() const override;

//	override virtual of GraphicObject
	bool    Draw(GraphDrawer& d) const override;
	bool GetTooltipText(TooltipCollector& ttc) const override; // can be location specific

private:
	SharedStr   m_Caption;
	COLORREF m_TextColor, m_BkColor; 
protected:
	bool m_IsInverted;

	DECL_RTTI(SHV_CALL, Class);
};

//----------------------------------------------------------------------
// class  : AbstrTextEditControl
//----------------------------------------------------------------------

class AbstrTextEditControl
{
public:
	virtual bool IsEditable(AspectNr) const=0;
	virtual void SetOrgText(SizeT currRec, CharPtr newText) =0;
	virtual void SetRevBorder(bool revBorder) = 0;

	virtual void InvalidateDrawnActiveElement() =0;
	virtual std::weak_ptr<DataView> GetDataView() const=0;
	virtual SharedStr GetOrgText(SizeT recNo, GuiReadLock& lock) const = 0;

};

//----------------------------------------------------------------------
// class  : EditableTextControl
//----------------------------------------------------------------------

class EditableTextControl : public TextControl, public AbstrTextEditControl
{
	typedef TextControl base_type;
public:
	EditableTextControl(MovableObject* owner,
		bool     isEditable
	,	TType    ctrlWidth  = DEF_TEXT_PIX_WIDTH
	,	TType    ctrlHeight = DEF_TEXT_PIX_HEIGHT
	,	CharPtr  caption    = "Uninitialized"
	,	DmsColor textColor  = UNDEFINED_VALUE(UInt32)
	);

	void SetIsEditable(bool v);

protected:
//	override AbstrTextEditControl callbacks
	void InvalidateDrawnActiveElement() override;
	bool IsEditable(AspectNr a) const override;
	std::weak_ptr<DataView> GetDataView() const override { return base_type::GetDataView(); }

//	override TextControl
	bool Draw(GraphDrawer& d) const override;
	void SetActive(bool newState) override;
	bool MouseEvent(MouseEventDispatcher& med) override;
	bool OnKeyDown(UInt32 virtKey) override;

	void SetRevBorder(bool revBorder) override;

private:
	bool m_IsEditable;
};


#endif // __TEXTCONTROL_H

