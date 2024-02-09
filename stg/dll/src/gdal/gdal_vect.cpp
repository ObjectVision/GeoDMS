// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "StoragePCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "RtcGeneratedVersion.h"

#include "gdal_base.h"
#include "gdal_vect.h"

#include "ogrsf_frmts.h"
#include "ogr_api.h"

#include "dbg/debug.h"
#include "dbg/SeverityType.h"
#include "geo/Conversions.h"
#include "geo/PointOrder.h"
#include "mci/ValueClass.h"
#include "mci/ValueClassID.h"
#include "mci/ValueComposition.h"
#include "mem/FixedAlloc.h"
#include "ser/FormattedStream.h"
#include "utl/mySPrintF.h"
#include "utl/encodes.h"
#include "xct/DmsException.h"

#include "ParallelTiles.h"
#include "RtcTypeLists.h"
#include "UnitProcessor.h"

#include "AbstrDataItem.h"
#include "AbstrUnit.h"
#include "DataArray.h"
#include "DataStoreManagerCaller.h"
#include "NameSet.h"
#include "PropFuncs.h"
#include "LispTreeType.h"
#include "TreeItemContextHandle.h"
#include "TreeItemProps.h"
#include "UnitClass.h"

#include "geo/BoostPolygon.h"
#include "geo/ConvertFunctor.h"
#include "Projection.h"

#include "stg/StorageClass.h"

namespace gdalVectImpl {
	UInt32 s_ComponentCount = 0;

	// *****************************************************************************
	//
	// FeaturePtr
	//
	// *****************************************************************************

	struct FeaturePtr : ptr_base<OGRFeature, boost::noncopyable>
	{
		FeaturePtr(OGRFeature* geo)
			: ptr_base(geo)
		{}

		~FeaturePtr()
		{
			if (has_ptr())
				OGRFeature::DestroyFeature(get_ptr());
		}
	};

	// *****************************************************************************
	//
	// helper funcs
	//
	// *****************************************************************************

	ValueClassID OGR2ValueType(OGRFieldType attrType, OGRFieldSubType subType)
	{
		switch (subType) {
			case OFSTBoolean:
				return ValueClassID::VT_Bool;
			case OFSTInt16:
				return ValueClassID::VT_UInt16;
			case OFSTFloat32:
				return ValueClassID::VT_Float32;
		}

		switch (attrType) {
		case OFTInteger:
			return ValueClassID::VT_Int32;
		case OFTInteger64:
			return ValueClassID::VT_Int64;
		case OFTReal:
			return ValueClassID::VT_Float64;
		case OFTString:
		case OFTStringList:
		case OFTWideString:     // Depreciated
		case OFTWideStringList: // Depreciated
			return ValueClassID::VT_SharedStr;
		case OFTIntegerList:
		case OFTRealList:
		case OFTBinary:
		case OFTDate:
		case OFTTime:
		case OFTDateTime:
		default:
			return ValueClassID::VT_Unknown;
		}
	}

	const ValueClass* OGR2ValueClass(OGRFieldType attrType, OGRFieldSubType subType)
	{
		ValueClassID vcid = OGR2ValueType(attrType, subType);
		if (vcid == ValueClassID::VT_Unknown)
			vcid = ValueClassID::VT_SharedStr;
		return ValueClass::FindByValueClassID(vcid);
	}

	ValueComposition OGR2ValueComposition(OGRwkbGeometryType geoType)
	{
		switch (geoType) {
		case wkbPoint:           return ValueComposition::Single;
		case wkbMultiPoint:      return ValueComposition::Sequence;
		case wkbCircularString:	 return ValueComposition::Sequence;
		case wkbCompoundCurve:	 return ValueComposition::Sequence;
		case wkbLineString:      return ValueComposition::Sequence;
		case wkbMultiLineString: return ValueComposition::Sequence;
		case wkbPolygon:         return ValueComposition::Polygon;
		case wkbMultiPolygon:    return ValueComposition::Polygon;
		case wkbCurvePolygon:	 return ValueComposition::Polygon;
		}
		return ValueComposition::Unknown;
	}

}	// namespace gdal2VectImpl

// ------------------------------------------------------------------------
// installation of gdalVect component
// ------------------------------------------------------------------------

gdalVectComponent::gdalVectComponent()
{
	//if (!gdalVectImpl::s_ComponentCount)
	//	OGRRegisterAll(); // can throw

	++gdalVectImpl::s_ComponentCount;
}

gdalVectComponent::~gdalVectComponent()
{
	--gdalVectImpl::s_ComponentCount;
//	if (!--gdalVectImpl::s_ComponentCount)
//		;
}

// *****************************************************************************
//
// Implementations of GdaVectlMetaInfo
//
// *****************************************************************************

GdalVectlMetaInfo::GdalVectlMetaInfo(const GdalVectSM* gdv, const TreeItem* storageHolder, const TreeItem* adi)
	:	GdalMetaInfo(storageHolder, adi)
	,	m_GdalVectSM(gdv)
{
	assert(storageHolder->DoesContain(adi)); // PRECONDITION

	const TreeItem* adiParent = adi;
	while (true) {
		if (sqlStringPropDefPtr->HasNonDefaultValue(adiParent))
		{
			m_SqlString = TreeItemPropertyValue(adiParent, sqlStringPropDefPtr);
			return;
		}
		if (adiParent == storageHolder)
		{
			assert(m_NameID == TokenID());
			break;
		}
		adiParent = adiParent->GetTreeParent();
		assert(adiParent != nullptr); // follows from PRECONDITION
	}

	adiParent = adi;
	while (true) {
		if (IsUnit(adiParent))
		{
			m_NameID = adiParent->GetID();
			return;
		}
		if (adiParent == storageHolder)
		{
			assert(m_NameID == TokenID());
			break;
		}
		adiParent = adiParent->GetTreeParent();
		assert(adiParent != nullptr); // follows from PRECONDITION
	}
	adiParent = adi;
	while (true) {
		if (!IsDataItem(adiParent))
		{
			m_NameID = adiParent->GetID();
			return;
		}

		if (adiParent == storageHolder)
		{
			assert(m_NameID == TokenID());
			break;
		}
		adiParent = adi->GetTreeParent();
		assert(adiParent != nullptr); // follows from PRECONDITION
	}
}

void GdalVectlMetaInfo::OnOpen()
{
	GDAL_ErrorFrame gdal_error_frame;

	const GdalVectSM* gdv = m_GdalVectSM;

	dms_assert(gdv);
	dms_assert(gdv->m_hDS);
	dms_assert(m_CurrFeatureIndex == 0);

	SizeT layerCount = 1;
	if (!m_SqlString.empty())
	{
		m_Layer =
			gdv->m_hDS->ExecuteSQL(
				m_SqlString.c_str(),
				nullptr, // 	OGRGeometry * 	poSpatialFilter
				nullptr  //     const char * 	pszDialect
			);

		if (gdal_error_frame.HasError())
		{
			throwErrorF("gdal.vect", "cannot find layer with name %s, invalid sql string %s,\n%s"
			,	m_NameID.GetStr().c_str()
			,	m_SqlString.c_str()
			,	gdal_error_frame.GetMsgAndReleaseError().c_str()
			);
		}

		m_IsOwner = true;
	}
	else
	{
		layerCount = gdv->m_hDS->GetLayerCount();

		m_Layer =
			(layerCount == 1)
			? gdv->m_hDS->GetLayer(0)
			: gdv->m_hDS->GetLayerByName(m_NameID.GetStr().c_str());

		if (m_Layer)
			m_Layer->SetNextByIndex(0);
		
		if (!m_Layer)
		{
			auto is_container = !IsDataItem(CurrRI()) && !IsUnit(CurrRI());
			if (!is_container)
				throwErrorF("gdal.vect", "cannot find layer with name %s in dataset.\n", m_NameID.GetStr().c_str());
		}

		m_IsOwner = false;
	}
}

GdalVectlMetaInfo::~GdalVectlMetaInfo()
{
	if (m_Layer && m_IsOwner)
		m_GdalVectSM->m_hDS->ReleaseResultSet(m_Layer);
}

OGRLayer* GdalVectlMetaInfo::Layer() const
{
	dms_assert(m_GdalVectSM);
	dms_assert(m_GdalVectSM->m_hDS);

	if (!m_Layer)
		throwErrorF("gdal.vect","cannot open layer %s%s", m_NameID.GetStr().c_str(), (m_GdalVectSM->m_hDS->GetLayerCount() == 1) ? "" : ", multiple layers available");
	return m_Layer;
}

void GdalVectlMetaInfo::SetCurrFeatureIndex(SizeT firstFeatureIndex) const
{
	MG_CHECK(IsDefined(firstFeatureIndex)); // REMOVE
	dms_assert(m_CurrFeatureIndex == firstFeatureIndex);
	auto layer = Layer();

	GDAL_ErrorFrame gdal_error_frame;
	while (m_CurrFeatureIndex < firstFeatureIndex)
	{
		gdalVectImpl::FeaturePtr feat = layer->GetNextFeature();
		++m_CurrFeatureIndex;
	}
	if (firstFeatureIndex == m_CurrFeatureIndex)
		return;

	layer->SetNextByIndex(firstFeatureIndex);
}

// *****************************************************************************
//
// Implementations of GdalVectSM
//
// *****************************************************************************

#if defined(MG_DEBUG)

GdalVectSM::GdalVectSM()
{
	dms_assert(m_hDS == nullptr);
}
#endif

GdalVectSM::~GdalVectSM()
{
	CloseStorage();
	dms_assert(m_hDS == nullptr);
}

void GdalVectSM::DoOpenStorage(const StorageMetaInfo& smi, dms_rw_mode rwMode) const
{
	DBG_START("GdalVectSM", "OpenStorage", false);
	assert(m_hDS == nullptr);
	if (rwMode != dms_rw_mode::read_only && !IsWritableGDAL())
		throwErrorF("gdal.vect", "Cannot use storage manager %s with readonly type %s for writing data"
			,	smi.StorageManager()->GetFullName().c_str()
			,	smi.StorageManager()->GetClsName().c_str()
			);

	m_hDS = Gdal_DoOpenStorage(smi, rwMode, GDAL_OF_VECTOR, m_DataItemsStatusInfo.m_continueWrite);
}

void GdalVectSM::DoCloseStorage(bool mustCommit) const
{
	DBG_START("GdalVectSM", "DoCloseStorage", false);
	dms_assert(m_hDS);

	m_hDS = nullptr; // calls GDALClose
}

template <typename PolygonType>
void AddPoint(typename DataArray<PolygonType>::reference dataElemRef, OGRPoint* point)
{
	dataElemRef.push_back(shp2dms_order(point->getX(), point->getY()));
}

template <typename PolygonType>
void AddLinePoint(typename DataArray<PolygonType>::reference dataElemRef, OGRLineString* lineString, SizeT p)
{
	dataElemRef.push_back(shp2dms_order(lineString->getX(p), lineString->getY(p)));
}

template <typename PolygonType>
void AddLineString(typename DataArray<PolygonType>::reference dataElemRef, OGRLineString* lineString)
{
	SizeT numPoints = lineString->getNumPoints();
	for (SizeT p = 0; p!= numPoints; ++p)
		AddLinePoint<PolygonType>(dataElemRef, lineString, p);
}

template <typename PolygonType>
void AddLinearRing(typename DataArray<PolygonType>::reference dataElemRef, OGRLinearRing* ring, bool isExterior)
{
	if (!ring)
		return;

	if (!(ring->isClockwise()))
		isExterior = !isExterior;
	SizeT numPoints = ring->getNumPoints();
	MG_CHECK(ring->getX(0) == ring->getX(numPoints-1));
	MG_CHECK(ring->getY(0) == ring->getY(numPoints-1));
	for (SizeT p = 0; p != numPoints; ++p)
		AddLinePoint<PolygonType>(dataElemRef, ring, isExterior ? p : (numPoints - p - 1));
}

template <typename PolygonType>
void AddLineStringAsRing(typename DataArray<PolygonType>::reference dataElemRef, OGRLineString* lineString)
{
	SizeT numPoints = lineString->getNumPoints();
	dms_assert(numPoints);
	for (SizeT p = 0; p != numPoints; ++p)
		AddLinePoint<PolygonType>(dataElemRef, lineString, p);
	// go back to start in reverse direction
	--numPoints;
	while (numPoints--)
		AddLinePoint<PolygonType>(dataElemRef, lineString, numPoints);
}

template <typename PolygonType>
void AddLineBegin(typename DataArray<PolygonType>::reference dataElemRef, OGRLineString* lineString)
{
	AddLinePoint<PolygonType>(dataElemRef, lineString, 0);
}

template <typename PolygonType>
void AddPolygon(typename DataArray<PolygonType>::reference dataElemRef, OGRPolygon* geoPoly)
{
	AddLinearRing<PolygonType>(dataElemRef, geoPoly->getExteriorRing(), true);

	SizeT numInteriorRings=geoPoly->getNumInteriorRings();
	if (numInteriorRings)
	{
		for (SizeT i=0; i!= numInteriorRings; ++i)
			AddLinearRing<PolygonType>(dataElemRef, geoPoly->getInteriorRing(i), false);
		--numInteriorRings;
		while (numInteriorRings--)
			AddLineBegin<PolygonType>(dataElemRef, geoPoly->getInteriorRing(numInteriorRings));
		AddLineBegin<PolygonType>(dataElemRef, geoPoly->getExteriorRing());
	}
}

template <typename PolygonType>
void AddMultiPolygon(typename DataArray<PolygonType>::reference dataElemRef, OGRMultiPolygon* geoMultiPolygon)
{
	SizeT numPolygons = geoMultiPolygon->getNumGeometries();
	if (numPolygons)
	{
		for (SizeT i=0; i!=numPolygons; ++i)
			AddPolygon<PolygonType>(dataElemRef, debug_cast<OGRPolygon*>(geoMultiPolygon->getGeometryRef(i)));
		--numPolygons;
		while (numPolygons--)
			AddLineBegin<PolygonType>(dataElemRef, debug_cast<OGRPolygon*>(geoMultiPolygon->getGeometryRef(numPolygons))->getExteriorRing());
	}
}
template <typename PolygonType>
void AddMultiLineString(typename DataArray<PolygonType>::reference dataElemRef, OGRMultiLineString* geoMultiLineString)
{
	SizeT numLineStrings = geoMultiLineString->getNumGeometries();
	if (numLineStrings)
	{
		for (SizeT i=0; i!=numLineStrings; ++i)
			AddLineStringAsRing<PolygonType>(dataElemRef, debug_cast<OGRLineString*>(geoMultiLineString->getGeometryRef(i)));
		--numLineStrings;
		while (numLineStrings--)
			AddLineBegin<PolygonType>(dataElemRef, debug_cast<OGRLineString*>(geoMultiLineString->getGeometryRef(numLineStrings)));
	}
}

template <typename PolygonType>
void AddMultiPoint(typename DataArray<PolygonType>::reference dataElemRef, OGRMultiPoint* multiPoint)
{
	SizeT numPoints = multiPoint->getNumGeometries();
	for (SizeT p = 0; p!= numPoints; ++p)
		AddPoint<PolygonType>(dataElemRef, debug_cast<OGRPoint*>(multiPoint->getGeometryRef(p)));
}

OGRFeature* GetNextFeatureInterleaved(OGRLayer* layer, WeakPtr<GDALDataset> hDS)
{
	OGRFeature* nextFeature;
	OGRLayer* poFeatureLayer;
	while (true)
	{
		nextFeature = hDS->GetNextFeature(&poFeatureLayer, nullptr, nullptr, nullptr);
		if (nextFeature == nullptr)
			break;

		if (layer->GetName() != poFeatureLayer->GetName())
		{
			OGRFeature::DestroyFeature(nextFeature);
			continue;
		}

		break;
	}

	return nextFeature;
}



template <typename PointType>
void ReadPointData(typename sequence_traits<PointType>::seq_t data, OGRLayer* layer, SizeT firstIndex, SizeT size, GDALDataset* hDS)
{
	dms_assert(layer);

	DBG_START("ReadPointData", typeid(PointType).name(), true);

	SizeT numPoints = 0;

	dms_assert(data.size() == size);
	
	SizeT i=0;
	auto lch = MakeLCH(
		[firstIndex, &i]() -> SharedStr { return mySSPrintF("Reading Point Feature %d", i + firstIndex); }
	);

	for (; i!=size; ++i)
	{
		typename DataArray<PointType>::reference dataElemRef = data[i];

		gdalVectImpl::FeaturePtr  feat = hDS->TestCapability(ODsCRandomLayerRead) ? GetNextFeatureInterleaved(layer, hDS) : layer->GetNextFeature();
		OGRGeometry* geo = feat ? feat ->GetGeometryRef() : nullptr;
		if (geo)
		{
			OGRPoint* point = dynamic_cast<OGRPoint*>(geo);
			MG_CHECK(point);

			dataElemRef.X() = point->getX();
			dataElemRef.Y() = point->getY();
		}
		else
			Assign( dataElemRef, Undefined() );
	}
}

template <typename PolygonType>
void ReadPolyData(typename sequence_traits<PolygonType>::seq_t dataArray, OGRLayer* layer, SizeT firstIndex, SizeT size, ResourceHandle& readBuffer, GDALDataset* m_hDS)
{
	dms_assert(layer);

	DBG_START("ReadPolyData", typeid(PolygonType).name(), true);

	DBG_TRACE(("firstIndex %d, size %d", firstIndex, size));

	typedef sequence_traits<PolygonType>::container_type dataBufType;
	if (!readBuffer)
	{
		dms_assert(!firstIndex || !size);
		readBuffer = makeResource<dataBufType>();
	}
	dataBufType& data = GetAs<dataBufType>(readBuffer);
	data.reset(size, 0 MG_DEBUG_ALLOCATOR_SRC("gdal_vect: ReadPolyData"));

	SizeT i=0;
	auto lch = MakeLCH(
		[firstIndex, &i]() -> SharedStr { return mySSPrintF("reading Points of Feature %d", i + firstIndex); }
	);

	for (; i!=size; ++i)
	{
		typename DataArray<PolygonType>::reference dataElemRef = data[i];

		gdalVectImpl::FeaturePtr feat = m_hDS->TestCapability(ODsCRandomLayerRead) ? GetNextFeatureInterleaved(layer, m_hDS) : layer->GetNextFeature();
		OGRGeometry* geo = feat ? feat ->GetGeometryRef() : 0;
		if (geo) {
			if (dynamic_cast<OGRLineString*>(geo))
				AddLineString<PolygonType>(dataElemRef, static_cast<OGRLineString*>(geo)); 
			else if (dynamic_cast<OGRCircularString*>(geo))
				AddLineString<PolygonType>(dataElemRef, static_cast<OGRLineString*>(geo->getLinearGeometry()));
			else if (dynamic_cast<OGRCompoundCurve*>(geo))
				AddLineString<PolygonType>(dataElemRef, static_cast<OGRLineString*>(geo->getLinearGeometry()));
			else if (dynamic_cast<OGRPolygon*>(geo))
				AddPolygon<PolygonType>(dataElemRef, static_cast<OGRPolygon*>(geo));
			else if (dynamic_cast<OGRMultiPolygon*>(geo))
				AddMultiPolygon<PolygonType>(dataElemRef, static_cast<OGRMultiPolygon*>(geo));
			else if (dynamic_cast<OGRCurvePolygon*>(geo))
				AddPolygon<PolygonType>(dataElemRef, static_cast<OGRPolygon*>(geo->getLinearGeometry()));
			else if (dynamic_cast<OGRMultiPoint*>(geo))
				AddMultiPoint<PolygonType>(dataElemRef, static_cast<OGRMultiPoint*>(geo));
			else if (dynamic_cast<OGRMultiLineString*>(geo))
			{
				if (!i && !firstIndex) // only report once
					reportD(MsgCategory::storage_read, SeverityTypeID::ST_MajorTrace, "gdal.vect: wicked conversion of MultiLineStrings to skinny MultiPolygons");
				AddMultiLineString<PolygonType>(dataElemRef, static_cast<OGRMultiLineString*>(geo));
			}
		}
		else
			Assign( dataElemRef, Undefined() );

		dms_assert(data.data_size() == data.actual_data_size()); // no holes
	}

	dms_assert(data.data_size() == data.actual_data_size()); // no holes

	Assign(dataArray, data);

	dms_assert(dataArray.get_sa().data_size()== data.actual_data_size());
}

void ReadStringData(sequence_traits<SharedStr>::seq_t dataArray, OGRLayer* layer, SizeT firstIndex, SizeT size, ResourceHandle& readBuffer, GDALDataset* hDS)
{
	dms_assert(layer);

	DBG_START("ReadStringData", "", true);

	DBG_TRACE(("firstIndex %d, size %d", firstIndex, size));

	SizeT numChars = 0;
	typedef sequence_traits<SharedStr>::container_type dataBufType;
	if (!readBuffer)
	{
		dms_assert(!firstIndex || !size);
		readBuffer = makeResource<dataBufType>();
	}
	dataBufType& data = GetAs<dataBufType>(readBuffer);
	data.reset(size, 0 MG_DEBUG_ALLOCATOR_SRC("gdal_vect"));

	SizeT i=0;
	auto lch = MakeLCH(
		[firstIndex, &i]() -> SharedStr { return mySSPrintF("Reading String Element %d", i + firstIndex); }
	);

	for (i=0; i!=size; ++i)
	{
		DataArray<SharedStr>::reference dataElemRef = data[i];

		gdalVectImpl::FeaturePtr  feat = hDS->TestCapability(ODsCRandomLayerRead) ? GetNextFeatureInterleaved(layer, hDS) : layer->GetNextFeature();
		OGRGeometry* geo = feat ? feat ->GetGeometryRef() : nullptr;
		if (geo)
		{
			CplString result;
			geo->exportToWkt(&result.m_Text);
			if (result.m_Text)
			{
				dataElemRef.assign(result.m_Text, result.m_Text +strlen(result.m_Text));
			}
		}
		else
			Assign( dataElemRef, Undefined() );
	}
	dms_assert(data.data_size() == data.actual_data_size()); // no holes

	Assign(dataArray, data);

	dms_assert(dataArray.get_sa().data_size() == data.actual_data_size());
}

SizeT LayerFieldEnable(OGRLayer* layer, CharPtrRange itemName, const Actor* context)
{
	dms_assert(layer);
	SizeT fieldID ;

	WeakPtr<OGRFeatureDefn> featureDefn = layer->GetLayerDefn();
	if (context) // attribute, not geometry
	{
		if (!itemName.empty())
		{
			fieldID = 0;
			while (fieldID < featureDefn->GetFieldCount())
			{
				CharPtr columnName = featureDefn->GetFieldDefn(fieldID)->GetNameRef();
				SizeT columnNameLen = StrLen(columnName);
				auto columnNameAsItemName = as_item_name(columnName, columnName + columnNameLen);
				if (!stricmp(itemName.first, columnNameAsItemName.c_str())) //layerNameSet.FieldNameToMappedName(featureDefn->GetFieldDefn(fieldID)->GetNameRef()).c_str()))
					goto found;
				fieldID++;
			}
		}
	}
	fieldID = -1;
found:

	featureDefn->SetGeometryIgnored(fieldID != SizeT(-1));
	for (SizeT i = 0, numFields = featureDefn->GetFieldCount(); i!=numFields; ++i)
	{
		WeakPtr<OGRFieldDefn> fieldDefn = featureDefn->GetFieldDefn(i);
		fieldDefn->SetIgnored(fieldID != i);
	}
	return fieldID;
}

bool GdalVectSM::ReadGeometry(const GdalVectlMetaInfo* br, AbstrDataObject* ado, tile_id t, SizeT firstIndex, SizeT size)
{
	dms_assert(br);
	OGRLayer* layer = br->m_Layer;
	if (!t)
		LayerFieldEnable(layer, CharPtrRange(""), nullptr); // only set once

	const ValueClass* vc = ado->GetValuesType();
	switch (vc->GetValueClassID())
	{
		case ValueClassID::VT_DArc:
		case ValueClassID::VT_DPolygon: ReadPolyData<DPolygon>(mutable_array_cast<DPolygon>(ado)->GetWritableTile(t), layer, firstIndex, size, br->m_ReadBuffer, m_hDS); break;
		case ValueClassID::VT_FArc:
		case ValueClassID::VT_FPolygon: ReadPolyData<FPolygon>(mutable_array_cast<FPolygon>(ado)->GetWritableTile(t), layer, firstIndex, size, br->m_ReadBuffer, m_hDS); break;
#if defined (DMS_TM_HAS_INT_SEQ)
		case ValueClassID::VT_IArc:
		case ValueClassID::VT_IPolygon: ReadPolyData<IPolygon>(mutable_array_cast<IPolygon>(ado)->GetWritableTile(t), layer, firstIndex, size, br->m_ReadBuffer, m_hDS); break;
		case ValueClassID::VT_UArc:
		case ValueClassID::VT_UPolygon: ReadPolyData<UPolygon>(mutable_array_cast<UPolygon>(ado)->GetWritableTile(t), layer, firstIndex, size, br->m_ReadBuffer, m_hDS); break;
		case ValueClassID::VT_WArc:
		case ValueClassID::VT_WPolygon: ReadPolyData<WPolygon>(mutable_array_cast<WPolygon>(ado)->GetWritableTile(t), layer, firstIndex, size, br->m_ReadBuffer, m_hDS); break;
		case ValueClassID::VT_SArc:
		case ValueClassID::VT_SPolygon: ReadPolyData<SPolygon>(mutable_array_cast<SPolygon>(ado)->GetWritableTile(t), layer, firstIndex, size, br->m_ReadBuffer, m_hDS); break;
#endif
		case ValueClassID::VT_DPoint: ReadPointData<DPoint>(mutable_array_cast<DPoint>(ado)->GetWritableTile(t), layer, firstIndex, size, m_hDS); break;
		case ValueClassID::VT_FPoint: ReadPointData<FPoint>(mutable_array_cast<FPoint>(ado)->GetWritableTile(t), layer, firstIndex, size, m_hDS); break;
		case ValueClassID::VT_IPoint: ReadPointData<IPoint>(mutable_array_cast<IPoint>(ado)->GetWritableTile(t), layer, firstIndex, size, m_hDS); break;
		case ValueClassID::VT_UPoint: ReadPointData<UPoint>(mutable_array_cast<UPoint>(ado)->GetWritableTile(t), layer, firstIndex, size, m_hDS); break;
		case ValueClassID::VT_SPoint: ReadPointData<SPoint>(mutable_array_cast<SPoint>(ado)->GetWritableTile(t), layer, firstIndex, size, m_hDS); break;
		case ValueClassID::VT_WPoint: ReadPointData<WPoint>(mutable_array_cast<WPoint>(ado)->GetWritableTile(t), layer, firstIndex, size, m_hDS); break;
		case ValueClassID::VT_SharedStr: ReadStringData(mutable_array_cast<SharedStr>(ado)->GetWritableTile(t), layer, firstIndex, size, br->m_ReadBuffer, m_hDS); break;
	default:
			ado->throwItemErrorF(
				"GdalVectSM::ReadDataItem not implemented for DataItems with ValuesUnitType: %s", 
				vc->GetName()
			);
	}

	br->m_CurrFeatureIndex += size;

	return true;
}

void ReadStrAttrData(OGRLayer* layer, SizeT currFieldIndex, sequence_traits<SharedStr>::seq_t data, SizeT firstIndex, SizeT size, GDALDataset* hDS)
{
	SizeT i=0;
	auto lch = MakeLCH(
		[firstIndex, &i]() -> SharedStr { return mySSPrintF("Reading String Field %d", i + firstIndex); }
	);

	for (; i!=size; ++i)
	{
		if (!(i & 0xf000))
			DMS_ASyncContinueCheck();

		DataArray<SharedStr>::reference dataElemRef = data[i];
		bool dataset_has_random_layer_read_capability = hDS->TestCapability(ODsCRandomLayerRead);
		gdalVectImpl::FeaturePtr feat = dataset_has_random_layer_read_capability ? GetNextFeatureInterleaved(layer, hDS) : layer->GetNextFeature();
		if (!feat)
		{
			Assign(dataElemRef, Undefined());
			continue;
		}

		bool feature_field_is_null = feat->IsFieldNull(currFieldIndex);
		bool feature_field_is_set = feat->IsFieldSet(currFieldIndex);

		if (feat && !feature_field_is_null && feature_field_is_set)
		{
			CharPtr fieldAsString;
			fieldAsString = feat->GetFieldAsString(currFieldIndex);
			dataElemRef.assign(fieldAsString, fieldAsString + StrLen(fieldAsString));
		}
		else
			Assign( dataElemRef, Undefined() );
	}
}

bool GDALFieldCanBeInterpretedAsInteger(gdalVectImpl::FeaturePtr &feat, SizeT &currFieldIndex)
{
	if ((!feat || !feat->IsFieldSetAndNotNull(currFieldIndex)) || (std::string(feat->GetFieldAsString(currFieldIndex)) != "0" && feat->GetFieldAsInteger64(currFieldIndex) == 0))
		return false;
	return true;
}

bool GDALFieldCanBeInterpretedAsDouble(gdalVectImpl::FeaturePtr& feat, SizeT& currFieldIndex)
{
	if (!feat)
		return false;
	if (!feat->IsFieldSetAndNotNull(currFieldIndex))
		return false;
	if (feat->GetFieldAsDouble(currFieldIndex) != 0)
		return true;
	auto fieldAsCharPtr = feat->GetFieldAsString(currFieldIndex); // who owns this ? lifetime ?
	if (!fieldAsCharPtr || !*fieldAsCharPtr)
		return false;
	Float64 fieldAsFloat64 = 0;
	AssignValueFromCharPtr(fieldAsFloat64, fieldAsCharPtr);
	if (fieldAsFloat64 == 0)
	{
		dms_assert(IsDefined(fieldAsFloat64));
		return true;
	}
	return false;
}

template <typename T>
void ReadInt32AttrData(OGRLayer* layer, SizeT currFieldIndex, typename sequence_traits<T>::seq_t data, SizeT firstIndex, SizeT size, GDALDataset* hDS)
{
	SizeT i=0;
	auto lch = MakeLCH(
		[firstIndex, &i]() -> SharedStr { return mySSPrintF("Reading Int32 Field %d", i + firstIndex); }
	);

	for (; i!=size; ++i)
	{
		typename DataArray<T>::reference dataElemRef = data[i];

		gdalVectImpl::FeaturePtr feat = hDS->TestCapability(ODsCRandomLayerRead) ? GetNextFeatureInterleaved(layer, hDS) : layer->GetNextFeature();
		
		if (GDALFieldCanBeInterpretedAsInteger(feat, currFieldIndex))
			dataElemRef = feat->GetFieldAsInteger(currFieldIndex);
		else
			Assign( dataElemRef, Undefined() );
	}
}

template <typename T>
void ReadInt64AttrData(OGRLayer* layer, SizeT currFieldIndex, typename sequence_traits<T>::seq_t data, SizeT firstIndex, SizeT size, GDALDataset* hDS)
{
	SizeT i = 0;
	auto lch = MakeLCH(
		[firstIndex, &i]() -> SharedStr { return mySSPrintF("Reading Int32 Field %d", i + firstIndex); }
	);

	for (; i != size; ++i)
	{
		typename DataArray<T>::reference dataElemRef = data[i];

		gdalVectImpl::FeaturePtr feat = hDS->TestCapability(ODsCRandomLayerRead) ? GetNextFeatureInterleaved(layer, hDS) : layer->GetNextFeature();
		
		if (GDALFieldCanBeInterpretedAsInteger(feat, currFieldIndex))
			dataElemRef = feat->GetFieldAsInteger64(currFieldIndex);
		else
			Assign(dataElemRef, Undefined());
	}
}

template <typename T>
void ReadDoubleAttrData(OGRLayer* layer, SizeT currFieldIndex, typename sequence_traits<T>::seq_t data, SizeT firstIndex, SizeT size, GDALDataset* hDS)
{
	SizeT i=0;
	auto lch = MakeLCH(
		[firstIndex, &i]() -> SharedStr { return mySSPrintF("Reading Float64 Field %d", i + firstIndex); }
	);

	for (; i!=size; ++i)
	{
		typename DataArray<T>::reference dataElemRef = data[i];

		gdalVectImpl::FeaturePtr feat = hDS->TestCapability(ODsCRandomLayerRead) ? GetNextFeatureInterleaved(layer, hDS) : layer->GetNextFeature();
		if (GDALFieldCanBeInterpretedAsDouble(feat, currFieldIndex))
			dataElemRef = feat->GetFieldAsDouble(currFieldIndex);
		else
			Assign( dataElemRef, Undefined() );
	}
}

bool GdalVectSM::ReadAttrData(const GdalVectlMetaInfo* br, AbstrDataObject * ado, tile_id t, SizeT firstIndex, SizeT size)
{
	OGRLayer* layer = br->Layer();
	if (layer && t==0 && firstIndex == 0 && size)
		layer->ResetReading();
//	dms_assert(br->m_CurrFieldIndex != -1); 
	// TODO G8: REMOVE following if, as it should have been set by the GdalVectlMetaInfo provider
	if (br->m_CurrFieldIndex==-1) {
		// TODO: Lock.
		auto adi = br->CurrWD();
		br->m_CurrFieldIndex = LayerFieldEnable(layer, adi->GetName().c_str(), adi); // only set once
		if (br->m_CurrFieldIndex == -1)
			throwErrorF("GdalVectSM::ReadAttrData", "No column '%s' available in datasource", br->m_RelativeName);
	}
	dms_assert(br->m_CurrFieldIndex != -1); 

	OGRFeatureDefn* layerDefn = layer->GetLayerDefn();
	dms_assert(layerDefn);
	OGRFieldDefn* fieldDefn = layerDefn->GetFieldDefn(br->m_CurrFieldIndex);
	
	ValueClassID ft = fieldDefn ? gdalVectImpl::OGR2ValueType(fieldDefn->GetType(), fieldDefn->GetSubType()) : ValueClassID::VT_Unknown;
	ValueClassID at = ado->GetValuesType()->GetValueClassID();

	switch (ft)
	{
		case ValueClassID::VT_Unknown:
		case ValueClassID::VT_SharedStr: switch (at) {
			case ValueClassID::VT_SharedStr: ::ReadStrAttrData           (layer, br->m_CurrFieldIndex, mutable_array_cast<SharedStr>(ado)->GetWritableTile(t, dms_rw_mode::write_only_all), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_Bool:      ::ReadInt64AttrData<Bool>   (layer, br->m_CurrFieldIndex, mutable_array_cast<Bool>     (ado)->GetWritableTile(t, dms_rw_mode::write_only_all), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_UInt2:     ::ReadInt64AttrData<UInt2>  (layer, br->m_CurrFieldIndex, mutable_array_cast<UInt2>    (ado)->GetWritableTile(t, dms_rw_mode::write_only_all), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_UInt4:     ::ReadInt64AttrData<UInt4>  (layer, br->m_CurrFieldIndex, mutable_array_cast<UInt4>    (ado)->GetWritableTile(t, dms_rw_mode::write_only_all), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_Int64:     ::ReadInt64AttrData< Int64> (layer, br->m_CurrFieldIndex, mutable_array_cast<Int64>    (ado)->GetWritableTile(t, dms_rw_mode::write_only_all), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_UInt64:    ::ReadInt64AttrData<UInt64> (layer, br->m_CurrFieldIndex, mutable_array_cast<UInt64>   (ado)->GetWritableTile(t, dms_rw_mode::write_only_all), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_Int32:     ::ReadInt64AttrData< Int32> (layer, br->m_CurrFieldIndex, mutable_array_cast<Int32>    (ado)->GetWritableTile(t, dms_rw_mode::write_only_all), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_UInt32:    ::ReadInt64AttrData<UInt32> (layer, br->m_CurrFieldIndex, mutable_array_cast<UInt32>   (ado)->GetWritableTile(t, dms_rw_mode::write_only_all), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_Int16:     ::ReadInt64AttrData< Int16> (layer, br->m_CurrFieldIndex, mutable_array_cast<Int16>    (ado)->GetWritableTile(t, dms_rw_mode::write_only_all), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_UInt16:    ::ReadInt64AttrData<UInt16> (layer, br->m_CurrFieldIndex, mutable_array_cast<UInt16>   (ado)->GetWritableTile(t, dms_rw_mode::write_only_all), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_Int8:      ::ReadInt64AttrData< Int8 > (layer, br->m_CurrFieldIndex, mutable_array_cast<Int8>     (ado)->GetWritableTile(t, dms_rw_mode::write_only_all), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_UInt8:     ::ReadInt64AttrData<UInt8 > (layer, br->m_CurrFieldIndex, mutable_array_cast<UInt8>    (ado)->GetWritableTile(t, dms_rw_mode::write_only_all), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_Float32:   ::ReadDoubleAttrData<Float32>(layer, br->m_CurrFieldIndex, mutable_array_cast<Float32>  (ado)->GetWritableTile(t, dms_rw_mode::write_only_all), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_Float64:   ::ReadDoubleAttrData<Float64>(layer, br->m_CurrFieldIndex, mutable_array_cast<Float64>  (ado)->GetWritableTile(t, dms_rw_mode::write_only_all), firstIndex, size, m_hDS); goto ready;
			default: goto typeConflict;
		}

		// gdal subtypes
		case ValueClassID::VT_Int64: switch (at) {
			case ValueClassID::VT_Bool:    ::ReadInt64AttrData<Bool>   (layer, br->m_CurrFieldIndex, mutable_array_cast<Bool> (ado)->GetWritableTile(t).get_view(), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_UInt2:   ::ReadInt64AttrData<UInt2>  (layer, br->m_CurrFieldIndex, mutable_array_cast<UInt2> (ado)->GetWritableTile(t).get_view(), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_UInt4:   ::ReadInt64AttrData<UInt4>  (layer, br->m_CurrFieldIndex, mutable_array_cast<UInt4> (ado)->GetWritableTile(t).get_view(), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_Int64:   ::ReadInt64AttrData< Int64> (layer, br->m_CurrFieldIndex, mutable_array_cast<Int64> (ado)->GetWritableTile(t).get_view(), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_UInt64:  ::ReadInt64AttrData<UInt64> (layer, br->m_CurrFieldIndex, mutable_array_cast<UInt64>(ado)->GetWritableTile(t).get_view(), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_Int32:   ::ReadInt64AttrData< Int32> (layer, br->m_CurrFieldIndex, mutable_array_cast<Int32> (ado)->GetWritableTile(t).get_view(), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_UInt32:  ::ReadInt64AttrData<UInt32> (layer, br->m_CurrFieldIndex, mutable_array_cast<UInt32> (ado)->GetWritableTile(t).get_view(), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_Int16:   ::ReadInt64AttrData< Int16> (layer, br->m_CurrFieldIndex, mutable_array_cast<Int16> (ado)->GetWritableTile(t).get_view(), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_UInt16:  ::ReadInt64AttrData<UInt16> (layer, br->m_CurrFieldIndex, mutable_array_cast<UInt16> (ado)->GetWritableTile(t).get_view(), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_Int8:    ::ReadInt64AttrData< Int8 > (layer, br->m_CurrFieldIndex, mutable_array_cast<Int8> (ado)->GetWritableTile(t).get_view(), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_UInt8:   ::ReadInt64AttrData<UInt8 > (layer, br->m_CurrFieldIndex, mutable_array_cast<UInt8> (ado)->GetWritableTile(t).get_view(), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_Float32: ::ReadInt64AttrData<Float32>(layer, br->m_CurrFieldIndex, mutable_array_cast<Float32>(ado)->GetWritableTile(t).get_view(), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_Float64: ::ReadInt64AttrData<Float64>(layer, br->m_CurrFieldIndex, mutable_array_cast<Float64>(ado)->GetWritableTile(t).get_view(), firstIndex, size, m_hDS); goto ready;
			default: goto typeConflict;
		}
		case ValueClassID::VT_Bool:
		case ValueClassID::VT_UInt16:
		case ValueClassID::VT_Int16:
		case ValueClassID::VT_Int32: switch (at) {
			case ValueClassID::VT_Bool:    ::ReadInt32AttrData<Bool>(layer, br->m_CurrFieldIndex, mutable_array_cast<Bool> (ado)->GetWritableTile(t).get_view(), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_UInt2:   ::ReadInt32AttrData<UInt2>(layer, br->m_CurrFieldIndex, mutable_array_cast<UInt2> (ado)->GetWritableTile(t).get_view(), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_UInt4:   ::ReadInt32AttrData<UInt4>(layer, br->m_CurrFieldIndex, mutable_array_cast<UInt4> (ado)->GetWritableTile(t).get_view(), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_Int64:   ::ReadInt32AttrData< Int64> (layer, br->m_CurrFieldIndex, mutable_array_cast<Int64> (ado)->GetWritableTile(t).get_view(), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_UInt64:  ::ReadInt32AttrData<UInt64> (layer, br->m_CurrFieldIndex, mutable_array_cast<UInt64>(ado)->GetWritableTile(t).get_view(), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_Int32:   ::ReadInt32AttrData< Int32> (layer, br->m_CurrFieldIndex, mutable_array_cast< Int32> (ado)->GetWritableTile(t).get_view(), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_UInt32:  ::ReadInt32AttrData<UInt32> (layer, br->m_CurrFieldIndex, mutable_array_cast<UInt32> (ado)->GetWritableTile(t).get_view(), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_Int16:   ::ReadInt32AttrData< Int16> (layer, br->m_CurrFieldIndex, mutable_array_cast< Int16> (ado)->GetWritableTile(t).get_view(), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_UInt16:  ::ReadInt32AttrData<UInt16> (layer, br->m_CurrFieldIndex, mutable_array_cast<UInt16> (ado)->GetWritableTile(t).get_view(), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_Int8:    ::ReadInt32AttrData< Int8 > (layer, br->m_CurrFieldIndex, mutable_array_cast< Int8 > (ado)->GetWritableTile(t).get_view(), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_UInt8:   ::ReadInt32AttrData<UInt8 > (layer, br->m_CurrFieldIndex, mutable_array_cast<UInt8 > (ado)->GetWritableTile(t).get_view(), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_Float32: ::ReadInt32AttrData<Float32>(layer, br->m_CurrFieldIndex, mutable_array_cast<Float32>(ado)->GetWritableTile(t).get_view(), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_Float64: ::ReadInt32AttrData<Float64>(layer, br->m_CurrFieldIndex, mutable_array_cast<Float64>(ado)->GetWritableTile(t).get_view(), firstIndex, size, m_hDS); goto ready;
			default: goto typeConflict;
		}

		case ValueClassID::VT_Float32:
		case ValueClassID::VT_Float64: switch (at) {
			case ValueClassID::VT_Int32:   ::ReadDoubleAttrData< Int32> (layer, br->m_CurrFieldIndex, mutable_array_cast< Int32> (ado)->GetWritableTile(t).get_view(), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_UInt32:  ::ReadDoubleAttrData<UInt32> (layer, br->m_CurrFieldIndex, mutable_array_cast<UInt32> (ado)->GetWritableTile(t).get_view(), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_Int16:   ::ReadDoubleAttrData< Int16> (layer, br->m_CurrFieldIndex, mutable_array_cast< Int16> (ado)->GetWritableTile(t).get_view(), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_UInt16:  ::ReadDoubleAttrData<UInt16> (layer, br->m_CurrFieldIndex, mutable_array_cast<UInt16> (ado)->GetWritableTile(t).get_view(), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_Int8:    ::ReadDoubleAttrData< Int8 > (layer, br->m_CurrFieldIndex, mutable_array_cast< Int8 > (ado)->GetWritableTile(t).get_view(), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_UInt8:   ::ReadDoubleAttrData<UInt8 > (layer, br->m_CurrFieldIndex, mutable_array_cast<UInt8 > (ado)->GetWritableTile(t).get_view(), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_Float32: ::ReadDoubleAttrData<Float32>(layer, br->m_CurrFieldIndex, mutable_array_cast<Float32>(ado)->GetWritableTile(t).get_view(), firstIndex, size, m_hDS); goto ready;
			case ValueClassID::VT_Float64: ::ReadDoubleAttrData<Float64>(layer, br->m_CurrFieldIndex, mutable_array_cast<Float64>(ado)->GetWritableTile(t).get_view(), firstIndex, size, m_hDS); goto ready;
			default: goto typeConflict;
		}
		default: goto typeConflict;
	}
ready:
	br->m_CurrFeatureIndex += size;
	return true;
typeConflict:
	throwErrorF("gdal.vect", "Cannot read attribute data of type %d into attribute of type %d", int(ft), int(at));
}

bool GdalVectSM::ReadLayerData(const GdalVectlMetaInfo* br, AbstrDataObject* ado, tile_id t)
{
	auto trd = ado->GetTiledRangeData();
	SizeT firstIndex = trd->GetFirstRowIndex(t);
	auto size      = trd->GetTileSize(t);
	if (size)
		br->SetCurrFeatureIndex(firstIndex);

	auto adi = br->CurrRD();
	if (adi->GetID() == token::geometry || adi->GetAbstrValuesUnit()->GetValueType()->GetNrDims() == 2)
		return ReadGeometry(br, ado, t, firstIndex, size);
	return ReadAttrData(br, ado, t, firstIndex, size);
}

StorageMetaInfoPtr GdalVectSM::GetMetaInfo(const TreeItem* storageHolder, TreeItem* adi, StorageAction sa) const
{
	return std::make_unique<GdalVectlMetaInfo>(this, storageHolder, adi);
}

bool GdalVectSM::ReadDataItem(StorageMetaInfoPtr smi, AbstrDataObject* borrowedReadResultHolder, tile_id t)
{
	dms_assert(IsOpen());

	return ReadLayerData(debug_cast<const GdalVectlMetaInfo*>(smi.get()), borrowedReadResultHolder, t);
}

OGRLayer* GetLayerFromDataItem(GDALDatasetHandle& m_hDS, SharedStr layername)
{
	SizeT layerCount = m_hDS->GetLayerCount();
	OGRLayer* layer =
		(layerCount == 1)
		? m_hDS->GetLayer(0)
		: m_hDS->GetLayerByName(layername.c_str());

	return layer;
}

SizeT ReadUnitRangeInterleaved(OGRLayer* layer, GDALDataset* m_hDS)
{
	SizeT count = 0;
	OGRLayer* poFeatureLayer = nullptr;

	while (true)
	{
		gdalVectImpl::FeaturePtr poFeature = m_hDS->GetNextFeature(&poFeatureLayer, nullptr, nullptr, nullptr);
		if (poFeature == nullptr)
			break;

		if (layer->GetName() != poFeatureLayer->GetName())
			continue;

		count++;
	}

	m_hDS->ResetReading(); // Reset read cursor

	return count;
}

bool GdalVectSM::WriteUnitRange(StorageMetaInfoPtr&& smi)
{
	DBG_START("gdal.vect", "WriteUnitRange", false);
	return true;
}

template<typename PointType>
void SetPointGeometryForFeature(OGRFeature * feature, PointType b, ValueComposition vc)
{
	dms_assert(vc == ValueComposition::Single);
	OGRPoint pt;
	pt.setX(b.Col());
	pt.setY(b.Row());
	feature->SetGeometry(&pt); // TODO: makes a copy, switch to SetGeometryDirectly
}

template<typename SequenceType>
void SetArcGeometryForFeature(OGRFeature* feature, SequenceType b, ValueComposition vc)
{
	dms_assert(vc == ValueComposition::Sequence);
	auto OGRLine = (OGRLineString*)OGRGeometryFactory::createGeometry(wkbLineString);

	typedef typename sequence_traits<SequenceType>::container_type SequenceArray;
//	typename DataArrayBase<SequenceType>::const_reference sequence = b;

	for (auto&& [y, x] : b) { // points in reverse order
		OGRPoint pt(x, y);
		OGRLine->addPoint(&pt);
	}
	feature->SetGeometry((OGRGeometry*)OGRLine); // TODO: makes a copy, switch to SetGeometryDirectly
}

template<typename PointType>
void SetPolygonGeometryForFeature(OGRFeature* feature, SA_ConstReference<PointType> b, ValueComposition vc)
{
	dms_assert(vc == ValueComposition::Polygon);


	auto OGRMultiPoly = (OGRMultiPolygon*)OGRGeometryFactory::createGeometry(wkbMultiPolygon);
	auto OGRPoly      = (OGRPolygon*)OGRGeometryFactory::createGeometry(wkbPolygon);

	typedef typename sequence_traits<PointType  >::container_type PolygonType;
	typedef typename sequence_traits<PolygonType>::container_type PolygonArray;
	typename DataArrayBase<PolygonType>::const_reference polygon = b;

	boost::polygon::SA_ConstRingIterator<PointType>
		ri(polygon, 0),
		re(polygon, -1);

	for (; ri != re; ++ri) // rings
	{
		auto ring = *ri;
		auto OGRRing = (OGRLinearRing*)OGRGeometryFactory::createGeometry(wkbLinearRing);

		for (auto&& [y, x] : ring) { // points, in reverse order
			OGRPoint pt;
			pt.setX(x);
			pt.setY(y);
			OGRRing->addPoint(&pt);
		}

		OGRPoly->addRing(OGRRing);
	}

	OGRMultiPoly->addGeometry(OGRPoly);

	feature->SetGeometry((OGRGeometry*)OGRMultiPoly); // TODO: makes a copy, switch to SetGeometryDirectly
}

bool GdalVectSM::WriteGeometryElement(const AbstrDataItem* adi, OGRFeature* feature, tile_id t, SizeT tileFeatureIndex)
{
	const AbstrDataObject* ado = adi->GetRefObj();
	auto				   avu = adi->GetAbstrValuesUnit();

	GDAL_ErrorFrame frame;

	visit<typelists::seq_points>(avu,
		[this, ado, adi, feature, tileFeatureIndex, t, &frame] <typename value_type> (const Unit<value_type>*) 
		{
			auto vc = adi->GetValueComposition();
			switch (vc) {
				case ValueComposition::Single: {
					auto darray = const_array_cast<value_type>(adi)->GetDataRead(t); // GetDataRead(t) can also accept tile t
					auto b = darray.begin(), e = darray.end();
					dms_assert(tileFeatureIndex < (e - b));

					SetPointGeometryForFeature(feature, *(b+tileFeatureIndex), vc);

					break;
				}
				case ValueComposition::Sequence:
				{
					using sequence_type = typename sequence_traits<value_type>::container_type;

					auto darray = debug_valcast<const DataArray<sequence_type>*>(ado)->GetDataRead(t);
					auto b = darray.begin(), e = darray.end();
					dms_assert(tileFeatureIndex < (e - b));

					SetArcGeometryForFeature(feature, b[tileFeatureIndex], vc);

					break;
				}
				case ValueComposition::Polygon: {
					using PolygonType = typename sequence_traits<value_type  >::container_type;
//					typedef typename sequence_traits<PolygonType>::container_type PolygonArray;
					
					auto polyData = debug_valcast<const DataArray<PolygonType>*>(ado)->GetDataRead(t);

					SetPolygonGeometryForFeature(feature, polyData[tileFeatureIndex], vc);
				}
				break;
			}
		}
	);

	return true;
}

template<typename T>
void SetField(OGRFeature* feature, int i, T b)
{
	if (IsDefined(b))
		feature->SetField(i, b);
}

void SetField(OGRFeature* feature, int i, SA_ConstReference<char> b)
{
	if (IsDefined(b))
		feature->SetField(i, b.size(), b.begin());
}

template<bit_size_t N, typename Block>
void SetField(OGRFeature* feature, int i, bit_reference<N, Block> b)
{
	// bit_values are always defined
	feature->SetField(i, b.value());
}

void SetField(OGRFeature* feature, int i, UInt8 b)
{
	if (IsDefined(b))
		feature->SetField(i, ThrowingConvert<Int32>(b));
}

void SetField(OGRFeature* feature, int i, UInt16 b)
{
	if (IsDefined(b))
		feature->SetField(i, ThrowingConvert<Int32>(b));
}

void SetField(OGRFeature* feature, int i, UInt32 b)
{
	if (IsDefined(b))
		feature->SetField(i, ThrowingConvert<Int32>(b));
}

void SetField(OGRFeature* feature, int i, UInt64 b)
{
	if (IsDefined(b))
		feature->SetField(i, ThrowingConvert<Int64>(b));
}


bool GdalVectSM::WriteFieldElement(const AbstrDataItem* adi, int field_index, OGRFeature* feature, tile_id t, SizeT tileFeatureIndex)
{
	const AbstrDataObject* ado = adi->GetRefObj();
	auto				   avu = adi->GetAbstrValuesUnit();

	GDAL_ErrorFrame frame;
	
	using field_types = boost::mpl::vector <Float32, Float64, Bool, UInt2, UInt4, Int8, UInt8, Int16, UInt16, Int32, UInt32, Int64, UInt64, SharedStr>;

	visit<field_types>(avu,
		[this, ado, adi, feature, tileFeatureIndex, field_index, t, &frame] <typename field_type> (const Unit<field_type>*) 
		{
			auto darray = const_array_cast<field_type>(adi)->GetDataRead(t); // GetDataRead(t) can also accept tile t

			auto b = darray.begin(), e = darray.end();
			dms_assert(tileFeatureIndex < (e - b));

			SetField(feature, field_index, *(b+tileFeatureIndex) );
		}
	);
	return true;
}

std::vector<DataReadLock> ReadableDataHandles(SharedStr layername, DataItemsWriteStatusInfo& dataItemsStatusInfo)
{
	std::vector<DataReadLock> dataReadLocks;

	for (auto& writableField : dataItemsStatusInfo.m_LayerAndFieldIDMapping[GetTokenID_mt(layername.c_str())])
	{
		if (not writableField.second.doWrite)
			continue;

		auto& adi_n = writableField.second.m_DataHolder;
		dataReadLocks.emplace_back(adi_n);
	}
	return dataReadLocks;
}

std::vector<TileCRef> ReadableTileHandles(const std::vector<DataReadLock>& drl, tile_id t)
{
	std::vector<TileCRef> tileReadLocks;
	tileReadLocks.reserve(drl.size());

	for (const auto& writableField : drl)
	{
		tileReadLocks.emplace_back(writableField->GetReadableTileLock(t));
	}
	return tileReadLocks;
}

SizeT ReadUnitRange(OGRLayer* layer, GDALDataset* m_hDS)
{
	if (!layer)
		return false;

	GDAL_ErrorFrame gdal_error_frame;

	SizeT count = layer->GetFeatureCount();

	const bool bRandomLayerReading = CPL_TO_BOOL(m_hDS->TestCapability(ODsCRandomLayerRead));

	if (count == -1 && bRandomLayerReading)
	{
		count = ReadUnitRangeInterleaved(layer, m_hDS);
	}


	return count;
}

OGRFieldType DmsType2OGRFieldType(ValueClassID id)
{
	switch (id) {
	case ValueClassID::VT_Int32:
		return OFTInteger;
	case ValueClassID::VT_Bool:
		return OFTInteger;
	case ValueClassID::VT_UInt2:
		return OFTInteger;
	case ValueClassID::VT_UInt4:
		return OFTInteger;
	case ValueClassID::VT_UInt8:
		return OFTInteger;
	case ValueClassID::VT_Int8:
		return OFTInteger;
	case ValueClassID::VT_UInt16:
		return OFTInteger;
	case ValueClassID::VT_Int16:
		return OFTInteger;
	case ValueClassID::VT_UInt32:
		return OFTInteger;
	case ValueClassID::VT_Int64:
		return OFTInteger64;
	case ValueClassID::VT_UInt64:
		return OFTInteger64;
	case ValueClassID::VT_Float32:
		return OFTReal;
	case ValueClassID::VT_Float64:
		return OFTReal;
	case ValueClassID::VT_SharedStr:
		return OFTString;
	default:
		return OGRFieldType::OFTMaxType;
	}
}

OGRFieldSubType DmsType2OGRSubFieldType(ValueClassID id)
{
	switch (id) {
	case ValueClassID::VT_Bool:
		return OFSTBoolean;
	case ValueClassID::VT_UInt16:
		return OFSTInt16;
	case ValueClassID::VT_Int16:
		return OFSTInt16;
	case ValueClassID::VT_Float32:
		return OFSTFloat32;
	default:
		return OFSTNone;
	}
}

OGRwkbGeometryType DmsType2OGRGeometryType(ValueComposition vc)
{
	switch (vc) {
	case ValueComposition::Single:
		return wkbPoint;
	case ValueComposition::Sequence:
		return wkbLineString;
	case ValueComposition::Polygon:
		return wkbMultiPolygon;
	}
	return wkbUnknown;
}

void SetFeatureDefnForOGRLayerFromLayerHolder(const TreeItem* subItem, OGRLayer* layerHandle, SharedStr layerName, DataItemsWriteStatusInfo& disi)
{
	GDAL_ErrorFrame error_frame;
	int geometryFieldCount = 0;
	for (auto fieldCandidate = subItem; fieldCandidate; fieldCandidate = subItem->WalkConstSubTree(fieldCandidate))
	{
		if (not (IsDataItem(fieldCandidate) and fieldCandidate->IsStorable()))
			continue;
			
		auto subDI = AsDataItem(fieldCandidate);
		auto vci = subDI->GetAbstrValuesUnit()->GetValueType()->GetValueClassID();
		auto vc = subDI->GetValueComposition();

		if (subItem->GetID() == token::geometry || vc <= ValueComposition::Sequence && (vci >= ValueClassID::VT_SPoint && vci < ValueClassID::VT_FirstAfterPolygon))
		{
			if (geometryFieldCount++)
				throwErrorF("gdalwrite.vect", "error, multiple geometry fields are unsupported in GeoDMS.");
		}
		else
		{
			SharedStr       fieldName = SharedStr(fieldCandidate->GetID());
			int             bApproxOK = TRUE;

			OGRFieldType    type    = DmsType2OGRFieldType(vci);
			OGRFieldSubType subtype = DmsType2OGRSubFieldType(vci);
			OGRFieldDefn    fieldDefn(fieldName.c_str(), type);         error_frame.ThrowUpWhateverCameUp();
			fieldDefn.SetSubType(subtype);                   error_frame.ThrowUpWhateverCameUp();
			layerHandle->CreateField(&fieldDefn, bApproxOK); error_frame.ThrowUpWhateverCameUp();

			// check for laundered fieldname
			gdalVectImpl::FeaturePtr feat = OGRFeature::CreateFeature(layerHandle->GetLayerDefn());
			auto currentFieldName = std::string(feat->GetFieldDefnRef(feat->GetFieldCount() - 1)->GetNameRef());
			if (std::string(fieldName.c_str()).compare(currentFieldName) != 0) // fieldname is laundered
				disi.SetLaunderedName(GetTokenID_mt(layerName), GetTokenID_mt(fieldName), SharedStr(currentFieldName.c_str()));
		}
	}
	return;
}

bool DataSourceHasNamelessLayer(SharedStr datasourceName)
{
	if (std::string(CPLGetExtension(datasourceName.c_str())) == "shp" || std::string(CPLGetExtension(datasourceName.c_str())) == "dbf" || std::string(CPLGetExtension(datasourceName.c_str())) == "csv")
		return true;
	return false;
}

void InitializeLayersFieldsAndDataitemsStatus(const StorageMetaInfo& smi, DataItemsWriteStatusInfo& disi, GDALDatasetHandle& result, CPLStringList& layerOptionArray)
{
	const TreeItem* storageHolder = smi.StorageHolder();
	const TreeItem* unitItem = nullptr;
	const AbstrUnit* layerDomain = nullptr;
	SharedStr layerName;
	SharedStr fieldName;
	TokenID layerID;
	int geometryFieldCount = 0;
	SharedStr datasourceName = smi.StorageManager()->GetNameStr();

	GDAL_ErrorFrame error_frame;
	GDAL_ConfigurationOptionsFrame config_frame(GetOptionArray(dynamic_cast<const GdalMetaInfo&>(smi).m_ConfigurationOptions));

	for (auto subItem = storageHolder->WalkConstSubTree(nullptr); subItem; subItem = storageHolder->WalkConstSubTree(subItem))
	{
		if (not (IsDataItem(subItem) and subItem->IsStorable()))
			continue;

		unitItem = GetLayerHolderFromDataItem(storageHolder, subItem);
		layerDomain = AsDynamicUnit(unitItem);
		layerName = unitItem->GetName().c_str();
		OGRLayer* layerHandle = result.dsh_->GetLayerByName(layerName.c_str()); error_frame.ThrowUpWhateverCameUp();

		if (not layerHandle && (DataSourceHasNamelessLayer(datasourceName)))
			layerHandle = result.dsh_->GetLayer(0);

		if (not layerHandle)
		{
			OGRwkbGeometryType eGType = GetGeometryTypeFromGeometryDataItem(unitItem);
			auto ogrSR = GetOGRSpatialReferenceFromDataItems(storageHolder);
			
			error_frame.ThrowUpWhateverCameUp();
			layerHandle = result.dsh_->CreateLayer(layerName.c_str(), ogrSR ? &ogrSR.value() : nullptr, eGType, layerOptionArray); 
			error_frame.ThrowUpWhateverCameUp();
			SetFeatureDefnForOGRLayerFromLayerHolder(unitItem, layerHandle, layerName, disi);
		}

		if (not layerHandle)
			throwErrorF("gdalwrite.vect", "unable to open gdal OGRLayer");

		auto subDI = AsDataItem(subItem);
		fieldName = subItem->GetName().c_str();

		if (layerDomain)
			subDI->GetAbstrDomainUnit()->UnifyDomain(layerDomain, "Domain of attribute", "layerDomain", UnifyMode::UM_Throw); // Check that domain of subItem is DomainUnifyable with layerItem
		auto adu = subDI->GetAbstrDomainUnit();
		auto unittype = subDI->GetAbstrValuesUnit()->GetValueType()->GetValueClassID();
		auto vci = subDI->GetAbstrValuesUnit()->GetValueType()->GetValueClassID();
		auto vc = subDI->GetValueComposition();

		if (vc <= ValueComposition::Sequence && (vci >= ValueClassID::VT_SPoint && vci < ValueClassID::VT_FirstAfterPolygon))
		{
			disi.setFieldIsWritten(GetTokenID_mt(layerName), GetTokenID_mt(fieldName), false);
			disi.setIsGeometry(GetTokenID_mt(layerName), GetTokenID_mt(fieldName), true);
		}
		else
		{
			disi.setFieldIsWritten(GetTokenID_mt(layerName), GetTokenID_mt(fieldName), false);
		}
	}
}

bool GdalVectSM::WriteDataItem(StorageMetaInfoPtr&& smiHolder)
{
	DBG_START("gdalwrite.vect", "WriteDataItem", false);
	
	auto smi = smiHolder.get();
	SharedStr datasourceName = smi->StorageManager()->GetNameStr();

	GDAL_ErrorFrame gdal_error_frame;
	GDAL_ConfigurationOptionsFrame config_frame(GetOptionArray(dynamic_cast<const GdalMetaInfo&>(*smi).m_ConfigurationOptions));
	auto layerOptionArray = GetOptionArray(dynamic_cast<const GdalMetaInfo&>(*smi).m_LayerCreationOptions);

	const TreeItem*	storageHolder	= smi->StorageHolder();
	const AbstrDataItem*   adi		= smi->CurrRD();
	const AbstrDataObject* ado		= adi->GetRefObj();
	auto				   adu      = adi->GetAbstrDomainUnit();
	auto				   avu		= adi->GetAbstrValuesUnit();
	const ValueClass*	   vc		= ado->GetValuesType();
	ValueClassID           vcID		= vc->GetValueClassID();

	auto unitItem = GetLayerHolderFromDataItem(storageHolder, adi);

	SharedStr layername(unitItem->GetName());

	if (not adi->IsStorable())
		return true;

//	auto data_object = adi->GetDataObj();
	SharedStr fieldname(adi->GetName());
	auto layerTokenT = GetTokenID_mt(layername.c_str());

	StorageWriteHandle storageHandle(std::move(smiHolder)); // open dataset
	if (not m_DataItemsStatusInfo.m_continueWrite)
	{
		// disable spatial index in case of GPKG, SQLite
		//if (!strcmpi(this->m_hDS->GetDriverName(), "GPKG") || !strcmpi(this->m_hDS->GetDriverName(), "SQLite"))
		//	layerOptionArray.AddString("SPATIAL_INDEX=NO");

		InitializeLayersFieldsAndDataitemsStatus(*smi, m_DataItemsStatusInfo, this->m_hDS, layerOptionArray); gdal_error_frame.ThrowUpWhateverCameUp();
		m_DataItemsStatusInfo.m_continueWrite = true;
	}

	m_DataItemsStatusInfo.RefreshInterest(storageHolder); // user may have set other iterests at this point.
	m_DataItemsStatusInfo.SetInterestForDataHolder(layerTokenT, GetTokenID_mt(fieldname), adi); // write once all dataitems are ready
	
	if (not m_DataItemsStatusInfo.LayerIsReadyForWriting(layerTokenT))
		return true;

	auto dataReadLocks = ReadableDataHandles(layername, m_DataItemsStatusInfo);
	auto layer = this->m_hDS->GetLayerByName(layername.c_str()); gdal_error_frame.ThrowUpWhateverCameUp();
	if (not layer && DataSourceHasNamelessLayer(datasourceName)) // Some drivers such as ESRI Shapefile use files as layers in contrast to GeoPackage that store the layer names internally.
		layer = this->m_hDS->GetLayer(0);

	if (not layer)
	{
		OGRwkbGeometryType eGType = GetGeometryTypeFromGeometryDataItem(unitItem);
		auto ogrSR = GetOGRSpatialReferenceFromDataItems(storageHolder); gdal_error_frame.ThrowUpWhateverCameUp();
		layer = this->m_hDS->CreateLayer(layername.c_str(), ogrSR ? &ogrSR.value() : nullptr, eGType, layerOptionArray); gdal_error_frame.ThrowUpWhateverCameUp();
		SetFeatureDefnForOGRLayerFromLayerHolder(unitItem, layer, layername, m_DataItemsStatusInfo);
	}

	if (not layer)
		throwErrorF("gdal.vect", "cannot find layer: %s in GDALDataset for writing.", layername);

	if (not layer->GetLayerDefn()->GetFieldCount()) // geosjon: fields uninitialized at this point
		SetFeatureDefnForOGRLayerFromLayerHolder(unitItem, layer, layername, m_DataItemsStatusInfo);

	auto numExistingFeatures = ::ReadUnitRange(layer, this->m_hDS);

	SizeT featureIndex = 0, tileFeatureIndex = 0;
	for (tile_id t = 0, te = adu->GetNrTiles(); t != te; ++t)
	{
		if (t+1 == te)
			reportF(MsgCategory::storage_write, SeverityTypeID::ST_MajorTrace, "gdalwrite.vect, written %u tiles of layer %s",
				adu->GetNrTiles(), layername);

		GDAL_TransactionFrame transaction_frame(this->m_hDS);
		auto tileReadLocks = ReadableTileHandles(dataReadLocks, t);

		SizeT numExistingFeaturesInTile = adu->GetTileCount(t);
		tileFeatureIndex = 0;

		auto& fieldIDMapping = m_DataItemsStatusInfo.m_LayerAndFieldIDMapping[layerTokenT]; // log(#layers) loopup outside row-loop
		// Preparation of writableFields to reduce inner-loop work
		{
			gdalVectImpl::FeaturePtr protoFeature = OGRFeature::CreateFeature(layer->GetLayerDefn()); gdal_error_frame.ThrowUpWhateverCameUp();
			for (auto& writableField : fieldIDMapping)
			{
				if (not writableField.second.doWrite)
					continue;

				if (not writableField.second.isGeometry)
				{
					if (not writableField.second.launderedName.empty())
					{
						writableField.second.name = writableField.second.launderedName;
					}
						
					auto& fieldname_n = writableField.second.name;
					writableField.second.field_index = protoFeature->GetFieldIndex(fieldname_n.c_str());
					dms_assert(writableField.second.field_index >= 0);
				}
				// destroy protoFeature
			}
		}
		for (; tileFeatureIndex < numExistingFeaturesInTile; ++tileFeatureIndex, ++featureIndex)
		{
			gdalVectImpl::FeaturePtr curFeature = numExistingFeatures ? layer->GetNextFeature() : OGRFeature::CreateFeature(layer->GetLayerDefn()); gdal_error_frame.ThrowUpWhateverCameUp();

			for (auto& writableField : fieldIDMapping)
			{
				if (not writableField.second.doWrite)
					continue;

				const AbstrDataItem* adi_n = writableField.second.m_DataHolder;
				if (writableField.second.isGeometry)
					WriteGeometryElement(adi_n, curFeature, t, tileFeatureIndex);
				else
				{
					dbg_assert( writableField.second.field_index == curFeature->GetFieldIndex(writableField.second.name.c_str()) );
					WriteFieldElement(adi_n, writableField.second.field_index, curFeature, t, tileFeatureIndex);
				}
			}
			if (not numExistingFeatures)
				layer->CreateFeature(curFeature);
			else 
				layer->SetFeature(curFeature);
		}
	}
	m_DataItemsStatusInfo.ReleaseAllLayerInterestPtrs(layerTokenT);


	//Spatial index explicitly for shapefile 
	if (CPLFetchBool(layerOptionArray, "SPATIAL_INDEX", false) && std::string(this->m_hDS->GetDriverName()).compare("ESRI Shapefile")==0) // spatial index file
	{
		if (DataSourceHasNamelessLayer(datasourceName))
			this->m_hDS->ExecuteSQL(std::format("CREATE SPATIAL INDEX ON {}", CPLGetBasename(datasourceName.c_str())).c_str(), NULL, NULL);
		else
			this->m_hDS->ExecuteSQL(std::format("CREATE SPATIAL INDEX ON {}", CPLGetBasename(layername.c_str())).c_str(), NULL, NULL);
	}

	return true;
}

bool GdalVectSM::ReadUnitRange(const StorageMetaInfo& smi) const
{
	dms_assert(IsOpen());
	auto au = smi.CurrWU();
	auto count = ::ReadUnitRange(static_cast<const GdalVectlMetaInfo*>(&smi)->m_Layer, m_hDS);
	au->SetCount(count);
	return true;
}

bool GdalVectSM::DoCheckExistence(const TreeItem* storageHolder) const
{
	return true;
/*	TODO: factor out DoOpenStorage and DoCloseStorage stuff to a RAII handle and use it to check for existence and report to stream reason for non existence 
	if (GetName() && !stricmpn(GetName(), "PG:", 3))
		return true;
	return base_type::DoCheckExistence(storageHolder);
*/
}


bool IsVatDomain(const AbstrUnit* au)
{
	return au->CanBeDomain() && au->GetNrDimensions() == 1;
}

#include "DataItemClass.h"
#include "Unit.h"
#include "UnitClass.h"
#include "mci/ValueWrap.h"
#include "mci/ValueClass.h"
#include "Unit.h"
#include "UnitClass.h"
#include "AbstrDataItem.h"

void GdalVectSM::DoUpdateTable(const TreeItem* storageHolder, AbstrUnit* layerDomain, OGRLayer* layer) const
{
	dms_assert(layer);
	GDAL_ErrorFrame gdal_error_frame;
	auto layer_geometry_type = layer->GetGeomType();
	ValueComposition gdal_vc = gdalVectImpl::OGR2ValueComposition(layer_geometry_type);
	bool create_vu_from_datasource = false;

	const OGRSpatialReference* ogrSR_ptr = layer->GetSpatialRef();
	std::optional<OGRSpatialReference> ogrSR;
	if (ogrSR_ptr)
		ogrSR = *ogrSR_ptr;

	if (!(layer_geometry_type == OGRwkbGeometryType::wkbNone))
	{
		auto geometry_item = layerDomain->GetSubTreeItemByID(token::geometry);
		AbstrDataItem* geometry = AsDynamicDataItem(geometry_item);
		if (geometry)
		{
			if (auto gvu = geometry->GetAbstrValuesUnit())
				if (gvu->GetValueType()->GetValueClassID() != ValueClassID::VT_String)
				{
					ValueComposition configured_vc = geometry->GetValueComposition();
					if (configured_vc != gdal_vc && gdal_vc != ValueComposition::Unknown)
						geometry->Fail("Value composition incompatible with GDAL's formal geometry type", FR_MetaInfo);
				}
		}
		else
		{
			if (gdal_vc == ValueComposition::Unknown) // attempt interpreting geometry type using first feature
			{
				OGRwkbGeometryType first_feature_geometry_type = OGRwkbGeometryType::wkbUnknown;
				auto first_feature = layer->GetNextFeature();
				layer->GetGeometryColumn();
				if (first_feature)
				{
					auto geometry_ref = first_feature->GetGeometryRef();
					first_feature_geometry_type = geometry_ref->getGeometryType();
					gdal_vc = gdalVectImpl::OGR2ValueComposition(first_feature_geometry_type);
				}
				layer->ResetReading();
			}

			// create default value unit from gdal_vc
			SharedUnit vu;
			if (gdal_vc == ValueComposition::Unknown)
				vu = Unit<SharedStr>::GetStaticClass()->CreateDefault();
			else
				vu = FindProjectionRef(storageHolder, layerDomain);

			if (ogrSR_ptr) // spatial reference available
			{
				SharedStr wkt = GetAsWkt(&*ogrSR_ptr);
				if (!wkt.empty())
				{
					auto vu_tmp = Unit<DPoint>::GetStaticClass()->CreateUnit(layerDomain, GetTokenID_mt("SpatialReference"));
					vu_tmp->SetSpatialReference(GetTokenID_mt(wkt));
					vu_tmp->DisableStorage(true); // used to avoid reentrance on DoUpdateTree
					if (!vu)
						vu = vu_tmp;
				}
			}
			else if (!vu) // default value
				vu = Unit<DPoint>::GetStaticClass()->CreateDefault();

			// create missing geometry treeitem
			geometry = CreateDataItem(
				layerDomain, token::geometry,
				layerDomain, vu, gdal_vc
			);
		}
		// check spatial reference
		auto gvu = GetBaseProjectionUnitFromValuesUnit(geometry);
		CheckSpatialReference(ogrSR, geometry, const_cast<AbstrUnit*>(gvu));
	}

	// Update Attribute Fields
	WeakPtr<OGRFeatureDefn> featureDefn = layer->GetLayerDefn();
	for (SizeT i = 0, numFields = featureDefn->GetFieldCount(); i!=numFields; ++i)
	{
		WeakPtr<OGRFieldDefn> fieldDefn = featureDefn->GetFieldDefn(i);
		auto subType = fieldDefn->GetSubType();
		auto raw_item_name = fieldDefn->GetNameRef();
		SharedStr itemName = as_item_name(raw_item_name, raw_item_name + StrLen(raw_item_name));
		TreeItem* tiColumn = layerDomain->GetItem(itemName.c_str());
		
		auto valueType = gdalVectImpl::OGR2ValueClass(fieldDefn->GetType(), fieldDefn->GetSubType());
		//auto explicitlyConfiguredItem = storageHolder->FindItem(itemName);
		auto explicitlyConfiguredItem = layerDomain->GetItem(itemName);
		if (explicitlyConfiguredItem && TreeItemIsColumn(explicitlyConfiguredItem)) // csv type ambiguity
			valueType = AsDataItem(explicitlyConfiguredItem)->GetAbstrValuesUnit()->GetValueType();

		CreateTreeItemColumnInfo(layerDomain, itemName.c_str(), layerDomain, valueType);
	}
}

void GdalVectSM::DoUpdateTree(const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const
{
	if (dynamic_cast<const GdalWritableVectSM*>(this))
		return;

	if (curr->IsDisabledStorage())
		return;

	NonmappableStorageManager::DoUpdateTree(storageHolder, curr, sm);

	dms_assert(storageHolder);

	if (IsUnit(curr))
	{
		if (curr->HasCalculator())
		{
			if (!IsReadOnly())
				return;
			if (curr != storageHolder)
				return;
			SharedStr currFullName = curr->GetFullName();
			reportF(SeverityTypeID::ST_Warning, "'%s' has a calculation rule and has a configured data source. If related attributes should be read from '%s', consider reading it as a separate table and use rjoin on a primary-key to obtain the read attribute values."
				, currFullName.c_str()
				, GetNameStr().c_str()
			);
		}

		// Get Table Attr info (= fields of its features)
		StorageReadHandle storageHandle(this, storageHolder, curr, StorageAction::updatetree);
		if (m_hDS)
		{
			auto layer = debug_cast<GdalVectlMetaInfo*>(storageHandle.MetaInfo().get())->m_Layer;
			if (layer)
				DoUpdateTable(storageHolder, AsUnit(curr), layer);
		}
		return;
	}

	if (curr->HasCalculator())
		return;

	if (curr != storageHolder || sm != SM_AllTables)
		return;

	GDAL_ErrorFrame gdal_error_frame;

	// Get AllTables info (= set of layers)
	StorageReadHandle storageHandle(this, storageHolder, curr, StorageAction::read); 

	SizeT layerCount = m_hDS->GetLayerCount(); // GDAL
	for (SizeT i = 0; i != layerCount; ++i)
	{
		WeakPtr<OGRLayer> layer = m_hDS->GetLayer(i); // GDAL
		auto temp_layer_name = SharedStr(layer->GetName());
		TokenID layerID = GetTokenID_mt(as_item_name(temp_layer_name.begin(), temp_layer_name.end())); // GDAL
		if (!curr->GetSubTreeItemByID(layerID))
		{
			Unit<UInt32>::GetStaticClass()->CreateUnit(curr, layerID);
		}
	}
}



prop_tables GdalVectSM::GetPropTables(const TreeItem* storageHolder, TreeItem* curr) const
{
	prop_tables vector_dataset_properties;

	// Filename
	auto grid_dataset_filename = GetNameStr();
	vector_dataset_properties.push_back({ 0, {GetTokenID_mt("Filename"), grid_dataset_filename} });

	GDAL_ErrorFrame gdal_error_frame;
	auto smi = GdalMetaInfo(storageHolder, curr);
	DoOpenStorage(smi, dms_rw_mode::read_only);
	auto gdal_ds_handle = Gdal_DoOpenStorage(smi, dms_rw_mode::read_only, 0, false);

	// Spatial reference
	auto srs = m_hDS->GetSpatialRef();
	if (srs)
	{
		char* pszWKT = nullptr;
		srs->exportToPrettyWkt(&pszWKT, false);
		vector_dataset_properties.push_back({ 1, {GetTokenID_mt("Spatial reference"), SharedStr(pszWKT)} });
	}

	// Layers
	auto layers = m_hDS->GetLayers();
	vector_dataset_properties.push_back({ 1, {GetTokenID_mt("Number of layers"), AsString(layers.size())} });
	for (auto layer : layers)
	{
		vector_dataset_properties.push_back({ 2, {GetTokenID_mt("Layer"), SharedStr(layer->GetName())}});
		auto layer_definition = layer->GetLayerDefn();

		// geometry field
		auto geometry_field = layer_definition->GetGeomFieldDefn(0);
		auto geometry_type = geometry_field->GetType();
		vector_dataset_properties.push_back({ 3, {GetTokenID_mt("Geometry"), SharedStr(OGRGeometryTypeToName(geometry_type)) }});

		auto field_count = layer_definition->GetFieldCount();
		// fields
		for (int i=0; i<field_count; i++)
		{
			auto field = layer_definition->GetFieldDefn(i);
			auto field_name = field->GetNameRef();
			vector_dataset_properties.push_back({ 3, {GetTokenID_mt(field_name), SharedStr(field->GetFieldTypeName(field->GetType()) + SharedStr(", ") + SharedStr(field->GetFieldSubTypeName(field->GetSubType())))} });
		}
	}

	DoCloseStorage(false);
	return vector_dataset_properties;
}

// Register
IMPL_DYNC_STORAGECLASS(GdalVectSM, "gdal.vect")
IMPL_DYNC_STORAGECLASS(GdalWritableVectSM, "gdalwrite.vect")

struct GdalVectSM2 : GdalVectSM
{
	GdalVectSM2()
	{
		reportD(SeverityTypeID::ST_Warning, "StorageManager gdal2.vect is depreciated and will be removed in GeoDms version 15.0.0");
		static_assert(DMS_VERSION_MAJOR != 15);
	}


	DECL_RTTI(STGDLL_CALL, StorageClass)
};

IMPL_DYNC_STORAGECLASS(GdalVectSM2, "gdal2.vect")

// type aliasses
namespace {
	StorageClass fgdbStorageManagers(CreateFunc<GdalVectSM>, GetTokenID_st("fgdb"));
}