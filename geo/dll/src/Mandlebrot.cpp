// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "mth/Mathlib.h"

#include "Param.h"
#include "UnitClass.h"

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
			DPoint origin   = GetValue<DPoint>(arg3A, 0);
			DPoint increment= GetValue<DPoint>(arg4A, 0);
			SPoint topLeft = rect.first;
			SPoint botRight= rect.second;
			
			origin.Row() += topLeft.Row() * increment.Row();
			origin.Col() += topLeft.Col() * increment.Col();

			dms_assert(topLeft.first  <= botRight.first);
			dms_assert(topLeft.second <= botRight.second);

			DataWriteLock resLock(res);
			auto result = mutable_array_cast<UInt32>(resLock); dms_assert(result);
			auto resultData = result->GetDataWrite();          dms_assert(resultData.size() == size);

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
