#include "ShvDllPch.h"

#include "FeatureLayer.h"
#include "DrawPolygons.h"

#include "rtcTypeLists.h"
#include "act/TriggerOperator.h"
#include "act/UpdateMark.h"
#include "dbg/debug.h"
#include "mci/CompositeCast.h"
#include "mci/ValueClass.h"
#include "mci/ValueClassID.h"
#include "geo/Conversions.h"
#include "geo/Area.h"
#include "geo/IsInside.h"
#include "geo/PointOrder.h"
#include "geo/CentroidOrMid.h"
#include "geo/MinMax.h"
#include "ser/StringStream.h"
#include "utl/IncrementalLock.h"

#include "AbstrUnit.h"
#include "DataArray.h"
#include "UnitProcessor.h"

#include "StgBase.h"
#include "gdal/gdal_base.h"

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
				for (tile_id t=0, tn= labelArray->m_TileRangeData->GetNrTiles(); t!=tn; ++t)
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
	auto theme = GetTheme(an);
	if (theme && theme->IsAspectParameter())
		return theme->GetColorAspectValue();

	DmsColor* defaultClr = nullptr;
	switch (an) {
	case AN_BrushColor: defaultClr = &m_DefaultBrushColor; break;
	case AN_PenColor: defaultClr = &m_DefaultArcColor; break;
	case AN_SymbolColor: defaultClr = &m_DefaultPointColor; break;
	default: MG_CHECK(false);
	}
	MG_CHECK(defaultClr);
	UpdateDefaultColor(*defaultClr);
	return *defaultClr;
}


void FeatureLayer::DoUpdateView()
{
	if (!HasNoExtent())
	{
		const AbstrDataItem* valuesItem = GetFeatureAttr();
		assert(valuesItem);
		if (PrepareDataOrUpdateViewLater(valuesItem)) 
		{
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

std::shared_ptr<const AbstrBoundingBoxCache> FeatureLayer::GetBoundingBoxCache() const
{
	return visit_and_return_result<typelists::seq_points, std::shared_ptr<const AbstrBoundingBoxCache>>(
		GetFeatureAttr()->GetAbstrValuesUnit(), 
		[this]<typename P>(const Unit<P>*) -> std::shared_ptr<const AbstrBoundingBoxCache>
		{
			if (GetFeatureAttr()->GetValueComposition() == ValueComposition::Single)
				return ::GetPointBoundingBoxCache<scalar_of_t<P>>(this);
			return ::GetSequenceBoundingBoxCache<scalar_of_t<P>>(this);
		}
	);
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
			,	GetBaseProjectionUnitFromValuesUnit(GetFeatureAttr())
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
			,	GetBaseProjectionUnitFromValuesUnit(GetFeatureAttr())
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

TRect FeatureLayer::GetFeatureLogicalExtents() const
{
	if (!GetEnabledTheme(AN_LabelText))
		return TRect(0, 0, 0, 0);

	Int32 
		maxFontSizeY = GetMaxValue( GetEnabledTheme(AN_LabelSize).get(), DEFAULT_FONT_PIXEL_SIZE ),
		maxFontSizeX = ((maxFontSizeY * 6) / 10) * GetMaxLabelStrLen();

	return TRect(
		- ((FONT_DECIFONTSIZE_LEFT *maxFontSizeX)/10+1), 
		- ((FONT_DECIFONTSIZE_ABOVE*maxFontSizeY)/10+1),
		+ ((FONT_DECIFONTSIZE_RIGHT*maxFontSizeX)/10+1), 
		+ ((FONT_DECIFONTSIZE_BELOW*maxFontSizeY)/10+1)
	);
}

TRect FeatureLayer::GetBorderLogicalExtents() const
{
	return GetFeatureLogicalExtents();
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

CrdRect FeatureLayer::GetExtentsInflator(const CrdTransformation& tr) const
{
	CrdRect symbRect = 
		tr.WorldScale(
			g2dms_order<CrdType>(TRect2GRect(GetBorderLogicalExtents(), GetScaleFactors()))
		);

	symbRect += GetFeatureWorldExtents();
	return symbRect;
}

CrdRect FeatureLayer::GetWorldClipRect  (const GraphDrawer& d) const
{
	return d.GetWorldClipRect() + - GetExtentsInflator(d.GetTransformation());
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

	auto bbCache = GetBoundingBoxCache();
	auto featureCount = GetFeatureAttr()->GetAbstrDomainUnit()->GetCount();

	if (m_Themes[AN_Selections])
	{
		auto selTheme = CreateSelectionsTheme();
		assert(selTheme);
		const AbstrDataItem* selAttr = selTheme->GetThemeAttr();
		assert(selAttr);

		PreparedDataReadLock selLock(selAttr);

		SizeT f = featureCount;
		while (f)
		{
			auto entityID = Feature2EntityIndex(--f);
			if (selAttr->GetValue<SelectionID>(entityID))
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
				SizeT f = featureCount;
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
SizeT FindNearestPoint(const GraphicPointLayer* layer, const CrdPoint& geoPnt)
{
	using PointType = Point<ScalarType>;

	SizeT entityID = UNDEFINED_VALUE(SizeT);
	Float64 sqrDist = MAX_VALUE(Float64);

	auto featureData = layer->GetFeatureAttr()->GetRefObj();
	auto trd = featureData->GetTiledRangeData();
	auto da = const_array_cast<PointType>(featureData);

	for (tile_id t=0, tn = trd->GetNrTiles(); t!=tn; ++t)
	{

		auto data = da->GetTile(t);
		auto b = data.begin(), e = data.end();

		// search backwards so that last drawn object will be first selected
		while (b!=e)
		{
			--e;
			if (MakeMin(sqrDist, SqrDist<Float64>(Convert<CrdPoint>(*e), geoPnt)))
			{
				entityID = trd->GetRowIndex(t, e - data.begin());
			}
		}
	}
	return entityID;
}

void FeatureLayer::SelectPoint(CrdPoint worldPnt, EventID eventID)
{
	const AbstrDataItem* featureItem = GetFeatureAttr();
	assert(featureItem);

	SizeT featureIndex = UNDEFINED_VALUE(SizeT);
	if (!SuspendTrigger::DidSuspend() && featureItem->PrepareData())
	{
		DataReadLock lck(featureItem);
		dms_assert(lck.IsLocked());
		auto geoPoint = GetGeoTransformation().Reverse(worldPnt);
		featureIndex = FindFeatureByPoint(geoPoint);
	}
	if (SuspendTrigger::DidSuspend())
		return;

	InvalidationBlock lock(this);
	auto selectionTheme = CreateSelectionsTheme();
	MG_CHECK(selectionTheme);

	DataWriteLock writeLock(
		const_cast<AbstrDataItem*>(selectionTheme->GetThemeAttr()),
		CompoundWriteType(eventID)
	);

	if (SelectFeatureIndex(writeLock, featureIndex, eventID))
	{
		writeLock.Commit();
		lock.ProcessChange();
	}
}

SizeT GraphicPointLayer::FindFeatureByPoint(const CrdPoint& geoPnt)
{
	return visit_and_return_result<typelists::points, SizeT>(GetFeatureAttr()->GetAbstrValuesUnit(), 
		[this, &geoPnt] <typename P> (const Unit<P>*) 
		{
			return FindNearestPoint<scalar_of_t<P>>(this, geoPnt);
		}
	);
}

template <typename ScalarType>
bool SelectPointsInRect(GraphicPointLayer* layer, const AbstrDataObject* points, Range<Point<ScalarType>> geoRect, EventID eventID)
{
	using PointType = Point<ScalarType>;

	DataWriteLock writeLock(const_cast<AbstrDataItem*>(layer->CreateSelectionsTheme()->GetThemeAttr()), CompoundWriteType(eventID));
	bool result = false;

	auto da = const_array_cast<PointType>(points);
	auto trd = da->GetTiledRangeData();

	auto bbCache = GetPointBoundingBoxCache<ScalarType>(layer);
	assert(bbCache);

	parallel_tileloop_if(!layer->HasEntityIndex(), trd->GetNrTiles(), [da, trd, layer, bbCache, geoRect, &writeLock, &result, eventID](tile_id t)
		{
			const auto& rectArray = bbCache->GetBoxData(t);
			if (!IsIntersecting(geoRect, rectArray.m_TotalBound))
				return;

			auto data = da->GetTile(t);
			tile_offset ts = data.size();
			for (tile_offset i = 0; i != ts; ++i)
			{
				if (i % AbstrBoundingBoxCache::c_BlockSize == 0)
					while (!IsIntersecting(geoRect, rectArray.m_BlockBoundArray[i / AbstrBoundingBoxCache::c_BlockSize]))
						if ((i += AbstrBoundingBoxCache::c_BlockSize) >= ts)
							return;

				if (IsIncluding(geoRect, data[i]))
				{
					SizeT entityID = trd->GetRowIndex(t, i);
					if (layer->SelectFeatureIndex(writeLock, entityID, eventID))
						result = true;
				}
			}
		}
	);
	if (result)
		writeLock.Commit();
	return result;
}

template <typename ScalarType>
bool SelectArcsInRect(FeatureLayer* layer, const AbstrDataObject* arcs, Range<Point<ScalarType>> geoRect, EventID eventID)
{
	using PointType = Point<ScalarType>;
	using ArcType = typename sequence_traits<PointType>::container_type ;

	DataWriteLock writeLock(const_cast<AbstrDataItem*>(layer->CreateSelectionsTheme()->GetThemeAttr()), CompoundWriteType(eventID));
	bool result = false;

	auto da = const_array_cast<ArcType>(arcs);
	auto trd = arcs->GetTiledRangeData();

	auto bbCache = GetSequenceBoundingBoxCache<ScalarType>(layer);
	assert(bbCache);

	parallel_tileloop_if(!layer->HasEntityIndex(), trd->GetNrTiles(), [da, trd, layer, bbCache, geoRect, &writeLock, &result, eventID](tile_id t)
		{
			const auto& rectArray = bbCache->GetBoxData(t);
			if (!IsIntersecting(geoRect, rectArray.m_TotalBound))
				return;

			auto data = da->GetTile(t);
			tile_offset ts = data.size();
			for (tile_offset i=0; i!=ts; ++i)
			{
				if (i % AbstrBoundingBoxCache::c_BlockSize == 0)
					while(!IsIntersecting(geoRect, rectArray.m_BlockBoundArray[i / AbstrBoundingBoxCache::c_BlockSize]))
						if ((i += AbstrBoundingBoxCache::c_BlockSize) >= ts)
							return;

				if (IsIncluding(geoRect, rectArray.m_FeatBoundArray[i]))
				{
					SizeT entityID = trd->GetRowIndex(t, i);
					if (layer->SelectFeatureIndex(writeLock, entityID, eventID))
						result = true;
				}
			}
		}
	);
	if (result)
		writeLock.Commit();
	return result;
}


template <typename ScalarType>
bool SelectPointsInCircle(GraphicPointLayer* layer, const AbstrDataObject* points, Point<ScalarType> geoPnt, ScalarType geoRadius, EventID eventID)
{
	using PointType = Point<ScalarType>;
	using RangeType = Range<PointType>;

	auto radialDelta = PointType(geoRadius, geoRadius);
	auto geoRect  = Inflate(geoPnt, radialDelta);
	auto geoRadius2 = Norm<CrdType>(radialDelta);

	DataWriteLock writeLock(const_cast<AbstrDataItem*>(layer->CreateSelectionsTheme()->GetThemeAttr()), CompoundWriteType(eventID));
	bool result = false;

	auto da = const_array_cast<PointType>(points);
	auto trd = points->GetTiledRangeData();

	auto bbCache = GetPointBoundingBoxCache<ScalarType>(layer);
	assert(bbCache);

	parallel_tileloop_if(!layer->HasEntityIndex(), trd->GetNrTiles(), [da, trd, layer, bbCache, geoRect, geoPnt, geoRadius2, &writeLock, &result, eventID](tile_id t)
		{
			const auto& rectArray = bbCache->GetBoxData(t);
			if (!IsIntersecting(geoRect, rectArray.m_TotalBound))
				return;

			auto data = da->GetTile(t);
			tile_offset ts = data.size();
			for (tile_offset i = 0; i != ts; ++i)
			{
				if (i % AbstrBoundingBoxCache::c_BlockSize == 0)
					while (!IsIntersecting(geoRect, rectArray.m_BlockBoundArray[i / AbstrBoundingBoxCache::c_BlockSize]))
						if ((i += AbstrBoundingBoxCache::c_BlockSize) >= ts)
							return;

				auto dataPnt = data[i];
				if (IsIncluding(geoRect, dataPnt) && SqrDist<CrdType>(geoPnt, dataPnt) <= geoRadius2)
				{
					SizeT entityID = trd->GetRowIndex(t, i);
					result |= layer->SelectFeatureIndex(writeLock, entityID, eventID);
				}
			}
		}
	);
	if (result)
		writeLock.Commit();
	return result;
}

template <typename ScalarType>
bool SelectArcsInCircle(FeatureLayer* layer, const AbstrDataObject* arcs, CrdPoint worldPnt, CrdType worldRadius, EventID eventID)
{
	using PointType = Point<ScalarType>;
	using ArcType = typename sequence_traits<PointType>::container_type;

	CrdRect worldRect = Inflate(worldPnt, CrdPoint(worldRadius, worldRadius));
	CrdPoint geoPnt = layer->GetGeoTransformation().Reverse(worldPnt);
	auto geoRect = Convert<Range<PointType>>(layer->GetGeoTransformation().Reverse(worldRect));
	CrdType geoRadius2 = Area(geoRect) / 4;

	PointType geoPntInT = Convert<PointType>(geoPnt);

	DataWriteLock writeLock(const_cast<AbstrDataItem*>(layer->CreateSelectionsTheme()->GetThemeAttr()), CompoundWriteType(eventID));
	bool result = false;

	auto da = const_array_cast<ArcType>(arcs);
	auto trd = arcs->GetTiledRangeData();

	auto bbCache = GetSequenceBoundingBoxCache<ScalarType>(layer);
	assert(bbCache);

	parallel_tileloop_if(!layer->HasEntityIndex(), trd->GetNrTiles(), [da, trd, layer, bbCache, geoRect, geoPntInT, geoRadius2, &writeLock, &result, eventID](tile_id t)
		{
			const auto& rectArray = bbCache->GetBoxData(t);
			if (!IsIntersecting(geoRect, rectArray.m_TotalBound))
				return;

			auto data = da->GetTile(t);
			tile_offset ts = data.size();
			for (tile_offset i = 0; i != ts; ++i)
			{
				if (i % AbstrBoundingBoxCache::c_BlockSize == 0) 
					while (!IsIntersecting(geoRect, rectArray.m_BlockBoundArray[i / AbstrBoundingBoxCache::c_BlockSize]))
						if ((i += AbstrBoundingBoxCache::c_BlockSize) >= ts)
							return;

				auto arcCPtr = data.begin() + i;
				if (IsIncluding(geoRect, rectArray.m_FeatBoundArray[i]))
				{
					for (const auto& p : *arcCPtr)
						if (SqrDist<CrdType>(geoPntInT, p) > geoRadius2)
							goto nextArc;

					SizeT entityID = trd->GetRowIndex(t, i);
					if (layer->SelectFeatureIndex(writeLock, entityID, eventID))
						result = true;
				}
			nextArc:
				;
			}
		}
	);
	if (result)
		writeLock.Commit();
	return result;
}

template <typename ScalarType>
bool SelectPointsInPolygon(GraphicPointLayer* layer, const AbstrDataObject* points, CrdRect worldRect, const CrdPoint* first, const CrdPoint* last, EventID eventID)
{
	using PointType = Point<ScalarType>;
	using RangeType = Range<PointType>;
	using ArcType = typename sequence_traits<PointType>::container_type;

	CrdTransformation geo2worldTr = layer->GetGeoTransformation();
	auto geoRect = Convert<RangeType>(geo2worldTr.Reverse(worldRect));

	DataWriteLock writeLock(const_cast<AbstrDataItem*>(layer->CreateSelectionsTheme()->GetThemeAttr()), CompoundWriteType(eventID));
	bool result = false;

	auto da = const_array_cast<PointType>(points);
	auto trd = points->GetTiledRangeData();

	auto bbCache = GetPointBoundingBoxCache<ScalarType>(layer);
	assert(bbCache);

	parallel_tileloop_if(!layer->HasEntityIndex(), trd->GetNrTiles(), [da, trd, layer, bbCache, geoRect, first, last, geo2worldTr, &writeLock, &result, eventID](tile_id t)
		{
			const auto& rectArray = bbCache->GetBoxData(t);
			if (!IsIntersecting(geoRect, rectArray.m_TotalBound))
				return;

			auto data = da->GetTile(t);
			tile_offset ts = data.size();
			for (tile_offset i = 0; i != ts; ++i)
			{
				if (i % AbstrBoundingBoxCache::c_BlockSize == 0)
					while (!IsIntersecting(geoRect, rectArray.m_BlockBoundArray[i / AbstrBoundingBoxCache::c_BlockSize]))
						if ((i += AbstrBoundingBoxCache::c_BlockSize) >= ts)
							return;

				auto geoPnt = data[i];
				if (IsIncluding(geoRect, geoPnt))
				{
					CrdPoint worldPnt = geo2worldTr.Apply(geoPnt);
					if (IsInside(first, last, worldPnt))
					{
						SizeT entityID = trd->GetRowIndex(t, i);
						result |= layer->SelectFeatureIndex(writeLock, entityID, eventID);
					}
				}
			}
		}
	);
	if (result)
		writeLock.Commit();
	return result;
}

#include "AbstrController.h"
#include "ViewPort.h"

void GraphicPointLayer::SelectRect(CrdRect worldRect, EventID eventID)
{
	const AbstrDataItem* featureAttr = GetFeatureAttr();
	dms_assert(featureAttr);

	InvalidationBlock lock1(this);

	dms_assert(  eventID & EID_REQUEST_SEL  );
	dms_assert(!(eventID & EID_REQUEST_INFO));

	bool result = false;

	if (featureAttr->PrepareData())
	{
		auto layer2worldTransformation = GetGeoTransformation();
		CrdRect geoRect = layer2worldTransformation.Reverse(worldRect);

		DataReadLock lck(featureAttr); 
		dms_assert(lck.IsLocked());

		result = visit_and_return_result<typelists::points, bool>(featureAttr->GetAbstrValuesUnit(), 
			[this, featureAttr, geoRect, eventID, &result] <typename P> (const Unit<P>*) 
			{
				return SelectPointsInRect< scalar_of_t<P> >(this, featureAttr->GetRefObj(), Convert<Range<P>>(geoRect), eventID);
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
		auto layer2worldTransformation = GetGeoTransformation();
		CrdPoint geoPnt = layer2worldTransformation.Reverse(worldPnt);
		CrdType  geoRadius = worldRadius;
		if (!layer2worldTransformation.IsSingular())
			geoRadius /= std::abs(layer2worldTransformation.Factor().X());

		DataReadLock lck(valuesItem);
		dms_assert(lck.IsLocked());

		result = visit_and_return_result<typelists::points, bool>(valuesItem->GetAbstrValuesUnit(), 
			[this, valuesItem, geoRadius, eventID, geoPnt, &result] <typename P> (const Unit<P>*) 
			{
				return SelectPointsInCircle< scalar_of_t<P> >(this, valuesItem->GetRefObj(), Convert<P>(geoPnt), Convert<scalar_of_t<P>>(geoRadius), eventID);
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

		auto f = featureAttr->GetAbstrDomainUnit()->GetCount();
		while (f)
		{
			auto entityID = Feature2EntityIndex(--f);
			if (selAttr->GetValue<SelectionID>(entityID))
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

void GraphicPointLayer::InvalidateFeature(SizeT featureIndex)
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
	auto trd = da->GetTiledRangeData();
	tile_id tn = trd->GetNrTiles();

	const GraphDrawer& d =fd.m_Drawer;

	DcTextAlignSelector selectCenterAlignment(d.GetDC(), TA_CENTER|TA_BASELINE|TA_NOUPDATECP);

	CrdTransformation transformer = d.GetTransformation();

	RangeType geoRect = Convert<RangeType>( layer->GetWorldClipRect(d) );

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
				auto data = da->GetTile(t);
				auto b = data.begin(), e = data.end();

				ResumableCounter itemCounter(d.GetCounterStacks(), true);

				for (auto i = b + itemCounter; i != e; ++i)
				{
					if (IsIncluding(geoRect, *i))
					{
						entity_id entityIndex = trd->GetRowIndex(t, itemCounter);

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

						auto viewPoint = Convert<TPoint>(transformer.Apply(*i));	
					
						CheckedGdiCall(
							TextOutW(
								d.GetDC(), 
								viewPoint.X(), viewPoint.Y(),
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

		ResumableCounter tileCounter(d.GetCounterStacks(), true);
		for (tile_id t=tileCounter; t!=tn; ++t)
		{
			auto data = da->GetTile(t);
			auto b = data.begin(), e = data.end();

			ResumableCounter itemCounter(d.GetCounterStacks(), true);

			for (auto i = b + itemCounter; i != e; ++i)
			{
				if (IsIncluding(geoRect, *i ))
				{
					auto viewDPoint = transformer.Apply(*i);
					GPoint viewPoint(viewDPoint.X(), viewDPoint.Y());

					SizeT entityIndex = trd->GetRowIndex(t, itemCounter);
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

TRect GraphicPointLayer::GetFeatureLogicalExtents() const
{
	TRect rect = base_type::GetFeatureLogicalExtents();
	if (!IsDisabledAspectGroup(AG_Symbol))
	{
		Int32 
			maxFontSizeY = GetMaxValue( GetEnabledTheme(AN_SymbolSize).get(), DEFAULT_SYMB_PIXEL_SIZE ),
			maxFontSizeX = (maxFontSizeY*6)/10;

		rect |= TRect(
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

			auto n = f1->GetCount();

			for (auto i = itemCounter.Value(); i != n; ++i)
			{
				auto
					f1i = f1->GetCardinalValue(i)
				,	f2i = f2->GetCardinalValue(i);

				PointType p1 = pointDataBegin[f1i];
				PointType p2 = pointDataBegin[f2i];
				if (IsIntersecting(clipRect, RangeType(p1, p2) ))
				{
					auto dp1 = transformer.Apply(p1), dp2 = transformer.Apply(p2);
					pointBuffer[0] = GPoint(dp1.X(), dp1.Y());
					pointBuffer[1] = GPoint(dp2.X(), dp2.Y());
					if (penIndices)
					{
						auto entityIndex = i;
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

			f1->GetThemeAttr()->GetAbstrDomainUnit()->GetPreparedCount();
			f1->GetThemeAttr()->GetAbstrValuesUnit()->GetPreparedCount();

			penIndices->UpdateForZoomLevel(fd.m_WorldZoomLevel, fd.m_Drawer.GetSubPixelFactor());
			bool result = visit_and_return_result<typelists::points, bool>(valuesItem->GetAbstrValuesUnit(),
				[this, valuesItem, penIndices, f1, f2, &fd] <typename P> (const Unit<P>*) 
				{
					return DrawNetwork< scalar_of_t<P> >(this, fd, valuesItem, penIndices, GetValueGetter(f1.get()), GetValueGetter(f2.get()));
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
SizeT FindArcByPoint(const GraphicArcLayer* layer, const Point<ScalarType>& pnt)
{
	using PointType = Point<ScalarType>;
	using PointSequenceType = typename sequence_traits<PointType>::container_type;

	auto featureData = layer->GetFeatureAttr()->GetRefObj();
	auto da = const_array_cast<PointSequenceType>(featureData);
	auto bbCache = GetSequenceBoundingBoxCache<ScalarType>(layer);
	assert(bbCache);

	SizeT entityID = UNDEFINED_VALUE(SizeT);

	ArcProjectionHandle<Float64, ScalarType> aph(pnt, MAX_VALUE(Float64));

	auto trd = featureData->GetTiledRangeData();
	for (tile_id t=trd->GetNrTiles(); t--; )
	{
		if (aph.CanSkip(bbCache->GetBoxData(t).m_TotalBound))
			continue;

		const auto& blocksArray = bbCache->GetBlockBoundArray(t);
		const auto& boundsArray = bbCache->GetBoundsArray(t);

		auto data = da->GetLockedDataRead(t);
		auto
			b = data.begin(),
			e = data.end();
		auto s = data.size();
		// search backwards so that last drawn object will be first selected
		if (s % AbstrBoundingBoxCache::c_BlockSize != 0)
			if (aph.CanSkip(blocksArray[s / AbstrBoundingBoxCache::c_BlockSize]))
				s &= ~(AbstrBoundingBoxCache::c_BlockSize - 1);
		while (s)
		{
			if (s % AbstrBoundingBoxCache::c_BlockSize == 0)
				while (aph.CanSkip(blocksArray[s / AbstrBoundingBoxCache::c_BlockSize - 1]))
				{
					s -= AbstrBoundingBoxCache::c_BlockSize;
					if (!s)
						goto continue_next_tile;
				}
			assert(s);
			--s; // now s could be zero
			if (aph.CanSkip(boundsArray[s]))
				continue;
			auto i = b + s;
			if (aph.Project2Arc(begin_ptr(*i), end_ptr(*i)))
				entityID = trd->GetRowIndex(t, s);
		}
	continue_next_tile:;
	}
	return entityID;
}

GraphicArcLayer::GraphicArcLayer(GraphicObject* owner)
	:	FeatureLayer(owner, GetStaticClass()) 
{}

SizeT GraphicArcLayer::FindFeatureByPoint(const CrdPoint& geoPnt)
{
	auto result = UNDEFINED_VALUE(SizeT);

	visit<typelists::seq_points>(GetFeatureAttr()->GetAbstrValuesUnit(),
		[this, &geoPnt, &result] <typename P> (const Unit<P>*)
		{
			result = FindArcByPoint< scalar_of_t<P> >(this, Convert<P>(geoPnt));
		}
	);

	return result;
}

void GraphicArcLayer::SelectRect  (CrdRect worldRect, EventID eventID)
{
	const AbstrDataItem* valuesItem = GetFeatureAttr();
	dms_assert(valuesItem);

	InvalidationBlock lock1(this);

	dms_assert(eventID & EID_REQUEST_SEL);
	dms_assert(!(eventID & EID_REQUEST_INFO));

	bool result = false;

	if (valuesItem->PrepareData())
	{
		CrdRect geoRect = GetGeoTransformation().Reverse(worldRect);

		DataReadLock lck(valuesItem);
		dms_assert(lck.IsLocked());

		result = visit_and_return_result<typelists::seq_points, bool>(valuesItem->GetAbstrValuesUnit(),
			[this, valuesItem, geoRect, eventID, &result] <typename P> (const Unit<P>*)
			{
				return SelectArcsInRect< scalar_of_t<P> >(this, valuesItem->GetRefObj(), Convert<Range<P>>(geoRect), eventID);
			}
		);
	}
	if (result && !HasEntityAggr())
	{
		lock1.ProcessChange();
		InvalidateWorldRect(worldRect + GetFeatureWorldExtents(), nullptr);
	}
}

void GraphicArcLayer::SelectCircle(CrdPoint worldPnt, CrdType worldRadius, EventID eventID)
{
	const AbstrDataItem* valuesItem = GetFeatureAttr();
	dms_assert(valuesItem);

	InvalidationBlock lock1(this);

	dms_assert(eventID & EID_REQUEST_SEL);
	dms_assert(!(eventID & EID_REQUEST_INFO));

	bool result = false;

	if (valuesItem->PrepareData())
	{
		auto layer2worldTransformation = GetGeoTransformation();

		DataReadLock lck(valuesItem);
		dms_assert(lck.IsLocked());

		result = visit_and_return_result<typelists::seq_points, bool>(valuesItem->GetAbstrValuesUnit(),
			[this, valuesItem, worldPnt, worldRadius, eventID] <typename P> (const Unit<P>*)
			{
				return SelectArcsInCircle< scalar_of_t<P> >(this, valuesItem->GetRefObj(), worldPnt, worldRadius, eventID);
			}
		);
	}
	if (result && !HasEntityAggr())
	{
		lock1.ProcessChange();
		InvalidateWorldRect(Inflate(worldPnt, CrdPoint(worldRadius, worldRadius)) + GetFeatureWorldExtents(), nullptr);
	}
}

void GraphicArcLayer::SelectPolygon(const CrdPoint* first, const CrdPoint* last, EventID eventID)
{
	throwItemError("SelectPolygon on ArcLayer Not Yet Implemented (NYI)");
}

void GraphicArcLayer::InvalidateFeature(SizeT featureIndex)
{
	dms_assert(IsDefined(featureIndex));

	DRect boundRect = GetBoundingBoxCache()->GetBounds(featureIndex);
	InvalidateWorldRect(GetGeoTransformation().Apply(boundRect) + GetFeatureWorldExtents(), nullptr);
}

// TODO: SelectedColor and selectedOnly
template <typename ScalarType>
bool DrawArcs(const GraphicArcLayer* layer, const FeatureDrawer& fd, const PenIndexCache*  penIndices)
{
	typedef Point<ScalarType>                           PointType;
	typedef Range<PointType>                            RangeType;
	typedef sequence_traits<PointType>::container_type  PolygonType;
	typedef DataArray<PolygonType>                      DataArrayType;
	typedef SequenceBoundingBoxCache<ScalarType>::RectArrayType RectArrayType;

	const AbstrDataItem* featureItem = layer->GetFeatureAttr();
	const GraphDrawer& d = fd.m_Drawer;

	const DataArrayType* da = const_array_cast<PolygonType>(featureItem);
	auto trd = da->GetTiledRangeData();
	tile_id tn = trd->GetNrTiles();

	CrdTransformation transformer = d.GetTransformation();
	auto geoRect = Convert<RangeType>( layer->GetWorldClipRect(d) );

	CrdType zoomLevel = Abs(transformer.ZoomLevel());
	dms_assert(zoomLevel > 1.0e-30); // we assume that nothing remains visible on such a small scale to avoid numerical overflow in the following inversion

	ScalarType minWorldWidth  = MIN_WORLD_SIZE / zoomLevel;
	ScalarType minWorldHeight = minWorldWidth;

	ResumableCounter mainCount(d.GetCounterStacks(), false);

	bool selectedOnly = layer->ShowSelectedOnly();
	SelectionIdCPtr selectionsArray; assert(!selectionsArray);
	if (fd.m_SelValues)
	{
		selectionsArray = fd.m_SelValues.value().begin();
		assert(selectionsArray);
	}

	WeakPtr<const IndexCollector> indexCollector = fd.GetIndexCollector();
	PenArray::SafePenHandle specialPenHolder;

	SizeT fe = UNDEFINED_VALUE(SizeT);
	if (layer->IsActive())
		fe = layer->GetFocusElemIndex();

	auto bbCache = GetSequenceBoundingBoxCache<ScalarType>(layer);

	if (mainCount == 0)
	{
		if (!layer->IsDisabledAspectGroup(AG_Pen))
		{
			std::vector<POINT> pointBuffer;

			PenArray pa(d.GetDC(), penIndices);
			ResumableCounter tileCounter(d.GetCounterStacks(), true);
			while (tileCounter !=tn)
			{
				const auto& rectArray = bbCache->GetBoxData(tileCounter);
				if (IsIntersecting(geoRect, rectArray.m_TotalBound))
				{
					auto data = da->GetTile(tileCounter);
					auto b = data.begin();
					lfs_assert(rectArray.m_FeatBoundArray.size() == data.size());
					tile_offset ts = data.size();
					ResumableCounter itemCounter(d.GetCounterStacks(), true);

					while (itemCounter < ts)
					{
						if (itemCounter % AbstrBoundingBoxCache::c_BlockSize == 0)
							while(!IsIntersecting(geoRect, rectArray.m_BlockBoundArray[itemCounter / AbstrBoundingBoxCache::c_BlockSize]))
							{
								itemCounter += AbstrBoundingBoxCache::c_BlockSize;
								if (itemCounter >= ts)
									goto endOfTile;
								if (itemCounter.MustBreak())
									return true;
							}

						auto arcCPtr = b + itemCounter;
						if (IsIntersecting(geoRect, rectArray.m_FeatBoundArray[itemCounter]))
						{

							UInt32 nrPoints = arcCPtr->size();
							pointBuffer.resize(nrPoints);
							auto bi = pointBuffer.begin();

							for (auto pnt : *arcCPtr)
							{
								auto deviceDPoint = transformer.Apply(pnt);
								*bi++ = GPoint(deviceDPoint.X(), deviceDPoint.Y());
							}

							// remove duplicates
							pointBuffer.erase(
								std::unique(pointBuffer.begin(), pointBuffer.end(), [](auto a, auto b) { return a.x == b.x && a.y == b.y;  })
								, pointBuffer.end()
							);

							if (pointBuffer.size() < 2)
								goto nextArc;

							entity_id entityIndex = trd->GetRowIndex(tileCounter, itemCounter);
							if (indexCollector)
							{
								entityIndex = indexCollector->GetEntityIndex(entityIndex);
								if (!IsDefined(entityIndex))
									goto nextArc;
							}

							bool isSelected = selectionsArray && SelectionID(selectionsArray[entityIndex]);
							if (selectedOnly)
							{
								if (!isSelected) goto nextArc;
								isSelected = false;
							}

							if (entityIndex == fe || isSelected)
							{
								int width = 4 * d.GetSubPixelFactor();
								assert(width > 0);
								if (penIndices)
								{
									width += penIndices->GetWidth(entityIndex);
									assert(width > 0);
								}
								width += width / 2;
								assert(width > 0);

								COLORREF brushColor = (entityIndex == fe)
									? ::GetSysColor(COLOR_HIGHLIGHT)
									: GetSelectedClr(selectionsArray[entityIndex]);

								specialPenHolder = CreatePen(PS_SOLID, width, brushColor);
								pa.SetSpecificPen(specialPenHolder);
							}
							else if (penIndices)
							{
								if (!pa.SelectPen(penIndices->GetKeyIndex(entityIndex)))
									goto nextArc;
							}
							else
								pa.ResetPen();


							CheckedGdiCall(
								Polyline(
									d.GetDC(),
									begin_ptr(pointBuffer),
									pointBuffer.size()
								)
								, "DrawArc"
							);
						}
					nextArc:
						++itemCounter;
						if (itemCounter.MustBreakOrSuspend100())
							return true;
					}
				endOfTile:
					itemCounter.Close();
				}
				++tileCounter;
				if (tileCounter.MustBreakOrSuspend()) return true;
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
		for (tile_id t=tileCounter; t!=tn; ++t)
		{
			const auto& rectArray = bbCache->GetBoxData(tileCounter);
			auto data = da->GetTile(t);
			tile_id ts = data.size();
			lfs_assert(rectArray.m_FeatBoundArray.size() == ts);

			ResumableCounter itemCounter(d.GetCounterStacks(), true);

			while (itemCounter < ts)
			{
				if (itemCounter.MustBreakOrSuspend100())
					return true;
				SizeT entityIndex = trd->GetRowIndex(t, itemCounter);
				if (indexCollector)
				{
					entityIndex = indexCollector->GetEntityIndex(entityIndex);
					if (!IsDefined(entityIndex))
						continue;
				}

				if (IsIntersecting(geoRect, rectArray.m_FeatBoundArray[itemCounter]))
				{
					auto dp = DynamicPoint(data[itemCounter], 0.5);
					ld.DrawLabel(entityIndex, GPoint(dp.X(), dp.Y()));
				}

				++itemCounter;
				if (itemCounter.MustBreakOrSuspend100())
					return true;
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
	bool result = false;

	visit<typelists::seq_points>(valuesItem->GetAbstrValuesUnit(),
		[&result, this, penIndices, &fd]<typename P>(const Unit<P>*) 
		{
			result = DrawArcs< scalar_of_t<P> >(this, fd, penIndices);
		}
	);

	return result;
}

TRect GraphicArcLayer::GetFeatureLogicalExtents() const
{
	TRect rect = base_type::GetFeatureLogicalExtents();

	if (!IsDisabledAspectGroup(AG_Pen))
	{
		Int32 maxSize = GetMaxValue( GetEnabledTheme(AN_PenWidth).get(), DEFAULT_PEN_PIXEL_WIDTH );

		rect |= TRect(
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
row_id FindPolygonByPoint(const GraphicPolygonLayer* layer, Point<ScalarType> pnt)
{
	using PointType = Point<ScalarType>;
	using PointSequenceType = typename sequence_traits<PointType>::container_type;

	auto featureData = layer->GetFeatureAttr()->GetRefObj();
	auto da = const_array_cast<PointSequenceType>(featureData);

	auto bbCache = GetSequenceBoundingBoxCache<ScalarType>(layer);
	auto trd = featureData->GetTiledRangeData();
	for (tile_id t = trd->GetNrTiles(); t--; )
	{
		const auto& rectArray  = bbCache->GetBoxData(t);
		if (!IsIncluding(rectArray.m_TotalBound, pnt))
			continue;

		auto data = da->GetTile(t);
		auto b = data.begin();
		SizeT i = data.size();

		if (i % AbstrBoundingBoxCache::c_BlockSize != 0)
			if (!IsIncluding(rectArray.m_BlockBoundArray[(i - 1) / AbstrBoundingBoxCache::c_BlockSize], pnt))
				i &= ~(AbstrBoundingBoxCache::c_BlockSize - 1);

		// search backwards so that last drawn object will be first selected
		while (i)
		{
			if (i % AbstrBoundingBoxCache::c_BlockSize == 0)
				while (!IsIncluding(rectArray.m_BlockBoundArray[i / AbstrBoundingBoxCache::c_BlockSize-1], pnt))
					if ((i -= AbstrBoundingBoxCache::c_BlockSize) == 0)
						goto nextTile;
			--i;
			auto polygonPtr = b + i;
			if (IsIncluding(rectArray.m_FeatBoundArray[i], pnt) && IsInside(polygonPtr->begin(), polygonPtr->end(), pnt))
				return trd->GetRowIndex(t, i);
		}
	nextTile:;
	}
	return UNDEFINED_VALUE(SizeT);
}

GraphicPolygonLayer::GraphicPolygonLayer(GraphicObject* owner)
	:	FeatureLayer(owner, GetStaticClass()) 
{}

SizeT GraphicPolygonLayer::FindFeatureByPoint(const CrdPoint& geoPnt)
{
	auto result = UNDEFINED_VALUE(SizeT);

	visit<typelists::seq_points>(GetFeatureAttr()->GetAbstrValuesUnit(),
		[this, &geoPnt, &result] <typename P> (const Unit<P>*)
		{
			result = FindPolygonByPoint<scalar_of_t<P>>(this, Convert<P>(geoPnt));
		}
	);

	return result;
}

void GraphicPolygonLayer::SelectRect  (CrdRect worldRect, EventID eventID)
{
	const AbstrDataItem* valuesItem = GetFeatureAttr();
	dms_assert(valuesItem);

	InvalidationBlock lock1(this);

	dms_assert(eventID & EID_REQUEST_SEL);
	dms_assert(!(eventID & EID_REQUEST_INFO));

	bool result = false;

	if (valuesItem->PrepareData())
	{
		CrdRect geoRect = GetGeoTransformation().Reverse(worldRect);

		DataReadLock lck(valuesItem);
		dms_assert(lck.IsLocked());

		result = visit_and_return_result<typelists::seq_points, bool>(valuesItem->GetAbstrValuesUnit(),
			[this, valuesItem, geoRect, eventID] <typename P> (const Unit<P>*)
		{
			return SelectArcsInRect< scalar_of_t<P> >(this, valuesItem->GetRefObj(), Convert<Range<P>>(geoRect), eventID);
		}
		);
	}
	if (result && !HasEntityAggr())
	{
		lock1.ProcessChange();
		InvalidateWorldRect(worldRect + GetFeatureWorldExtents(), nullptr);
	}
}

void GraphicPolygonLayer::SelectCircle(CrdPoint worldPnt, CrdType worldRadius, EventID eventID)
{
	const AbstrDataItem* valuesItem = GetFeatureAttr();
	dms_assert(valuesItem);

	InvalidationBlock lock1(this);

	dms_assert(eventID & EID_REQUEST_SEL);
	dms_assert(!(eventID & EID_REQUEST_INFO));

	bool result = false;

	if (valuesItem->PrepareData())
	{
		auto layer2worldTransformation = GetGeoTransformation();

		DataReadLock lck(valuesItem);
		dms_assert(lck.IsLocked());

		result = visit_and_return_result<typelists::seq_points, bool>(valuesItem->GetAbstrValuesUnit(),
			[this, valuesItem, worldPnt, worldRadius, eventID] <typename P> (const Unit<P>*)
			{
				return SelectArcsInCircle< scalar_of_t<P> >(this, valuesItem->GetRefObj(), worldPnt, worldRadius, eventID);
			}
		);
	}
	if (result && !HasEntityAggr())
	{
		lock1.ProcessChange();
		InvalidateWorldRect(Inflate(worldPnt, CrdPoint(worldRadius, worldRadius)) + GetFeatureWorldExtents(), nullptr);
	}
}

void GraphicPolygonLayer::SelectPolygon(const CrdPoint* first, const CrdPoint* last, EventID eventID)
{
	throwItemError("SelectPolygon on PolygonLayer Not Yet Implemented (NYI)");
}

void GraphicPolygonLayer::InvalidateFeature(SizeT featureIndex)
{
	dms_assert(IsDefined(featureIndex));

	DRect boundRect = GetBoundingBoxCache()->GetBounds(featureIndex);

	InvalidateWorldRect(GetGeoTransformation().Apply(boundRect) + GetFeatureWorldExtents(), nullptr);
}

bool GraphicPolygonLayer::DrawImpl(FeatureDrawer& fd) const
{
	auto featureItem = GetFeatureAttr();
	assert(featureItem);

	HDC dc = fd.m_Drawer.GetDC();

	DcPolyFillModeSelector selectAlternatePolyFillMode(dc);

	auto penIndices = GetPenIndexCache(GetDefaultOrThemeColor(AN_PenColor));

	if (penIndices)
		penIndices->UpdateForZoomLevel(fd.m_WorldZoomLevel, fd.m_Drawer.GetSubPixelFactor());

	bool result = false;
	visit<typelists::seq_points>(featureItem->GetAbstrValuesUnit(), [&result, this, &fd, featureItem, penIndices]<typename P>(const Unit<P>*)
		{
			result = DrawPolygons<scalar_of_t<P>>(this, fd, featureItem, penIndices);
		}
	);
	return result;
}

TRect GraphicPolygonLayer::GetFeatureLogicalExtents() const
{
	TRect rect = base_type::GetFeatureLogicalExtents();
	if (!IsDisabledAspectGroup(AG_Pen))
	{
		Int32 maxSize = GetMaxValue( GetEnabledTheme(AN_PenWidth).get(), DEFAULT_PEN_PIXEL_WIDTH );
		
		rect |= TRect(
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