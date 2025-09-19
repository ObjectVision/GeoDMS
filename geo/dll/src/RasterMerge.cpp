// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "RtcTypeLists.h"
#include "utl/TypeListOper.h"

#include "geo/RangeIndex.h"
#include "ViewPortInfoEx.h"

#include "Unit.h"
#include "UnitClass.h"

// *****************************************************************************
//                            RasterMergeOperator
// een operator om een serie deelkaarten in 1 keer samen te voegen
//	raster_merge<I,V>: (
//		indexmap: GRID->I; 
//      resultUnit: V;
//      datamaps[0..I-1]: SUBGRIDS->V
//	): GRID->V
// *****************************************************************************

#include "DataLockContainers.h"
namespace 
{

CommonOperGroup cog_rasterMerge("raster_merge", oper_policy::allow_extra_args | oper_policy::better_not_in_meta_scripting);
CommonOperGroup cog_simpleMerge("merge",        oper_policy::allow_extra_args | oper_policy::better_not_in_meta_scripting);


struct AbstrRasterMergeOperator : public BinaryOperator
{
	AbstrRasterMergeOperator(const DataItemClass* argV, const DataItemClass* arg1, const UnitClass* arg2, bool isRasterMerge)
		:	BinaryOperator(isRasterMerge ? &cog_rasterMerge : &cog_simpleMerge, argV,
				(arg1 != nullptr) ? arg1 : AbstrUnit::GetStaticClass(), arg2)
		,	m_IsRasterMerge(isRasterMerge)
		,	m_IsIndexed(arg1 != nullptr)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() >= 2);

		const AbstrDataItem* arg1A = m_IsIndexed ? AsDataItem(args[0]) : nullptr;
		const AbstrUnit*     v1    = AsUnit(args[1]); dms_assert(v1);
		const AbstrUnit*     e1    = m_IsIndexed ? arg1A->GetAbstrDomainUnit() : AsUnit(args[0]); assert(e1);

		if (m_IsRasterMerge && e1->GetNrDimensions() != 2)
			resultHolder.throwItemError("Raster domain expected");

		for (arg_index a=2, nrArg=args.size(); a!=nrArg; ++a)
		{
			if (!IsDataItem(args[a]))
				resultHolder.throwItemErrorF("DataItem expected at arg %d", a);
			const AbstrDataItem* argDi = AsDataItem(args[a]);
			if (m_IsRasterMerge && argDi->GetAbstrDomainUnit()->GetNrDimensions() != 2 && !(m_IsIndexed && argDi->HasVoidDomainGuarantee()))
				resultHolder.throwItemErrorF("Raster data expected at arg %d", a);
			v1->UnifyValues(argDi->GetAbstrValuesUnit(), "v1", "Values of a subsequent attribute", UnifyMode(UM_AllowVoidRight | UM_Throw));
		}

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(e1, v1);
		AbstrDataItem* res = AsDataItem(resultHolder.GetNew());

		if (!mustCalc)
			return true;


		DataReadLock  drlReg(arg1A);
		DataWriteLock dwlReg(res);

		DataReadLockContainer drlArgs; drlArgs.reserve(args.size()-2);
		for (UInt32 a=2, nrArg=args.size(); a!=nrArg; ++a)
			drlArgs.push_back(DataReadLock(AsDataItem(args[a])));

		const AbstrUnit* e1_range = AsUnit(e1->GetCurrRangeItem());
		for (tile_id t=0, tn=e1_range->GetNrTiles(); t!=tn; ++t)
		{
			InitTile(dwlReg.get(), t);

			// move to AbstrTileRageData
			auto currTileRect = e1_range->GetTiledRangeData()->GetTileRangeAsI64Rect(t);

			for (arg_index a=2, nrArg=args.size(); a!=nrArg; ++a)
			{
				const AbstrDataItem* argDi = AsDataItem(args[a]);
				const AbstrUnit* argDU = argDi->GetAbstrDomainUnit();
				const AbstrUnit* argDU_range = AsUnit(argDU->GetCurrRangeItem());

				// m_IsIndexed ? "Domain of Index" : "First argument", "Domain of any subsequent attribute"
				bool isSame = e1->UnifyDomain(argDU, "", "", UM_AllowVoidRight);
				if (!m_IsRasterMerge || isSame)
				{
					if (!isSame)
						e1->UnifyDomain(argDU, "e1", "Domain of a subsequent attribute", UM_AllowVoidRight | UM_Throw);

					ViewPortInfoEx<Int64> vpi(res, e1_range, t, e1_range, t);
					if (vpi.IsGridCurrVisible())
						CopyWhere(dwlReg.get(), arg1A, t, argDi, t, vpi, a - 2);
				}
				else
				{
					for (tile_id u=0, un=argDU->GetNrTiles(); u!=un; ++u)
					{
						ViewPortInfoEx<Int64> arg2AllProj(res, argDU_range, u, e1_range, t);
						if (!arg2AllProj.IsNonScaling())
							res->throwItemErrorF("RasterMerge: Scale or projection of argument %d incompatible with the %s, which determined teh domain of the resulting attribute"
								, a
								, m_IsIndexed ? "Domain of the first attribute" : "First argument"
							);

						dms_assert(arg2AllProj.GetGridExtents() == currTileRect);
						MG_DEBUGCODE( 
							auto uRect = argDU_range->GetTiledRangeData()->GetTileRangeAsI64Rect(u);
							dms_assert(arg2AllProj.GetViewPortExtents() == uRect);
							uRect = arg2AllProj.Apply(uRect);
						);
						if (arg2AllProj.IsGridCurrVisible())
						{
							dbg_assert( IsIntersecting(currTileRect, uRect) );
							CopyWhere(dwlReg.get(), arg1A, t, argDi, u, arg2AllProj, a-2);
						}
						else
						{
							dbg_assert(!IsIntersecting(currTileRect, uRect) );
						}
					}
				}
			}
		}
		dwlReg.Commit();

		return true;
	}
	virtual void InitTile(AbstrDataObject* resData, tile_id t) const=0;
	virtual void CopyWhere(AbstrDataObject* resData, 
		const AbstrDataItem* indexDataA, tile_id t, 
		const AbstrDataItem* valueDataA, tile_id u, 
		const ViewPortInfoEx<Int64>& vpi, UInt32 a) const=0;

	bool m_IsRasterMerge, m_IsIndexed;
};

template <typename IndexType, typename ValueType>
struct MergeOperatorBase : public AbstrRasterMergeOperator
{
	typedef DataArray<IndexType> Arg1Type; // indices vector
	typedef Unit<ValueType>      Arg2Type; // values unit of result
	typedef DataArray<ValueType> ArgVType; // values and result vector
         
	MergeOperatorBase(bool isRasterMerge)
		:	AbstrRasterMergeOperator(
				ArgVType::GetStaticClass()
			,	Arg1Type::GetStaticClass() 
			,	Arg2Type::GetStaticClass()
			,	isRasterMerge
			)
	{}

	void InitTile(AbstrDataObject* resDataA, tile_id t) const override
	{
		auto resultData = mutable_array_cast<ValueType>(resDataA)->GetDataWrite(t);
		fast_fill(resultData.begin(), resultData.end(), UNDEFINED_OR_ZERO(ValueType));
	}

	void CopyWhere(AbstrDataObject* resDataA, 
		const AbstrDataItem* indexDataA, tile_id t, 
		const AbstrDataItem* valueDataA, tile_id u, 
		const ViewPortInfoEx<Int64>& vpi, UInt32 a) const override
	{
		assert(resDataA);
		assert(indexDataA);
		assert(valueDataA);

		I64Rect tRect = vpi.GetGridExtents();
		I64Rect uRect = vpi.GetViewPortInGridAsIRect();
		bool valueVoidDomain = valueDataA->HasVoidDomainGuarantee();
		assert(u == t || !valueVoidDomain);
		if (valueVoidDomain)
			u = 0;

		auto indexData  = const_array_cast  <IndexType>(indexDataA)->GetTile(t);

		I64Rect tuRect = tRect & uRect;
		SizeT
			tWidth = Width(tRect),
			uWidth = Width(uRect),
			tuWidth = Width(tuRect);

		assert(!tuRect.empty());
		bool found = false;
		auto indexIter = indexData.begin() + Range_GetIndex_naked(tRect, tuRect.first);
		Int64 r = tuRect.first.Row(), re = tuRect.second.Row();
		for (; r != re and not(found); ++r)
		{
			for (UInt64 c = 0; c != tuWidth; ++indexIter, ++c)
				if (*indexIter == a)
				{
					found = true;
					break;
				}
			if (found)
				break; // do not increase r for the row on which the value a a found has to be reprocessed
			indexIter += (tWidth - tuWidth);
		}

		if (not(found))
			return;

		auto currPos = shp2dms_order(tuRect.first.Col(), r);
		indexIter = indexData.begin() + Range_GetIndex_naked(tRect, currPos); // reset to beginning of the row

		// delay this up to the point that whe know some values need to be copied at all, see https://github.com/ObjectVision/GeoDMS/issues/927
		auto valueData  = const_array_cast  <ValueType>(valueDataA)->GetTile(u);
		auto resultData = mutable_array_cast<ValueType>(  resDataA)->GetWritableTile(t);

		auto resIter = resultData.begin() + Range_GetIndex_naked(tRect, currPos);

		if (valueVoidDomain) {
			ValueType value = valueData[0];
			for (re = tuRect.second.Row(); r != re; ++r)
			{
				auto indexEnd = indexIter + tuWidth;

				for (UInt64 c = 0; c != tuWidth; ++indexIter, ++c)
					if (*indexIter == a)
						resIter[c] = value;

				indexIter += (tWidth - tuWidth);
				resIter += tWidth;
			}
		}
		else {
			auto valueIter = valueData.begin() + Range_GetIndex_naked(uRect, currPos);

			for (; r != re; ++r)
			{
				auto indexEnd = indexIter + tuWidth;

				for (UInt64 c = 0; c != tuWidth; ++indexIter, ++c)
					if (*indexIter == a)
						resIter[c] = valueIter[c];

				indexIter += (tWidth - tuWidth);
				valueIter += uWidth;
				resIter += tWidth;
			}
		}
	}
};

template <typename ValueType>
struct RasterImpressOperator: public AbstrRasterMergeOperator
{
	typedef Unit<ValueType>      Arg2Type; // values unit of result
	typedef DataArray<ValueType> ArgVType; // values and result vector
         
	RasterImpressOperator()
		:	AbstrRasterMergeOperator(
				ArgVType::GetStaticClass()
			,	nullptr 
			,	Arg2Type::GetStaticClass()
			,	true
			)
	{}

	void InitTile(AbstrDataObject* resDataA, tile_id t) const override
	{
		auto resultData = mutable_array_cast<ValueType>(resDataA)->GetDataWrite(t);
		fast_fill(resultData.begin(), resultData.end(), UNDEFINED_OR_ZERO(ValueType));
	}

	void CopyWhere(AbstrDataObject* resDataA, 
		const AbstrDataItem* indexDataA, tile_id t, 
		const AbstrDataItem* valueDataA, tile_id u, 
		const ViewPortInfoEx<Int64>& vpi, arg_index a) const override
	{
		dms_assert(resDataA);
		dms_assert(!indexDataA);
		dms_assert(valueDataA);

		I64Rect tRect = vpi.GetGridExtents();
		I64Rect uRect = vpi.GetViewPortInGridAsIRect();

		auto valueData  = const_array_cast<ValueType>(valueDataA)->GetLockedDataRead(u);
		auto resultData = mutable_array_cast<ValueType>(  resDataA)->GetDataWrite(t, ((u==0) && (a==0)) ? dms_rw_mode::write_only_mustzero : dms_rw_mode::read_write);

		I64Rect tuRect = tRect & uRect; 
		UInt32 
			tWidth  = Width(tRect),
			uWidth  = Width(uRect),
			tuWidth = Width(tuRect);

		dms_assert(!tuRect.empty());

		auto valueIter = valueData.begin() + Range_GetIndex_naked(uRect, tuRect.first);
		auto resIter   = resultData.begin() + Range_GetIndex_naked(tRect, tuRect.first);
		for (Int64 r=tuRect.first.Row(), re=tuRect.second.Row(); r!=re; ++r)
		{
			for(UInt64 c=0; c!=tuWidth; ++c)
				resIter[c] = valueIter[c];

			valueIter += uWidth;
			resIter   += tWidth;
		}
	}
};


template <typename IndexType, typename ValueType>
struct RasterMergeOperator : public MergeOperatorBase<IndexType, ValueType>
{
	RasterMergeOperator() : MergeOperatorBase<IndexType, ValueType>(true)
	{}
};

template <typename IndexType, typename ValueType>
struct SimpleMergeOperator : public MergeOperatorBase<IndexType, ValueType>
{
	SimpleMergeOperator() : MergeOperatorBase<IndexType, ValueType>(false)
	{}
};

}	// end anonymous namespace

// *****************************************************************************
//                               INSTANTIATION
// *****************************************************************************

namespace 
{
//	tl_oper::inst_tuple<typelists::value_elements, tl::bind_placeholders<RasterMergeOperator, UInt8, ph::_1> > rasterMerge08Opers;
//	tl_oper::inst_tuple<typelists::value_elements, tl::bind_placeholders<RasterMergeOperator, UInt16, ph::_1> > rasterMerge16Opers;
	tl_oper::inst_tuple_templ<typelists::numerics, RasterImpressOperator > rasterImpressOpers;
	tl_oper::inst_tuple<typelists::numerics, tl::bind_placeholders<RasterMergeOperator, UInt8, ph::_1> > rasterMerge08Opers;
	tl_oper::inst_tuple<typelists::numerics, tl::bind_placeholders<RasterMergeOperator, UInt16, ph::_1> > rasterMerge16Opers;

	tl_oper::inst_tuple<typelists::numerics, tl::bind_placeholders<SimpleMergeOperator,  UInt8, ph::_1> > simpleMerge08Opers;
	tl_oper::inst_tuple<typelists::numerics, tl::bind_placeholders<SimpleMergeOperator, UInt16, ph::_1> > simpleMerge16Opers;
}	// end anonymous namespace

/******************************************************************************/

