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

#include "GeoPCH.h"
#pragma hdrstop


#include "dbg/SeverityType.h"
#include "geo/BoostPolygon.h"
#include "geo/SpatialIndex.h"
#include "mci/ValueWrap.h"
#include "ptr/Resource.h"
#include "ptr/ResourceArray.h"
#include "utl/mySPrintF.h"

#include "ParallelTiles.h"
#include "LispTreeType.h"
#include "LockLevels.h"
#include "TileChannel.h"
#include "TreeItemClass.h"
#include "Unit.h"
#include "UnitClass.h"
#include "UnitProcessor.h"

//#pragma warning (disable: 4146 4267)
//#define MG_DEBUG_POLYGON

#include <ipolygon/polygon.hpp>

const Int32 MAX_COORD = (1 << 26);

#include "VersionComponent.h"
static VersionComponent s_Boost("boost " BOOST_LIB_VERSION);
static VersionComponent s_BoostPolygon("boost::polygon " BOOST_STRINGIZE(BOOST_POLYGON_VERSION));


template <typename T>
typename boost::enable_if_c<is_numeric_v<T> && (sizeof(T) < 4), bool>::type
	coords_using_more_than_25_bits(T coord)
{
	return false;
}

bool coords_using_more_than_25_bits(Int32 v)
{
	return v <= -MAX_COORD || v >= MAX_COORD;
}

bool coords_using_more_than_25_bits(UInt32 v)
{
	return v >= MAX_COORD;
}

template <typename Point>
typename boost::enable_if<typename gtl::is_point_concept<typename gtl::geometry_concept<Point>::type>::type, bool>::type
coords_using_more_than_25_bits(Point p)
{
	return coords_using_more_than_25_bits(x(p)) || coords_using_more_than_25_bits(y(p));
}

template <typename Rect>
typename boost::enable_if<typename gtl::is_rectangle_concept<typename gtl::geometry_concept<Rect>::type>::type, bool>::type
coords_using_more_than_25_bits(const Rect& rect)
{
	return coords_using_more_than_25_bits(ll(rect)) || coords_using_more_than_25_bits(ur(rect));
}

// *****************************************************************************
//	PolygonOverlay
// *****************************************************************************


static CommonOperGroup gr("overlay_polygon", oper_policy::dynamic_result_class);

static TokenID 
	s_tGM = token::geometry,
	s_tFR = token::first_rel,
	s_tSR = token::second_rel;

class AbstrPolygonOverlayOperator : public BinaryOperator
{
protected:
	AbstrPolygonOverlayOperator(const DataItemClass* polyAttrClass)
		:	BinaryOperator(&gr, Unit<UInt32>::GetStaticClass()
			,	polyAttrClass
			,	polyAttrClass
			)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 2);

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		const AbstrDataItem* arg2A = AsDataItem(args[1]);
		dms_assert(arg1A); 
		dms_assert(arg2A); 

		const AbstrUnit* domain1Unit = arg1A->GetAbstrDomainUnit(); bool e1IsVoid = domain1Unit->GetValueType() == ValueWrap<Void>::GetStaticClass();
		const AbstrUnit* values1Unit = arg1A->GetAbstrValuesUnit();

		const AbstrUnit* domain2Unit = arg2A->GetAbstrDomainUnit(); bool e2IsVoid = domain2Unit->GetValueType() == ValueWrap<Void>::GetStaticClass();
		const AbstrUnit* values2Unit = arg2A->GetAbstrValuesUnit();

		values1Unit->UnifyValues(values2Unit, UM_Throw);

		AbstrUnit* res = Unit<UInt32>::GetStaticClass()->CreateResultUnit(resultHolder);
		resultHolder = res;

		AbstrDataItem* resG = CreateDataItem(res, s_tGM, res, values1Unit, ValueComposition::Polygon);
		AbstrDataItem* res1 = e1IsVoid ? nullptr : CreateDataItem(res, s_tFR, res, domain1Unit);
		AbstrDataItem* res2 = e2IsVoid ? nullptr : CreateDataItem(res, s_tSR, res, domain2Unit);

		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);

			ResourceHandle resData;
			ResourceHandle pointBoxDataHandle;

			for (tile_id t=0, te = domain1Unit->GetNrTiles(); t != te; ++t)
			{
				ReadableTileLock readPointLock (arg1A->GetCurrRefObj(), t);
				CreatePointHandle(arg1A, t, pointBoxDataHandle);
			}

			for (tile_id u=0, ue = domain2Unit->GetNrTiles(); u != ue; ++u)
			{
				ReadableTileLock readPolyLock (arg2A->GetCurrRefObj(), u);
				ResourceHandle polyInfoHandle;
				CreatePolyHandle(arg2A, u, polyInfoHandle);

// 				for (tile_id t=0, te = domainUnit->GetNrTiles(); t != te; ++t)
				leveled_critical_section resInsertSection(item_level_type(0), ord_level_type::SpecificOperatorGroup, "PolygonOverlay.InsertSection");

				std::atomic<tile_id> intersectCount = 0; // DEBUG

				parallel_tileloop(domain1Unit->GetNrTiles(), [this, &resInsertSection, arg1A, arg2A, u, &pointBoxDataHandle, &polyInfoHandle, &resData, &intersectCount](tile_id t)->void
					{
						if (this->IsIntersecting(t, u, pointBoxDataHandle, polyInfoHandle))
						{
							ReadableTileLock readPoly1Lock (arg1A->GetCurrRefObj(), t);

							Calculate(resData, resInsertSection, arg1A, arg2A, t, u, polyInfoHandle);

							++intersectCount; // DEBUG
						}
					}
				);
				reportF(SeverityTypeID::ST_MajorTrace, "Intersect count %d of %dx%d", intersectCount, domain1Unit->GetNrTiles(), ue); // DEBUG
			}

			StoreRes(res, resG, res1, res2, resData);
		}
		return true;
	}
	virtual void CreatePolyHandle(const AbstrDataItem* polyDataA, tile_id u, ResourceHandle& polyInfoHandle) const =0;
	virtual void CreatePointHandle(const AbstrDataItem* pointDataA, tile_id t, ResourceHandle& pointBoxDataHandle) const =0;
	virtual bool IsIntersecting(tile_id t, tile_id u, ResourceHandle& pointBoxDataHandle, ResourceHandle& polyInfoHandle) const=0;
	virtual void Calculate(ResourceHandle& resData, leveled_critical_section& resInsertSection, const AbstrDataItem* poly1DataA, const AbstrDataItem* poly2DataA, tile_id t, tile_id u, const ResourceHandle& polyInfoHandle) const=0;
	virtual void StoreRes(AbstrUnit* res, AbstrDataItem* resG, AbstrDataItem* res1, AbstrDataItem* res2, ResourceHandle& resData) const=0;
};

template <typename P>
class PolygonOverlayOperator : public AbstrPolygonOverlayOperator
{
	typedef P                      PointType;
	typedef std::vector<PointType>  PolygonType;
	typedef Unit<PointType>        PointUnitType;
	typedef Range<PointType>       BoxType;
	typedef std::vector<BoxType>    BoxArrayType;

	typedef DataArray<PolygonType> Arg1Type;
	typedef DataArray<PolygonType> Arg2Type;

	struct ResDataElemType {
		PolygonType  m_Geometry;
		Point<SizeT> m_OrgRel;

		bool operator < (const ResDataElemType& oth) { return m_OrgRel < oth.m_OrgRel;  }
	};

	typedef std::vector<ResDataElemType> ResTileDataType;
	typedef std::map<Point<tile_id>, ResTileDataType> ResDataType;

	typedef typename scalar_of<PointType>::type  ScalarType;
	typedef SpatialIndex<ScalarType, typename Arg2Type::const_iterator> SpatialIndexType;
	typedef std::vector<SpatialIndexType> SpatialIndexArrayType;

public:
	PolygonOverlayOperator()
		:	AbstrPolygonOverlayOperator(Arg1Type::GetStaticClass())
	{}

	// Override Operator
	void CreatePolyHandle(const AbstrDataItem* polyDataA, tile_id u, ResourceHandle& polyInfoHandle) const override
	{
		const Arg2Type* polyData  = const_array_cast<PolygonType>(polyDataA);
		dms_assert(polyData); 

		auto polyArray = polyData->GetTile(u);
		polyInfoHandle = makeResource<SpatialIndexType>(polyArray.begin(), polyArray.end(), 0);
	}

	void CreatePointHandle(const AbstrDataItem* polyDataA, tile_id t, ResourceHandle& pointBoxDataHandle) const override
	{
		if (!pointBoxDataHandle)
			pointBoxDataHandle = makeResource<BoxArrayType>(polyDataA->GetAbstrDomainUnit()->GetNrTiles());
		BoxArrayType& boxArray = GetAs<BoxArrayType>(pointBoxDataHandle);

		const Arg1Type* polyData = const_array_cast<PolygonType>(polyDataA);
		dms_assert(polyData); 

		auto polyArray = polyData->GetTile(t);
		dms_assert(t < boxArray.size());
		boxArray[t] = RangeFromSequence(polyArray.begin(), polyArray.end());
	}

	bool IsIntersecting(tile_id t, tile_id u, ResourceHandle& pointBoxDataHandle, ResourceHandle& polyInfoHandle) const override
	{
		BoxArrayType&     boxArray   = GetAs<BoxArrayType>(pointBoxDataHandle);
		SpatialIndexType* spIndexPtr = GetOptional<SpatialIndexType>(polyInfoHandle);

		return ::IsIntersecting(spIndexPtr->GetBoundingBox(), boxArray[t]);
	}

	void Calculate(ResourceHandle& resDataHandle, leveled_critical_section& resInsertSection, const AbstrDataItem* poly1DataA, const AbstrDataItem* poly2DataA, tile_id t, tile_id u, const ResourceHandle& polyInfoHandle) const override
	{
		const Arg1Type* poly1Data = const_array_cast<PolygonType>(poly1DataA);
		const Arg2Type* poly2Data = const_array_cast<PolygonType>(poly2DataA);

		dms_assert(poly1Data); 
		dms_assert(poly2Data);

		auto poly1Array = poly1Data->GetTile(t);
		auto poly2Array = poly2Data->GetTile(u);

		SizeT p1Offset = poly1DataA->GetAbstrDomainUnit()->GetTileFirstIndex(t);
		SizeT p2Offset = poly2DataA->GetAbstrDomainUnit()->GetTileFirstIndex(u);

		const SpatialIndexType* spIndexPtr = GetOptional<SpatialIndexType>(polyInfoHandle);

		typedef typename SpatialIndexType::template iterator<BoxType> box_iter_type;
		typedef typename scalar_of<P>::type coordinate_type;
		typedef gtl::polygon_with_holes_data<coordinate_type> polygon_with_holes_type;
		typedef gtl::polygon_set_data< coordinate_type > polygon_set_type;

		typedef ResDataElemType res_data_elem_type;

		ResTileDataType* resTileData = nullptr;
		if (!resTileData)
		{
			leveled_critical_section::scoped_lock resLock(resInsertSection); // free this lock after *resTileData has been created.

			if (!resDataHandle)
				resDataHandle = makeResource<ResDataType>();
			ResDataType& resData = GetAs<ResDataType>(resDataHandle);
			resTileData = &(resData[Point<tile_id>(t, u)]);
		}
		dms_assert(resTileData);

		leveled_critical_section resLocalAdditionSection(item_level_type(0), ord_level_type::SpecificOperator, "Polygon.LocalAdditionSection");

		parallel_for(SizeT(0), poly1Array.size(), [p1Offset, p2Offset, &poly1Array, &poly2Array, spIndexPtr, resTileData, &resLocalAdditionSection](SizeT i)->void
		{
			auto polyPtr = poly1Array.begin()+i;
			SizeT p1_rel = p1Offset + i;
			BoxType bbox = RangeFromSequence(polyPtr->begin(), polyPtr->end());
			if (!::IsIntersecting(spIndexPtr->GetBoundingBox(), bbox))
				return;

			typename polygon_set_type::clean_resources cleanResources;
			box_iter_type iter;
			for (iter = spIndexPtr->begin(bbox); iter; ++iter)
			{
				polygon_set_type geometry;
				gtl::assign(geometry, *((*iter)->get_ptr()) & *polyPtr, cleanResources);

				if (!geometry.size(cleanResources))
					continue;

				res_data_elem_type back;
				dms_assign(back.m_Geometry, geometry, cleanResources);
				back.m_OrgRel.first = p1_rel;
				back.m_OrgRel.second = p2Offset + (((*iter)->get_ptr()) - poly2Array.begin());

				leveled_critical_section::scoped_lock resLock(resLocalAdditionSection);
				resTileData->push_back(std::move(back));
			}
		});
	}

	void StoreRes(AbstrUnit* res, AbstrDataItem* resG, AbstrDataItem* res1, AbstrDataItem* res2, ResourceHandle& resDataHandle) const override
	{
		ResDataType* resData = GetOptional<ResDataType>(resDataHandle);

		SizeT count = 0;
		if (resData)
			for (auto& resTiles : *resData) // ordered by (t, u)
				count += resTiles.second.size();
		res->SetCount(count);

		locked_tile_write_channel<PolygonType> resGWriter(resG);
		locked_tile_write_channel<UInt32> res1Writer(res1);
		locked_tile_write_channel<UInt32> res2Writer(res2);

		if (resData)
		{
			for (auto& resTiles : *resData) // ordered by (t, u)
			{
				ResTileDataType& resTileData = resTiles.second;
				std::sort(resTileData.begin(), resTileData.end()); // sort on (m_OrgRel)

				for (auto& resElemData: resTileData)
				{
					if (resG) resGWriter.Write(std::move(resElemData.m_Geometry));
					if (res1) res1Writer.Write(resElemData.m_OrgRel.first);
					if (res2) res2Writer.Write(resElemData.m_OrgRel.second);
				}
			}
		}

		if (resG) { MG_CHECK(resGWriter.IsEndOfChannel()); resGWriter.Commit(); }
		if (res1) { MG_CHECK(res1Writer.IsEndOfChannel()); res1Writer.Commit(); }
		if (res2) { MG_CHECK(res2Writer.IsEndOfChannel()); res2Writer.Commit(); }
	}
};

// *****************************************************************************
//	UnionPolygon func
// *****************************************************************************

#include "IndexGetterCreator.h"

enum class PolygonFlags {
	none = 0,
	F_DoSplit = 1,
	F_DoUnion = 2,
	F_HasPartition = 4,
	F_DoPartUnion = 6,

	F_Inflate1 = 0x0010,
	F_Deflate1 = 0x0020,
	F_Filter1 = 0x0030,
	F_I4HV1 = 0x0040,
	F_I4D1 = 0x0050,
	F_I8D1 = 0x0060,
	F_I16D1 = 0x0070,
	F_IXHV1 = 0x0080,
	F_IXD1 = 0x0090,
	F_D4HV1 = 0x00A0,
	F_D4D1 = 0x00B0,
	F_D8D1 = 0x00C0,
	F_D16D1 = 0x00D0,
	F_DXHV1 = 0x00E0,
	F_DXD1 = 0x00F0,
	F_Mask1 = 0x00F0,

	F_Inflate2 = 0x0100,
	F_Deflate2 = 0x0200,
	F_Filter2 = 0x0300,
	F_I4HV2 = 0x0400,
	F_I4D2 = 0x0500,
	F_I8D2 = 0x0600,
	F_I16D2 = 0x0700,
	F_IXHV2 = 0x0800,
	F_IXD2 = 0x0900,
	F_D4HV2 = 0x0A00,
	F_D4D2 = 0x0B00,
	F_D8D2 = 0x0C00,
	F_D16D2 = 0x0D00,
	F_DXHV2 = 0x0E00,
	F_DXD2 = 0x0F00,
	F_Mask2 = 0x0F00,
};

constexpr PolygonFlags operator | (PolygonFlags a, PolygonFlags b)
{
	return static_cast<PolygonFlags>(static_cast<int>(a) | static_cast<int>(b));
}

bool operator & (PolygonFlags a, PolygonFlags b)
{
	return static_cast<int>(a) & static_cast<int>(b);
}

template <typename PolygonType, typename polygon_set_data_type>
void UnionPolygon(ResourceArrayHandle& r, SizeT n, const AbstrDataItem* polyDataA, const AbstrDataItem* permDataA, tile_id t)
{
#if defined(MG_DEBUG_POLYGON)
	DBG_START("UnionPolygon", "", true);
	DBG_TRACE(("UnionPolygonTotal %d/%d", t,  polyDataA->GetAbstrDomainUnit()->GetNrTiles()));
#endif

	const DataArray<PolygonType>* polyData = const_array_cast<PolygonType>(polyDataA);
	dms_assert(polyData); 

	OwningPtr<IndexGetter> vg;

	if (permDataA)
		vg = IndexGetterCreator::Create(permDataA, t);

	auto polyArray = polyData->GetTile(t);

	if (!r)
		r.reset( ResourceArray<polygon_set_data_type>::Create(n) );
	ResourceArray<polygon_set_data_type>* geometriesPtr = debug_cast<ResourceArray<polygon_set_data_type>*>(r.get_ptr());
	dms_assert(geometriesPtr->Size() == n);

	PolygonFlags unionPermState =
		(permDataA) ? PolygonFlags::F_DoPartUnion :
		(n==1)      ? PolygonFlags::F_DoUnion     : PolygonFlags::none;
	
	typename polygon_set_data_type::clean_resources cleanResources;

	for (auto p1=polyArray.begin(), pb=p1, e1=polyArray.end(); p1!=e1; ++p1)
	{
		polygon_set_data_type* geometryPtr = geometriesPtr->m_Data;
		if (unionPermState != PolygonFlags::F_DoUnion)
		{
			SizeT i = p1-pb;
			if (unionPermState != PolygonFlags::none)
			{
				dms_assert(unionPermState == PolygonFlags::F_DoPartUnion);
				SizeT ri = vg->Get(i);	
				if (ri >= n)
				{
					if (!IsDefined(ri))
						continue;
					throwErrorF("Polygon", "Unexpected partion index %d at row %d", ri, i);
				}
				i = ri;
			}
			dms_assert( i < n);
			geometryPtr += i;
		}
#if defined(MG_DEBUG_POLYGON)
		DBG_TRACE(("index %d", p1 - pb));
#endif
		dms_insert(*geometryPtr, *p1, cleanResources);
	}
}
namespace std {
    inline UInt32 abs(UInt32 _X)
    {
        return _X;
    }

}

struct AbstrPolygonOperator : public VariadicOperator
{
/* Variants, each line represents 2*3 operator-groups:

	[split_][[partitioned_]union_]polygon                  (geometry[, partitioning]);
	[split_][[partitioned_]union_]polygon_filtered         (geometry[, partitioning], minArea);
	[split_][[partitioned_]union_]polygon_inflated         (geometry[, partitioning], increment);
	[split_][[partitioned_]union_]polygon_deflated         (geometry[, partitioning], decrement);
	[split_][[partitioned_]union_]polygon_filtered_inflated(geometry[, partitioning], minArea,   increment);
	[split_][[partitioned_]union_]polygon_deflated_filtered(geometry[, partitioning], decrement, minArea  );
	[split_][[partitioned_]union_]polygon_deflated_inflated(geometry[, partitioning], decrement, increment);
	[split_][[partitioned_]union_]polygon_inflated_deflated(geometry[, partitioning], increment, decrement);

*/
	static arg_index CalcNrArgs(PolygonFlags f)
	{
		dms_assert((f & PolygonFlags::F_DoUnion) || !(f & PolygonFlags::F_HasPartition));

		dms_assert((f & PolygonFlags::F_Mask1)  || !(f & PolygonFlags::F_Mask2));

		arg_index nrArgs = 1;

		if (f & PolygonFlags::F_HasPartition ) ++nrArgs;

		if (f & PolygonFlags::F_Mask1) ++nrArgs;
		if (f & PolygonFlags::F_Mask2) ++nrArgs;

		return nrArgs;
	}
	static const Class* ResultCls(const DataItemClass* polyAttrClass, PolygonFlags f)
	{
		if (f & PolygonFlags::F_DoSplit)
			return Unit<UInt32>::GetStaticClass();
		return polyAttrClass;
	}
protected:
	PolygonFlags m_Flags;

	AbstrPolygonOperator(AbstrOperGroup* og, const DataItemClass* polyAttrClass, const DataItemClass* numericAttrClass, PolygonFlags f)
		:	VariadicOperator(og, ResultCls(polyAttrClass, f), CalcNrArgs(f) )
		,	m_Flags(f)
	{
		ClassCPtr* argClsIter = m_ArgClasses.get();
		*argClsIter++ = polyAttrClass;
		if (f & PolygonFlags::F_HasPartition ) *argClsIter++ = AbstrDataItem::GetStaticClass(); // partition attribute

		if (f & PolygonFlags::F_Mask1) *argClsIter++ = numericAttrClass;
		if (f & PolygonFlags::F_Mask2) *argClsIter++ = numericAttrClass;

		dms_assert(m_ArgClassesEnd == argClsIter);
	}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
#if defined(MG_DEBUG_POLYGON)
		DBG_START("AbstrPolygon", "CreateResult", true);
		for (arg_index i = 0; i != args.size(); ++i)
		{
			DBG_TRACE(("arg %d: %s", i, args[i]->GetName().c_str()));
		}
#endif
		dms_assert(args.size() == CalcNrArgs(m_Flags));

		arg_index argCount = 0;
		const AbstrDataItem* argPoly = AsDataItem(args[argCount++]);
		const AbstrDataItem* argPart = (m_Flags & PolygonFlags::F_HasPartition) ? AsDataItem(args[argCount++]) : nullptr;
		const AbstrDataItem* argNum1 = (m_Flags & PolygonFlags::F_Mask1       ) ? AsDataItem(args[argCount++]) : nullptr;
		const AbstrDataItem* argNum2 = (m_Flags & PolygonFlags::F_Mask2       ) ? AsDataItem(args[argCount++]) : nullptr;

		dms_assert(args.size() == argCount);

		dms_assert(argPoly); 

		const AbstrUnit* domain1Unit = argPoly->GetAbstrDomainUnit();
		const AbstrUnit* values1Unit = argPoly->GetAbstrValuesUnit();

		const AbstrUnit* resDomain = (argPart) ? argPart->GetAbstrValuesUnit() : (m_Flags & PolygonFlags::F_DoUnion) ? Unit<Void>::GetStaticClass()->CreateDefault() : domain1Unit;

		if (argPart)
			domain1Unit->UnifyDomain(argPart->GetAbstrDomainUnit(), UM_Throw);
		if (argNum1)
			resDomain  ->UnifyDomain(argNum1->GetAbstrDomainUnit(), UnifyMode(UM_Throw|UM_AllowVoidRight));
		if (argNum2)
			resDomain  ->UnifyDomain(argNum2->GetAbstrDomainUnit(), UnifyMode(UM_Throw|UM_AllowVoidRight));

		AbstrDataItem* resGeometry;
		AbstrDataItem* resNrOrgEntity = nullptr;
		AbstrUnit*     resUnit;

		if (m_Flags & PolygonFlags::F_DoSplit)
		{
			resUnit = Unit<UInt32>::GetStaticClass()->CreateResultUnit(resultHolder);
			resultHolder = resUnit;
			resGeometry = CreateDataItem(resUnit, token::geometry, resUnit, values1Unit, ValueComposition::Polygon);
			if (!resDomain->IsKindOf( Unit<Void>::GetStaticClass() ))
				resNrOrgEntity = CreateDataItem(resUnit, token::nr_OrgEntity, resUnit, resDomain, ValueComposition::Single);
		}
		else
		{
			resUnit = nullptr;
			if (!resultHolder)
				resultHolder = CreateCacheDataItem(resDomain, values1Unit, ValueComposition::Polygon);
			resGeometry = AsDataItem(resultHolder.GetNew());
		}

		if (mustCalc)
		{
			DataReadLock arg1Lock(argPoly);
			DataReadLock arg2Lock(argPart);
			DataReadLock arg3Lock(argNum1);
			DataReadLock arg4Lock(argNum2);

			if (DoDelayStore())
			{
				SizeT domainCount = resDomain->GetCount();
				ResourceArrayHandle r;
				for (tile_id t=0, te = domain1Unit->GetNrTiles(); t != te; ++t)
				{
					ReadableTileLock readArg1Lock (argPoly->GetCurrRefObj(), t);
					ReadableTileLock readArg2Lock (argPart ? argPart->GetCurrRefObj() : nullptr, t);

					Calculate(r, domainCount, argPoly, argPart, t);
				}
				DataWriteLock resGeometryHandle; // will be assigned after establishing the count of resUnit
				Store(resUnit, resGeometry, resGeometryHandle, resNrOrgEntity, no_tile, r, argNum1, argNum2);
				resGeometryHandle.Commit();
			}
			else
			{
				DataWriteLock resGeometryHandle(resGeometry);
				parallel_tileloop(domain1Unit->GetNrTiles(), [this, &resultHolder, resUnit, resDomain, &resGeometryHandle, resNrOrgEntity, argPoly, argPart, argNum1, argNum2](tile_id t) {
					if (resultHolder.WasFailed(FR_Data))
						resultHolder.ThrowFail();
					ResourceArrayHandle r;
					ReadableTileLock readArg1Lock (argPoly->GetCurrRefObj(), t);
					ReadableTileLock readArg2Lock (argPart ? argPart->GetCurrRefObj() : nullptr, t);

					Calculate(r, resDomain->GetTileCount(t), argPoly, argPart, t);
					Store(resUnit, nullptr, resGeometryHandle, resNrOrgEntity, t, r, argNum1, argNum2);
				});
				resGeometryHandle.Commit();
			}
		}
		return true;
	}
	bool DoDelayStore() const 
	{
		return m_Flags & (PolygonFlags::F_DoSplit | PolygonFlags::F_DoUnion);
	}
	void ProcessNumOper( ResourceArrayHandle& r, const AbstrDataItem* argNum, tile_id t, PolygonFlags f) const
	{
		if (f == PolygonFlags::none)
			return;

		dms_assert(r);
		dms_assert(argNum);

		tile_id numT = argNum->HasVoidDomainGuarantee() ? 0 : t;
		ReadableTileLock readNum1Lock (argNum ? argNum->GetCurrRefObj() : nullptr, numT);
#if defined(MG_DEBUG_POLYGON)
		switch (f) {
			case F_Inflate1:
				reportF(ST_MajorTrace, "UnionPolygon.Inflate %d", t); // DEBUG
				break;
			case F_Deflate1:
				reportF(ST_MajorTrace, "UnionPolygon.Deflate %d", t); // DEBUG
				break;
			case F_Filter1:
				reportF(ST_MajorTrace, "UnionPolygon.Filter %d", t); // DEBUG
				break;
			default:
				reportF(ST_MajorTrace, "UnionPolygon.Oper %d %d", f / F_Inflate1, t); // DEBUG
				break;
		}
#endif //defined(MG_DEBUG_POLYGON)

		ProcessNumOperImpl(r, argNum, numT, f);
	}
	void Store (AbstrUnit* resUnit, AbstrDataItem* resGeometry, DataWriteHandle& resGeometryHandle, AbstrDataItem* resNrOrgEntity, tile_id t, ResourceArrayHandle& r, const AbstrDataItem* argNum1, const AbstrDataItem* argNum2) const
	{
		if (r)
		{
			ProcessNumOper(r, argNum1, t, PolygonFlags(int(m_Flags) & int(PolygonFlags::F_Mask1)));
			ProcessNumOper(r, argNum2, t, PolygonFlags((int(m_Flags) & int(PolygonFlags::F_Mask2)) / (int(PolygonFlags::F_Mask2) / int(PolygonFlags::F_Mask1))));
		}
#if defined(MG_DEBUG_POLYGON)
		reportF(ST_MajorTrace, "UnionPolygon.Store %d", t);
#endif //defined(MG_DEBUG_POLYGON)
		StoreImpl(resUnit, resGeometry, resGeometryHandle, resNrOrgEntity, t, r);
	}
	virtual void Calculate(ResourceArrayHandle& r, SizeT domainCount, const AbstrDataItem* polyDataA, const AbstrDataItem* partitionDataA, tile_id t) const=0;
	virtual void StoreImpl(AbstrUnit* resUnit, AbstrDataItem* resGeometry, DataWriteHandle& resLock, AbstrDataItem* resNrOrgEntity, tile_id t, ResourceArrayHandle& r) const=0;
	virtual void ProcessNumOperImpl (ResourceArrayHandle& r, const AbstrDataItem* argNum, tile_id numT, PolygonFlags f) const = 0;
};

template <typename P>
class PolygonOperator : public AbstrPolygonOperator
{
	typedef union_poly_traits<P> traits_t;

	typedef typename traits_t::coordinate_type ScalarType;
	typedef typename traits_t::PointType       PointType;
	typedef typename traits_t::PolygonType     PolygonType;
	typedef typename Float64                   NumType;

	typedef DataArray<PolygonType>    ArgPolyType;
	typedef DataArray<NumType>        ArgNumType;

public:
	PolygonOperator(const std::pair<AbstrOperGroup*, PolygonFlags>& params)
		:	AbstrPolygonOperator(params.first, ArgPolyType::GetStaticClass(), ArgNumType::GetStaticClass(), params.second)
	{}

	void Calculate(ResourceArrayHandle& r, SizeT domainCount, const AbstrDataItem* polyDataA, const AbstrDataItem* partitionDataA, tile_id t) const override
	{
		UnionPolygon<PolygonType, traits_t::polygon_set_data_type>(r, domainCount, polyDataA, partitionDataA, t);
	}

	static void SetKernel(typename traits_t::point_seq_type& kernel, NumType value, PolygonFlags f)
	{
		if (f <= PolygonFlags::F_Filter1)
			return;
		kernel.clear();
		const NumType c0 = 0;

		const NumType csqrtHalf = sqrt(0.5);
		switch (f) {
			case PolygonFlags::F_I4HV1:
			case PolygonFlags::F_D4HV1:
				kernel.reserve(5);
				kernel.push_back(traits_t::point_type( value,  value));
				kernel.push_back(traits_t::point_type( value, -value));
				kernel.push_back(traits_t::point_type(-value, -value));
				kernel.push_back(traits_t::point_type(-value,  value));
				kernel.push_back(kernel.front());
				break;
			case PolygonFlags::F_I4D1:
			case PolygonFlags::F_D4D1:
				kernel.reserve(5);
				kernel.push_back(traits_t::point_type(    c0,  value));
				kernel.push_back(traits_t::point_type( value,     c0));
				kernel.push_back(traits_t::point_type(    c0, -value));
				kernel.push_back(traits_t::point_type(-value,     c0));
				kernel.push_back(kernel.front());
				break;
			case PolygonFlags::F_I8D1:
			case PolygonFlags::F_D8D1:
				kernel.reserve(9);
				kernel.push_back(traits_t::point_type(        c0        ,              value));
				kernel.push_back(traits_t::point_type( csqrtHalf * value,  csqrtHalf * value));
				kernel.push_back(traits_t::point_type(             value,         c0        ));
				kernel.push_back(traits_t::point_type( csqrtHalf * value, -csqrtHalf * value));
				kernel.push_back(traits_t::point_type(        c0        ,            - value));
				kernel.push_back(traits_t::point_type(-csqrtHalf * value, -csqrtHalf * value));
				kernel.push_back(traits_t::point_type(           - value,         c0        ));
				kernel.push_back(traits_t::point_type(-csqrtHalf * value,  csqrtHalf * value));
				kernel.push_back(kernel.front());
				break;
			case PolygonFlags::F_I16D1:
			case PolygonFlags::F_D16D1:
				kernel.reserve(17);
				kernel.push_back(traits_t::point_type(        c0        ,              value));
				kernel.push_back(traits_t::point_type( 0.3826834 * value,  0.9238795 * value));
				kernel.push_back(traits_t::point_type( csqrtHalf * value,  csqrtHalf * value));
				kernel.push_back(traits_t::point_type( 0.9238795 * value,  0.3826834 * value));
				kernel.push_back(traits_t::point_type(             value,         c0        ));
				kernel.push_back(traits_t::point_type( 0.9238795 * value, -0.3826834 * value));
				kernel.push_back(traits_t::point_type( csqrtHalf * value, -csqrtHalf * value));
				kernel.push_back(traits_t::point_type( 0.3826834 * value, -0.9238795 * value));
				kernel.push_back(traits_t::point_type(        c0        ,            - value));
				kernel.push_back(traits_t::point_type(-0.3826834 * value, -0.9238795 * value));
				kernel.push_back(traits_t::point_type(-csqrtHalf * value, -csqrtHalf * value));
				kernel.push_back(traits_t::point_type(-0.9238795 * value, -0.3826834 * value));
				kernel.push_back(traits_t::point_type(           - value,         c0        ));
				kernel.push_back(traits_t::point_type(-0.9238795 * value,  0.3826834 * value));
				kernel.push_back(traits_t::point_type(-csqrtHalf * value,  csqrtHalf * value));
				kernel.push_back(traits_t::point_type(-0.3826834 * value,  0.9238795 * value));
				kernel.push_back(kernel.front());
				break;

			case PolygonFlags::F_IXHV1:
			case PolygonFlags::F_DXHV1:
				kernel.reserve(9);
				kernel.push_back(traits_t::point_type( 0.1 * value,  0.1 * value));
				kernel.push_back(traits_t::point_type(     + value,  c0         ));
				kernel.push_back(traits_t::point_type( 0.1 * value, -0.1 * value));
				kernel.push_back(traits_t::point_type( c0         ,      - value));
				kernel.push_back(traits_t::point_type(-0.1 * value, -0.1 * value));
				kernel.push_back(traits_t::point_type(     - value,  c0         ));
				kernel.push_back(traits_t::point_type(-0.1 * value,  0.1 * value));
				kernel.push_back(traits_t::point_type( c0         ,      + value));
				kernel.push_back(kernel.front());
				break;

			case PolygonFlags::F_IXD1:
			case PolygonFlags::F_DXD1:
				kernel.reserve(9);
				kernel.push_back(traits_t::point_type(       value,        value));
				kernel.push_back(traits_t::point_type( 0.1 * value,  c0         ));
				kernel.push_back(traits_t::point_type(       value, -      value));
				kernel.push_back(traits_t::point_type( c0         , -0.1 * value));
				kernel.push_back(traits_t::point_type(     - value,      - value));
				kernel.push_back(traits_t::point_type(-0.1 * value,  c0         ));
				kernel.push_back(traits_t::point_type(     - value,        value));
				kernel.push_back(traits_t::point_type( c0         ,  0.1 * value));
				kernel.push_back(kernel.front());
				break;
		}
	}

	void ProcessNumOperImpl(ResourceArrayHandle& r, const AbstrDataItem* argNum, tile_id numT, PolygonFlags flag) const override
	{
		dms_assert(r);
		dms_assert(argNum);
		dms_assert(flag != PolygonFlags::none);

		ResourceArray<typename traits_t::polygon_set_data_type>* geometryDataPtr = debug_cast<ResourceArray<typename traits_t::polygon_set_data_type>*>(r.get_ptr());
		auto argNumData = const_array_cast<NumType>(argNum)->GetTile(numT);

		bool isParam = argNumData.size() == 1;
		SizeT domainCount = r->Size();
		dms_assert(isParam || argNumData.size() == domainCount);

		typename traits_t::point_seq_type          kernel;
		typename traits_t::polygon_result_type     geometry;
		typename traits_t::polygon_with_holes_type polygon;

		NumType value;
		if (isParam)
		{
			value = argNumData[0];
			SetKernel(kernel, value, flag);
		}

		typename traits_t::polygon_set_data_type::convolve_resources convolveResources;
		typename traits_t::polygon_set_data_type::clean_resources& cleanResources = convolveResources.cleanResources;

		for (SizeT i = 0; i!=domainCount; ++i)
		{
			typename traits_t::polygon_set_data_type& geometryData = geometryDataPtr->m_Data[ i ];

			if (empty(geometryData, cleanResources))
				continue;

			if (!isParam)
			{
				value = argNumData[i];
				SetKernel(kernel, value, flag);
			}

			typename traits_t::rect_type rectangle;
			typename traits_t::point_type p;
			geometryData.extents(rectangle, cleanResources);

			bool mustTranslate = coords_using_more_than_25_bits(rectangle);
			if (mustTranslate)
			{
				/* translate to zero to avoid numerical round-off errors */
				gtl::center(p, rectangle);
				typename traits_t::point_type mp(-x(p), -y(p));
				gtl::convolve(rectangle, mp);
				MG_CHECK(!coords_using_more_than_25_bits(rectangle)); // throw exception if this remains an issue
				geometryData.move(mp);
			}
			geometryData.clean(cleanResources);

			switch (flag) {
				case PolygonFlags::F_Inflate1:
					resize(geometryData, value, true, 8, convolveResources); //geometryData += value;
					break;
				case PolygonFlags::F_Deflate1:
					resize(geometryData, -value, true, 8, convolveResources); //geometryData -= value;
					break;
				case PolygonFlags::F_Filter1:
					gtl::keep(geometryData, 
						value, MAX_VALUE(traits_t::area_type), 
						0, MAX_VALUE(traits_t::unsigned_area_type), 
						0, MAX_VALUE(traits_t::unsigned_area_type)
					, cleanResources
					);
					break;
				default:
					dms_assert(flag > PolygonFlags::F_Filter1 && flag <= PolygonFlags::F_DXD1);
					geometry.clear();
					geometryData.get(geometry, cleanResources);

					gtl::detail::minkowski_offset<ScalarType>::convolve_kernel_with_polygon_set(geometryData, geometry, kernel.begin(), kernel.end(), (flag >= PolygonFlags::F_D4HV1));
			}
			/* translate result back to where it came from */
			geometryData.clean(cleanResources); // remove internal edges
			if (mustTranslate)
				geometryData.move(p);
		}
	}

	void StoreImpl (AbstrUnit* resUnit, AbstrDataItem* resGeometry, DataWriteHandle& resGeometryLock, AbstrDataItem* resNrOrgEntity, tile_id t, ResourceArrayHandle& r) const override
	{
		SizeT domainCount = 0;
		OwningPtrSizedArray<typename traits_t::polygon_result_type> geometryPtr;
		if (r)
		{
			ResourceArray<typename traits_t::polygon_set_data_type>* geometryDataPtr = debug_cast<ResourceArray<typename traits_t::polygon_set_data_type>*>(r.get_ptr());
			dms_assert(geometryDataPtr);

			domainCount = geometryDataPtr->Size();
			geometryPtr = OwningPtrSizedArray<typename traits_t::polygon_result_type>(domainCount MG_DEBUG_ALLOCATOR_SRC_STR("BoostPolygon: geometryPtr"));

			typename traits_t::polygon_set_data_type::clean_resources cleanResources;
			for (SizeT i = 0; i != domainCount; ++i) // TODO G8: parallel_for and cleanResources in a threadLocal thing
			{
				typename traits_t::polygon_set_data_type& geometryData = geometryDataPtr->m_Data[i];
				geometryData.get(geometryPtr[i], cleanResources);
				geometryData = typename traits_t::polygon_set_data_type(); // free no longer required resources
			}
			r.reset();
		}
		if (m_Flags & PolygonFlags::F_DoSplit)
		{
			dms_assert(resUnit);
			SizeT splitCount = 0;
			for (SizeT i = 0; i != domainCount; ++i)
				splitCount += geometryPtr[i].size();
			resUnit->SetCount(splitCount); // we must be in delayed store now

			if (resNrOrgEntity)
			{
				DataWriteLock resRelLock(resNrOrgEntity);
				SizeT splitCount2 = 0;
				for (SizeT i = 0; i != domainCount; ++i)
				{
					SizeT nrSplits = geometryPtr[i].size();
					SizeT nextCount = splitCount2 + nrSplits;
					while (splitCount2 != nextCount)
						resRelLock->SetValueAsSizeT(splitCount2++, i);
				}
				resRelLock.Commit();
			}
		}
		if (DoDelayStore())
		{
			dms_assert(resGeometry);
			resGeometryLock = DataWriteHandle(resGeometry, dms_rw_mode::write_only_all);
		}
		dms_assert(resGeometryLock);

		auto resArray = mutable_array_cast<PolygonType>(resGeometryLock)->GetLockedDataWrite(t, dms_rw_mode::write_only_all); // t may be no_tile
		auto resIter = resArray.begin();

		for (SizeT i = 0; i!=domainCount; ++i)
		{
			if (m_Flags & PolygonFlags::F_DoSplit)
			{
				dms_split_assign(resIter, geometryPtr[i]);
				resIter += geometryPtr[i].size();
			}
			else
			{
				dms_assign(*resIter, geometryPtr[i]);
				++resIter;
			}
			geometryPtr[i].clear();
		}
		dms_assert(resIter == resArray.end() || domainCount == 0);
	}
};

// *****************************************************************************
//	PolygonOverlay
// *****************************************************************************

#include "IndexAssigner.h"

CommonOperGroup cogPC("polygon_connectivity");
static TokenID tF1 = GetTokenID_st("F1"), tF2 = GetTokenID_st("F2");

class AbstrPolygonConnectivityOperator : public UnaryOperator
{
protected:
	AbstrPolygonConnectivityOperator(const DataItemClass* polyAttrClass)
		:	UnaryOperator(&cogPC, Unit<UInt32>::GetStaticClass()
			,	polyAttrClass
			)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 1);

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		dms_assert(arg1A); 

		const AbstrUnit* domain1Unit = arg1A->GetAbstrDomainUnit();

		AbstrUnit* res = Unit<UInt32>::GetStaticClass()->CreateResultUnit(resultHolder);
		resultHolder = res;

		AbstrDataItem* resF1 = CreateDataItem(res, tF1, res, domain1Unit);
		AbstrDataItem* resF2 = CreateDataItem(res, tF2, res, domain1Unit);

		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);

			Calculate(res, resF1, resF2, arg1A);
		}
		return true;
	}
	virtual void Calculate(AbstrUnit* res, AbstrDataItem* resF1, AbstrDataItem* resF2, const AbstrDataItem*arg1A) const=0;
};

template <typename P>
class PolygonConnectivityOperator : public AbstrPolygonConnectivityOperator
{
	typedef P                      PointType;
	typedef std::vector<PointType>  PolygonType;

	typedef DataArray<PolygonType> Arg1Type;

//	typedef std::vector<ResDataElemType> ResDataType;

	typedef typename scalar_of<PointType>::type  ScalarType;

public:
	PolygonConnectivityOperator()
		:	AbstrPolygonConnectivityOperator(Arg1Type::GetStaticClass())
	{}
	void Calculate(AbstrUnit* res, AbstrDataItem* resF1, AbstrDataItem* resF2, const AbstrDataItem* arg1A) const override
	{
		auto polyData = const_array_cast<PolygonType>(arg1A)->GetDataRead();
		SizeT nrVertices = polyData.size();
		SizeT nrEdges = 0;
		std::vector<std::set<int> > graph;
		std::vector<SizeT> domain_rel; domain_rel.reserve(nrVertices);

		if (nrVertices)
		{
			gtl::connectivity_extraction<ScalarType> ce;
			typename gtl::connectivity_extraction<ScalarType>::insert_resources ceResources;

			gtl::polygon_set_data<ScalarType> polygon;
			typename gtl::polygon_set_data<ScalarType>::clean_resources claimResources;
			for (SizeT i=0; i!=nrVertices; ++i)
			{
				if (polyData[i].size() > 2)
				{
					gtl::assign(polygon, polyData[i], claimResources);
					int result = ce.insert(polygon, ceResources);
					dms_assert(result == domain_rel.size());
					domain_rel.emplace_back(i);
				}
			}

			if (domain_rel.size())
			{
				typename gtl::connectivity_extraction<ScalarType>::extract_resources extractResources;
				graph.resize(domain_rel.size());
				ce.extract(graph, extractResources);

				for (auto gi = graph.begin(), ge = graph.end(); gi != ge; ++gi)
					nrEdges += gi->size();

				dms_assert(nrEdges % 2 == 0);
				nrEdges /= 2;
			}
		}
		res->SetCount(nrEdges);
		DataWriteLock resF1Lock(resF1);
		DataWriteLock resF2Lock(resF2);

		IndexAssigner32 indexAssigner1(resF1, resF1Lock.get(), no_tile, 0, nrEdges);
		IndexAssigner32 indexAssigner2(resF2, resF2Lock.get(), no_tile, 0, nrEdges);

		SizeT i=0, e=0;
		for (auto gi=graph.begin(), ge=graph.end(); gi!=ge; ++i, ++gi)
		{
			for (auto vsi = gi->begin(), vse=gi->end(); vsi!=vse; ++vsi)
				if (i < *vsi)
				{
					dms_assert(*vsi < domain_rel.size());
					indexAssigner1.m_Indices[e] = domain_rel[i];
					indexAssigner2.m_Indices[e] = domain_rel[ * vsi ];
					++e;
				}
		}
		indexAssigner1.Store();
		indexAssigner2.Store();

		dms_assert( e == nrEdges);
		resF2Lock.Commit();
		resF1Lock.Commit();
	}
};

// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

namespace 
{
	struct PolyOperatorGroup : CommonOperGroup
	{
		PolyOperatorGroup(CharPtr name, PolygonFlags flags)
			: CommonOperGroup(name)
			, m_Instances(std::pair<AbstrOperGroup*, PolygonFlags>(this, flags))
		{}

		tl_oper::inst_tuple<typelists::sint_points, PolygonOperator<_>, std::pair<AbstrOperGroup*, PolygonFlags>>
			m_Instances;
	};

	struct PolyOperatorGroups
	{
		PolyOperatorGroup simplePO, unionPO, partitionedPO;

		PolyOperatorGroups(WeakStr nameTempl, PolygonFlags flags)
			: simplePO(mySSPrintF(nameTempl.c_str(), "").c_str(), flags)
			, unionPO(mySSPrintF(nameTempl.c_str(), "union_").c_str(), PolygonFlags(flags | PolygonFlags::F_DoUnion))
			, partitionedPO(mySSPrintF(nameTempl.c_str(), "partitioned_union_").c_str(), PolygonFlags(flags | PolygonFlags::F_DoPartUnion))
		{}
	};

	struct PolyOperatorGroupss
	{
		PolyOperatorGroups simple, split;
		PolyOperatorGroupss(CharPtr suffix, PolygonFlags flags)
			: simple(SharedStr("%spolygon") + suffix, flags)
			, split(SharedStr("split_%spolygon") + suffix, PolygonFlags(flags | PolygonFlags::F_DoSplit))
		{}
	};

	struct PolyOperatorGroupsss
	{
		PolyOperatorGroupss f1, f2, f3, f4, f5, f6, f7, f8, f9, fa, fb, fc, fd, fe, ff;

		PolyOperatorGroupsss(WeakStr s2, PolygonFlags f)
			: f1(("_filtered" + s2).c_str(), PolygonFlags(f | PolygonFlags::F_Filter1))
			, f2(("_inflated" + s2).c_str(), PolygonFlags(f | PolygonFlags::F_Inflate1))
			, f3(("_deflated" + s2).c_str(), PolygonFlags(f | PolygonFlags::F_Deflate1))
			, f4(("_i4HV" + s2).c_str(), PolygonFlags(f | PolygonFlags::F_I4HV1))
			, f5(("_i4D" + s2).c_str(), PolygonFlags(f | PolygonFlags::F_I4D1))
			, f6(("_i8D" + s2).c_str(), PolygonFlags(f | PolygonFlags::F_I8D1))
			, f7(("_i16D" + s2).c_str(), PolygonFlags(f | PolygonFlags::F_I16D1))
			, f8(("_iXHV" + s2).c_str(), PolygonFlags(f | PolygonFlags::F_IXHV1))
			, f9(("_iXD" + s2).c_str(), PolygonFlags(f | PolygonFlags::F_IXD1))
			, fa(("_d4HV" + s2).c_str(), PolygonFlags(f | PolygonFlags::F_D4HV1))
			, fb(("_d4D" + s2).c_str(), PolygonFlags(f | PolygonFlags::F_D4D1))
			, fc(("_d8D" + s2).c_str(), PolygonFlags(f | PolygonFlags::F_D8D1))
			, fd(("_d16D" + s2).c_str(), PolygonFlags(f | PolygonFlags::F_D16D1))
			, fe(("_dXHV" + s2).c_str(), PolygonFlags(f | PolygonFlags::F_DXHV1))
			, ff(("_dXD" + s2).c_str(), PolygonFlags(f | PolygonFlags::F_DXD1))
		{}
	};

	tl_oper::inst_tuple<typelists::sint_points, PolygonOverlayOperator     <_>> polygonOverlayOperators;
	tl_oper::inst_tuple<typelists::sint_points, PolygonConnectivityOperator<_>>	polygonConnectivityOperators;

	PolyOperatorGroupss simple("", PolygonFlags());
	PolyOperatorGroupsss f2 (SharedStr(""),          PolygonFlags());
	PolyOperatorGroupsss f21(SharedStr("_filtered"), PolygonFlags::F_Filter2);
	PolyOperatorGroupsss f22(SharedStr("_inflated"), PolygonFlags::F_Inflate2);
	PolyOperatorGroupsss f23(SharedStr("_deflated"), PolygonFlags::F_Deflate2);
	PolyOperatorGroupsss f24(SharedStr("_i4HV"), PolygonFlags::F_I4HV2);
	PolyOperatorGroupsss f25(SharedStr("_i4D"), PolygonFlags::F_I4D2);
	PolyOperatorGroupsss f26(SharedStr("_i8D"), PolygonFlags::F_I8D2);
	PolyOperatorGroupsss f27(SharedStr("_i16D"), PolygonFlags::F_I16D2);
	PolyOperatorGroupsss f28(SharedStr("_iXHV"), PolygonFlags::F_IXHV2);
	PolyOperatorGroupsss f29(SharedStr("_iXD"), PolygonFlags::F_IXD2);
	PolyOperatorGroupsss f2a(SharedStr("_d4HV"), PolygonFlags::F_D4HV2);
	PolyOperatorGroupsss f2b(SharedStr("_d4D"), PolygonFlags::F_D4D2);
	PolyOperatorGroupsss f2c(SharedStr("_d8D"), PolygonFlags::F_D8D2);
	PolyOperatorGroupsss f2d(SharedStr("_d16D"), PolygonFlags::F_D16D2);
	PolyOperatorGroupsss f2e(SharedStr("_dXHV"), PolygonFlags::F_DXHV2);
	PolyOperatorGroupsss f2f(SharedStr("_dXD"), PolygonFlags::F_DXD2);
}

/******************************************************************************/
