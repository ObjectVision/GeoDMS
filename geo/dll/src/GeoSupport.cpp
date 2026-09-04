// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// Small geo satellites, merged (2026-08): GeoInterface (DMS_Geo_Load),
// AbstrBoundingBoxCache, PolyOper, Mandlebrot.

// ==== GeoInterface ====

#include "GeoInterface.h"

#include "StgBase.h"
#include "TicInterface.h"

GEO_CALL void DMS_CONV DMS_Geo_Load() 
{
}



// ==== AbstrBoundingBoxCache ====

#include "BoundingBoxCache.h"

#include "LockLevels.h"

//----------------------------------------------------------------------
// class  : AbstrBoundingBoxCache
//----------------------------------------------------------------------

std::map < const AbstrDataObject*, std::weak_ptr<const AbstrBoundingBoxCache>> g_BB_Register;
leveled_critical_section cs_BB(item_level_type(0), ord_level_type::BoundingBoxCache1, "BoundingBoxCache");

AbstrBoundingBoxCache::AbstrBoundingBoxCache(const AbstrDataObject* featureData)
	:	m_FeatureData(featureData)
{}

AbstrBoundingBoxCache::~AbstrBoundingBoxCache()
{
	DMS_ENTERS(ord_level_type::BoundingBoxCache1, dms_exclusive_v);
	leveled_critical_section::scoped_lock lockBB_register(cs_BB);
	auto ptr = g_BB_Register.find(m_FeatureData);
	if (ptr != g_BB_Register.end() && !ptr->second.lock())
		g_BB_Register.erase(ptr);
}

DRect AbstrBoundingBoxCache::GetBounds(tile_id t, tile_offset featureID) const
{
	throwIllegalAbstract(MG_POS, "AbstrBoundingBoxCache::GetBounds");
}

DRect AbstrBoundingBoxCache::GetBounds(SizeT featureID) const
{
	tile_loc tl = m_FeatureData->GetTiledLocation(featureID);
	return GetBounds(tl.first, tl.second);
}







// ==== PolyOper ====

#include "PolyOper.h"


// *****************************************************************************
//											INSTANTIATION helpers
// *****************************************************************************

template <typename TL, template <typename T> class MetaFunc>
struct BinaryPolyOperInstantiation
{
	using OperTemplate = tl::bind_placeholders < BinaryPolyAttrAssignOper, MetaFunc<ph::_1> >;
	tl_oper::inst_tuple<TL, OperTemplate> m_OperList;

	BinaryPolyOperInstantiation(AbstrOperGroup* gr)
		: m_OperList(gr)
	{}

};


// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************
#include "RtcTypeLists.h"

using namespace typelists;


namespace {
	CommonOperGroup
		cogBpIntersect("bp_intersect"),
		cogBpUnion("bp_union"),
		cogBpSymmetricDifference("bp_xor"),
		cogBpDifference("bp_difference");

	BinaryPolyOperInstantiation<typelists::sint_points, poly_and>  sAndPoly(&cog_bitand), sMulPoly(&cog_mul), sIntersectPoly(&cogBpIntersect);
	BinaryPolyOperInstantiation<typelists::sint_points, poly_or >  sOrPoly(&cog_bitor), sAddPoly(&cog_add), sUnionPoly(&cogBpUnion);
	BinaryPolyOperInstantiation<typelists::sint_points, poly_xor>  sXOrPoly(&cog_pow), sBpSymmetricDifference(&cogBpSymmetricDifference);
	BinaryPolyOperInstantiation<typelists::sint_points, poly_sub>  sSubPoly(&cog_sub), sBpDifference(&cogBpDifference);
//	BinaryPolyOperInstantiation<typelists::sint_points, poly_eq>   sEqPoly (&cog_eq);
//	BinaryPolyOperInstantiation<typelists::sint_points, poly_ne>   sEqPoly (&cog_ne);
} // namespace


// ==== Mandlebrot ====

#include "mth/Mathlib.h"

#include "Param.h"
#include "UnitClass.h"
#include "DataArrayValue.h"

// *****************************************************************************
//											Mandelbrot Operator
// *****************************************************************************

UInt32 MandelbrodCount(DPoint currLoc, UInt32 maxCount)
{
	DPoint z = currLoc;
	UInt32 count = 0;
	while (count < maxCount)
	{
		if (sqr(z.Col())+sqr(z.Row()) > 4)
			return count;

		// z := z*z + currLoc;
		Float64 newZY = 2*z.Col()*z.Row() + currLoc.Row();
		z.Col() = sqr(z.Col()) - sqr(z.Row()) + currLoc.Col();
		z.Row() = newZY;
		++count;
	}
	return maxCount;
}

class MandelbrotOperator : public QuaternaryOperator
{
	typedef Unit<SPoint>        Arg1Type; // resulting value unit for resulting dist2 attr of newly created entity
	typedef Unit<UInt32>        Arg2Type; // resulting count range
	typedef DataArray<DPoint>   Arg3Type; // location of Arg1 origin
	typedef DataArray<DPoint>   Arg4Type; // increment of Arg1 cells
	typedef DataArray<UInt32>   ResultType;
			
public:
	MandelbrotOperator(AbstrOperGroup* gr)
		:	QuaternaryOperator(gr, ResultType::GetStaticClass(), 
				Arg1Type::GetStaticClass(), 
				Arg2Type::GetStaticClass(), 
				Arg3Type::GetStaticClass(), 
				Arg4Type::GetStaticClass()
			) 
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 4);

		const Arg1Type* arg1  = debug_cast<const Arg1Type*>(args[0]);
		const Arg2Type* arg2  = debug_cast<const Arg2Type*>(args[1]);
		const AbstrDataItem* arg3A = debug_cast<const AbstrDataItem*>(args[2]);
		const AbstrDataItem* arg4A = debug_cast<const AbstrDataItem*>(args[3]);

		dms_assert(arg1);
		dms_assert(arg2);
		dms_assert(arg3A);
		dms_assert(arg4A);

		checked_domain<Void>(arg3A, "a3");
		checked_domain<Void>(arg4A, "a4");

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(arg1, arg2);

		if (mustCalc)
		{
			AbstrDataItem* res = debug_cast<AbstrDataItem*>(resultHolder.GetNew());
			dms_assert(res->GetAbstrDomainUnit() == arg1);
			dms_assert(res->GetAbstrValuesUnit() == arg2);

			SRect  rect     = arg1->GetRange();
			UInt32 size     = Cardinality(Size(rect));
			UInt32 maxCount = arg2->GetCount();
			// GetCurrValue, not GetValue: the free GetValue<V> calls PrepareDataUsage(CertainOrThrow),
			// whose Update bits are meta-thread-only (TreeItem.cpp:4143). CreateResult runs on a
			// portable_task_group worker, so that tripped the IsMetaThread assert. The arguments are
			// already prepared by the time the task runs, so a plain DataReadLock -- which is all
			// GetCurrValue does -- is what is wanted here, as in OperDistrict's Diversity operator.
			DPoint origin   = GetCurrValue<DPoint>(arg3A, 0);
			DPoint increment= GetCurrValue<DPoint>(arg4A, 0);
			SPoint topLeft = rect.first;
			SPoint botRight= rect.second;
			
			origin.Row() += topLeft.Row() * increment.Row();
			origin.Col() += topLeft.Col() * increment.Col();

			dms_assert(topLeft.first  <= botRight.first);
			dms_assert(topLeft.second <= botRight.second);

			DataWriteLock resLock(res);
			auto result = mutable_array_cast<UInt32>(resLock); assert(result);
			// write_only_all: the row/col loop below assigns every one of `size` elements. mustzero was
			// unsatisfiable here anyway (untiled result, allocated by the lock above as write_only_all).
			auto resultData = result->GetDataWrite(no_tile, dms_rw_mode::write_only_all); assert(resultData.size() == size);

			ResultType::iterator dai = resultData.begin();
			DPoint currLoc = origin;
			for (Int16 row=topLeft.first;  row!=botRight.first;  ++row)
			{
				for (Int16 col=topLeft.second; col!=botRight.second; ++col)
				{
					*dai++ = MandelbrodCount(currLoc, maxCount);
					currLoc.Col() += increment.Col();
				}
				currLoc.Row() += increment.Row();
				currLoc.Col() =  origin.Col();
			}

			resLock.Commit();
		}
		return true;
	}
};

// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

namespace
{
	CommonOperGroup mandel("Mandelbrot", oper_policy::better_not_in_meta_scripting);
	MandelbrotOperator m(&mandel);
}
