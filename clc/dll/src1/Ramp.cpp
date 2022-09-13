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

#include "DataArray.h"
#include "DataItemClass.h"
#include "Unit.h"
#include "UnitClass.h"

#include "ParallelTiles.h"

// *****************************************************************************
//                         Ramp
// *****************************************************************************

struct AbstrRampOperator : TernaryOperator
{
	// Override Operator
	AbstrRampOperator(AbstrOperGroup* og, const Class* argClass)
		:	TernaryOperator(og, argClass, argClass, argClass, AbstrUnit::GetStaticClass()) 
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 3);

		const AbstrUnit* e = AsUnit(args[2]);
		dms_assert(e);

		const AbstrDataItem* adi1 = AsDataItem(args[0]); dms_assert(adi1);
		const AbstrDataItem* adi2 = AsDataItem(args[1]); dms_assert(adi2);

		const AbstrUnit* v1 = adi1->GetAbstrValuesUnit();
		dms_assert(v1);
		v1->UnifyValues(adi2->GetAbstrValuesUnit());

		MG_CHECK(const_unit_dynacast<Void>(adi1->GetAbstrDomainUnit()));
		MG_CHECK(const_unit_dynacast<Void>(adi2->GetAbstrDomainUnit()));

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(e, v1);
		
		if (mustCalc)
		{
			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res);

			Calculate(resLock, e, adi1, adi2);

			resLock.Commit();
		}
		return true;
	}
	virtual void Calculate(DataWriteLock& res, const AbstrUnit* e, const AbstrDataItem* adi1, const AbstrDataItem* adi2) const =0;
};

template <typename RampFunc, typename ValuesType>
struct RampOperator : AbstrRampOperator
{
	typedef DataArray<ValuesType> ArgType;
	typedef ArgType               ResultType;

	// Override Operator
	RampOperator(AbstrOperGroup* og, bool isClosed)
		:	AbstrRampOperator(og, ArgType::GetStaticClass())
		,	m_IsClosed(isClosed)
	{}

	void Calculate(DataWriteLock& res, const AbstrUnit* e, const AbstrDataItem* adi1, const AbstrDataItem* adi2) const override
	{
		SizeT n = e->GetCount();
		MG_CHECK(n > 1);

		ValuesType firstV = GetTheCurrValue<ValuesType>(adi1);
		ValuesType lastV = GetTheCurrValue<ValuesType>(adi2);

		auto resData = mutable_array_cast<ValuesType>(res)->GetLockedDataWrite();

		auto resultIter = resData.begin();

		dms_assert((resData.end() -resultIter) == n);

		RampFunc rf(m_IsClosed ? n-1 : n, firstV, lastV);
		for (UInt32 i=0; i != n; ++i, ++resultIter)
			*resultIter = rf(i);
	}
	bool m_IsClosed;
};

struct RampLinearFunc
{
	RampLinearFunc(SizeT n, Float64 firstV, Float64 lastV)
		:	m_N(n)
		,	m_FirstV(firstV)
		,	m_LastV(lastV)
	{
		dms_assert(n>1);

		m_FirstV /= m_N;
		m_LastV  /= m_N;
	}
	Float64 operator() (SizeT i) const
	{
		dms_assert( i <= m_N );
		return m_LastV * i + m_FirstV * (m_N-i);
	}
	SizeT   m_N;
	Float64 m_FirstV, m_LastV;
};

#include "geo/Color.h"

struct RampRgbFunc
{
	RampRgbFunc(SizeT n, DmsColor firstV, DmsColor lastV)
		:	m_N(n)
		,	m_FirstV(firstV)
		,	m_LastV(lastV)
	{

	}

	DmsColor operator() (SizeT i) const
	{
		dms_assert( i <= m_N);
		return
			CombineRGB(
				(GetRed  (m_FirstV)*(m_N-i) + GetRed  (m_LastV)*i) / m_N
			,	(GetGreen(m_FirstV)*(m_N-i) + GetGreen(m_LastV)*i) / m_N
			,	(GetBlue (m_FirstV)*(m_N-i) + GetBlue (m_LastV)*i) / m_N
			,	(GetTrans(m_FirstV)*(m_N-i) + GetTrans(m_LastV)*i) / m_N
			);
	}
	SizeT   m_N;
	DmsColor m_FirstV, m_LastV;
};

#include "RtcTypeLists.h"
#include "utl/TypeListOper.h"

// *****************************************************************************
//                               INSTANTIATION
// *****************************************************************************

namespace 
{
	CommonOperGroup cog_ramp   ("ramp");
	CommonOperGroup cog_rampRgb("ramp_rgb");

	CommonOperGroup cog_rampOpen   ("ramp_open");
	CommonOperGroup cog_rampOpenRgb("ramp_open_rgb");

	template <typename Numeric>
	struct RampLinearOperator 
	{
		RampLinearOperator()
			: m_Ramp(&cog_ramp, true)
			, m_RampOpen(&cog_rampOpen, false)
		{}
		RampOperator<RampLinearFunc, Numeric> m_Ramp, m_RampOpen;
	};

	tl_oper::inst_tuple< typelists::numerics, RampLinearOperator<_> >
		operRampLinearInstances;

	RampOperator<RampRgbFunc, DmsColor> operRampRgb(&cog_rampRgb, true);
	RampOperator<RampRgbFunc, DmsColor> operRampOpenRgb(&cog_rampOpenRgb, false);
} // end anonymous namespace

