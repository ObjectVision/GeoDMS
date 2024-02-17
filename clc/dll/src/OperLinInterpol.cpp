// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "dbg/debug.h"
#include "geo/Pair.h"
#include "mci/CompositeCast.h"
#include "ptr/OwningPtr.h"
#include "set/CompareFirst.h"

#include "DataItemClass.h"
#include "DataArray.h"
#include "AbstrDataItem.h"
#include "UnitClass.h"

#include "OperRelUni.h"


namespace 
{

template <typename T, typename V>
void DoInterpolateLinear(
	DataArray<V>*       resultObj, 
	const DataArray<T>* dataObj, 
	const DataArray<T>* xCoordObj, 
	const DataArray<V>* yCoordObj)
{
	dms_assert(resultObj);
	dms_assert(dataObj);
	dms_assert(xCoordObj);
	dms_assert(yCoordObj);

	auto data    = dataObj  ->GetDataRead();
	auto xCoords = xCoordObj->GetDataRead();
	auto yCoords = yCoordObj->GetDataRead();
	auto resData = resultObj->GetDataWrite();

	dms_assert(   data.size() == resData.size());
	dms_assert(xCoords.size() == yCoords.size());

	typedef DataArray<T>::const_iterator iter_x_t;
	typedef DataArray<V>::iterator       iter_r_t;
	typedef Pair<T, V>                   chart_elem;
	typedef std::vector<chart_elem>      chart_t;
	typedef chart_t::iterator            chart_iter_t;

	iter_r_t
		resultI = resData.begin(),
		resultE = resData.end();

	if (!xCoords.size())
	{
		fast_undefine(resultI, resultE);
		return;
	}

	iter_x_t xi = xCoords.begin();
	SizeT    n  = xCoords.size();

	OwningPtrSizedArray<SizeT> index(n, dont_initialize MG_DEBUG_ALLOCATOR_SRC( "DoInterpolateLinear: index"));
	auto indexBegin = index.begin(), indexEnd = index.end(); assert(indexEnd - indexBegin == n);
	make_index_in_existing_span(indexBegin, indexEnd, xi);

	chart_t chart;
	chart.reserve(n);

	for(; indexBegin != indexEnd; ++indexBegin)
		chart.push_back(chart_t::value_type(xCoords[*indexBegin], yCoords[*indexBegin]));
	chart_iter_t
		chartB = chart.begin(),
		chartE = chart.end();

	chartE = 
		std::unique(
			chartB,
			chartE,
			[](const chart_elem& lhs, const chart_elem& rhs)->bool { return lhs.first == rhs.first; }
		);
	dms_assert(chartB != chartE);

	iter_x_t dataB = data.begin();
	iter_x_t dataE = data.end();

	comp_first<T, chart_elem> xComparator;

	// OPTIMIZE: reken RC vooraf uit voor alle entries (laatste RC=0 en gooi irrelevante dubbele keys weg) en gebruik upper_bound om chartM te vinden; check of chartE kan dan weg.

	for (; dataB != dataE; ++resultI, ++dataB) 
	{
		T x = *dataB;
		if (!IsDefined(x) )
			*resultI = UNDEFINED_VALUE(V);
		else
		{
			chart_iter_t chartM = std::lower_bound(chartB, chartE, x, xComparator);
			if (chartM == chartB)
				*resultI = chartB->second; // take first y-x
			else
			{
				chart_iter_t chartP = chartM-1;
				if (chartM == chartE)
					*resultI = chartP->second; // take last x
				else
				{
					dms_assert(chartP->first != chartM->first); // result of unique, ifs guarantee that both values are defined.
					if (chartM->first == x)
						*resultI = chartM->second;
					else if (IsDefined(chartP->second) && IsDefined(chartM->second))
					{
						using intermediate_type = std::conditional_t<std::is_integral_v<T>&& std::is_integral_v<V>, SizeT, Float64>;
						intermediate_type dPX = x; dPX -= chartP->first;
						intermediate_type dXM = chartM->first; dXM -= x;
						auto y = (chartP->second * dXM + chartM->second * dPX) / (dPX+dXM);
						*resultI = Convert<V>(y);
					}
					else
						*resultI = UNDEFINED_VALUE(V);
				}
			}
		}
	}
}

CommonOperGroup cog("interpolate_linear", oper_policy::better_not_in_meta_scripting);

class AbstrInterpolateLinearOperator : public TernaryOperator
{
public:
	AbstrInterpolateLinearOperator(const Class* argX_type, const Class* argY_type)
		:	TernaryOperator(&cog,
				argY_type, 
				argX_type, argX_type, argY_type
			)
	{}

	virtual void Calculate(
		AbstrDataObject* resultObj, 
		const AbstrDataObject* dataObj, 
		const AbstrDataObject* xCoordObj, 
		const AbstrDataObject* yCoordObj) const=0;

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 3);

		const AbstrDataItem* arg1A       = debug_cast<const AbstrDataItem*>(args[0]);
		dms_assert(arg1A);

		const AbstrDataItem* xCoordA = debug_cast<const AbstrDataItem*>(args[1]);
		const AbstrDataItem* yCoordA = debug_cast<const AbstrDataItem*>(args[2]);

		const AbstrUnit* dataUnit = arg1A->GetAbstrValuesUnit();
		dataUnit->UnifyValues( xCoordA->GetAbstrValuesUnit(), "v1", "v2", UnifyMode(UM_AllowDefault | UM_Throw));
		xCoordA->GetAbstrDomainUnit()->UnifyDomain(yCoordA->GetAbstrDomainUnit(), "e1", "e2", UM_Throw);

		const AbstrUnit* reclassUnit = yCoordA->GetAbstrValuesUnit();
		const AbstrUnit* domainUnit  = arg1A->GetAbstrDomainUnit();

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(domainUnit, reclassUnit);

		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(xCoordA);
			DataReadLock arg3Lock(yCoordA);

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res);

			Calculate(resLock.get(), arg1A->GetCurrRefObj(), xCoordA->GetCurrRefObj(), yCoordA->GetCurrRefObj());
			resLock.Commit();
		}
		return true;
	}
};

template <typename T, typename U>
class InterpolateLinearOperator : public AbstrInterpolateLinearOperator
{
	typedef DataArray<T>                    DomainType;
	typedef DataArray<U>                    ImageType;
			
public:
	InterpolateLinearOperator()
		:	AbstrInterpolateLinearOperator( 
				DomainType::GetStaticClass(), 
				ImageType::GetStaticClass()
			)
	{}

	void Calculate(AbstrDataObject* resultObj, 
		const AbstrDataObject* dataObj, 
		const AbstrDataObject* xCoordObj, 
		const AbstrDataObject* yCoordObj) const override
	{
		DoInterpolateLinear(
			debug_cast<ImageType*>(resultObj), 
			debug_cast<const DomainType*>(dataObj), 
			debug_cast<const DomainType*>(xCoordObj),
			debug_cast<const ImageType *>(yCoordObj)
		);
	}
};

// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

	#define INSTANTIATE(T) \
		InterpolateLinearOperator<T,UInt32>  interpolOper##T##UInt32 ; \
		InterpolateLinearOperator<T,Int32>   interpolOper##T##Int32  ; \
		InterpolateLinearOperator<T,UInt16>  interpolOper##T##UInt16 ; \
		InterpolateLinearOperator<T,Int16>   interpolOper##T##Int16  ; \
		InterpolateLinearOperator<T,UInt8>   interpolOper##T##UInt8  ; \
		InterpolateLinearOperator<T,Int8>    interpolOper##T##Int8   ; \
		InterpolateLinearOperator<T,Float32> interpolOper##T##Float32; \
		InterpolateLinearOperator<T,Float64> interpolOper##T##Float64;

	INSTANTIATE_NUM_ORG
	// INSTANTIATE_NUM_ELEM
	#undef INSTANTIATE
}
