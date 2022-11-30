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

#include "mci/CompositeCast.h"
#include "ptr/InterestHolders.h"

#include "DataArray.h"
#include "DataItemClass.h"
#include "ParallelTiles.h"
#include "Unit.h"
#include "UnitClass.h"
#include "UnitProcessor.h"
#include "TileFunctorImpl.h"

// *****************************************************************************
//                         UNARY RELATIONAL FUNCTIONS
// *****************************************************************************
// Generate_key(E1->Bool): E1->E2 x (E2) NOT!
// ID(E1)          : E1->E1
// Subset(E1->Bool): E2 x (E2->E1)
// Unique
// Relate

// *****************************************************************************
//                         ID
// *****************************************************************************

CommonOperGroup cog_id("id");

class AbstrIDOperator : public UnaryOperator
{
public:
	// Override Operator
	AbstrIDOperator(const Class* resultClass, const Class* arg1Class)
		: UnaryOperator(&cog_id, resultClass, arg1Class) 
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 1);

		const AbstrUnit* e1 = AsUnit(args[0]);
		dms_assert(e1);

		if (!resultHolder)
		{
			resultHolder = CreateCacheDataItem(e1, e1);
			resultHolder.GetNew()->SetFreeDataState(true); // never cache
			resultHolder->SetTSF(DSF_Categorical);
		}
		if (mustCalc)
		{
			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());

		
			const AbstrIDOperator* idOper = this;
			auto trd = e1->GetTiledRangeData();
			auto lazyFunctorCreator = [idOper, res, trd]<typename V>(const Unit<V>*domainUnit) {

				auto retainedDomainUnit = InterestPtr< SharedPtr<const Unit<V>> >( domainUnit );
				auto lazyTileFunctor = make_unique_LazyTileFunctor<V>(trd, domainUnit->m_RangeDataPtr, domainUnit->GetNrTiles()
				,	[idOper, res, retainedDomainUnit](AbstrDataObject* self, tile_id t) {
						idOper->Calculate(self, retainedDomainUnit.get_ptr(), t); // write into the same tile.
					}
					MG_DEBUG_ALLOCATOR_SRC("res->md_FullName +  := id()")
				);
				res->m_DataObject = lazyTileFunctor.release();
			};

			e1 = AsUnit(e1->GetCurrRangeItem());
			visit<typelists::domain_elements>(e1, std::move(lazyFunctorCreator));
		}
		return true;
	}
	virtual void Calculate(AbstrDataObject* borrowedDataHandle, const AbstrUnit* au, tile_id t) const =0;
};

template <class E1>
class IDOperator : public AbstrIDOperator
{
	typedef Unit<E1>       Arg1Type;
	typedef DataArray<E1>  ResultType;

public:
	// Override Operator
	IDOperator() : AbstrIDOperator(ResultType::GetStaticClass(), Arg1Type::GetStaticClass()) 
	{}

	void Calculate(AbstrDataObject* borrowedDataHandle, const AbstrUnit* au, tile_id t) const override
	{
		const Arg1Type *e1 = debug_cast<const Arg1Type*>(au);
		dms_assert(e1);

		ResultType* result = mutable_array_cast<E1>(borrowedDataHandle);
		dms_assert(result);

		auto resData = result->GetWritableTile(t, dms_rw_mode::write_only_all);
		auto resRange = e1->GetTileRange(t);
		CalcTile(resData, resRange MG_DEBUG_ALLOCATOR_SRC("borrowedDataHandle->md_SrcStr"));
	}

	void CalcTile(sequence_traits<E1>::seq_t resData, Unit<E1>::range_t resRange MG_DEBUG_ALLOCATOR_SRC_ARG) const
	{
		auto
			i = resData.begin(),
			e = resData.end();

		dms_assert((e-i) == Cardinality(resRange));

		for (row_id count = 0; i != e; ++i)
			*i = Range_GetValue_naked(resRange, count++);
	}
};


// *****************************************************************************
//                               INSTANTIATION
// *****************************************************************************

#include "RtcTypeLists.h"
#include "utl/TypeListOper.h"

namespace 
{
	tl_oper::inst_tuple<typelists::domain_elements, IDOperator<_> >
		operInstances;
} // end anonymous namespace


