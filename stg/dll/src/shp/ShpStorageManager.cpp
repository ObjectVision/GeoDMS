// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "StoragePCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)


// *****************************************************************************
//
// Implementation of  ShpStorageManager
//
// *****************************************************************************

#include <iterator>

#include "ShpStorageManager.h"
#include "shp/ShpImp.h"

#include "rtcTypeLists.h"
#include "dbg/debug.h"
#include "geo/BoostPolygon.h"
#include "geo/ConvertFunctor.h"
#include "geo/GeoSequence.h"  
#include "geo/PointOrder.h"
#include "mci/ValueClassID.h"
#include "mci/ValueComposition.h"
#include "mem/SeqLock.h"
#include "ser/BaseStreamBuff.h"  
#include "stg/StorageClass.h"
#include "utl/mySPrintF.h"

#include "GDAL/gdal_base.h" // CplString
#include "Projection.h"
#include "ogr_spatialref.h"
#include "gdal_priv.h"

#include "Unit.h"
#include "AbstrDataItem.h"
#include "DataArray.h"
#include "DataStoreManagerCaller.h"
#include "UnitProcessor.h"
#include "Param.h"  // 'Hidden DMS-interface functions'

#include "LispTreeType.h"
#include "TreeItemContextHandle.h"

#define POLYGON_DATA "PolyData"
#define POINT_DATA "PointData"

namespace {
	TokenID	POLYGON_DATA_ID = GetTokenID_st(POLYGON_DATA);
	TokenID	POINT_DATA_ID = GetTokenID_st(POINT_DATA);
	TokenID SHAPEID_ID = GetTokenID_st("ShapeID");
	TokenID	SHAPERANGE_ID = GetTokenID_st("ShapeRange");
}

bool IsPointData(const AbstrDataItem* pData)
{
	return pData
		&& !pData->GetDynamicObjClass()->GetValuesType()->IsSequence()
		&& pData->GetDynamicObjClass()->GetValuesType()->GetNrDims() == 2;
}

bool IsPolyData(const AbstrDataItem* pData)
{
	return pData
		&& pData->GetDynamicObjClass()->GetValuesType()->IsSequence()
		&& pData->GetDynamicObjClass()->GetValuesType()->GetNrDims() == 2;
}

static const AbstrDataItem* GetPolyData(const TreeItem* storageHolder)
{
	dms_assert(storageHolder);
	// Look up storageHolder item
	const AbstrDataItem* pData = AsDynamicDataItem(storageHolder);
	if (!IsPolyData(pData))
		pData = nullptr;

	if (!pData)
		pData = AsDynamicDataItem(storageHolder->GetConstSubTreeItemByID(POLYGON_DATA_ID));
	if (!IsPolyData(pData))
		pData = nullptr;
	return pData;
}

static const AbstrDataItem* GetPointData(const TreeItem* storageHolder)
{
	assert(storageHolder);
	// Look up storageHolder item
	auto pData = AsDynamicDataItem(storageHolder);
	if (!IsPointData(pData))
		pData = nullptr;

	// Look up in 'griddata' item
	if (!pData)
		pData = AsDynamicDataItem(storageHolder->GetConstSubTreeItemByID(POINT_DATA_ID));
	if (!IsPointData(pData))
		pData = nullptr;
	return pData;
}

struct DPoint2FPoint_Converter
{ 
	FPoint operator() ( const DPoint& src) 
	{ 
		return Convert<FPoint>(src); 
	} 
};

bool IsRingClosed(ConstPointIterRange pr)
{
	dms_assert(pr.size() > 0);
	return pr.front() == pr.back();
}

template <typename PointType>
void ReadSequences(AbstrDataObject* ado, UInt32 shpImpFeatureCount, ShpImp* pImp)
{
	typedef typename sequence_traits<PointType>::container_type PolygonType;

	auto polyData = debug_valcast<DataArray<PolygonType>*>(ado)->GetDataWrite();

	assert(polyData.size() == shpImpFeatureCount);
	MG_CHECK(pImp->GetShapeType() != ShapeTypes::ST_Point);

	bool mustCloseRings = (pImp->GetShapeType() == ShapeTypes::ST_Polygon);

	SeqLock<sequence_array<ShpPointIndex> > lockParts (pImp->m_SeqParts , dms_rw_mode::read_only);
	SeqLock<sequence_array<ShpPoint>      > lockPoints(pImp->m_SeqPoints, dms_rw_mode::read_only);
				
	typename DataArray<PolygonType>::iterator polygonPtr = polyData.begin();
	for (UInt32 p=0, n=polyData.size(); p!=n; ++p, ++polygonPtr)
	{
		auto polygonSize = pImp->ShapeSet_NrPoints(p);
		if (!polygonSize)
		{
			vector_resize_uninitialized(*polygonPtr, 0);
			continue;
		}
		UInt32 nrExtraParts = pImp->ShapeSet_NrParts(p) - 1;
		assert(nrExtraParts < polygonSize); // since there are points, there is at least one part, and parts are not allowed to have less that three or at least one points

		UInt32 nrUnclosedRings = 0;
		if	(mustCloseRings)
			for (UInt32 partI=0; partI<=nrExtraParts; ++partI)
				if (!IsRingClosed(pImp->ShapeSet_GetPoints(p, partI)))
					++nrUnclosedRings;

		vector_resize_uninitialized(*polygonPtr, SizeT(polygonSize) + nrExtraParts + nrUnclosedRings);

		typename sequence_traits<PointType>::pointer
			pointData = (*polygonPtr).begin();
		ConvertResultFunctor<PointType, DPoint, shp_reorder_functor> converter;

		for (UInt32 partI = 0; partI <= nrExtraParts; ++partI)
		{
			ConstPointIterRange pointRange = pImp->ShapeSet_GetPoints(p, partI);
			pointData = convert_copy(pointRange.begin(), pointRange.end(), pointData, shp_reorder_functor());
			if (mustCloseRings)
			{
				if (!IsRingClosed(pointRange))
					*pointData++ = converter(*pointRange.begin());
			}
			else
			{	
				if (partI < nrExtraParts) // include a multi-linstring separator if another linestring follows
					*pointData++ = UNDEFINED_VALUE(PointType);
			}
			
		}

		// connect extra parts in possible reverse order to origins of previous parts
		if (mustCloseRings)
		{
			for (UInt32 i = 0; i < nrExtraParts; ++pointData)
				*pointData = converter(
					pImp->ShapeSet_GetPoint(p, nrExtraParts - ++i, 0) // i increments here
				);
			assert(pointData == (*polygonPtr).end());
		}
	}
}
			
template <typename PointType>
void ReadArray(AbstrDataObject* ado, UInt32 shpImpFeatureCount, ShpImp* pImp)
{
	MG_CHECK(pImp->GetShapeType() == ShapeTypes::ST_Point);

	auto pointData 
		= debug_valcast<DataArray<PointType>*>(ado)->GetDataWrite();

	convert_copy(
		pImp->PointSet_GetPointsBegin(), 
		pImp->PointSet_GetPointsEnd(), 
		pointData.begin(), shp_reorder_functor()
	);
}

bool ShpStorageManager::ReadDataItem(StorageMetaInfoPtr smi, AbstrDataObject* borrowedReadResultHolder, tile_id t)
{
	if (t)
		return true;

	const TreeItem* storageHolder = smi->StorageHolder();
	AbstrDataItem*   adi = smi->CurrWD();

	assert(adi->GetDataObjLockCount() < 0); // Write lock is already set.

	ShpImp impl;

	auto sfwa = DSM::GetSafeFileWriterArray();
	if (!sfwa)
		return false;

	if (!impl.Read( GetNameStr(), sfwa.get()) )
		return false;
	
	const ValueClass* vc = borrowedReadResultHolder->GetValuesType();
	ValueClassID    vtId = vc->GetValueClassID();

	UInt32 shpImpFeatureCount = impl.NrRecs();
	adi->GetAbstrDomainUnit()->ValidateCount(shpImpFeatureCount);

	if (adi->GetValueComposition() == ValueComposition::Single)
	{
		visit<typelists::points>(adi->GetAbstrValuesUnit(), 
			[borrowedReadResultHolder, shpImpFeatureCount, &impl] <typename P> (const Unit<P>*) 
			{
				ReadArray<P>(borrowedReadResultHolder, shpImpFeatureCount, &impl);
			}
		);
	}
	else
	{
		visit<typelists::seq_points>(adi->GetAbstrValuesUnit(),
			[borrowedReadResultHolder, shpImpFeatureCount, &impl] <typename P> (const Unit<P>*)
			{
				ReadSequences<P>(borrowedReadResultHolder, shpImpFeatureCount, &impl); // also reads (Multi)Polygons
			}
		);
	}
	assert(borrowedReadResultHolder->GetNrFeaturesNow() == shpImpFeatureCount);
	return true;
}

template <typename InIter, typename UnaryOper>
struct FuncIter
{ 
	using value_type = typename std::iterator_traits<InIter>::value_type;
	using pointer = typename std::iterator_traits<InIter>::pointer;
	using reference = UnaryOper::result_type;
	using difference_type = std::iterator_traits<InIter>::difference_type;
	using iterator_category = std::iterator_traits<InIter>::iterator_category;

	FuncIter(InIter i, UnaryOper oper = UnaryOper())
		:	m_Iter(i)
		,	m_Oper(oper)
	{}

	// dereference wrapping
	typename reference operator *() const { return m_Oper(*m_Iter); }

	// iter API wrapping
	difference_type operator - (const FuncIter& oth) const { return m_Iter -  oth.m_Iter; }
	bool            operator !=(const FuncIter& oth) const { return m_Iter != oth.m_Iter; } 
	bool            operator ==(const FuncIter& oth) const { return m_Iter == oth.m_Iter; } 
	bool            operator < (const FuncIter& oth) const { return m_Iter <  oth.m_Iter; } 
	const FuncIter& operator ++() { ++m_Iter; return *this; }

	InIter    m_Iter;
	UnaryOper m_Oper;
};

template <typename PointType>
void WriteSequences(const AbstrDataObject* ado, ShpImp* pImp, WeakStr nameStr, const TreeItem* storageHolder, const AbstrDataItem* adi)
{
	typedef typename sequence_traits<PointType  >::container_type PolygonType;
	typedef typename sequence_traits<PolygonType>::container_type PolygonArray;

	auto polyData = debug_valcast<const DataArray<PolygonType>*>(ado)->GetDataRead(); 		

	UInt32 nrRecs = polyData.size();
	pImp->ShapeSet_PrepareDataStore(nrRecs, 0);

	SeqLock<sequence_array<ShpPointIndex>> lockParts  (pImp->m_SeqParts , dms_rw_mode::write_only_all);
	SeqLock<sequence_array<ShpPoint>     > lockPoints (pImp->m_SeqPoints, dms_rw_mode::write_only_all);

	typename PolygonArray::const_iterator
		polygonIter = polyData.begin(),
		polygonEnd  = polyData.end();

	for (; polygonIter != polygonEnd; ++polygonIter)
	{
		typename DataArrayBase<PolygonType>::const_reference polygon = *polygonIter;

		using func_iter = FuncIter<
			typename PolygonArray::const_reference::const_iterator, 
			ConvertResultFunctor<DPoint, PointType, shp_reorder_functor> 
		>;

		auto feature = pImp->ShapeSet_PushBackPolygon();
		if (pImp->GetShapeType() == ShapeTypes::ST_Polygon)
		{
			boost::polygon::SA_ConstRingIterator<PointType> 
				ri(polygon,  0), 
				re(polygon, -1);

			for (; ri  != re; ++ri)
			{
				auto ring = *ri;
				feature.AddPoints(
					func_iter(ring.begin()),
					func_iter(ring.end  ())
				);
			}
		}
		else
		{
			auto linestringIter = polygon.begin();
			auto polygonEnd = polygon.end();
			while (true)
			{
				auto linestringEnd = std::find(linestringIter, polygonEnd, UNDEFINED_VALUE(PointType));

				feature.AddPoints(
					func_iter(linestringIter),
					func_iter(linestringEnd)
				);
				if (linestringEnd == polygonEnd)
					break;
				linestringIter = ++linestringEnd;
			}
		}
	}
	assert(pImp->m_Polygons.size() == nrRecs);

	auto wktPrjStr = GetWktProjectionFromValuesUnit(adi);

	auto sfwa = DSM::GetSafeFileWriterArray(); MG_CHECK(sfwa);
	if (!pImp->Write( nameStr, sfwa.get(), wktPrjStr) )
		adi->throwItemErrorF("ShpStorage error: Cannot write to %s", nameStr.c_str());
}


template <typename PointType>
void WriteArray(const AbstrDataObject* ado, ShpImp* pImp, WeakStr nameStr, const TreeItem* storageHolder, const AbstrDataItem* adi)
{
	auto pointData = debug_valcast<const DataArray<PointType>*>(ado)->GetDataRead(); 		

	UInt32 nrRecs = pointData.size();

	typedef FuncIter<
		typename DataArrayBase<PointType>::const_iterator, 
		ConvertResultFunctor<DPoint, PointType, shp_reorder_functor> 
	> func_iter;

	pImp->PointSet_AddPoints(
		func_iter(pointData.begin()),
		func_iter(pointData.end  ())
	);

	auto wktPrjStr = GetWktProjectionFromValuesUnit(adi);

	auto sfwa = DSM::GetSafeFileWriterArray();

	if (!sfwa || !pImp->Write( nameStr, sfwa.get(), wktPrjStr) )
		adi->throwItemErrorF("ShpStorage error: Cannot write to %s", nameStr.c_str());
}

/*const AbstrUnit* CompositeBase(const AbstrUnit* proj)
{
	const AbstrUnit* res = proj->GetCurrProjection()->GetCompositeBase();
	if (!res)
		return proj;
	return res;
}*/

bool ShpStorageManager::WriteDataItem(StorageMetaInfoPtr&& smiHolder)
{
	auto smi = smiHolder.get();
	StorageWriteHandle hnd(std::move(smiHolder));

	const TreeItem* storageHolder = smi->StorageHolder();
	const AbstrDataItem* adi = smi->CurrRD();
	dms_assert(adi);
	dms_assert(adi->GetDataRefLockCount() > 0); // Read lock already set

	const AbstrDataObject* ado        = adi->GetRefObj();
	const AbstrUnit*       valuesUnit = adi->GetAbstrValuesUnit();
	const ValueClass*      vClass     = ado->GetValuesType();
	ValueClassID           vtId       = vClass->GetValueClassID();
	ValueComposition       vComp    =   adi->GetValueComposition();

	ShapeTypes shapeType = ShapeTypes::ST_Point;
	if (vComp == ValueComposition::Sequence)
	{
		shapeType = ShapeTypes::ST_Polyline;
		vtId = ValueClassID(int(vtId) + int(ValueClassID::VT_DArc) - int(ValueClassID::VT_DPolygon)); // (D/F/S/I)Poly -> (D/F/S/I)Arc
	}
	if (vComp == ValueComposition::Polygon)
		shapeType = ShapeTypes::ST_Polygon;

	ShpImp impl;
	impl.SetShapeType(shapeType);

	if (adi->GetValueComposition() == ValueComposition::Single)
	{
		visit<typelists::points>(adi->GetAbstrValuesUnit(), 
			[this, ado, storageHolder, adi, &impl] <typename P> (const Unit<P>*)
			{
				WriteArray<P>(ado, &impl, this->GetNameStr(), storageHolder, adi);
			}
		);
	}
	else
	{
		visit<typelists::seq_points>(adi->GetAbstrValuesUnit(), 
			[this, ado, storageHolder, adi, &impl] <typename P> (const Unit<P>*)
			{
				WriteSequences<P>(ado, &impl, this->GetNameStr(), storageHolder, adi); // also reads (Multi)Polygons
			}
		);
	}
	return true;
}


// Insert params into client tree
void ShpStorageManager::DoUpdateTree(const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const
{
	NonmappableStorageManager::DoUpdateTree(storageHolder, curr, sm);

	dms_assert(storageHolder);
	if (storageHolder != curr)
		return;
	if (curr->IsStorable() && curr->HasCalculator())
		return;

	StorageReadHandle storageHandle(this, storageHolder, curr, StorageAction::updatetree);

	auto sfwa = DSM::GetSafeFileWriterArray();
	if (!sfwa)
		return;

	ShpImp impl;
	if (!impl.OpenAndReadHeader( GetNameStr(), sfwa.get() ))
		return; // nothing read; file does not exist; probably has to write data and not read

	const AbstrDataItem* pData = nullptr;
	TokenID dataNameID;
	auto vc = ValueComposition::Single;
	ShapeTypes shapeType = impl.GetShapeType();

	switch (shapeType)
	{
		case ShapeTypes::ST_MultiPoint:
		case ShapeTypes::ST_Polyline: vc = ValueComposition::Sequence; pData = GetPolyData(storageHolder); dataNameID = POLYGON_DATA_ID; break;
		case ShapeTypes::ST_Polygon:  vc = ValueComposition::Polygon;  pData = GetPolyData(storageHolder); dataNameID = POLYGON_DATA_ID; break;
		case ShapeTypes::ST_Point:    pData=GetPointData(storageHolder);dataNameID = POINT_DATA_ID;   break;
		default: throwItemErrorF("ShapeType %d is not supported", int(shapeType));
	}

	if (pData)
		return;

	AbstrUnit* u_size    = Unit<UInt32>::GetStaticClass()->CreateUnit(curr, SHAPEID_ID   );
	AbstrUnit* u_content = Unit<DPoint>::GetStaticClass()->CreateUnit(curr, SHAPERANGE_ID);

	assert(u_size);
	assert(u_content);

	pData = CreateDataItem(curr, dataNameID, u_size, u_content, vc);
}


bool ShpStorageManager::ReadUnitRange(const StorageMetaInfo& smi) const
{
	auto sfwa = DSM::GetSafeFileWriterArray();
	if (!sfwa)
		return false;
	ShpImp impl;
	if (!impl.OpenAndReadHeader( GetNameStr(), sfwa.get()))
		return false;

	AbstrUnit* au = smi.CurrWU();
	dms_assert(au);
	if (au->GetValueType()->IsIntegral())
		au->SetCount( impl.NrRecs() );
	else
	{
		const ShpBox& box = impl.GetBoundingBox();
		au->SetRangeAsDPoint(
			box.first.Row(), 
			box.first.Col(), 
			box.second.Row(), 
			box.second.Col()
		);
	}
	return true;
}


// Register
IMPL_DYNC_STORAGECLASS(ShpStorageManager, "shp")
