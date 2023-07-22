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

#include "ClcPCH.h"
#pragma hdrstop

#include "Invert.h"

#include "mci/CompositeCast.h"
#include "mci/ValueClass.h"

#include "DataArray.h"
#include "DataItemClass.h"
#include "ParallelTiles.h"
#include "Unit.h"
#include "UnitClass.h"
#include "TreeItemClass.h"
#include "UnitProcessor.h"

#include "RtcTypeLists.h"
#include "utl/TypeListOper.h"

// *****************************************************************************
//									Invert
// *****************************************************************************
namespace { // anonymous

CommonOperGroup cog_Invert   ("invert", oper_policy::dynamic_result_class);
CommonOperGroup cog_InvertAll("invertAll", oper_policy::dynamic_result_class);

// rlookup(D->V, E->V): D->E'
// invert(A->B) := B->A';
// => rlookup(D->D, E->D) == invert(E->D)
// rewrite: invert(X) -> rlookup(ID(ValuesUnit(X)), X)
// rewrite: invert(A->B) -> rlookup(B->B, A->B)

static TokenID nextToken = GetTokenID_st("Next");

struct AbstrInvertOperator : public UnaryOperator
{
	typedef AbstrDataItem     ResultType;
	typedef AbstrDataItem     ResultSubType;
	AbstrInvertOperator(bool all, const Class* arg1Cls)
		:	UnaryOperator(all ? &cog_InvertAll : &cog_Invert
			,	ResultType::GetStaticClass()
			,	arg1Cls
			)
		,	m_All(all)
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 1);

		const AbstrDataItem* arg1A= debug_cast<const AbstrDataItem*>(args[0]);
		dms_assert(arg1A);

		const AbstrUnit* entity    = arg1A->GetAbstrDomainUnit();
		const AbstrUnit* newDomain = arg1A->GetAbstrValuesUnit();

		dms_assert(entity);
		dms_assert(newDomain);

		if (!resultHolder)
		{
			resultHolder = CreateCacheDataItem(newDomain, entity);
			resultHolder->SetTSF(TSF_Categorical);
		}
		AbstrDataItem* res = AsDataItem(resultHolder.GetNew());

		AbstrDataItem* resSub = nullptr;
		if (m_All)
		{
			resSub = CreateDataItem(res, nextToken, entity, entity);
		}

		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);

			DataWriteLock resLock1(res);
			DataWriteLock resLock2(resSub); // there's no mechanism yet to do invertAll with tiles

			Calculate(arg1A, resLock1, resLock2);

			if (m_All)
				resLock2.Commit();
			resLock1.Commit();
		}
		return true;
	}
	virtual void  Calculate(const AbstrDataItem* arg1A, DataWriteLock& res, DataWriteLock& resSub) const= 0;

protected:
	bool m_All;
};

template <class T>
class InvertOperator : public AbstrInvertOperator
{
	typedef DataArray<T>      Arg1Type;
			
public:
	InvertOperator(bool all)
		:	AbstrInvertOperator(all, Arg1Type::GetStaticClass())
	{}

	void  Calculate(const AbstrDataItem* arg1A, DataWriteLock& res, DataWriteLock& resSub) const override
	{
		const Arg1Type* arg1 = const_array_cast<T>(arg1A);
		dms_assert(arg1);

		DataCheckMode dcm = (res->GetTiledRangeData()->GetNrTiles() > 1) ? DCM_CheckRange : arg1A->GetCheckMode();
		visit<typelists::domain_elements>(arg1A->GetAbstrDomainUnit(), 
			[&res, &resSub, arg1A, dcm] <typename E> (const Unit<E>* inviter)
			{
				DataArray<E>* resultObj = mutable_array_cast<E>(res);
				DataArray<E>* resSubData = resSub ? mutable_array_cast<E>(resSub) : nullptr;
				tile_id at_n = arg1A->GetAbstrDomainUnit()->GetNrTiles();
				tile_id rt_n = res->GetTiledRangeData()->GetNrTiles();

				typename Unit<E>::range_t entityRange = inviter->GetRange();

				if (!at_n)
					parallel_tileloop(rt_n, [arg1A, rt_n, &resultObj](tile_id rt)->void
						{
							auto resultDataPerV = resultObj->GetWritableTile(rt, dms_rw_mode::write_only_all);
							fast_fill(resultDataPerV.begin(), resultDataPerV.end(), UNDEFINED_OR_ZERO(E));
						}
					);
				else for (tile_id at = 0; at != at_n; ++at)  // iterate over tiles of E
				{
					auto arg1Data = const_array_cast<T>(arg1A)->GetTile(at);
					typename Unit<E>::range_t arg1TileRange = inviter->GetTileRange(at);
					typename Unit<T>::range_t valueRange;
					if constexpr (is_bitvalue_v<T>)
					{
						assert(rt_n == 1);
//						valueRange = Unit<T>::range_t(0, 1 << nrbits_of<T>);
					}
					else
					{
						if (rt_n > 1)
							valueRange = Unit<T>::range_t(arg1Data.begin(), arg1Data.end(), dcm != DCM_None, true);
					}
					DataCheckMode dcm2 = IsIncluding(arg1TileRange, entityRange) ? dcm: DCM_CheckRange;

					typename DataArray<E>::locked_seq_t resSubArray;
					if (resSubData)
					{
						resSubArray = resSubData->GetDataWrite(at, dms_rw_mode::write_only_all);
						if (rt_n > 1) // init separately (and not inline) only when multiple result tiles will be run with this resSubArray and it will be unknonw which is the first run, if any.
							fast_fill(resSubArray.begin(), resSubArray.end(), UNDEFINED_OR_ZERO(E));
					}

					//				for (tile_id rt = 0; rt!=rt_n; ++rt)  // iterate over tiles of V
					parallel_tileloop(rt_n, [arg1A, rt_n, &valueRange, &resultObj, &resSubArray, &arg1Data, arg1TileRange, dcm2, at](tile_id rt)->void
						{
							auto resultTileRange = const_array_cast<T>(arg1A)->GetValueRangeData()->GetTileRange(rt);
							auto resultDataPerV = resultObj->GetWritableTile(rt, at == 0 ? dms_rw_mode::write_only_all : dms_rw_mode::read_write);
							if (at == 0)
								fast_fill(resultDataPerV.begin(), resultDataPerV.end(), UNDEFINED_OR_ZERO(E));
							if (rt_n <= 1 || IsTouching(valueRange, resultTileRange))
								invertAll2values_array<T, E>(resultDataPerV
									, resSubArray ? &resSubArray : nullptr
									, arg1Data
									, resultTileRange
									, arg1TileRange
									, dcm2
									, (rt_n <= 1) // init resSubArray if this is the only run
								);
						}
					);
				}
			}
		);
	}
};

// *****************************************************************************
//                               INSTANTIATION
// *****************************************************************************

	template <typename T>
	struct InvertOperators
	{
		InvertOperator<T> i1, i2;
		InvertOperators()
			:	i1(true)
			,	i2(false)
		{}

	};

	tl_oper::inst_tuple<typelists::domain_elements, InvertOperators<_> > invertOperators;

} // end anonymous namespace
