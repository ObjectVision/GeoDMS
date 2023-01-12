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
#include "ShvDllPch.h"

#include "FeatureLayer.h"
#include "DrawPolygons.h"

#include "rtcTypeLists.h"
#include "act/TriggerOperator.h"
#include "act/UpdateMark.h"
#include "dbg/Debug.h"
#include "mci/CompositeCast.h"
#include "mci/ValueClass.h"
#include "mci/ValueClassID.h"
#include "geo/Conversions.h"
#include "geo/Area.h"
#include "geo/IsInside.h"
#include "geo/PointOrder.h"
#include "geo/CentroidOrMid.h"
#include "geo/MinMax.h"
#include "utl/IncrementalLock.h"

#include "AbstrUnit.h"
#include "DataArray.h"
#include "UnitProcessor.h"

#include "StgBase.h"

#include "AbstrCmd.h"
#include "AbstrController.h"
#include "AspectGroup.h"
#include "BoundingBoxCache.h"
#include "CounterStacks.h"
#include "DataView.h"
#include "DcHandle.h"
#include "FontIndexCache.h"
#include "FontRole.h"
#include "IndexCollector.h"
#include "PenIndexCache.h"
#include "GraphVisitor.h"
#include "LayerClass.h"
#include "MenuData.h"
#include "ShvUtils.h"
#include "TextControl.h"
#include "Theme.h"
#include "ThemeReadLocks.h"
#include "ThemeValueGetter.h"

//----------------------------------------------------------------------
// struct  : FeatureDrawer
//----------------------------------------------------------------------

FeatureDrawer::FeatureDrawer(const FeatureLayer* layer, GraphDrawer&  drawer)
	:	m_Layer                ( layer  )
	,	m_Drawer               ( drawer )
	,	m_WorldZoomLevel       ( drawer.GetTransformation().ZoomLevel() )
	,	m_IndexCollector       ( layer->GetIndexCollector(), no_tile )
	,	m_SelValues            ( )
{
	if (layer->GetEnabledTheme(AN_Selections))
		m_SelValues = const_array_cast<SelectionID>(layer->GetTheme(AN_Selections)->GetThemeAttr())->GetLockedDataRead();

	dms_assert(!SuspendTrigger::DidSuspend());
}

bool FeatureDrawer::HasLabelText() const
{
	return m_Layer->GetEnabledTheme(AN_LabelText ).get();
}

const FontIndexCache* FeatureDrawer::GetUpdatedLabelFontIndexCache() const
{
	const FontIndexCache* result = m_Layer->GetFontIndexCache(FR_Label);
	dms_assert(result);
	dms_assert(m_Drawer.DoDrawData());
	result->UpdateForZoomLevel(m_WorldZoomLevel, m_Drawer.GetSubPixelFactor());
	return result;
}

struct LabelDrawer : WeakPtr<const FontIndexCache>, SelectingFontArray
{
	LabelDrawer(const FeatureDrawer& fd)
		:	WeakPtr<const FontIndexCache>(fd.GetUpdatedLabelFontIndexCache())
		,	SelectingFontArray(fd.m_Drawer.GetDC(), get_ptr(), false)
		,	m_FeatureDrawer(fd)
		,	m_TextColorTheme(fd.m_Layer->GetEnabledTheme(AN_LabelTextColor))
		,	m_BackColorTheme(fd.m_Layer->GetEnabledTheme(AN_LabelBackColor))
		,	m_TextColorSelector(fd.m_Drawer.GetDC(), (m_TextColorTheme && m_TextColorTheme->IsAspectParameter()) ? m_TextColorTheme->GetColorAspectValue() : fd.m_Layer->GetDefaultTextColor() ) // default: black
		,	m_BackColorSelector(fd.m_Drawer.GetDC(), (m_BackColorTheme && m_BackColorTheme->IsAspectParameter()) ? m_BackColorTheme->GetColorAspectValue() : fd.m_Layer->GetDefaultBackColor() ) // default: TRANSPARENT
		,	m_BkModeSelector   (fd.m_Drawer.GetDC(), (m_BackColorTheme && m_BackColorTheme->IsAspectParameter() && m_BackColorTheme->GetColorAspectValue() != DmsTransparent) ? OPAQUE : TRANSPARENT)
		,	m_LabelTextValueGetter     ( GetValueGetter(fd.m_Layer->GetEnabledTheme(AN_LabelText     ).get()) )
		,	m_LabelTextColorValueGetter( (m_TextColorTheme && !m_TextColorTheme->IsAspectParameter()) ? m_TextColorTheme->GetValueGetter() : nullptr )
		,	m_LabelBackColorValueGetter( (m_BackColorTheme && !m_BackColorTheme->IsAspectParameter()) ? m_BackColorTheme->GetValueGetter() : nullptr )
	{
		dms_assert(fd.m_Layer->m_FontIndexCaches[FR_Label] == NULL || fd.m_Layer->GetTheme(AN_LabelText) ); // maybe the label sublayer has been turned off.
		if (SelectSingleton())
			WeakPtr<const FontIndexCache>::reset();
	}

	void DrawLabel(entity_id entityIndex, const GPoint& location)
	{
		if (has_ptr())
			if (!SelectFontHandle(get_ptr()->GetKeyIndex(entityIndex))) // returns false if fontSize == 0;
				return;

		HDC hdc = m_FeatureDrawer.m_Drawer.GetDC();

		if (m_LabelTextColorValueGetter)
			SetTextColor(
				hdc, 
				DmsColor2COLORREF(
					m_LabelTextColorValueGetter->GetColorValue(entityIndex)
				)
			);

		if (m_LabelBackColorValueGetter)
		{
			COLORREF clr = DmsColor2COLORREF( m_LabelBackColorValueGetter->GetColorValue(entityIndex) );
			if (clr == TRANSPARENT_COLORREF)
				SetBkMode(hdc, TRANSPARENT);
			else
			{
				SetBkMode(hdc, OPAQUE);
				SetBkColor(hdc, clr);
			}
		}

		dms_assert(m_FeatureDrawer.HasLabelText() );
		SharedStr label = m_LabelTextValueGetter->GetStringValue(entityIndex, m_LabelLock);
		TextOutLimited(
			hdc 
		,	location.x, location.y
		,	label.c_str()
		,	label.ssize()
		);

	}

private:
	const FeatureDrawer& m_FeatureDrawer;

	std::shared_ptr<const Theme> m_TextColorTheme;
	std::shared_ptr<const Theme> m_BackColorTheme;

	WeakPtr<const AbstrThemeValueGetter> m_LabelTextValueGetter;
	WeakPtr<const AbstrThemeValueGetter> m_LabelTextColorValueGetter;
	WeakPtr<const AbstrThemeValueGetter> m_LabelBackColorValueGetter;

	DcTextColorSelector  m_TextColorSelector;
	DcBackColorSelector  m_BackColorSelector;
	DcBkModeSelector     m_BkModeSelector;
	mutable GuiReadLock  m_LabelLock;
};

//----------------------------------------------------------------------
// class  : FeatureLayer
//----------------------------------------------------------------------

FeatureLayer::FeatureLayer(GraphicObject* owner, const LayerClass* cls)
	:	GraphicLayer(owner, cls)
	,	m_MaxLabelStrLen(-1)
{
}
void FeatureLayer::Init()
{
	base_type::Init();
	InvalidateAt(UpdateMarker::tsBereshit);
}

GraphVisitState FeatureLayer::InviteGraphVistor(class AbstrVisitor& v)
{
	return v.DoFeatureLayer(this);
}

Int32 FeatureLayer::GetMaxLabelStrLen() const
{
	if (m_MaxLabelStrLen == -1)
	{
		UInt32 result = 0;

		auto labelTextTheme = GetEnabledTheme(AN_LabelText);
		if (labelTextTheme)
		{
			const AbstrDataItem* labelPalette = labelTextTheme->GetPaletteOrThemeAttr();
			PreparedDataReadLock drl(labelPalette);

			const DataArray<SharedStr>* labelArray = const_array_dynacast<SharedStr>(labelPalette);
			if (labelArray) //use fast way
				for (tile_id t=0, tn=labelPalette->GetAbstrDomainUnit()->GetNrTiles(); t!=tn; ++t)
				{
					auto labelData = labelArray->GetTile(t);
					auto
						i = labelData.begin(),
						e = labelData.end();
					for (;i!=e;++i)
						MakeMax(result, i->size());
				}
			else
			{
				GuiReadLock lock;
				const AbstrDataObject* abstrLabelArray = labelPalette->GetRefObj();
				for (tile_id t=0, tn= labelPalette->GetAbstrDomainUnit()->GetNrTiles(); t!=tn; ++t)
				{
					auto labelTileLock = ReadableTileLock(abstrLabelArray, t);
					SizeT i = labelPalette->GetAbstrDomainUnit()->GetTileFirstIndex(t);
					SizeT n = i + labelPalette->GetAbstrDomainUnit()->GetTileCount(t);
					for (;i!=n;++i)
						MakeMax(result, abstrLabelArray->AsString(i, lock).ssize() );
				}
			}
		}
		m_MaxLabelStrLen = result;
	}
	return m_MaxLabelStrLen;
}

const AbstrUnit* FeatureLayer::GetGeoCrdUnit() const
{
	dms_assert(m_Themes[AN_Feature]);
	return m_Themes[AN_Feature]->GetPaletteValuesUnit();
}

const AbstrDataItem* FeatureLayer::GetFeatureAttr() const
{
	dms_assert(m_Themes[AN_Feature]);
	return m_Themes[AN_Feature]->GetPaletteAttr();
}

const AbstrUnit* FeatureLayer::GetThemeDomainEntity() const
{
	dms_assert(m_Themes[AN_Feature]);
	return m_Themes[AN_Feature]->GetThemeEntityUnit();
}

DmsColor& FeatureLayer::UpdateDefaultColor(DmsColor& mutableDefaultPaletteColor) const
{
	if (mutableDefaultPaletteColor == -2)
	{
		auto dv = GetDataView().lock();
		if (dv)
			mutableDefaultPaletteColor = dv->GetNextDmsColor();
	}
	return mutableDefaultPaletteColor;
}

DmsColor& FeatureLayer::GetDefaultPointColor() const
{
	return UpdateDefaultColor(m_DefaultPointColor);
}

DmsColor& FeatureLayer::GetDefaultArcColor() const
{
	return UpdateDefaultColor(m_DefaultArcColor);
}

DmsColor& FeatureLayer::GetDefaultBrushColor() const
{
	return UpdateDefaultColor(m_DefaultBrushColor);
}

DmsColor FeatureLayer::GetDefaultOrThemeColor(AspectNr an) const
{
	DmsColor defaultClr;
	switch (an) {
	case AN_BrushColor: defaultClr = m_DefaultBrushColor; break;
	case AN_PenColor: defaultClr = m_DefaultArcColor; break;
	case AN_SymbolColor: defaultClr = m_DefaultPointColor; break;
	default: MG_CHECK(false);
	}

	if (IsDefined(defaultClr))
		return defaultClr;
	auto theme = GetTheme(an);
	MG_CHECK(theme);
	return theme->GetColorAspectValue();
}


void FeatureLayer::DoUpdateView()
{
	if (!HasNoExtent())
	{
		const AbstrDataItem* valuesItem = GetFeatureAttr();
		dms_assert(valuesItem);
	if (PrepareDataOrUpdateViewLater(valuesItem)) {
			PreparedDataReadLock lock(valuesItem);
			m_FeatureDataExtents = AsWorldExtents(
				valuesItem->GetCurrRefObj()->GetActualRangeAsDRect(
					valuesItem->HasUndefinedValues()
				),
				GetGeoCrdUnit()
			);
			goto ready;
		}
	}
	m_FeatureDataExtents = GetDefaultCrdRect();
ready:
	SetWorldClientRect(m_FeatureDataExtents + GetFeatureWorldExtents());  // include BorderWorldExtents
}

void FeatureLayer::DoInvalidate() const
{
	base_type::DoInvalidate();

	dms_assert(!m_State.HasInvalidationBlock());

	m_MaxLabelStrLen = -1;
	for (UInt32 fr = 0; fr != FR_Count; ++fr)
		m_FontIndexCaches[fr].reset();
	m_PenIndexCache.reset();
	m_BoundingBoxCache.reset();

	const_cast<FeatureLayer*>(this)->InvalidateDraw();

	dms_assert(DoesHaveSupplInterest() || !GetInterestCount());
}

const AbstrBoundingBoxCache* FeatureLayer::GetBoundingBoxCache() const
{
	const AbstrDataItem* featureItem = GetFeatureAttr();
	dms_assert(featureItem);

	switch ( featureItem->GetAbstrValuesUnit()->GetValueType()->GetValueClassID() )
	{
#define INSTANTIATE(P) case VT_##P: return ::GetBoundingBoxCache<P::field_type>(this);
		INSTANTIATE_SEQ_POINTS
#undef	INSTANTIATE
	}
	return nullptr;
}

FontIndexCache* FeatureLayer::GetFontIndexCache(FontRole fr) const
{
	assert(fr >= 0);
	assert(fr < FR_Count);
	dms_assert( WasValid() ); // we should only get here from after successful update in Draw

	if (!m_FontIndexCaches[fr])
	{
		m_FontIndexCaches[fr].assign(
			new FontIndexCache(
				GetEnabledTheme(fontSizeAspect [fr]).get()
			,	GetEnabledTheme(worldSizeAspect[fr]).get()
			,	GetEnabledTheme(fontNameAspect [fr]).get()
			,	GetEnabledTheme(fontAngleAspect[fr]).get()
			,	GetThemeDomainEntity()
			,	defFontPixelSize[fr]
			,	defFontWorldSize[fr]
			,	GetTokenID_mt(defFontNames[fr])
			,	0
			)
		);
	}
	return m_FontIndexCaches[fr];
}

PenIndexCache* FeatureLayer::GetPenIndexCache(DmsColor defaultColor) const
{
//	dms_assert( WasValid(PS_DataReady) ); // we should only get here from after successful update in Draw

	if (!m_PenIndexCache)
	{
		m_PenIndexCache.assign(
			new PenIndexCache(
				GetEnabledTheme(AN_PenWidth).get()
			,	GetEnabledTheme(AN_PenWorldWidth).get()
			,	GetEnabledTheme(AN_PenColor).get()
			,	GetEnabledTheme(AN_PenStyle).get()
			,	GetThemeDomainEntity()
			,	defaultColor
			)
		);
	}
	return m_PenIndexCache;
}

bool FeatureLayer::ShowSelectedOnlyEnabled() const
{
	return m_Themes[AN_Selections].get(); // first make a selection before non selected can be hidden
}

void FeatureLayer::UpdateShowSelOnly()
{
	dms_assert(
			(!ShowSelectedOnly()) 
		||	m_Themes[AN_Selections]
	);

/*	TODO, NYI use AN_OrderBy
	UpdateShowSelOnlyImpl( this, 
		GetActiveEntity(), GetTheme(AN_OrderBy)->GetThemeAttr(), // XXX is dit wel een index of een sort?
		m_SelEntity, m_SelIndexAttr,
		m_Themes[AN_Selections]
	);
*/
}

bool FeatureLayer::Draw(GraphDrawer& d) const
{
	if (d.MustBreak())
		return true;

	if (!VisibleLevel(d))
		return false;

	if (PrepareThemeSetData(this) == AVS_SuspendedOrFailed) // also checks suppliers for changes and calls DoInvalidate if any supplier changed
	{
		dms_assert(SuspendTrigger::DidSuspend() || WasFailed(FR_Data));
		return SuspendTrigger::DidSuspend();
	}

	dms_assert(d.DoUpdateData());

	ThemeReadLocks readLocks;
	if (!readLocks.push_back(this, DrlType::Suspendible))
		return readLocks.ProcessFailOrSuspend(this);
	if (!d.DoDrawData())
		return false;

	FeatureDrawer featureDrawer(this, d);
	dms_assert (!SuspendTrigger::DidSuspend());

	AddTransformation addTr(&d, GetGeoTransformation());

	return DrawImpl(featureDrawer);
}

#include "act/InvalidationBlock.h"

const int FONT_DECIFONTSIZE_ABOVE = 10;
const int FONT_DECIFONTSIZE_BELOW = 10;
const int FONT_DECIFONTSIZE_LEFT  =  6;
const int FONT_DECIFONTSIZE_RIGHT =  6;
const int FONT_DECIFONTSIZE_HOR = Max<int>(FONT_DECIFONTSIZE_LEFT,  FONT_DECIFONTSIZE_RIGHT);
const int FONT_DECIFONTSIZE_VER = Max<int>(FONT_DECIFONTSIZE_ABOVE, FONT_DECIFONTSIZE_BELOW);

GRect FeatureLayer::GetFeaturePixelExtents(CrdType subPixelFactor) const
{
	if (!GetEnabledTheme(AN_LabelText))
		return GRect(0, 0, 0, 0);

	Int32 
		maxFontSizeY = GetMaxValue( GetEnabledTheme(AN_LabelSize).get(), DEFAULT_FONT_PIXEL_SIZE )*subPixelFactor,
		maxFontSizeX = maxFontSizeY * GetMaxLabelStrLen();

	return GRect(
		- ((FONT_DECIFONTSIZE_LEFT *maxFontSizeX)/10+1), 
		- ((FONT_DECIFONTSIZE_ABOVE*maxFontSizeY)/10+1),
		+ ((FONT_DECIFONTSIZE_RIGHT*maxFontSizeX)/10+1), 
		+ ((FONT_DECIFONTSIZE_BELOW*maxFontSizeY)/10+1)
	);
}

GRect FeatureLayer::GetBorderPixelExtents(CrdType subPixelFactor) const
{
	return GetFeaturePixelExtents(subPixelFactor);
}

CrdRect FeatureLayer::GetFeatureWorldExtents() const
{
	if (GetEnabledTheme(AN_LabelText)) {
		Float32 maxFontSizeY = GetMaxValue(GetEnabledTheme(AN_LabelWorldSize).get(), DEFAULT_FONT_WORLD_SIZE);
		if (maxFontSizeY) {
			Float32 maxFontSizeX = maxFontSizeY * GetMaxLabelStrLen();
			if (maxFontSizeX) {

				// LET OP: SOLVE ORIENTATION PROBLEM BY BEING CONSERVATIVE
				return
					shp2dms_order(
						CrdRect(
							CrdPoint(
								-((FONT_DECIFONTSIZE_HOR * maxFontSizeX) / 10 + 1),
								-((FONT_DECIFONTSIZE_VER * maxFontSizeY) / 10 + 1)
							),
							CrdPoint(
								+((FONT_DECIFONTSIZE_HOR * maxFontSizeX) / 10 + 1),
								+((FONT_DECIFONTSIZE_VER * maxFontSizeY) / 10 + 1)
							)
						)
					);
			}
		}
	}
	return CrdRect(CrdPoint(0, 0), CrdPoint(0, 0));
}

CrdRect FeatureLayer::GetExtentsInflator(const CrdTransformation& tr, CrdType subPixelFactor) const
{
	CrdRect symbRect = 
		tr.WorldScale(
			Convert<CrdRect>(GetBorderPixelExtents(subPixelFactor)) 
		);

	symbRect += GetFeatureWorldExtents();
	return symbRect;
}

CrdRect FeatureLayer::GetWorldClipRect  (const GraphDrawer& d) const
{
	return d.GetWorldClipRect() + - GetExtentsInflator(d.GetTransformation(), d.GetSubPixelFactor());
}

#include "MapControl.h"

CrdRect FeatureLayer::CalcSelectedFullWorldRect()  const
{
	CrdRect result = CalcSelectedClientWorldRect();
	if (!result.inverted())
		result += GetFeatureWorldExtents();
	return result;
}

bool FeatureLayer::OnCommand(ToolButtonID id)
{
	switch (id)
	{
		case TB_ShowSelOnlyOn:  if (ShowSelectedOnlyEnabled()) SetShowSelectedOnly( true); return true;
		case TB_ShowSelOnlyOff:                                SetShowSelectedOnly(false); return true;
	}
	return base_type::OnCommand(id);
}

void FeatureLayer::FillLcMenu(MenuData& menuData)
{
	base_type::FillLcMenu(menuData);

	SubMenu subMenu(menuData, SharedStr("Select SubLayer(s)")); // SUBMENU
	
	AspectNrSet layerClassSet = GetLayerClass()->GetPossibleAspects();

	for (AspectGroup ag=AG_First; ag != AG_Other; ++reinterpret_cast<UInt32&>(ag))
		if (IsPossibleAspectGroup(ag) && AspectGroupArray[ag].menuFunc)
		{
			AspectGroupArray[ag].menuFunc(this, ag, menuData);
		}
}

// default impl used by arc and polygon

CrdRect FeatureLayer::CalcSelectedClientWorldRect() const
{
	DRect selectRect;

	const AbstrBoundingBoxCache* bbCache = GetBoundingBoxCache();

	if (m_Themes[AN_Selections])
	{

		auto selTheme = CreateSelectionsTheme();
		dms_assert(selTheme);
		const AbstrDataItem* selAttr = selTheme->GetThemeAttr();

		PreparedDataReadLock selLock(selAttr);

		SizeT f = bbCache->GetFeatureCount();
		while (f)
		{
			if (IsFeatureSelected(--f))
				selectRect |= bbCache->GetBounds(f);
		}
	}
	// Include FocusElement in selected rect
	if (IsActive())
	{
		SizeT e = GetFocusElemIndex();
		if (IsDefined(e))
		{
			if (HasEntityAggr())
			{
				SizeT f = bbCache->GetFeatureCount();
				while (f)
				{
					if (Feature2EntityIndex(--f) == e)
						selectRect |= bbCache->GetBounds( f );
				}
			}
			else
				selectRect |= bbCache->GetBounds( Entity2FeatureIndex(e) );
		}
	}
	if (! selectRect.inverted() )
		return GetGeoTransformation().Apply(Convert<CrdRect>(selectRect) );
	return selectRect;
}

IMPL_ABSTR_CLASS(FeatureLayer)

//----------------------------------------------------------------------
// class  : GraphicPointLayer
//----------------------------------------------------------------------

const int SYMB_DECIFONTSIZE_ABOVE = 15;
const int SYMB_DECIFONTSIZE_BELOW = 10;
const int SYMB_DECIFONTSIZE_LEFT  =  6;
const int SYMB_DECIFONTSIZE_RIGHT =  6;
const int SYMB_DECIFONTSIZE_HOR = Max<int>(SYMB_DECIFONTSIZE_LEFT,  SYMB_DECIFONTSIZE_RIGHT);
const int SYMB_DECIFONTSIZE_VER = Max<int>(SYMB_DECIFONTSIZE_ABOVE, SYMB_DECIFONTSIZE_BELOW);

GraphicPointLayer::GraphicPointLayer(GraphicObject* owner , const LayerClass* cls)
	:	FeatureLayer(owner, cls) 
{}

template <typename ScalarType>
SizeT FindNearestPoint(
	const GraphicPointLayer* layer,
	const AbstrDataObject*   points,
	const CrdPoint&          geoPnt)
{
	typedef Point<ScalarType>    PointType;
	typedef DataArray<PointType> DataArrayType;

	SizeT entityID = UNDEFINED_VALUE(SizeT);
	Float64 sqrDist = MAX_VALUE(Float64);

	auto domain = points->GetTiledRangeData();
	for (tile_id t=0, tn = domain->GetNrTiles(); t!=tn; ++t)
	{
		const DataArrayType* da = debug_cast<const DataArrayType*>(points);

		auto data = da->GetTile(t);
		auto
			b = data.begin(),
			e = data.end();

		// search backwards so that last drawn object will be first selected
		while (b!=e)
		{
			--e;
			if (MakeMin(sqrDist, SqrDist<Float64>(Convert<CrdPoint>(*e), geoPnt)))
			{
				entityID = domain->GetRowIndex(t, e - data.begin());
			}
		}
	}
	return entityID;
}

void FeatureLayer::SelectPoint(CrdPoint worldPnt, EventID eventID)
{
	const AbstrDataItem* featureItem = GetFeatureAttr();
	dms_assert(featureItem);

	SizeT featureIndex = UNDEFINED_VALUE(SizeT);
	if (!SuspendTrigger::DidSuspend() && featureItem->PrepareData())
	{
		DataReadLock lck(featureItem);
		dms_assert(lck.IsLocked());
		featureIndex = _FindFeatureByPoint(
			GetGeoTransformation().Reverse(worldPnt),
			featureItem->GetRefObj(),
			featureItem->GetAbstrValuesUnit()->GetValueType()->GetValueClassID()
		);
	}
	if (SuspendTrigger::DidSuspend())
		return;

	InvalidationBlock lock(this);
	DataWriteLock writeLock(
		const_cast<AbstrDataItem*>(CreateSelectionsTheme()->GetThemeAttr()),
		CompoundWriteType(eventID)
	);

	if (SelectFeatureIndex(writeLock, featureIndex, eventID))
	{
		writeLock.Commit();
		lock.ProcessChange();
	}
}

SizeT GraphicPointLayer::_FindFeatureByPoint(const CrdPoint& geoPnt, const AbstrDataObject* featureData, ValueClassID vid)
{
	SizeT result = UNDEFINED_VALUE(SizeT);
	visit<typelists::points>(featureData->GetValueClass(), 
		[this, featureData, &geoPnt, &result] <typename P> (const P*) 
		{
			result = FindNearestPoint< scalar_of_t<P> >(this, featureData, geoPnt);
		}
	);

	return result;
}

template <typename ScalarType>
bool SelectPointsInRect(
	GraphicPointLayer*     layer,
	const AbstrDataObject* points,
	const CrdRect&         geoRect,
	EventID eventID)
{
	typedef Point<ScalarType>    PointType;
	typedef DataArray<PointType> DataArrayType;

	const DataArrayType* da = debug_cast<const DataArrayType*>(points);

	auto data = da->GetDataRead();
	auto
		b = data.begin(),
		e = data.end();

	DataWriteLock writeLock(
		const_cast<AbstrDataItem*>(layer->CreateSelectionsTheme()->GetThemeAttr()), 
		CompoundWriteType(eventID)
	);

	bool result = false;

	while (b!=e)
	{
		if (IsIncluding(geoRect, Convert<CrdPoint>(*b)))
		{
			SizeT entityID = b - data.begin();
			result |= layer->SelectFeatureIndex(writeLock, entityID, eventID);
		}
		++b;
	}
	if (result)
		writeLock.Commit();
	return result;
}



template <typename ScalarType>
bool SelectPointsInCircle(
	GraphicPointLayer*     layer,
	const AbstrDataObject* points,
	const CrdPoint& worldPnt, CrdType worldRadius,
	EventID eventID)
{
	typedef Point<ScalarType>    PointType;
	typedef DataArray<PointType> DataArrayType;

	CrdRect worldRect  = Inflate(worldPnt, CrdPoint(worldRadius, worldRadius));
	CrdPoint geoPnt    = layer->GetGeoTransformation().Reverse(worldPnt);
	CrdRect geoRect    = layer->GetGeoTransformation().Reverse(worldRect);
	CrdType geoRadius2 = Area(geoRect) / 4;

	const DataArrayType* da = debug_cast<const DataArrayType*>(points);

	auto data = da->GetDataRead();
	auto
		b = data.begin(),
		e = data.end();

	DataWriteLock writeLock(
		const_cast<AbstrDataItem*>(layer->CreateSelectionsTheme()->GetThemeAttr()), 
		CompoundWriteType(eventID)
	);

	bool result = false;

	while (b!=e)
	{
		CrdPoint dataPnt = Convert<CrdPoint>(*b);
		if (IsIncluding(geoRect, dataPnt) && SqrDist<CrdType>(geoPnt, dataPnt) <= geoRadius2)
		{
			SizeT entityID = b - data.begin();
			result |= layer->SelectFeatureIndex(writeLock, entityID, eventID);
		}
		++b;
	}
	if (result)
		writeLock.Commit();
	return result;
}

template <typename ScalarType>
bool SelectPointsInPolygon(
	GraphicPointLayer*     layer,
	const AbstrDataObject* points,
	CrdRect                polyWorldRect,
	const CrdPoint*        first, 
	const CrdPoint*        last,
	EventID                eventID)
{
	typedef Point<ScalarType>    PointType;
	typedef DataArray<PointType> DataArrayType;

	const DataArrayType* da = debug_cast<const DataArrayType*>(points);

	auto data = da->GetDataRead();
	auto
		b = data.begin(),
		e = data.end();

	DataWriteLock writeLock(
		const_cast<AbstrDataItem*>(layer->CreateSelectionsTheme()->GetThemeAttr()), 
		CompoundWriteType(eventID)
	);

	bool result = false;

	CrdTransformation geo2worldTr = layer->GetGeoTransformation();
	CrdRect           polyGeoRect = geo2worldTr.Reverse(polyWorldRect);

	while (b!=e)
	{
		CrdPoint geoPnt = Convert<CrdPoint>(*b);
		if (IsIncluding(polyGeoRect, geoPnt))
		{
			CrdPoint worldPnt = geo2worldTr.Apply(geoPnt);
			if (IsInside(first, last, worldPnt))
			{
				SizeT entityID = b - data.begin();
				result |= layer->SelectFeatureIndex(writeLock, entityID, eventID);
			}
		}
		++b;
	}
	if (result)
		writeLock.Commit();
	return result;
}

#include "AbstrController.h"
#include "ViewPort.h"

void GraphicPointLayer::SelectRect(CrdRect worldRect, EventID eventID)
{
	const AbstrDataItem* valuesItem = GetFeatureAttr();
	dms_assert(valuesItem);

	InvalidationBlock lock1(this);

	dms_assert(  eventID & EID_REQUEST_SEL  );
	dms_assert(!(eventID & EID_REQUEST_INFO));

	bool result = false;

	if (valuesItem->PrepareData())
	{
		CrdRect geoRect = GetGeoTransformation().Reverse(worldRect);

		DataReadLock lck(valuesItem); 
		dms_assert(lck.IsLocked());

		visit<typelists::seq_points>(valuesItem->GetAbstrValuesUnit(), 
			[this, valuesItem, geoRect, eventID, &result] <typename a_type> (const Unit<a_type>*) 
			{
				result = SelectPointsInRect< scalar_of_t<a_type> >(this, valuesItem->GetRefObj(), geoRect, eventID);
			}
		);
	}
	if	(result && !HasEntityAggr())
	{
		lock1.ProcessChange();
		InvalidateWorldRect(worldRect + GetFeatureWorldExtents(), nullptr);
	}
}

void GraphicPointLayer::SelectCircle(CrdPoint worldPnt, CrdType worldRadius, EventID eventID)
{
	const AbstrDataItem* valuesItem = GetFeatureAttr();
	dms_assert(valuesItem);

	InvalidationBlock lock1(this);

	dms_assert(  eventID & EID_REQUEST_SEL  );
	dms_assert(!(eventID & EID_REQUEST_INFO));

	bool result = false;

	if (valuesItem->PrepareData())
	{
		DataReadLock lck(valuesItem); 
		dms_assert(lck.IsLocked());

		visit<typelists::points>(valuesItem->GetAbstrValuesUnit(), 
			[this, valuesItem, worldRadius, eventID, worldPnt, &result] <typename P> (const Unit<P>*) 
			{
				result = SelectPointsInCircle< scalar_of_t<P> >(this, valuesItem->GetRefObj(), worldPnt, worldRadius, eventID);
			}
		);
	}
	if	(result && !HasEntityAggr())
	{
		lock1.ProcessChange();
		InvalidateWorldRect(Inflate(worldPnt, CrdPoint(worldRadius, worldRadius)) + GetFeatureWorldExtents(), nullptr);
	}
}

void GraphicPointLayer::SelectPolygon(const CrdPoint* first, const CrdPoint* last, EventID eventID)
{
	const AbstrDataItem* valuesItem = GetFeatureAttr();
	dms_assert(valuesItem);

	InvalidationBlock lock1(this);

	dms_assert(  eventID & EID_REQUEST_SEL  );
	dms_assert(!(eventID & EID_REQUEST_INFO));

	bool result = false;

	CrdRect polyRect = Range<CrdPoint>(first, last, false, true);

	if (valuesItem->PrepareData())
	{
		DataReadLock lck(valuesItem); 
		dms_assert(lck.IsLocked());
		visit<typelists::points>(valuesItem->GetAbstrValuesUnit(),
			[this, valuesItem, first, last, polyRect, eventID, &result] <typename P> (const Unit<P>*)
			{
				result = SelectPointsInPolygon< typename scalar_of<P>::type >(this, valuesItem->GetRefObj(), polyRect, first, last, eventID);
			}
		);
	}
	if	(result && !HasEntityAggr())
	{
		lock1.ProcessChange();
		InvalidateWorldRect(polyRect + GetFeatureWorldExtents(), nullptr);
	}
}

CrdRect GraphicPointLayer::CalcSelectedClientWorldRect() const
{
	DRect selectRect;

	const AbstrDataItem* featureAttr = GetFeatureAttr();
	PreparedDataReadLock featLock(featureAttr);
	const AbstrDataObject* featureData = featureAttr->GetRefObj();

	if (m_Themes[AN_Selections])
	{

		auto selTheme = CreateSelectionsTheme();
		dms_assert(selTheme);
		const AbstrDataItem* selAttr = selTheme->GetThemeAttr();

		PreparedDataReadLock selLock(selAttr);

		UInt32 f = featureAttr->GetAbstrDomainUnit()->GetCount();
		while (f)
		{
			if (IsFeatureSelected(--f))
				selectRect |= featureData->GetValueAsDPoint(f);
		}
	}
	SizeT e = GetFocusElemIndex();
	if (IsDefined(e))
	{
		if (HasEntityAggr())
		{
			SizeT f = featureAttr->GetAbstrDomainUnit()->GetCount();
			while (f)
			{
				if (Feature2EntityIndex(--f) == e)
					selectRect |= featureData->GetValueAsDPoint(f);
			}
		}
		else
			selectRect |= featureData->GetValueAsDPoint( Entity2FeatureIndex(e) );
	}

	if (! selectRect.inverted())
		return GetGeoTransformation().Apply(Convert<CrdRect>(selectRect) );
	return selectRect;
}

void GraphicPointLayer::_InvalidateFeature(SizeT featureIndex)
{
	dms_assert(IsDefined(featureIndex));

	const AbstrDataItem* valuesItem = GetFeatureAttr();
	dms_assert(valuesItem);

	PreparedDataReadLock lck(valuesItem); 
	dms_assert(lck.IsLocked());

	DPoint pnt  = valuesItem->GetRefObj()->GetValueAsDPoint(featureIndex);
	InvalidateWorldRect(GetFeatureWorldExtents() + GetGeoTransformation().Apply(pnt), nullptr);
}

template <typename ScalarType>
bool DrawPoints(
	const FeatureDrawer&        fd,
	const GraphicPointLayer*    layer,
	const AbstrDataItem*        pointsItem,
	const FontIndexCache*       fontIndices)
{
	typedef Point<ScalarType>    PointType;
	typedef Range<PointType>     RangeType;
	typedef DataArray<PointType> DataArrayType;

	const DataArrayType* da = const_array_cast<PointType>(pointsItem);
	tile_id tn = pointsItem->GetAbstrDomainUnit()->GetNrTiles();

	const GraphDrawer& d =fd.m_Drawer;

	DcTextAlignSelector selectCenterAlignment(d.GetDC(), TA_CENTER|TA_BASELINE|TA_NOUPDATECP);

	CrdTransformation transformer = d.GetTransformation();

	RangeType clipRect = Convert<RangeType>( layer->GetWorldClipRect(d) );

	ResumableCounter mainCount(d.GetCounterStacks(), false);

	SizeT focusElem = UNDEFINED_VALUE(SizeT);
	if (layer->IsActive())
		focusElem = layer->GetFocusElemIndex();

	// ================== draw symbols

	SelectionIdCPtr selectionsArray; dms_assert(!selectionsArray);
	if (fd.m_SelValues) {
		selectionsArray = fd.m_SelValues.value().begin();
		MG_CHECK(selectionsArray);
	}
	bool selectedOnly = layer->ShowSelectedOnly();
	dms_assert(selectionsArray || !selectedOnly);

	WeakPtr<const IndexCollector> indexCollector = fd.GetIndexCollector();
	if (mainCount == 0)
	{
		if (!layer->IsDisabledAspectGroup(AG_Symbol))
		{
			//	thematic presentation
			SelectingFontArray fontStock(d.GetDC(), fontIndices, false);
			if (fontStock.SelectSingleton()) 
				fontIndices = nullptr;

			DmsColor defaultColor = layer->GetDefaultPointColor();
			WCHAR defaultSymbol = defSymbol;
			WeakPtr<const AbstrThemeValueGetter> symbolIdGetter;
			if (layer->GetEnabledTheme(AN_SymbolIndex))
			{
				auto theme = layer->GetTheme(AN_SymbolIndex);
				dms_assert(theme);
				if (theme->IsAspectParameter())
					defaultSymbol = theme->GetOrdinalAspectValue();
				else
					symbolIdGetter = theme->GetValueGetter();
			}

			WeakPtr<const AbstrThemeValueGetter> colorGetter;
			if (layer->GetEnabledTheme(AN_SymbolColor))
			{
				auto theme = layer->GetTheme(AN_SymbolColor);
				dms_assert(theme);
				if (theme->IsAspectParameter())
					defaultColor = theme->GetColorAspectValue();
				else
					colorGetter = theme->GetValueGetter();
			}

			DcTextColorSelector selectTextColor(d.GetDC(), defaultColor );

			ResumableCounter tileCounter(d.GetCounterStacks(), true);
			for (tile_id t=tileCounter.Value(); t!=tn; ++t)
			{
				auto data = da->GetLockedDataRead(t);
				auto
					b = data.begin(),
					e = data.end();
				SizeT tileIndexBase = pointsItem->GetAbstrDomainUnit()->GetTileFirstIndex(t);

				ResumableCounter itemCounter(d.GetCounterStacks(), true);

				for (auto i = b + itemCounter.Value(); i != e; ++i)
				{
					if (IsIncluding(clipRect, *i))
					{
						entity_id entityIndex = (i - b) + tileIndexBase;

						if (indexCollector)
						{
							auto ei = indexCollector->GetEntityIndex(entityIndex);
							if (!IsDefined(ei))
								goto nextSymbol;
							entityIndex = ei;
						}
				
						if (fontIndices)
							if (!fontStock.SelectFontHandle(fontIndices->GetKeyIndex(entityIndex))) // returns false if fontSize == 0;
								goto nextSymbol;

						bool isSelected = selectionsArray && SelectionID( selectionsArray[entityIndex] );
						if (selectedOnly)
						{
							if (!isSelected) goto nextSymbol;
							isSelected = false;
						}

						DmsColor textColor = defaultColor;
						if (entityIndex == focusElem)
							textColor = GetFocusClr();
						else if (isSelected)
							textColor = GetSelectedClr(selectionsArray[entityIndex]);
						else if (colorGetter)
							textColor = colorGetter->GetColorValue(entityIndex);

						if (textColor == TRANSPARENT_COLORREF) 
							goto nextSymbol;

						CheckColor(textColor);

						SetTextColor(d.GetDC(), DmsColor2COLORREF(textColor) );

						if (symbolIdGetter)
							defaultSymbol = symbolIdGetter->GetOrdinalValue(entityIndex);

						TPoint viewPoint = Convert<TPoint>(transformer.Apply(*i));	
					
						CheckedGdiCall(
							TextOutW(
								d.GetDC(), 
								viewPoint.x(), viewPoint.y(),
								&defaultSymbol, 1
							) 
						,	"DrawPoint");
					}
				nextSymbol:
					++itemCounter; if (itemCounter.MustBreakOrSuspend1000()) return true;
				}
				itemCounter.Close();
				++tileCounter; if (tileCounter.MustBreakOrSuspend()) return true;
			}
			tileCounter.Close();
		}
		++mainCount; if (mainCount.MustBreakOrSuspend()) return true;
	}

	// ================== draw labels

	if (mainCount == 1 && fd.HasLabelText() && !layer->IsDisabledAspectGroup(AG_Label))
	{	
		LabelDrawer ld(fd); // allocate font(s) required for drawing labels
//		Int32 maxFontSize = ld.m_LabelFontIndexCache->GetMaxFontSize();

		ResumableCounter tileCounter(d.GetCounterStacks(), true);
		for (tile_id t=tileCounter.Value(); t!=tn; ++t)
		{
			auto data = da->GetLockedDataRead(t);
			auto
				b = data.begin(),
				e = data.end();
			SizeT tileIndexBase = pointsItem->GetAbstrDomainUnit()->GetTileFirstIndex(t);

			ResumableCounter itemCounter(d.GetCounterStacks(), true);

			for (auto i = b + itemCounter.Value(); i != e; ++i)
			{
				if (IsIncluding(clipRect, *i ))
				{
					GPoint viewPoint = Convert<GPoint>(transformer.Apply(*i));

					SizeT entityIndex = (i - b) + tileIndexBase;
					if (indexCollector)
					{
						entityIndex = indexCollector->GetEntityIndex(entityIndex);
						if (!IsDefined(entityIndex))
							goto nextLabel;
					}

					if (selectedOnly && !(selectionsArray && SelectionID( selectionsArray[entityIndex] ) ))
						goto nextLabel;

					ld.DrawLabel(entityIndex, viewPoint);
				}
			nextLabel:
				++itemCounter; if (itemCounter.MustBreakOrSuspend100()) return true;
			}
			itemCounter.Close();
			++tileCounter; if (tileCounter.MustBreakOrSuspend()) return true;
		}
		tileCounter.Close();
	}
	mainCount.Close();
	return false;
}

bool GraphicPointLayer::DrawImpl(FeatureDrawer& fd) const
{
	const AbstrDataItem* valuesItem = GetFeatureAttr();
	dms_assert(valuesItem);


	const FontIndexCache* fontIndices = GetFontIndexCache(FR_Symbol);
	if (!fontIndices)
		return false;

	fontIndices->UpdateForZoomLevel(fd.m_WorldZoomLevel, fd.m_Drawer.GetSubPixelFactor());

	bool result = false;
	visit<typelists::points>(valuesItem->GetAbstrValuesUnit(), 
		[this, valuesItem, fontIndices, &fd, &result] <typename P> (const Unit<P>*) 
		{
			result = DrawPoints< typename scalar_of<P>::type >(fd, this, valuesItem, fontIndices);
		}
	);
	return result;
}

GRect GraphicPointLayer::GetFeaturePixelExtents(CrdType subPixelFactor) const
{
	GRect rect = base_type::GetFeaturePixelExtents(subPixelFactor);
	if (!IsDisabledAspectGroup(AG_Symbol))
	{
		Int32 
			maxFontSizeY = GetMaxValue( GetEnabledTheme(AN_SymbolSize).get(), DEFAULT_SYMB_PIXEL_SIZE )*subPixelFactor,
			maxFontSizeX = maxFontSizeY;		

		rect |= GRect(
			- ((SYMB_DECIFONTSIZE_LEFT *maxFontSizeX)/10+1), 
			- ((SYMB_DECIFONTSIZE_ABOVE*maxFontSizeY)/10+1),
			+ ((SYMB_DECIFONTSIZE_RIGHT*maxFontSizeX)/10+1), 
			+ ((SYMB_DECIFONTSIZE_BELOW*maxFontSizeY)/10+1)
		);
	}
	return rect;
}

CrdRect GraphicPointLayer::GetFeatureWorldExtents() const
{
	CrdRect rect = base_type::GetFeatureWorldExtents();

	if (!IsDisabledAspectGroup(AG_Symbol))
	{
		CrdType 
			maxFontSizeY = GetMaxValue( GetEnabledTheme(AN_SymbolWorldSize).get(), DEFAULT_SYMB_WORLD_SIZE ),
			maxFontSizeX = maxFontSizeY;
			
		// LET OP: SOLVE ORIENTATION PROBLEM BY BEING CONSERVATIVE
		if (maxFontSizeY)
			rect |= shp2dms_order(
				CrdRect(
					CrdPoint(
						- ((SYMB_DECIFONTSIZE_HOR*maxFontSizeX)/10+1), 
						- ((SYMB_DECIFONTSIZE_VER*maxFontSizeY)/10+1)
					),
					CrdPoint(
						+ ((SYMB_DECIFONTSIZE_HOR*maxFontSizeX)/10+1), 
						+ ((SYMB_DECIFONTSIZE_VER*maxFontSizeY)/10+1)
					)
				)
			);
	}
	return rect;
}

IMPL_DYNC_LAYERCLASS(GraphicPointLayer, ASE_Feature|ASE_OrderBy|ASE_Label|ASE_Symbol|ASE_PixSizes|ASE_Selections, AN_SymbolColor, 0)

//----------------------------------------------------------------------
// class  : GraphicNetworkLayer
//----------------------------------------------------------------------

// TODO: SelectedColor and selectedOnly
template <typename ScalarType>
bool DrawNetwork(
	WeakPtr<const GraphicNetworkLayer> layer
,	const FeatureDrawer& fd
,	WeakPtr<const AbstrDataItem>    featureItem
,	        const PenIndexCache*    penIndices
,	WeakPtr<const AbstrThemeValueGetter> f1
,	WeakPtr<const AbstrThemeValueGetter> f2
)
{
	typedef Point<ScalarType>    PointType;
	typedef Range<PointType>     RangeType;
	typedef UInt32               IndexType;
	typedef DataArray<PointType> PointDataType;
	typedef DataArray<IndexType> IndexDataType;

	const GraphDrawer& d = fd.m_Drawer;

	auto data = const_array_cast<PointType>(featureItem.get_ptr())->GetLockedDataRead();
	auto pointDataBegin = data.begin();

	CrdTransformation transformer = d.GetTransformation();
	RangeType clipRect = Convert<RangeType>( layer->GetWorldClipRect(d) );

	ResumableCounter mainCount(d.GetCounterStacks(), false);

	WeakPtr<const IndexCollector> indexCollector = fd.GetIndexCollector();

	if (mainCount == 0)
	{
		if (!layer->IsDisabledAspectGroup(AG_Pen))
		{
			GPoint pointBuffer[2];

			PenArray pa(d.GetDC(), penIndices);

			ResumableCounter itemCounter(d.GetCounterStacks(), true);

			UInt32 n = f1->GetCount();

			for (UInt32 i = itemCounter.Value(); i != n; ++i)
			{
				UInt32
					f1i = f1->GetCardinalValue(i)
				,	f2i = f2->GetCardinalValue(i);

				PointType p1 = pointDataBegin[f1i];
				PointType p2 = pointDataBegin[f2i];
				if (IsIntersecting(clipRect, RangeType(p1, p2) ))
				{
					pointBuffer[0] = Convert<GPoint>(transformer.Apply(p1));
					pointBuffer[1] = Convert<GPoint>(transformer.Apply(p2));
					if (penIndices)
					{
						UInt32 entityIndex = i;
						if (indexCollector)
						{
							entityIndex = indexCollector->GetEntityIndex(entityIndex);
							if (!IsDefined(entityIndex))
								goto nextLink;
						}
						if (! pa.SelectPen(penIndices->GetKeyIndex(entityIndex)) )
							goto nextLink;
					}

					CheckedGdiCall(
						Polyline(
							d.GetDC(),
							&pointBuffer[0],
							2
						)
					,	"DrawLink"
					);
				}
			nextLink:
				++itemCounter; if (itemCounter.MustBreakOrSuspend100()) return true;
			}
			itemCounter.Close();
		}
		++mainCount; if (mainCount.MustBreakOrSuspend()) return true;
	}
	mainCount.Close();
	return false;
}

bool GraphicNetworkLayer::DrawImpl(FeatureDrawer& fd) const
{
	const AbstrDataItem* valuesItem = GetFeatureAttr();
	dms_assert(valuesItem);

	ResumableCounter mainCount(fd.m_Drawer.GetCounterStacks(), false);

	if (mainCount == 0)
	{
		auto f1 = GetTheme(AN_NetworkF1);
		auto f2 = GetTheme(AN_NetworkF2);
		if (f1 && f2)
		{
			const PenIndexCache*  penIndices = GetPenIndexCache(GetDefaultOrThemeColor(AN_PenColor) );
			if (!penIndices)
				return false;

			penIndices->UpdateForZoomLevel(fd.m_WorldZoomLevel, fd.m_Drawer.GetSubPixelFactor());
			bool result = false;
			visit<typelists::points>(valuesItem->GetAbstrValuesUnit(), 
				[this, valuesItem, penIndices, f1, f2, &fd, &result] <typename P> (const Unit<P>*) 
				{
					using T = typename scalar_of<P>::type;
					result = DrawNetwork< T >(this, fd, valuesItem, penIndices, GetValueGetter(f1.get()), GetValueGetter(f2.get()));
				}
			);
			if (result)
				return true;
		}
		++mainCount; if (mainCount.MustBreakOrSuspend()) return true;
	}

	if (mainCount == 1)
	{
		if (base_type::DrawImpl(fd))
			return true;
	}
	mainCount.Close();
	return false;
}

IMPL_DYNC_LAYERCLASS(GraphicNetworkLayer, ASE_Feature|ASE_OrderBy|ASE_Label|ASE_Network|ASE_PixSizes|ASE_Selections, AN_PenColor, 0)

//----------------------------------------------------------------------
// class  : GraphicArcLayer
//----------------------------------------------------------------------

#include "geo/DynamicPoint.h"
#include "geo/GeoDist.h"

template <typename ScalarType>
SizeT FindArcByPoint(
	const GraphicArcLayer*   layer,
	const AbstrDataObject*   featureData,
	const Point<ScalarType>& pnt)
{
	typedef Point<ScalarType>                           PointType;
	typedef sequence_traits<PointType>::container_type  PolygonType;
	typedef DataArray<PolygonType>                      DataArrayType;
	typedef BoundingBoxCache<ScalarType>::RectArrayType RectArrayType;

	const DataArrayType* da = debug_valcast<const DataArrayType*>(featureData);
	SizeT entityID = UNDEFINED_VALUE(SizeT);

	auto domain = featureData->GetTiledRangeData();
	for (tile_id t=0, tn = domain->GetNrTiles(); t!=tn; ++t)
	{
		const RectArrayType& rectArray = GetBoundingBoxCache<ScalarType>(layer)->GetBoundsArray(t);

		auto data = da->GetLockedDataRead(t);
		auto
			b = data.begin(),
			e = data.end();

		ArcProjectionHandle<Float64, ScalarType> aph(&pnt, MAX_VALUE(Float64));

		// search backwards so that last drawn object will be first selected
		while (b!=e)
		{
			--e;
			if (aph.Project2Arc(begin_ptr(*e), end_ptr(*e)))
				entityID = domain->GetRowIndex(t, e - data.begin());
		}
	}
	return entityID;
}

GraphicArcLayer::GraphicArcLayer(GraphicObject* owner)
	:	FeatureLayer(owner, GetStaticClass()) 
{}

SizeT GraphicArcLayer::_FindFeatureByPoint(const CrdPoint& geoPnt, const AbstrDataObject* featureData, ValueClassID vid)
{
	switch (vid)
	{
		#define INSTANTIATE(P) case VT_##P: return FindArcByPoint< scalar_of<P>::type >(this, featureData, Convert<P>(geoPnt));
			INSTANTIATE_SEQ_POINTS
		#undef INSTANTIATE
	}
	return UNDEFINED_VALUE(SizeT);
}

void GraphicArcLayer::SelectRect  (CrdRect worldRect, EventID eventID)
{
	throwItemError("SelectRect on ArcLayer Not Yet Implemented (NYI)");
}

void GraphicArcLayer::SelectCircle(CrdPoint worldPnt, CrdType worldRadius, EventID eventID)
{
	throwItemError("SelectCircle on ArcLayer Not Yet Implemented (NYI)");
}

void GraphicArcLayer::SelectPolygon(const CrdPoint* first, const CrdPoint* last, EventID eventID)
{
	throwItemError("SelectPolygon on ArcLayer Not Yet Implemented (NYI)");
}

void GraphicArcLayer::_InvalidateFeature(SizeT featureIndex)
{
	dms_assert(IsDefined(featureIndex));

	DRect boundRect = GetBoundingBoxCache()->GetBounds(featureIndex);
	InvalidateWorldRect(GetGeoTransformation().Apply(boundRect) + GetFeatureWorldExtents(), nullptr);
}

// TODO: SelectedColor and selectedOnly
template <typename ScalarType>
bool DrawArcs(
	const GraphicArcLayer* layer, 
	const FeatureDrawer& fd, 
	const PenIndexCache*  penIndices)
{
	typedef Point<ScalarType>                           PointType;
	typedef Range<PointType>                            RangeType;
	typedef sequence_traits<PointType>::container_type  PolygonType;
	typedef DataArray<PolygonType>                      DataArrayType;
	typedef BoundingBoxCache<ScalarType>::RectArrayType RectArrayType;

	const AbstrDataItem* featureItem = layer->GetFeatureAttr();
	const GraphDrawer& d = fd.m_Drawer;

	const DataArrayType* da = const_array_cast<PolygonType>(featureItem);
	tile_id tn = featureItem->GetAbstrDomainUnit()->GetNrTiles();

	CrdTransformation transformer = d.GetTransformation();
	RangeType clipRect = Convert<RangeType>( layer->GetWorldClipRect(d) );

	CrdType zoomLevel = Abs(transformer.ZoomLevel());
	dms_assert(zoomLevel > 1.0e-30); // we assume that nothing remains visible on such a small scale to avoid numerical overflow in the following inversion

	ScalarType minWorldWidth  = MIN_WORLD_SIZE / zoomLevel;
	ScalarType minWorldHeight = minWorldWidth;

	ResumableCounter mainCount(d.GetCounterStacks(), false);

	WeakPtr<const IndexCollector> indexCollector = fd.GetIndexCollector();

	if (mainCount == 0)
	{
		if (!layer->IsDisabledAspectGroup(AG_Pen))
		{
			std::vector<POINT> pointBuffer;

			PenArray pa(d.GetDC(), penIndices);

			ResumableCounter tileCounter(d.GetCounterStacks(), true);
			for (tile_id t=tileCounter.Value(); t!=tn; ++t)
			{
				const RectArrayType& rectArray = GetBoundingBoxCache<ScalarType>(layer)->GetBoundsArray(t);
				auto data = da->GetLockedDataRead(t);
				auto
					b = data.begin(),
					e = data.end();
				lfs_assert(rectArray.size() == e-b);
				SizeT tileIndexBase = featureItem->GetAbstrDomainUnit()->GetTileFirstIndex(t);

				ResumableCounter itemCounter(d.GetCounterStacks(), true);
				typename RectArrayType::const_iterator ri = rectArray.begin() + itemCounter.Value();

				for (auto i=b + itemCounter.Value(); i != e; ++i, ++ri)
				{
					if 
					(	IsIntersecting(clipRect, *ri )
					&&	(_Width (*ri) >= minWorldWidth || _Height(*ri) >= minWorldHeight)
					)
					{
						if (penIndices)
						{
							UInt32 entityIndex = (i - b) + tileIndexBase;
							if (indexCollector)
							{
								entityIndex = indexCollector->GetEntityIndex(entityIndex);
								if (!IsDefined(entityIndex))
									goto nextArc;
							}
							if (! pa.SelectPen(penIndices->GetKeyIndex(entityIndex)) )
								goto nextArc;
						}

						UInt32 nrPoints = i->size();
						pointBuffer.resize(nrPoints);

						std::vector<POINT>::iterator 
							bi = pointBuffer.begin();
						auto
							ii = i->begin(),
							ie = i->end  ();

						for (; ii!=ie; ++bi, ++ii)
							*bi = Convert<GPoint>(transformer.Apply(*ii));
					
						if (pointBuffer.size() >= 2)
						CheckedGdiCall(
							Polyline(
								d.GetDC(),
								&*pointBuffer.begin(),
								nrPoints
							)
						,	"DrawArc"
						);
					}
				nextArc:
					++itemCounter; if (itemCounter.MustBreakOrSuspend100()) return true;
				}
				itemCounter.Close();
				++tileCounter; if (tileCounter.MustBreakOrSuspend()) return true;
			}
			tileCounter.Close();
		}
		++mainCount; if (mainCount.MustBreakOrSuspend()) return true;
	}

	// draw labels
	if (mainCount == 1 && fd.HasLabelText()  && !layer->IsDisabledAspectGroup(AG_Label) )
	{
		LabelDrawer ld(fd); // allocate font(s) required for drawing labels

		ResumableCounter tileCounter(d.GetCounterStacks(), true);
		for (tile_id t=tileCounter.Value(); t!=tn; ++t)
		{
			const RectArrayType& rectArray = GetBoundingBoxCache<ScalarType>(layer)->GetBoundsArray(t);
			auto data = da->GetLockedDataRead(t);
			auto
				b = data.begin(),
				e = data.end();
			lfs_assert(rectArray.size() == e-b);
			SizeT tileIndexBase = featureItem->GetAbstrDomainUnit()->GetTileFirstIndex(t);

			ResumableCounter itemCounter(d.GetCounterStacks(), true);
			typename RectArrayType::const_iterator ri = rectArray.begin() + itemCounter.Value();

			for (auto i=b+itemCounter.Value(); i != e; ++itemCounter, ++i, ++ri)
			{
				if (itemCounter.MustBreakOrSuspend100()) 
					return true;
				SizeT entityIndex = (i-b) + tileIndexBase;
				if (indexCollector)
				{
					entityIndex = indexCollector->GetEntityIndex(entityIndex);
					if (!IsDefined(entityIndex))
						continue;
				}

				if (IsIntersecting(clipRect, *ri ))
					ld.DrawLabel(entityIndex, Convert<GPoint>(DynamicPoint(*i, 0.5) ));
			}
			itemCounter.Close();
			++tileCounter; if (tileCounter.MustBreakOrSuspend()) return true;
		}
		tileCounter.Close();
	}
	mainCount.Close();
	return false;
}

bool GraphicArcLayer::DrawImpl(FeatureDrawer& fd) const
{
	const AbstrDataItem* valuesItem = GetFeatureAttr();
	dms_assert(valuesItem);

	const PenIndexCache* penIndices = GetPenIndexCache(GetDefaultOrThemeColor(AN_PenColor) );
	if (!penIndices)
		return false;

	penIndices->UpdateForZoomLevel(fd.m_WorldZoomLevel, fd.m_Drawer.GetSubPixelFactor());

	switch (valuesItem->GetAbstrValuesUnit()->GetValueType()->GetValueClassID())
	{
		#define INSTANTIATE(P) case VT_##P: return DrawArcs< scalar_of<P>::type >(this, fd, penIndices);
			INSTANTIATE_SEQ_POINTS
		#undef INSTANTIATE
	}
	return false;
}

GRect GraphicArcLayer::GetFeaturePixelExtents(CrdType subPixelFactor) const
{
	GRect rect = base_type::GetFeaturePixelExtents(subPixelFactor);

	if (!IsDisabledAspectGroup(AG_Pen))
	{
		Int32 maxSize = GetMaxValue( GetEnabledTheme(AN_PenWidth).get(), DEFAULT_PEN_PIXEL_WIDTH )*subPixelFactor;

		rect |= GRect(
			- (maxSize+1), 
			- (maxSize+1),
			+ (maxSize+1), 
			+ (maxSize+1)
		);
	}	

	return rect;
}

CrdRect GraphicArcLayer::GetFeatureWorldExtents() const
{
	CrdRect rect = base_type::GetFeatureWorldExtents();

	if (!IsDisabledAspectGroup(AG_Pen))
	{
		CrdType 
			maxSize = GetMaxValue( GetEnabledTheme(AN_PenWorldWidth).get(), DEFAULT_PEN_WORLD_WIDTH );
		if (maxSize > 0)
			rect |= CrdRect(CrdPoint(- maxSize, - maxSize), CrdPoint(+ maxSize, + maxSize) );
	}
	return rect;
}

IMPL_DYNC_LAYERCLASS(GraphicArcLayer, ASE_Feature|ASE_OrderBy|ASE_Label|ASE_Pen|ASE_PixSizes|ASE_Selections, AN_PenColor, 1)

//----------------------------------------------------------------------
// class  : GraphicPolygonLayer
//----------------------------------------------------------------------

#include "geo/Centroid.h"
#include "geo/SelectPoint.h"
#include "geo/IsInside.h"

template <typename ScalarType>
row_id FindPolygonByPoint(
	const GraphicPolygonLayer* layer,
	const AbstrDataObject*     featureData,
	const Point<ScalarType>&   pnt)
{
	typedef Point<ScalarType>                           PointType;
	typedef sequence_traits<PointType>::container_type  PolygonType;
	typedef DataArray<PolygonType>                      DataArrayType;
	typedef BoundingBoxCache<ScalarType>::RectArrayType RectArrayType;

	const DataArrayType* da = debug_valcast<const DataArrayType*>(featureData);

	auto domain = featureData->GetTiledRangeData();

	for (tile_id t=0, tn = domain->GetNrTiles(); t<tn; ++t)
	{
		const RectArrayType& rectArray  = GetBoundingBoxCache<ScalarType>(layer)->GetBoundsArray(t);
		const RectArrayType& blockArray = GetBoundingBoxCache<ScalarType>(layer)->GetBlockBoundArray(t);

		auto data = da->GetLockedDataRead(t);
		auto
			b = data.begin(),
			e = data.end();
		typename RectArrayType::const_iterator ri = rectArray.end();
		SizeT i = data.size();
		if (!i) goto nextTile;

		if ((i & (BoundingBoxCache<ScalarType>::c_BlockSize - 1)))
		{
			if (!IsIncluding(blockArray[(i - 1) / BoundingBoxCache<ScalarType>::c_BlockSize], pnt))
			{
				i &= ~(BoundingBoxCache<ScalarType>::c_BlockSize - 1);
				e = b + i;
				ri = rectArray.begin() + i;
			}
		}
		// search backwards so that last drawn object will be first selected
		while (i)
		{
			if (!(i & (BoundingBoxCache<ScalarType>::c_BlockSize -1))) // bitwize modulo, assuming c_BlockSize is a power of 2
				while (!IsIncluding(blockArray[i / BoundingBoxCache<ScalarType>::c_BlockSize-1], pnt))
				{
					i  -= BoundingBoxCache<ScalarType>::c_BlockSize;
					ri -= BoundingBoxCache<ScalarType>::c_BlockSize;
					e  -= BoundingBoxCache<ScalarType>::c_BlockSize;
					if (!i) goto nextTile;
				}

			--e; --ri; --i;
			if (	IsIncluding(*ri, pnt)
				&& IsInside(e->begin(), e->end(), pnt))
				return domain->GetRowIndex(t, i);
		}
	nextTile:
		dms_assert(ri == rectArray.begin());
	}
	return UNDEFINED_VALUE(SizeT);
}

GraphicPolygonLayer::GraphicPolygonLayer(GraphicObject* owner)
	:	FeatureLayer(owner, GetStaticClass()) 
{}

SizeT GraphicPolygonLayer::_FindFeatureByPoint(const CrdPoint& geoPnt, const AbstrDataObject* featureData, ValueClassID vid)
{
	switch (vid)
	{
		#define INSTANTIATE(P) case VT_##P: return FindPolygonByPoint< scalar_of<P>::type >(this, featureData, Convert<P>(geoPnt));
			INSTANTIATE_SEQ_POINTS
		#undef INSTANTIATE
	}
	return UNDEFINED_VALUE(SizeT);
}

void GraphicPolygonLayer::SelectRect  (CrdRect worldRect, EventID eventID)
{
	throwItemError("SelectRect on PolygonLayer Not Yet Implemented (NYI)");
}

void GraphicPolygonLayer::SelectCircle(CrdPoint worldPnt, CrdType worldRadius, EventID eventID)
{
	throwItemError("SelectCircle on PolygonLayer Not Yet Implemented (NYI)");
}

void GraphicPolygonLayer::SelectPolygon(const CrdPoint* first, const CrdPoint* last, EventID eventID)
{
	throwItemError("SelectPolygon on PolygonLayer Not Yet Implemented (NYI)");
}

void GraphicPolygonLayer::_InvalidateFeature(SizeT featureIndex)
{
	dms_assert(IsDefined(featureIndex));

	DRect boundRect = GetBoundingBoxCache()->GetBounds(featureIndex);

	InvalidateWorldRect(GetGeoTransformation().Apply(boundRect) + GetFeatureWorldExtents(), nullptr);
}

#include "DrawPolygons.h"

bool GraphicPolygonLayer::DrawImpl(FeatureDrawer& fd) const
{
	const AbstrDataItem* valuesItem = GetFeatureAttr();
	dms_assert(valuesItem);

	HDC dc = fd.m_Drawer.GetDC();

	DcPolyFillModeSelector selectAlternatePolyFillMode(dc);

	const PenIndexCache* penIndices = GetPenIndexCache(GetDefaultOrThemeColor(AN_PenColor));

	if (penIndices)
		penIndices->UpdateForZoomLevel(fd.m_WorldZoomLevel, fd.m_Drawer.GetSubPixelFactor());

	switch (valuesItem->GetAbstrValuesUnit()->GetValueType()->GetValueClassID())
	{
		#define INSTANTIATE(P) case VT_##P: return DrawPolygons< scalar_of<P>::type >(this, fd, valuesItem, penIndices);
			INSTANTIATE_SEQ_POINTS
		#undef INSTANTIATE
	}
	return false;
}

GRect GraphicPolygonLayer::GetFeaturePixelExtents(CrdType subPixelFactor) const
{
	GRect rect = base_type::GetFeaturePixelExtents(subPixelFactor);
	if (!IsDisabledAspectGroup(AG_Pen))
	{
		Int32 maxSize = GetMaxValue( GetEnabledTheme(AN_PenWidth).get(), DEFAULT_PEN_PIXEL_WIDTH )*subPixelFactor;
		
		rect |= GRect(
			- (maxSize+1), 
			- (maxSize+1),
			+ (maxSize+1), 
			+ (maxSize+1)
		);
	}	

	return rect;
}

CrdRect GraphicPolygonLayer::GetFeatureWorldExtents() const
{
	CrdRect rect = base_type::GetFeatureWorldExtents();
	if (!IsDisabledAspectGroup(AG_Pen))
	{
		CrdType 
			maxSize = GetMaxValue( GetEnabledTheme(AN_PenWorldWidth).get(), DEFAULT_PEN_WORLD_WIDTH );
			
		if (maxSize > 0)
			rect|= CrdRect(CrdPoint(- maxSize, - maxSize), CrdPoint(+ maxSize, + maxSize) );
	}
	return rect;
}

IMPL_DYNC_LAYERCLASS(GraphicPolygonLayer, ASE_Feature|ASE_OrderBy|ASE_Label|ASE_Brush|ASE_Pen|ASE_PixSizes|ASE_Selections, AN_BrushColor, 2)

