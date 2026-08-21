// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

// Small drawing/theme satellites of shv, merged (2026-08): LayerClass,
// CaretOperators, RegionTower, AspectGroup, ThemeReadLocks, DrawContext.

// ==== LayerClass ====

#include "LayerClass.h"

//----------------------------------------------------------------------
// class  : LayerClass
//----------------------------------------------------------------------

static const LayerClass* s_First = 0;

LayerClass::LayerClass(
	ShvCreateFunc cFunc, 
	const Class*  baseCls, 
	TokenID       typeID, 
	AspectNrSet   possibleAspects,
	AspectNr      mainAspect,
	DimType       nrDims
)	:	ShvClass(cFunc, baseCls, typeID )
	,	m_PossibleAspects(possibleAspects)
	,	m_NrDims(nrDims)
	,	m_MainAspect(mainAspect)
{
}



IMPL_RTTI_METACLASS(LayerClass, "GRAPHIC", nullptr);



// ==== CaretOperators ====

#include "dbg/DebugCast.h"

#include "CaretOperators.h"
#include "Carets.h"

//----------------------------------------------------------------------
// class  : PointCaretOperator
//----------------------------------------------------------------------

PointCaretOperator::PointCaretOperator(GPoint point, GraphicObject* givenObject)
   :	m_StartPoint(point) 
	,	m_GivenObject(givenObject)
{}

void PointCaretOperator::operator() (AbstrCaret* caret) const
{
	caret->SetUsedObject(m_GivenObject);
	caret->SetStartPoint(m_StartPoint);
}

//----------------------------------------------------------------------
// class  : DualPointCaretOperator
//----------------------------------------------------------------------

DualPointCaretOperator::DualPointCaretOperator(GPoint start, GPoint end, GraphicObject* givenObject
)  :	PointCaretOperator(start, givenObject)
	,	m_EndPoint(end) 
{}

void DualPointCaretOperator::operator() (AbstrCaret* caret) const
{
	base_type::operator()(caret);
	debug_cast<DualPointCaret*>(caret)->SetEndPoint(m_EndPoint);
}

//----------------------------------------------------------------------
// class  : NeedleCaretOperator
//----------------------------------------------------------------------

NeedleCaretOperator::NeedleCaretOperator(
	const GPoint& start, 
	const GRect& viewRect, 
	GraphicObject* givenObject
)	:	PointCaretOperator(start, givenObject)
	,	m_ViewRect(viewRect)
{}

void NeedleCaretOperator::operator() (AbstrCaret* caret) const
{
	base_type::operator()(caret);
	debug_cast<NeedleCaret*>(caret)->SetViewRect(m_ViewRect);
}



// ==== RegionTower ====


#include "RegionTower.h"

void RegionTower::Add(Region&& rgn) // move semantics
{
	if (rgn.Empty())
		return;
	for (
		RegionCollection::iterator 
			currRgnPtr = m_Regions.begin(),
			lastRgnPtr = m_Regions.end();
		currRgnPtr != lastRgnPtr;
		++currRgnPtr
		)
	{
		if (currRgnPtr->Empty())
		{
			currRgnPtr->swap(rgn);
			dms_assert(rgn.Empty()); // post-condition
			return;
		}
		rgn |= *currRgnPtr;
		*currRgnPtr = Region();
	}
	m_Regions.push_back(std::move(rgn));
	assert(rgn.Empty()); // post-condition
}

Region RegionTower::GetResult()
{
	if (m_Regions.empty())
		return Region();
	while (m_Regions.size() > 1)
	{
		m_Regions[m_Regions.size()-2] |= m_Regions.back();
		m_Regions.pop_back();
	}
	Region rgn = std::move(m_Regions.front());
	m_Regions.pop_back();
	assert(m_Regions.empty());
	return rgn;
}


// ==== AspectGroup ====

#include "AspectGroup.h"

#include "GraphicLayer.h"
#include "MenuData.h"
#include "Theme.h"
#include "themecmd.h"

//----------------------------------------------------------------------
// AspectGroupData
//----------------------------------------------------------------------

//NO LONGER SUPPORTED: 'RotationPalette', VT_FLOAT64

void AspectGroupMenuFunc(GraphicLayer* layer, AspectGroup ag, MenuData& menuData)
{
	dms_assert(AspectArray);

	SubMenu subMenu( menuData, SharedStr(AspectGroupArray[ag].name) ); // SUBMENU
	if (ag != AG_Other)
	{
		bool isDisabled = layer->IsDisabledAspectGroup(ag);
		menuData.push_back( MenuItem(SharedStr("Visible"), std::make_unique<SubLayerCmd>(ag, isDisabled), layer, isDisabled ? 0 : MF_CHECKED) );
	}

	for (AspectNr a = AN_First; a != AN_AspectCount; a = AspectNr(a + 1))
		if (AspectArray[a].aspectGroup == ag)
		{
			auto theme = layer->GetTheme(a);

			std::unique_ptr<AbstrCmd> themeCmd;
			UInt32    flags = 0;

			if (theme)
			{
				themeCmd = std::make_unique<ThemeCmd>(
					theme->IsDisabled() 
					?	&Theme::Enable
					:	&Theme::Disable,
					a
				);
				if (!theme->IsDisabled())
					flags = MFS_CHECKED;
			}
			else 
				flags = MFS_GRAYED;
			menuData.push_back( MenuItem(SharedStr(AspectArray[a].name), std::move(themeCmd), layer, flags) );
		}
}

AspectGroupData AspectGroupArray[] =
{
	{ "Symbol",  ASE_Symbol,   AspectGroupMenuFunc },
	{ "Pen",     ASE_Pen,      AspectGroupMenuFunc },
	{ "Brush",   ASE_Brush,    AspectGroupMenuFunc },
	{ "Label",   ASE_Label,    AspectGroupMenuFunc },

	{ "Other",   ASE_Other,    AspectGroupMenuFunc },

	{ "None",    ASE_None    , NULL },
};

//----------------------------------------------------------------------
// Support func
//----------------------------------------------------------------------

TokenID g_AspectGroupIds[AG_Count];


TokenID GetAspectGroupNameID(AspectGroup ag)
{
	static_assert(AG_Count <= 32);

	assert( ag >= 0 && ag < AG_Count );
	if (! g_AspectGroupIds[ag] )
		g_AspectGroupIds[ag] = GetTokenID_mt(AspectGroupArray[ag].name);
	return g_AspectGroupIds[ag];
}



// ==== ThemeReadLocks ====

#include "ThemeReadLocks.h"

#include "act/TriggerOperator.h"
#include "AbstrDataItem.h"

#include "GraphicObject.h"
#include "Theme.h"
#include "ThemeSet.h"

//----------------------------------------------------------------------
// class  : ThemeReadLocks
//----------------------------------------------------------------------

ThemeReadLocks::~ThemeReadLocks()
{
	for (auto i=m_Themes.begin(), e=m_Themes.end(); i!=e; ++i)
	{
		dms_assert(*i);
		(*i)->ResetDataLock();
	}
}


bool ThemeReadLocks::push_back(const Theme* theme, const AbstrDataItem* adi, DrlType drlType)
{
	dms_assert(drlType != DrlType::Certain || SuspendTrigger::BlockerBase::IsBlocked()); // Callers responsibility
	dms_assert((drlType != DrlType::Suspendible) || !SuspendTrigger::DidSuspend()); // should have been acted upon

	dms_assert(theme);

	m_Themes.insert(theme);
	try {
		if (DataReadLockContainer::Add(adi, drlType))
		{
			dms_assert(!SuspendTrigger::DidSuspend());
			return true;
		}
	}
	catch (...)
	{
		if (!adi->IsFailed())
			throw; // rethrow if source of exception is unknown
	}

	dms_assert((drlType == DrlType::Suspendible && SuspendTrigger::DidSuspend()) || adi->WasFailed());
	if ( adi->WasFailed() )
	{
		theme->Fail( adi );
		dms_assert(!m_FailedTheme);
		m_FailedTheme = theme;
	}
	return false;
}

bool ThemeReadLocks::push_back(const Theme* theme, DrlType drlType)
{
	if (!theme)
		return true;

	dms_assert(drlType != DrlType::Certain || SuspendTrigger::BlockerBase::IsBlocked()); // Callers responsibility
	dms_assert((drlType != DrlType::Suspendible) || !SuspendTrigger::DidSuspend()); // should have been acted upon

	return	push_back(theme, theme->GetThemeAttr(),      drlType)
		&&	push_back(theme, theme->GetClassification(), drlType)
		&&	push_back(theme, theme->GetPaletteAttr(),    drlType);
}


bool ThemeReadLocks::push_back(const ThemeSet* ts, DrlType drlType)
{
	dms_assert(ts);
	const std::shared_ptr<Theme>* themePtrPtr = ts->m_Themes;
	const std::shared_ptr<Theme>* themePtrEnd = ts->m_Themes + AN_AspectCount;

	for (; themePtrPtr != themePtrEnd; ++themePtrPtr)
	{
		Theme* themePtr = themePtrPtr->get();
		dms_assert(!SuspendTrigger::DidSuspend()); // should have been acted upon
		if (themePtr && !push_back(themePtr, drlType))
		{
			dms_assert(SuspendTrigger::DidSuspend() || m_FailedTheme);
			return false;
		}
	}
	dms_assert(!SuspendTrigger::DidSuspend());
	return true;
}

bool ThemeReadLocks::ProcessFailOrSuspend(const GraphicObject* go) const
{
	dms_assert(go);
	if (m_FailedTheme)
	{
		go->Fail(m_FailedTheme);
		return false; // calculation complete
	}
	dms_assert(SuspendTrigger::DidSuspend());
	return true; // suspended?
}


// ==== DrawContext ====

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

// 3D raised button appearance. The outermost ring is the soft one; every further ring repeats the
// bright highlight, so nrRings == 2 draws exactly what it always drew (issue #828).
void DrawContext::DrawButtonBorder(GRect& rect, int nrRings)
{
	if (nrRings <= 0)
		return;
	DrawShadowEdges(this, rect, CombineRGB(227, 227, 227), CombineRGB(64, 64, 64));
	rect.Shrink(1);
	for (int ring = 1; ring != nrRings; ++ring)
	{
		DrawShadowEdges(this, rect, CombineRGB(255, 255, 255), CombineRGB(160, 160, 160));
		rect.Shrink(1);
	}
}

// 3D sunken/pressed appearance
void DrawContext::DrawReversedBorder(GRect& rect, int nrRings)
{
	if (nrRings <= 0)
		return;
	DrawShadowEdges(this, rect, CombineRGB(64, 64, 64), CombineRGB(227, 227, 227));
	rect.Shrink(1);
	for (int ring = 1; ring != nrRings; ++ring)
	{
		DrawShadowEdges(this, rect, CombineRGB(160, 160, 160), CombineRGB(255, 255, 255));
		rect.Shrink(1);
	}
}

// Portable transformed blit: CPU inverse-map (nearest) resample into the device-AABB, then one
// DrawImage. POINT-ORDER INDEPENDENT: src2device maps a source pixel (X=col, Y=row) to a device
// pixel (X,Y); the source buffer is row-major (src32[row*srcWidth + col]). Uses the semantic
// .X()/.Y() accessors (not raw .first/.second) so it is correct on both colrow and rowcol builds.
void DrawContext::DrawImageTransformed(const CrdTransformation& src2device, const void* pixelData32, int srcWidth, int srcHeight, DmsRasterOp op)
{
	if (!pixelData32 || srcWidth <= 0 || srcHeight <= 0)
		return;
	if (!src2device.CanReverse()) // singular (e.g. a not-yet-fitted view): nothing sensible to inverse-map
		return;
	auto src32 = static_cast<const UInt32*>(pixelData32);

	GRect clip = GetClipRect();
	auto clampD = [](double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); };

	// In-front reference: the screen-clip CENTRE (always a visible, in-front device point) back-projected into
	// this tile's source coords. Its forward-denominator sign is the in-front sign -- NOT the tile's own source
	// centre, which for a far-field tile can lie beyond the horizon and invert the sign. ApplyDenom()==1 for
	// <= Affine2D, so affine blits keep every pixel and stay byte-identical.
	const CrdType refSign = (src2device.ApplyDenom(src2device.Reverse(Center(g2dms_order<CrdType>(clip)))) >= 0) ? 1.0 : -1.0;

	// Classify the four source corners by forward-denominator (horizon) side. The forward denominator is LINEAR
	// in source coords, so the w==0 line is straight: all four corners on one side => the whole tile is on that
	// side. Wholly behind => nothing to draw. Wholly in front => the corner AABB is the exact device extent.
	// STRADDLING => the in-front part excursions towards Y=+/-inf as w->0 in the tile INTERIOR (not at a corner),
	// so the corner AABB badly underestimates the extent and would drop the near-horizon (foreground) half;
	// instead cover the whole clip and let the per-pixel skip below select the in-front pixels.
	const CrdPoint srcCorner[4] = {
		shp2dms_order<CrdType>(0.0, 0.0),
		shp2dms_order<CrdType>(double(srcWidth), 0.0),
		shp2dms_order<CrdType>(0.0, double(srcHeight)),
		shp2dms_order<CrdType>(double(srcWidth), double(srcHeight)) };
	int nFront = 0;
	for (const auto& s : srcCorner)
		if (src2device.ApplyDenom(s) * refSign > 0) ++nFront;
	if (nFront == 0)
		return; // wholly behind the horizon

	GRect destRect = clip; // straddling: cover the whole clip (per-pixel skip selects in-front pixels)
	if (nFront == 4)
	{
		// wholly in front: the AABB of the four transformed corners is exact; clamp to the clip range before the
		// int conversion so a far-but-in-front corner can't overflow GType(floor(...)).
		CrdRect devBounds = src2device.ApplyBounds(CrdRect(CrdPoint(0.0, 0.0), shp2dms_order<CrdType>(double(srcWidth), double(srcHeight))));
		devBounds = CrdRect(
			shp2dms_order<CrdType>(clampD(devBounds.first.X(),  clip.left, clip.right), clampD(devBounds.first.Y(),  clip.top, clip.bottom)),
			shp2dms_order<CrdType>(clampD(devBounds.second.X(), clip.left, clip.right), clampD(devBounds.second.Y(), clip.top, clip.bottom)));
		destRect = GRect(
			GType(std::floor(devBounds.first.X())),  GType(std::floor(devBounds.first.Y())),
			GType(std::ceil (devBounds.second.X())), GType(std::ceil (devBounds.second.Y())));
	}
	destRect &= clip;
	if (destRect.empty())
		return;

	int destW = destRect.Width(), destH = destRect.Height();
	if (destW <= 0 || destH <= 0)
		return;
	std::vector<UInt32> destBuf(SizeT(destW) * SizeT(destH), 0x00FFFFFFu); // white = transparent under SrcAnd

	// Under a PROJECTIVE (tilted) src2device a device pixel beyond the horizon inverse-maps to a source point
	// that FOLDS back into [0,srcWidth)x[0,srcHeight) (Reverse is not monotone across the horizon), so the
	// in-bounds test below would paint a mirrored copy in the "sky". Skip a pixel whose reversed source point is
	// on the far side of the horizon (opposite forward-denominator sign to the in-front reference above).

	for (int dy = 0; dy != destH; ++dy)
	{
		double deviceY = double(destRect.top + dy) + 0.5;
		// DrawImage expects a bottom-up DIB (positive height), so store device row dy at buffer row destH-1-dy.
		UInt32* outRow = &destBuf[SizeT(destH - 1 - dy) * SizeT(destW)];
		for (int dx = 0; dx != destW; ++dx)
		{
			double deviceX = double(destRect.left + dx) + 0.5;
			CrdPoint srcP = src2device.Reverse(shp2dms_order<CrdType>(deviceX, deviceY)); // -> source pixel (X=col, Y=row)
			if (src2device.ApplyDenom(srcP) * refSign <= 0.0)
				continue; // device pixel is beyond the horizon (a folded mirror) -> leave transparent
			int col = int(std::floor(srcP.X()));
			int row = int(std::floor(srcP.Y()));
			if (unsigned(col) < unsigned(srcWidth) && unsigned(row) < unsigned(srcHeight))
				outRow[dx] = src32[SizeT(row) * SizeT(srcWidth) + SizeT(col)];
		}
	}
	// bottom-up 32bpp DIB; reuse the backend's DrawImage so `op` (e.g. SrcAnd) applies.
	DrawImage(destRect, destBuf.data(), destW, destH, 32, nullptr, 0, op);
}
