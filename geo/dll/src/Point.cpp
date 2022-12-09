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

#include "mci/CompositeCast.h"
#include "mth/MathLib.h"
#include "geo/PointOrder.h"
#include "ser/AsString.h"
#include "ser/RangeStream.h"
#include "set/VectorFunc.h"
#include "utl/Swap.h"

#include "Param.h"
#include "UnitClass.h"
#include "DataItemClass.h"

// *****************************************************************************
//									RELATIONAL FUNCTIONS
// *****************************************************************************

// *****************************************************************************
//									ConvertAttrToPointOperator
// *****************************************************************************

template <class T>
class ConvertAttrToPointOperator : public TernaryOperator
{
	typedef Point<T>                       PointType;
	typedef DataArray<T>                   Arg1Type;	
	typedef DataArray<T>                   Arg2Type;	
	typedef Unit<PointType>	               Arg3Type;
	typedef DataArray<PointType>           ResultType;
			
public:
	ConvertAttrToPointOperator(AbstrOperGroup* gr, UInt32 nrArgs)
		:	TernaryOperator(gr, 
				ResultType::GetStaticClass(), 
				Arg1Type::GetStaticClass(), Arg2Type::GetStaticClass(), Arg3Type::GetStaticClass()
			) 
	{
		if (nrArgs == 2)
			--m_ArgClassesEnd;
	}

	// Override Operator
	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, OperationContext*, LispPtr) const override
	{
		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		const AbstrDataItem* arg2A = AsDataItem(args[1]);
		dms_assert(arg1A);
		dms_assert(arg2A);

		const Arg3Type* arg3 = debug_cast<const Arg3Type*>
			(
			(args.size() > 2)
				? GetItem(args[2])
				: Arg3Type::GetStaticClass()->CreateDefault()
				);
		dms_assert(arg3);

		const AbstrUnit* entity1 = arg1A->GetAbstrDomainUnit();
		const AbstrUnit* entity2 = arg2A->GetAbstrDomainUnit();
		entity1->UnifyDomain(entity2, "e1", "e2", UM_Throw);

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(entity1, arg3);
		dms_assert(resultHolder);
	}

	bool CalcResult(TreeItemDualRef& resultHolder, const ArgRefs& args, OperationContext*, Explain::Context* context = nullptr) const override
	{
		dms_assert(resultHolder);

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		const AbstrDataItem* arg2A = AsDataItem(args[1]);
		dms_assert(arg1A); 
		dms_assert(arg2A); 

		const AbstrUnit* entity1 = arg1A->GetAbstrDomainUnit();

		// GROTE WISSELTRUUK
		if (g_cfgColFirst != dms_order_tag::col_first)
		{
			omni::swap(arg1A, arg2A);
		}

		DataReadLock arg1Lock(arg1A);
		DataReadLock arg2Lock(arg2A);

		TreeItem* res = resultHolder;
		DataWriteLock resLock(AsDataItem(res));

		for (tile_id t = 0, tn = entity1->GetNrTiles(); t!=tn; ++t)
		{
			auto arg1Data = const_array_cast<T>(arg1A)->GetLockedDataRead(t);
			auto arg2Data = const_array_cast<T>(arg2A)->GetLockedDataRead(t);

			auto resData = mutable_array_cast<PointType>(resLock)->GetWritableTile(t);

			auto cardinality = entity1->GetTileCount(t);

			dms_assert(arg1Data.size() == cardinality);
			dms_assert(arg2Data.size() == cardinality);
			dms_assert(resData.size() == cardinality);

			auto
				b1 = arg1Data.begin(),
				e1 = arg1Data.end();
			auto b2 = arg2Data.begin();
			auto br = resData.begin();


			if	(	arg1A->HasUndefinedValues()
				||	arg2A->HasUndefinedValues())
			{
				for ( ; b1 != e1; ++br, ++b1, ++b2)
					if (!IsDefined(*b1) || !IsDefined(*b2))
						MakeUndefined(*br);
					else
						*br = PointType(*b1, *b2);
			}
			else
				for ( ; b1 != e1; ++br, ++b1, ++b2)
				{
					dms_assert(IsDefined(*b1) && IsDefined(*b2));
					*br = PointType(*b1, *b2);
				}
		}
		resLock.Commit();

		return true;
	}
};

// *****************************************************************************
//									Distance. 
// *****************************************************************************
// Distance. 
//
// The function distance : 
//		POINTS X RECTSET -> S<POINTS, n> X V<n, R> 
// is defined by distance(p, set)[i] = ((set[i], i), 
// distance(p, set[i])), 1 <= i <= | set |.
//#include "Param.h"
#include "DataArray.h"

using square_dist_type = UInt32;

// TODO G8: generalize DistOper for domains with other Point types (IPoint, UPoint, WPoint) and resulting square_dist_types

class Dist2Operator : public BinaryOperator
{
	typedef DataArray<SPoint>           Arg1Type; // domain of dist matrix
	typedef Unit<square_dist_type>      Arg2Type; // resulting values unit for resulting dist2 attr of newly created entity
	typedef DataArray<square_dist_type> ResultType;
			
public:
	Dist2Operator(AbstrOperGroup* gr)
		:	BinaryOperator(gr, ResultType::GetStaticClass(), 
				Arg1Type::GetStaticClass(), Arg2Type::GetStaticClass()
			)
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 2);

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		const Arg2Type* arg2 = const_unit_cast<square_dist_type>(args[1]);
		dms_assert(arg1A);
		dms_assert(arg2);

		checked_domain<Void>(arg1A, "a1");
		const Unit<SPoint>* domain = const_unit_cast<SPoint>(arg1A->GetAbstrValuesUnit());
		dms_assert(domain);

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(domain, arg2);

		if (mustCalc)
		{
			const Arg1Type* arg1 = const_array_cast<SPoint>(arg1A);
			dms_assert(arg1);

			SPoint mid     = GetCurrValue<SPoint>(arg1A, 0);
			SRect  rect    = domain->GetRange();
			UInt32 size    = Cardinality(Size(rect));
			UInt32 nrCol   = Size(rect).Col();

//			SPoint topLeft = rect.first;
//			SPoint botRight= rect.second;
			
			dms_assert(rect.first.first  <= rect.second.first);
			dms_assert(rect.first.second <= rect.second.second);

			TreeItem* res = resultHolder;
			DataWriteLock resLock(debug_cast<AbstrDataItem*>(res));
			auto resultData =  mutable_array_cast<UInt32>(resLock)->GetDataWrite();
			dms_assert(resultData.size() == size);

			ResultType::iterator dai = resultData.begin();
			for (Int16 row=rect.first.Row();  row!=rect.second.Row();  ++row)
			{
				UInt32 sumSqr = sqr(Int32(mid.Row()-row));
				Int32  colV   =  rect.first.Col()- mid.Col();
				sumSqr += sqr(colV);
				for (ResultType::iterator dae = dai + nrCol; dai != dae; ++dai)
				{
					*dai = sumSqr;
					sumSqr += colV; ++colV; sumSqr += colV;
				}
			}
			resLock.Commit();
		}
		return true;
	}
};

// *****************************************************************************
//											FUNCTORS
// *****************************************************************************

#include "Prototypes.h"
#include "UnitCreators.h"

template <typename Scalar>
struct point2rowFunc : unary_func<Scalar, Point<Scalar> >
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return default_unit_creator<Scalar>(); }

	typename point2rowFunc::res_type operator()(typename point2rowFunc::arg1_cref x) const
	{
		return x.Row();
	}
};

template <typename Scalar>
struct point2colFunc : unary_func<Scalar, Point<Scalar> >
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return default_unit_creator<Scalar>(); }

	typename point2colFunc::res_type operator()(typename point2colFunc::arg1_cref x) const
	{
		return x.Col();
	}
};


// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************


#include "RtcTypeLists.h"
#include "utl/TypeListOper.h"

#include "LispTreeType.h"

#include "OperAttrUni.h"

namespace 
{
	CommonOperGroup cog_Point(token::point), cog_PointRow("pointrow"), cog_PointCol("pointcol");

	template <typename P>
	struct PointOpers
	{
		typedef typename scalar_of<P>::type S;

		PointOpers()
			: ca2Point(&cog_Point, 2)
			, ca3Point(&cog_Point, 3)
			, ca2Row(&cog_PointRow)
			, ca2Col(&cog_PointCol)
		{}

		ConvertAttrToPointOperator<S> ca2Point, ca3Point;

		UnaryAttrSpecialFuncOperator<point2rowFunc<S> > ca2Row;
		UnaryAttrSpecialFuncOperator<point2colFunc<S> > ca2Col;
	};
	//	oper_arg_policy oap_point[3] = { oper_arg_policy::calc_as_result, oper_arg_policy::calc_as_result, oper_arg_policy::calc_never };
	tl_oper::inst_tuple<typelists::points, PointOpers<_> > pointOpers;

	CommonOperGroup dist2("dist2");       
	Dist2Operator dist2Oper(&dist2);
}
