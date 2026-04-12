// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#include "DrawContext.h"
#include "GeoTypes.h"

//----------------------------------------------------------------------
// DrawContext base class: portable 3D border rendering
//----------------------------------------------------------------------

static void DrawShadowEdges(DrawContext* dc, GRect rect, DmsColor light, DmsColor dark)
{
	if (rect.top >= rect.bottom || rect.left >= rect.right)
		return;
	Int32 nextLeft = rect.left + 1, nextTop = rect.top + 1;
	Int32 prevRight = rect.right - 1, prevBottom = rect.bottom - 1;

	dc->FillRect(GRect(prevRight, rect.top, rect.right, rect.bottom), dark);     // right edge
	dc->FillRect(GRect(rect.left, prevBottom, prevRight, rect.bottom), dark);     // bottom edge
	if (rect.top >= prevBottom || rect.left >= prevRight) return;
	dc->FillRect(GRect(rect.left, rect.top, prevRight, nextTop), light);          // top edge
	dc->FillRect(GRect(rect.left, nextTop, nextLeft, prevBottom), light);          // left edge
}

// 3D raised button appearance
void DrawContext::DrawButtonBorder(GRect& rect)
{
	DrawShadowEdges(this, rect, CombineRGB(227, 227, 227), CombineRGB(64, 64, 64));
	rect.Shrink(1);
	DrawShadowEdges(this, rect, CombineRGB(255, 255, 255), CombineRGB(160, 160, 160));
	rect.Shrink(1);
}

// 3D sunken/pressed appearance
void DrawContext::DrawReversedBorder(GRect& rect)
{
	DrawShadowEdges(this, rect, CombineRGB(64, 64, 64), CombineRGB(227, 227, 227));
	rect.Shrink(1);
	DrawShadowEdges(this, rect, CombineRGB(160, 160, 160), CombineRGB(255, 255, 255));
	rect.Shrink(1);
}
