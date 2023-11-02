// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#include "LayerInfo.h"

#include "dbg/DmsCatch.h"
#include "mci/ValueClass.h"
#include "mci/ValueWrap.h"
#include "utl/mySPrintF.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "DataItemClass.h"
#include "LispTreeType.h"
#include "Metric.h"
#include "PropFuncs.h"
#include "Unit.h"
#include "UnitClass.h"

#include "StgBase.h"
#include "GridStorageManager.h"

#include "ShvUtils.h"
#include "GridLayer.h"
#include "FeatureLayer.h"
#include "LayerClass.h"
#include "Waiter.h"

//----------------------------------------------------------------------
// section : LayerInfo
//----------------------------------------------------------------------

LayerInfo::LayerInfo(
	State state, 
	const AbstrDataItem* diAspectOrFeature, 
	const AbstrDataItem* diClassBreaksOrExtKey, 
	const AbstrDataItem* diThemeOrGeoRel,
	const LayerClass*    layerClass
)	:	m_State(state)
	,	m_diAspectOrFeature    (diAspectOrFeature)
	,	m_diClassBreaksOrExtKey(diClassBreaksOrExtKey)
	,	m_diThemeOrGeoRel      (diThemeOrGeoRel)
	,	m_uAspectOrFeature(nullptr)
	,	m_uEntity         (nullptr)
	,	m_LayerClass(layerClass)

{
	dms_assert(diThemeOrGeoRel || diClassBreaksOrExtKey || diAspectOrFeature);

	dms_assert(!IsGrid  () || (diClassBreaksOrExtKey && HasGridDomain(diClassBreaksOrExtKey) && !diAspectOrFeature));
	dms_assert(!IsFeat  () || diAspectOrFeature);
	dms_assert(!IsAspect() || diThemeOrGeoRel);
	dms_assert(!IsComplete() || m_LayerClass || IsAspect());

	InitUnits();
	dms_assert(m_uAspectOrFeature);
	dms_assert(m_uEntity);
}

LayerInfo::LayerInfo(WeakStr descr)
	:	m_State(ConfigError)
	,	m_diAspectOrFeature(nullptr)
	,	m_diClassBreaksOrExtKey(nullptr)
	,	m_diThemeOrGeoRel(nullptr)
	,	m_uAspectOrFeature(nullptr)
	,	m_uEntity(nullptr)
	,	m_LayerClass(nullptr)
	,	m_Descr(descr)
{
	reportD(SeverityTypeID::ST_MajorTrace, "LayerInfo: ", m_Descr.c_str());
}

LayerInfo::LayerInfo(
	WeakStr descr,
	State state,
	const AbstrDataItem* diAspectOrFeature, 
	const AbstrDataItem* diClassBreaksOrExtKey, 
	const AbstrDataItem* diThemeOrGeoRel
)
	:	m_State(state)
	,	m_diAspectOrFeature(diAspectOrFeature)
	,	m_diClassBreaksOrExtKey(diClassBreaksOrExtKey)
	,	m_diThemeOrGeoRel(diThemeOrGeoRel)
	,	m_uAspectOrFeature(nullptr)
	,	m_uEntity(nullptr)
	,	m_LayerClass(nullptr)
	,	m_Descr(descr)
{
	dms_assert(state != ConfigError); // Precondition
	InitUnits();
}

void LayerInfo::InitUnits()
{
	dms_assert(m_uAspectOrFeature == nullptr);

	if (m_diAspectOrFeature)
		m_uAspectOrFeature = m_diAspectOrFeature->GetAbstrValuesUnit();
	else if (m_diClassBreaksOrExtKey)
		m_uAspectOrFeature = m_diClassBreaksOrExtKey->GetAbstrDomainUnit();
	else if (m_diThemeOrGeoRel)
		m_uAspectOrFeature = m_diThemeOrGeoRel->GetAbstrValuesUnit();

	m_uAspectOrFeature = GetWorldCrdUnitFromGeoUnit( m_uAspectOrFeature );

	dms_assert(m_uEntity == nullptr);
	if (m_diThemeOrGeoRel)
		m_uEntity = m_diThemeOrGeoRel ->GetAbstrDomainUnit();
	else if (m_diClassBreaksOrExtKey)
		m_uEntity = m_diClassBreaksOrExtKey->GetAbstrValuesUnit();
	else if (m_diAspectOrFeature)
		m_uEntity = m_diAspectOrFeature->GetAbstrDomainUnit();
}

const AbstrUnit* LayerInfo::GetPaletteDomain() const
{
	if (m_diAspectOrFeature)
		return m_diAspectOrFeature->GetAbstrDomainUnit();
	if (m_diClassBreaksOrExtKey)
		return m_diClassBreaksOrExtKey->GetAbstrDomainUnit();
	if (m_diThemeOrGeoRel)
		return m_diThemeOrGeoRel->GetAbstrValuesUnit();
	return nullptr;
}

const AbstrUnit* LayerInfo::GetWorldCrdUnit() const
{
	dms_assert(IsComplete()); // PRECONDITION
	dms_assert(!IsAspect());  // PRECONDITION
	return m_uAspectOrFeature;
}

//----------------------------------------------------------------------
// section : ViewType classificaiton funcs
//----------------------------------------------------------------------

bool IsEqualUnit(const AbstrUnit* a, const AbstrUnit* b)
{
	return a->GetUltimateItem() == b->GetUltimateItem();
}

const AbstrDataItem* GeometrySubItem(const TreeItem* ti)
{
	dms_assert(ti);
	ti->UpdateMetaInfo();
/* REMOVE
	auto pi = ti->GetTreeParent();
	if (pi) pi->UpdateMetaInfo();
	if (!ti->_GetFirstSubItem() && IsDataItem(ti))
		return nullptr;
*/
	const TreeItem* si = const_cast<TreeItem*>(ti)->GetSubTreeItemByID(token::geometry);
//	if (!si) 
//		si = ti->GetConstSubTreeItemByID(geoTokenID); // can call UpdateMetaInfo to retrieve stuff from a StorageManager
	if (!IsDataItem(si))
		return nullptr;
	auto gi = AsDataItem(si);
	if (gi->GetAbstrValuesUnit()->GetValueType()->GetNrDims() != 2)
		return nullptr;
	return gi;
}

bool IsThisMappable(const TreeItem* ti)
{
	dms_assert(ti);
	return HasMapType(ti) || GeometrySubItem(ti);
}

bool RefersToMappable(const TreeItem* ti)
{
	dms_assert(ti);
	do {
		if (IsThisMappable(ti))
			return true;
		ti = ti->GetReferredItem();
	} while (ti);
	return false;
}

const AbstrDataItem* GetDialogDataAttr(const TreeItem* ti)
{
	dms_assert( IsThisMappable(ti) );
	auto ref = GetDialogDataRef(ti);
	if (!ref)
		return nullptr;

	return AsDynamicDataItem( ref );
}

auto GetMappingItem(const TreeItem* ti) -> const TreeItem*
{
	dms_assert(ti); // PRECONDITION
	do
	{
		dms_assert(!SuspendTrigger::DidSuspend());
		if (IsThisMappable(ti))
			return ti;
		ti = Waiter::IsWaiting() ? ti->GetCurrRefItem() : ti->GetReferredItem();
	} while (ti);
	return nullptr;
}

auto GetMappedData(const TreeItem* ti) -> const AbstrDataItem*
{
	dms_assert(ti); // PRECONDITION
	ti = GetMappingItem(ti);
	if (!ti)
		return nullptr;
	if (HasMapType(ti))
		return GetDialogDataAttr(ti);
	return GeometrySubItem(ti);
}

bool IsFeatureData(const AbstrDataItem* adi)
{
	dms_assert(adi);
	return adi->GetAbstrValuesUnit()->GetNrDimensions() == 2;
}

const LayerClass* GetLayerClassFromFeatureType(const AbstrDataItem* adi)
{
	switch (adi->GetValueComposition())
	{
		case ValueComposition::Sequence: return GraphicArcLayer::GetStaticClass();
		case ValueComposition::Polygon:  return GraphicPolygonLayer::GetStaticClass();
	}
	dms_assert(adi->GetValueComposition() == ValueComposition::Single);
	return GraphicPointLayer::GetStaticClass();
}

bool IsCompatibleValueType(const ValueClass* vc, AspectType at)
{
	switch (at) {
		case AT_Numeric:  return vc->IsNumeric();
		case AT_Cardinal:
		case AT_Ordinal:  return vc->IsIntegral();
		case AT_Color:    return vc == ValueWrap<UInt32>::GetStaticClass();
		case AT_Text:     return vc == ValueWrap<SharedStr>::GetStaticClass();
		case AT_Feature:  return vc->GetNrDims() == 2;
	}
	return false;
}

static TokenID oldColorPaletteNameID = GetTokenID_st("Palette");
static TokenID oldFontPaletteNameID = GetTokenID_st("FontPalette");

bool IsAspectData(AspectNr aNr, const AbstrDataItem* adi, const LayerClass* layerClass)
{
	dms_assert(AspectArray);
	dms_assert(adi);
	try {
		if (!IsCompatibleValueType(adi->GetDynamicObjClass()->GetValuesType(), AspectArray[aNr].aspectType))
			return false;
	}
	catch (...) { return false; }
	TokenID dialogTypeID = TreeItem_GetDialogType(adi);
	if (dialogTypeID == GetAspectNameID(aNr) )
		return true;

#if !defined(SHV_SUPPORT_OLDNAMES)
	return false;
#else
	// stuff for backward compatibility
	if (dialogTypeID == oldColorPaletteNameID)
		if (layerClass)
			switch (layerClass->GetNrDims())
			{
				case 0: return aNr == AN_SymbolColor; // Palette -> AN_SymbolColor only for PointLayer
				case 1: return aNr == AN_PenColor;    // Palette -> AN_PenColor    only for ArcLayer 
				case 2: return aNr == AN_BrushColor;  // Palette -> AN_BrushColor  only for PolygonLayer and GridLayer
			}
		else
			return aNr == AN_LabelTextColor;

	if (dialogTypeID == oldFontPaletteNameID) 
	{
		if (aNr == AN_SymbolFont)
			return (layerClass->GetNrDims() == 0); // FontPalette -> SymbolFont only for PointLayer, else AN_LabelFont
		if (aNr == AN_LabelFont)
			return (layerClass->GetNrDims() != 0); // FontPalette -> LabelFont only for ArcLayer and PolyLayer
		return false;
	}

	return dialogTypeID == AspectArray[aNr].oldDialogTypeID; 
#endif
}

const AbstrDataItem* _FindAspectAttr(AspectNr aNr, const TreeItem* item, const AbstrUnit* adu, const LayerClass* layerClass)
{
	item = item->GetFirstSubItem();
	while (item)
	{
		if ( IsDataItem(item) )
		{
			const AbstrDataItem* dataItem = AsDataItem(item);
			if ( IsAspectData(aNr, dataItem, layerClass) )
			{
				const AbstrUnit* du = dataItem->GetAbstrDomainUnit();
				if	(adu->UnifyDomain(du) )
					return dataItem;
			}
		}
		item = item->GetNextItem();
	}
	return nullptr;
}

const AbstrDataItem* _FindAspectParam(AspectNr aNr, const TreeItem* item, const LayerClass* layerClass)
{
	item = item->GetFirstSubItem();
	while (item)
	{
		if ( IsDataItem(item) )
		{
			const AbstrDataItem* result = AsDataItem(item);
			if (result->HasVoidDomainGuarantee() && IsAspectData(aNr, result, layerClass) )
				return result;
		}
		item = item->GetNextItem();
	}
	return nullptr;
}

const AbstrDataItem* FindAspectParam(AspectNr aNr, const TreeItem* context, const LayerClass* layerClass)
{
	dms_assert(AspectArray);
	if (AspectArray[aNr].relType != AR_AttrOnly)
		while (context)
		{
			const AbstrDataItem* result = _FindAspectParam(aNr, context, layerClass);
			if (result)
				return result;

			MG_DEBUGCODE(const TreeItem* orgContext = context; )
			context = context->GetSourceItem();
			dbg_assert(context != orgContext);
		} 
	return nullptr;
}

const AbstrDataItem* FindAspectAttr(AspectNr aNr, const TreeItem* context, const AbstrUnit* adu, const LayerClass* layerClass = 0)
{
	dms_assert(AspectArray);
	AspectRelType rt = AspectArray[aNr].relType;
	if (rt != AR_ParamOnly)
		while (context)
		{
			const AbstrDataItem* result = _FindAspectAttr(aNr, context, adu, layerClass);
			if (result)
				return result;
			MG_DEBUGCODE(const TreeItem* orgContext = context; )
			context = context->GetSourceItem();
			dbg_assert(context != orgContext);
		} 

	return nullptr;
}


extern "C" SHV_CALL const AbstrDataItem* SHV_DataItem_FindClassification(const AbstrDataItem* themeAttr)
{
	DMS_CALL_BEGIN
		dms_assert(themeAttr);
		if (HasCdfProp(themeAttr))
			return GetCdfAttr(themeAttr);
		const AbstrUnit* avu = themeAttr->GetAbstrValuesUnit();
		if (HasCdfProp(avu))
			return GetCdfAttr(avu);

	DMS_CALL_END
	return nullptr;
}


const CharPtr TXT_NOT_MAPPABLE   = " is not mappable (DialogType property is not \"map\")";
const CharPtr TXT_MAPPABLE       = " is mappable (DialogType property is \"map\")";
const CharPtr TXT_DIALOGDATA_ERR = "\nbut its DialogData property does not refer to an existing DataItem";
const CharPtr TXT_DIALOGDATA_REF = "\nof which the DialogData property refers to DataItem %s";
const CharPtr TXT_DOMAIN_ERR     = "\nthat has an incompatible Domain\n";
const CharPtr TXT_VALUES_ERR     = "\nthat has an incompatible ValuesUnit\n";
const CharPtr TXT_GRIDDATA_ERR   = "\nwhich contains GridData with an incompatible ValuesUnit\n";
const CharPtr TXT_AS_EKD         = " (assumed to be the ExternalKeyData of the Feature Entity)";

LayerInfo GetLayerInfo(const AbstrDataItem* adi)
{
	if (HasGridDomain(adi))
		return LayerInfo(LayerInfo::DirGrid, 
			nullptr, adi, nullptr,                         // adi: SPoint->V
			GridLayer::GetStaticClass()
		); 

	if (IsFeatureData(adi))
		return LayerInfo(LayerInfo::DirFeat, 
			adi, nullptr, nullptr,                         // adi: E->Crd
			GetLayerClassFromFeatureType(adi)
		);

	// look for most specific first: Feature as sub-items of adi; prefer with adu as domain
	const AbstrUnit* adu = adi->GetAbstrDomainUnit();
	const AbstrDataItem* res  = FindAspectAttr (AN_Feature, adi, adu); 
	if (res) return LayerInfo(LayerInfo::DirFeat, res, 0, 0, GetLayerClassFromFeatureType(res));

	SharedStr infoTxt, resultMsg;

	const TreeItem* mappingItem = GetMappingItem(adi);
	if (mappingItem)
		infoTxt = mySSPrintF("the selected '%s'", mappingItem->GetFullName().c_str());
	else
	{
		infoTxt = mySSPrintF("the DomainUnit '%s'", adu->GetFullName().c_str());
		mappingItem = GetMappingItem(adu);
	}
	if (!mappingItem)
		return LayerInfo(infoTxt + TXT_NOT_MAPPABLE);

	infoTxt += TXT_MAPPABLE;
	const AbstrDataItem* mapItem  = GetMappedData(mappingItem);

	const AbstrDataItem* mapItem2 = nullptr;
	if (!mapItem) 
		return LayerInfo(infoTxt + TXT_DIALOGDATA_ERR);

	infoTxt += mySSPrintF(TXT_DIALOGDATA_REF, mapItem->GetFullName().c_str());

	if (HasGridDomain(mapItem))
	{

		if (!adu->UnifyDomain(mapItem->GetAbstrValuesUnit(), "Domain of the thematic attribute", "Values of BaseGrid", UM_AllowDefault, &resultMsg))
			return LayerInfo(infoTxt + TXT_GRIDDATA_ERR + resultMsg);

		return LayerInfo(LayerInfo::Grid, 
			0, mapItem, 0,                           // adi: E->V; mapItem: SPoint->E
			GridLayer::GetStaticClass()
		); 
	}
	if (adu->UnifyDomain(mapItem->GetAbstrValuesUnit(), "Domain of the thematic attribute", "Values of MapItem", UM_AllowDefaultRight))
	{
		mapItem2 = mapItem; mapItem = nullptr;
		infoTxt += TXT_AS_EKD;
	}
	else
	{
 		if (!adu->UnifyDomain(mapItem->GetAbstrDomainUnit(), "Domain of the thematic attribute", "Domain of MapItem", UnifyMode(), &resultMsg))
			return LayerInfo(infoTxt + TXT_DOMAIN_ERR + resultMsg);

		// adi: E->V; mapItem: E->???

		if (IsFeatureData(mapItem))
			return LayerInfo(LayerInfo::Feature, 
				mapItem, 0, 0,                         // adi: E->V; mapItem: E->Crd
				GetLayerClassFromFeatureType(mapItem)
			); 

		// only remaining alternatives: mapItem is relation to (external key values of) shapes
		const AbstrUnit* avu = mapItem->GetAbstrValuesUnit(); infoTxt += mySSPrintF("\nwith ValuesUnit %s", avu->GetFullName().c_str());

		if (!GetMappingItem(avu))
			return LayerInfo(infoTxt + " which is not mappable (it was expected to be the set [of ExternalKeys] of Features)");
		
		mapItem2 = GetMappedData(avu);
		if (!mapItem2)
			return LayerInfo(infoTxt + TXT_DIALOGDATA_ERR);

		infoTxt += mySSPrintF(TXT_DIALOGDATA_REF, mapItem2->GetFullName().c_str());
		if (IsFeatureData(mapItem2))
		{
			if (!avu->UnifyDomain(mapItem2->GetAbstrDomainUnit(), "Values of MapItem", "Domain of MapItem2", UnifyMode(), &resultMsg)) // mapItem will be inverted to EK->E and must be compatible with EK->Crd
				return LayerInfo(infoTxt + TXT_DOMAIN_ERR + resultMsg);

			return LayerInfo(LayerInfo::Feature,                         // adi: E->V; mapItem: E->EK; mapItem2: EK->Crd
				mapItem2, 0, mapItem,
				GetLayerClassFromFeatureType(mapItem2)
			);
		}
		infoTxt += TXT_AS_EKD;

		if (!avu->UnifyValues(mapItem2->GetAbstrValuesUnit(), "Values of MapItem", "Values of MapItem2", UM_AllowDefault, &resultMsg))
			return LayerInfo(infoTxt + TXT_VALUES_ERR + resultMsg);
	}
	// adi: E->V; mapItem: E->EK; mapItem2: P->EK
	const AbstrUnit* adu3 = mapItem2->GetAbstrDomainUnit(); infoTxt += mySSPrintF("\nwith Domain %s", adu3->GetFullName().c_str());
	if (!GetMappingItem(adu3))
		return LayerInfo(infoTxt + "\nwhich is not mappable (it was expected to be the Feature Entity)");

	const AbstrDataItem* mapItem3 = GetMappedData(adu3);
	if (!mapItem3)
		return LayerInfo(infoTxt + TXT_DIALOGDATA_ERR + " (a reference to a FeatureAttr was expected)");

	infoTxt += mySSPrintF(TXT_DIALOGDATA_REF, mapItem3->GetFullName().c_str());
	if (!adu3->UnifyDomain(mapItem3->GetAbstrDomainUnit(), "Domain of MapItem2", "Domain of MappedData of MapItem2", UnifyMode(0), &resultMsg))
		return LayerInfo(infoTxt + TXT_DOMAIN_ERR + resultMsg);

	if (!IsFeatureData(mapItem3)) 
		return LayerInfo(infoTxt + "\nwhich was expected to contain Features (Points, Arcs, of Polygons)");

	return LayerInfo(LayerInfo::Feature, 
		mapItem3, mapItem2, mapItem, // adi: E->V; mapItem: E->EK; mapItem2: P->EK; mapItem3: P->Crd
		GetLayerClassFromFeatureType(mapItem3)
	);
}

const AbstrUnit* GetRealAbstrValuesUnit(const AbstrDataItem* adi)
{
	dms_assert(adi);
	while (true)
	{
		const AbstrUnit* result = adi->GetAbstrValuesUnit();
		if (result && result != result->GetUnitClass()->CreateDefault())
			return result;
		adi = AsDataItem(adi->GetCurrRefItem());
		if (!adi)
			return result;
	}
}

LayerInfo GetAspectInfo(AspectNr aNr, const AbstrDataItem* adi, const LayerInfo& featureInfo, bool allowParameter)
{
	dms_assert(adi);
	dms_assert(AspectArray);

	if	(IsAspectData(aNr, adi, featureInfo.m_LayerClass))
		return LayerInfo(LayerInfo::Aspect, 0, 0, adi); // adi: E->Aspect

	const AbstrUnit* avu = GetRealAbstrValuesUnit(adi);
	const LayerClass* layerClass = featureInfo.m_LayerClass;

	SharedStr infoTxt;
	// look for most specific first: as sub-items of adi; prefer with avu as domain
	const AbstrDataItem* res = FindAspectAttr (aNr, adi, avu, layerClass); if (res) return LayerInfo(LayerInfo::Aspect, res, 0, adi);
	if (allowParameter)  res = FindAspectParam(aNr, adi, layerClass);      if (res) goto returnAspectParam;

	// Then, look at avu, the valuesunit of adi
	                     res = FindAspectAttr (aNr, avu, avu, layerClass); if (res) return LayerInfo(LayerInfo::Aspect, res, 0, adi);
	if (allowParameter)  res = FindAspectParam(aNr, avu, layerClass);      
	if (res) goto returnAspectParam;
	else {
		infoTxt = mgFormat2SharedStr("thematic attribute %s has ValuesUnit %s of type %s",
			adi->GetFullName().c_str(),
			avu->GetFullName().c_str(),
			avu->GetValueType()->GetName()
		);

		// look for a classification: mapItem2. adi's cdf propdef precedes avu's cdf.
		const AbstrDataItem* mapItem2 = 0;
		if (HasCdfProp(adi))
		{
			mapItem2 = GetCdfAttr(adi);
			if (!mapItem2)
				return LayerInfo(
					infoTxt
					+ " but its cdf property value '"
					+ GetCdfProp(adi)
					+ "' does not refer to an existing DataItem"
				);
		}
		else if (HasCdfProp(avu))
		{
			mapItem2 = GetCdfAttr(avu);
			if (!mapItem2)
				return LayerInfo(
					infoTxt
					+ " of which the cdf property value '"
					+ GetCdfProp(avu)
					+ "' does not refer to an existing DataItem"
				);
		}

		const AbstrUnit* classIdUnit = avu;
		if (mapItem2)
		{
			SharedStr resultMsg;
			infoTxt += mySSPrintF("\nwhich has DataItem %s as associated Classification item", mapItem2->GetFullName().c_str());
			if (!avu->UnifyValues(mapItem2->GetAbstrValuesUnit(), "Values of thematic attribute", "Values of MapItem2", UM_AllowDefault, &resultMsg))
				return LayerInfo(infoTxt + TXT_VALUES_ERR + resultMsg);

			classIdUnit = mapItem2->GetAbstrDomainUnit();
			infoTxt += mySSPrintF("\nwith DomainUnit %s", classIdUnit->GetFullName().c_str());

			res = FindAspectAttr(aNr, adi, classIdUnit, layerClass); if (res) return LayerInfo(LayerInfo::Aspect, res, mapItem2, adi);
			res = FindAspectAttr(aNr, mapItem2, classIdUnit, layerClass); if (res) return LayerInfo(LayerInfo::Aspect, res, mapItem2, adi);
			res = FindAspectAttr(aNr, mapItem2, avu, layerClass); if (res) return LayerInfo(LayerInfo::Aspect, res, 0, adi);
			if (allowParameter)
				res = FindAspectParam(aNr, mapItem2, layerClass); if (res) goto returnAspectParam;

			res = FindAspectAttr(aNr, classIdUnit, classIdUnit, layerClass); if (res) return LayerInfo(LayerInfo::Aspect, res, mapItem2, adi);
			if (allowParameter)
				res = FindAspectParam(aNr, classIdUnit, layerClass); if (res) goto returnAspectParam;
			infoTxt += mySSPrintF("\nbut no %s attribute found for the ClassEntity %s\nStart the palette editor for this ClassEntity."
				, AspectArray[aNr].name
				, classIdUnit->GetFullName().c_str()
			);
		}
		else
		{
			infoTxt += "\nbut no associated "; if (avu->CanBeDomain()) infoTxt += "Palette or ";
			infoTxt += "Classification found for these values;\nconfigure the cdf property or start the classification editor";
		}

		if (featureInfo.m_uEntity)
		{
			const AbstrUnit* adu = featureInfo.m_uEntity;
			res = FindAspectAttr(aNr, adu, adu, layerClass); if (res) return LayerInfo(LayerInfo::Aspect, 0, 0, res);
			if (allowParameter)
				res = FindAspectParam(aNr, adu, layerClass); if (res) goto returnAspectParam;
		}

		if (aNr == AN_LabelText
			&& adi->GetDynamicObjClass()->GetValuesType()->GetValueComposition() == ValueComposition::Single // FeatureAttr will not be displayed as label
			&& !featureInfo.m_LayerClass->IsDerivedFrom(GraphicArcLayer::GetStaticClass())
			)
			return LayerInfo(LayerInfo::Aspect, 0, 0, adi); // adi: E->Aspect

		// from here we cannot find entity related attr anymore, look for AspectParameters
		if (AspectArray[aNr].relType != AR_AttrOnly && allowParameter)
		{
			// now go even deeper, back to ExternalKey, FeatureEntity or even GeoCoord or WorldCoord
			if (featureInfo.m_diThemeOrGeoRel)
			{
				res = FindAspectParam(aNr, featureInfo.m_diThemeOrGeoRel.get_ptr(), layerClass); if (res) goto returnAspectParam;
				res = FindAspectParam(aNr, featureInfo.m_diThemeOrGeoRel->GetAbstrValuesUnit(), layerClass); if (res) goto returnAspectParam;
			}
			if (featureInfo.m_diClassBreaksOrExtKey)
			{
				res = FindAspectParam(aNr, featureInfo.m_diClassBreaksOrExtKey.get_ptr(), layerClass); if (res) goto returnAspectParam;
				res = FindAspectParam(aNr, featureInfo.m_diClassBreaksOrExtKey->GetAbstrDomainUnit(), layerClass); if (res) goto returnAspectParam;
			}
			if (featureInfo.m_diAspectOrFeature) {
				res = FindAspectParam(aNr, featureInfo.m_diAspectOrFeature.get_ptr(), layerClass); if (res) goto returnAspectParam;
			}
			if (featureInfo.m_uAspectOrFeature) {
				res = FindAspectParam(aNr, featureInfo.m_uAspectOrFeature.get_ptr(), layerClass); if (res) goto returnAspectParam;
			}
#		if defined(SHV_SUPPORT_OLDNAMES)
			if (featureInfo.m_diAspectOrFeature)
			{
				res = AsDynamicDataItem(featureInfo.m_diAspectOrFeature->GetAbstrDomainUnit()->GetConstSubTreeItemByID(GetTokenID_mt("ExternalKeyData")));
				if (res)
				{
					const AbstrUnit* adu = res->GetAbstrValuesUnit();
					res = FindAspectParam(aNr, adu, layerClass); if (res) goto returnAspectParam;
				}
			}
#		endif
		}
		// Bad luck, return error

		infoTxt += mySSPrintF(
			"\nNo related DataItem found with the DialogType property indicating the requested %s aspect;"
			"\nconfigure the requested subItem or start the palette editor",
			AspectArray[aNr].name
		);
		LayerInfo::State state = (avu->GetValueType()->IsNumeric())
			? LayerInfo::ClassificationMissing
			: LayerInfo::NotNumeric;

		if (classIdUnit->CanBeDomain() && classIdUnit->GetValueType()->GetBitSize() <= 8)
			state = LayerInfo::PaletteMissing;
		else
			mapItem2 = nullptr;

		return LayerInfo(infoTxt, state, 0, mapItem2, adi);
	}
returnAspectParam:
	return LayerInfo(LayerInfo::Aspect, 0, 0, res);
}
