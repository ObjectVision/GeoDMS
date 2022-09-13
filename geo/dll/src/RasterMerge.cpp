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

CommonOperGroup cog_rasterMerge("raster_merge", oper_policy(oper_policy::allow_extra_args));
CommonOperGroup cog_simpleMerge("merge",        oper_policy(oper_policy::allow_extra_args));


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
		const AbstrUnit*     e1    = m_IsIndexed ? arg1A->GetAbstrDomainUnit() : AsUnit(args[0]); dms_assert(e1);

		if (m_IsRasterMerge && e1->GetNrDimensions() != 2)
			resultHolder.throwItemError("Raster domain expected");

		for (UInt32 a=2, nrArg=args.size(); a!=nrArg; ++a)
		{
			if (!IsDataItem(args[a]))
				resultHolder.throwItemErrorF("DataItem expected at arg %d", a);
			const AbstrDataItem* argDi = AsDataItem(args[a]);
			if (m_IsRasterMerge && argDi->GetAbstrDomainUnit()->GetNrDimensions() != 2 && !(m_IsIndexed && argDi->HasVoidDomainGuarantee()))
				resultHolder.throwItemErrorF("Raster data expected at arg %d", a);
			v1->UnifyValues(argDi->GetAbstrValuesUnit(), UnifyMode(UM_AllowVoidRight | UM_Throw));
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

				if (e1->UnifyDomain(argDU, UM_AllowVoidRight))
				{
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
							res->throwItemErrorF("RasterMerge: Scale of argument %d incompatible with the domain of the result", a);

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
		dms_assert(resDataA);
		dms_assert(indexDataA);
		dms_assert(valueDataA);

		I64Rect tRect = vpi.GetGridExtents();
		I64Rect uRect = vpi.GetViewPortInGridAsIRect();
		bool valueVoidDomain = valueDataA->HasVoidDomainGuarantee();
		dms_assert(u == t || !valueVoidDomain);
		if (valueVoidDomain)
			u = 0;

		auto indexData  = const_array_cast  <IndexType>(indexDataA)->GetTile(t);
		auto valueData  = const_array_cast  <ValueType>(valueDataA)->GetTile(u);
		auto resultData = mutable_array_cast<ValueType>(  resDataA)->GetWritableTile(t);

		I64Rect tuRect = tRect & uRect; 
		SizeT
			tWidth  = _Width(tRect),
			uWidth  = _Width(uRect),
			tuWidth = _Width(tuRect);

		dms_assert(!tuRect.empty());

		if (valueVoidDomain) {
			auto indexIter = indexData.begin() + Range_GetIndex_naked(tRect, tuRect.first);
			ValueType value = valueData[0];
			auto resIter = resultData.begin() + Range_GetIndex_naked(tRect, tuRect.first);
			for (Int64 r = tuRect.first.Row(), re = tuRect.second.Row(); r != re; ++r)
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
			auto indexIter = indexData.begin() + Range_GetIndex_naked(tRect, tuRect.first);
			auto valueIter = valueData.begin() + Range_GetIndex_naked(uRect, tuRect.first);
			auto resIter   = resultData.begin() + Range_GetIndex_naked(tRect, tuRect.first);
			for (Int64 r = tuRect.first.Row(), re = tuRect.second.Row(); r != re; ++r)
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
			tWidth  = _Width(tRect),
			uWidth  = _Width(uRect),
			tuWidth = _Width(tuRect);

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
//	tl_oper::inst_tuple<typelists::value_elements, RasterMergeOperator< UInt8, _> > rasterMerge08Opers;
//	tl_oper::inst_tuple<typelists::value_elements, RasterMergeOperator<UInt16, _> > rasterMerge16Opers;
	tl_oper::inst_tuple<typelists::numerics, RasterImpressOperator< _> > rasterImpressOpers;
	tl_oper::inst_tuple<typelists::numerics, RasterMergeOperator< UInt8, _> > rasterMerge08Opers;
	tl_oper::inst_tuple<typelists::numerics, RasterMergeOperator<UInt16, _> > rasterMerge16Opers;

	tl_oper::inst_tuple<typelists::numerics, SimpleMergeOperator< UInt8, _> > simpleMerge08Opers;
	tl_oper::inst_tuple<typelists::numerics, SimpleMergeOperator<UInt16, _> > simpleMerge16Opers;
}	// end anonymous namespace

/******************************************************************************/

