// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#include "DrawContext.h"
#include "GeoTypes.h"

#include <cmath>
#include <vector>

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

// Portable transformed blit: CPU inverse-map (nearest) resample into the device-AABB, then one
// DrawImage. Coordinates are dms order (first=row/Y, second=col/X), matching GridCoord's grid2device.
void DrawContext::DrawImageTransformed(const CrdTransformation& src2device, const void* pixelData32, int srcWidth, int srcHeight, DmsRasterOp op)
{
	if (!pixelData32 || srcWidth <= 0 || srcHeight <= 0)
		return;
	auto src32 = static_cast<const UInt32*>(pixelData32);

	// device-space AABB of the transformed source rectangle (source pixel coords: first=row, second=col)
	CrdRect devDms = src2device.ApplyBounds(CrdRect(CrdPoint(0.0, 0.0), CrdPoint(double(srcHeight), double(srcWidth))));
	GRect destRect(
		GType(std::floor(devDms.first.second)),  GType(std::floor(devDms.first.first)),   // left, top   (minX, minY)
		GType(std::ceil (devDms.second.second)), GType(std::ceil (devDms.second.first))    // right, bottom(maxX, maxY)
	);
	destRect &= GetClipRect();
	if (destRect.empty())
		return;

	int destW = destRect.Width(), destH = destRect.Height();
	if (destW <= 0 || destH <= 0)
		return;
	std::vector<UInt32> destBuf(SizeT(destW) * SizeT(destH), 0x00FFFFFFu); // white = transparent under SrcAnd

	for (int dy = 0; dy != destH; ++dy)
	{
		double deviceY = double(destRect.top + dy) + 0.5;
		// DrawImage expects a bottom-up DIB (positive height), so store device row dy at buffer row destH-1-dy.
		UInt32* outRow = &destBuf[SizeT(destH - 1 - dy) * SizeT(destW)];
		for (int dx = 0; dx != destW; ++dx)
		{
			double deviceX = double(destRect.left + dx) + 0.5;
			CrdPoint srcP = src2device.Reverse(CrdPoint(deviceY, deviceX)); // -> (srcRow, srcCol)
			int r = int(std::floor(srcP.first));
			int c = int(std::floor(srcP.second));
			if (unsigned(r) < unsigned(srcHeight) && unsigned(c) < unsigned(srcWidth))
				outRow[dx] = src32[SizeT(r) * SizeT(srcWidth) + SizeT(c)];
		}
	}
	// bottom-up 32bpp DIB; reuse the backend's DrawImage so `op` (e.g. SrcAnd) applies.
	DrawImage(destRect, destBuf.data(), destW, destH, 32, nullptr, 0, op);
}
