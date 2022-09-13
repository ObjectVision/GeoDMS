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
#include "set/DataCompare.h"
#include "utl/TypeListOper.h"
#include "RtcTypeLists.h"

#include "DataArray.h"
#include "DataItemClass.h"
#include "Unit.h"
#include "UnitClass.h"

// *****************************************************************************
//                         Sort
// *****************************************************************************

// REMOVE, TODO: AbstrSortOperator

template <class V>
class SortOperator : public UnaryOperator
{
	typedef DataArray<V> ArgumentType;
	typedef DataArray<V> ResultType;

public:
	// Override Operator
	SortOperator(AbstrOperGroup* gr)
		:	UnaryOperator(gr, ResultType::GetStaticClass(), ArgumentType::GetStaticClass()) 
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 1);

		const AbstrDataItem* adi = debug_cast<const AbstrDataItem*>(args[0]);
		dms_assert(adi);

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(adi->GetAbstrDomainUnit(), adi->GetAbstrValuesUnit(), COMPOSITION(V) );

		if (mustCalc)
		{
			const ArgumentType *  di = const_array_cast<V>(adi);
			dms_assert(di);


			DataReadLock arg1Lock(adi);
			auto unsortedData = di->GetDataRead();

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res);

			ResultType* result = mutable_array_cast<V>(resLock);
			auto resData = result->GetDataWrite();
			dms_assert(resData.size() == unsortedData.size());
			fast_copy(unsortedData.begin(), unsortedData.end(), resData.begin());

			std::sort(resData.begin(), resData.end(), DataCompare<V>());

			resLock.Commit();
		}
		return true;
	}
};


// *****************************************************************************
//                               INSTANTIATION
// *****************************************************************************

namespace 
{

	CommonOperGroup cog_sort("sort");

	tl_oper::inst_tuple<typelists::ranged_unit_objects, SortOperator<_>, AbstrOperGroup* > sortOperators(&cog_sort);
} // end anonymous namespace


