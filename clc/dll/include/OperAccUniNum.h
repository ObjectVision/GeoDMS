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

#pragma once

#if !defined(__CLC_OPERACCUNINUM_H)
#define __CLC_OPERACCUNINUM_H

#include "set/VectorFunc.h"
#include "mci/CompositeCast.h"
#include "geo/Conversions.h"
#include "geo/Point.h"

#include "Param.h"
#include "DataItemClass.h"

#include "AggrUniStructNum.h"
#include "OperAccUni.h"
#include "OperRelUni.h"

// *****************************************************************************
//											CLASSES
// *****************************************************************************

template <class TAcc1Func> 
struct OperAccTotUniNum : OperAccTotUni<TAcc1Func>
{
	OperAccTotUniNum(AbstrOperGroup* gr, const TAcc1Func& acc1Func = TAcc1Func()) 
		: OperAccTotUni<TAcc1Func>(gr, acc1Func)
	{}

	// Override Operator
	void Calculate(DataWriteLock& res, const AbstrDataItem* arg1A) const override
	{
		auto arg1 = const_array_cast<typename OperAccTotUniNum::ValueType>(arg1A);
		dms_assert(arg1);

		auto result = mutable_array_cast<typename OperAccTotUniNum::ResultValueType>(res);
		dms_assert(result);

		typename TAcc1Func::assignee_type value;
		this->m_Acc1Func.Init(value);

		const AbstrUnit* e = arg1A->GetAbstrDomainUnit();
		// TODO G8: OPTIMIZE, use parallel_for and ThreadLocal container and aggregate afterwards.
		for (tile_id t = 0, te = e->GetNrTiles(); t!=te; ++t)
		{
			this->m_Acc1Func(
				value, 
				arg1->GetTile(t).get_view(),
				arg1A->HasUndefinedValues()
			);
		}

		auto resData = result->GetDataWrite();
		dms_assert(resData.size() == 1);
		this->m_Acc1Func.AssignOutput(resData[0], value );
	}
};


// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

#include "RtcTypeLists.h"
#include "utl/TypeListOper.h"

namespace OperAccUniNum
{
	template<
		template <typename> class TotlOper, 
		template <typename> class PartOper, 
		typename ValueTypes
	>
	struct AggrOperators
	{
		AggrOperators(AbstrOperGroup* gr) 
			: m_TotlAggrOpers(gr)
			, m_PartAggrOpers(gr)
		{}

		template <typename T> struct OperAccTotUniNumOper : OperAccTotUniNum   <TotlOper<T>> {
			using OperAccTotUniNum<TotlOper<T>>::OperAccTotUniNum; 
		};
		template <typename T> struct OperAccPartUniNumOper : OperAccPartUniBest<PartOper<T>> {
			using OperAccPartUniBest<PartOper<T>>::OperAccPartUniBest;
		};

	private:
		tl_oper::inst_tuple<ValueTypes, OperAccTotUniNumOper <_>, AbstrOperGroup*> m_TotlAggrOpers;
		tl_oper::inst_tuple<ValueTypes, OperAccPartUniNumOper<_>, AbstrOperGroup*> m_PartAggrOpers;
	};
}

#endif //!defined(__CLC_OPERACCUNINUM_H)
