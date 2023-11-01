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

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "OperUnit.h"

#include "geo/Conversions.h"
#include "mci/CompositeCast.h"
#include "set/VectorFunc.h"
#include "utl/Environment.h"

#include "stg/AbstrStorageManager.h"
#include "DataItemClass.h"
#include "ParallelTiles.h"
#include "Param.h"
#include "Unit.h"
#include "UnitClass.h"

#include <functional>
#include <iterator>
#include <algorithm>

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_real.hpp>
#include <boost/random/uniform_01.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/random/uniform_smallint.hpp>

// *****************************************************************************
//											CLASSES
// *****************************************************************************

template <typename T>
struct uniform_engine_real
{
	uniform_engine_real(boost::mt19937::result_type seed, T min, T max)
		:	m_Engine(boost::mt19937(seed) )
		,	m_Distr(min, max)
	{}

//	typedef boost::uniform_01< boost::mt19937, T> engine_type;
	typedef boost::mt19937 engine_type;
	

	engine_type            m_Engine;
	boost::uniform_real<T> m_Distr;
};


template <typename T>
struct uniform_engine_int
{
	uniform_engine_int(boost::mt19937::result_type seed, T min, T max)
		:	m_Engine(seed)
		,	m_Distr(min, max-1)
	{}


//	typedef boost::uniform_int<T> distribution_type;
//	typedef boost::uniform_smallint< boost::mt19937, T> type;

	boost::mt19937        m_Engine;
	boost::uniform_int<T> m_Distr;
};


template <typename T>
struct uniform_engine_int_f
{
	typedef uniform_engine_int<T> type;
};

template <typename T>
struct uniform_engine_real_f
{
	typedef uniform_engine_real<T> type;
};

/* 
template <>
struct uniform_engine_int<UInt8>
{
	typedef boost::uniform_smallint< UInt8> distribution_type;
//	typedef boost::uniform_smallint< boost::mt19937, T> type;
};
*/

template <typename T>
struct uniform_engine
{
	typedef boost::mpl::eval_if_c<std::numeric_limits<T>::is_integer
	,	uniform_engine_int_f<T>
	,	uniform_engine_real_f<T>
	>
	base_engine_f;

	typename base_engine_f::type m_Base;

	uniform_engine(UInt32 seed, T min, T max)
		:	m_Base(seed, min, max)
	{}

	T operator () ()
	{
		return m_Base.m_Distr(m_Base.m_Engine);
	}
};

namespace {
	CommonOperGroup cogRndUniform("rnd_uniform");
}

// *****************************************************************************
//											AbstrRndUniformOperator
// *****************************************************************************

class AbstrRndUniformOperator : public TernaryOperator
{
	typedef DataArray<UInt32> Arg1Type; // Random Seed
	typedef AbstrUnit         Arg2Type; // domain of result
//	typedef Unit<T>           Arg3Type; // values of result

public:
	AbstrRndUniformOperator(const Class* resultType, const Class* valuesUnitType)
		:	TernaryOperator(&cogRndUniform
			,	resultType
			,	Arg1Type::GetStaticClass()
			,	Arg2Type::GetStaticClass()
			,	valuesUnitType
			)
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 3);

		const AbstrUnit* arg2 = AsUnit(args[1]);
		dms_assert(arg2);

		const AbstrUnit* arg3 = AsUnit(args[2]);
		dms_assert(arg3);

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(arg2, arg3);

		if (mustCalc)
		{
			boost::mt19937::result_type seed = GetTheCurrValue<UInt32>(args[0]) + 1; // we don't want zero seed

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res);

			Calculate(resLock, seed, arg2->GetNrTiles(), arg3);

			resLock.Commit();
		}
		return true;
	}
	virtual void Calculate(DataWriteLock& res, boost::mt19937::result_type seed, tile_id te, const AbstrUnit* arg3) const =0;
};

template <typename T>
typename std::enable_if<is_integral_v<T>, bool>::type
FloatRangeOK(const Range<T>& r)
{
	return true;
}

template <typename T>
typename std::enable_if<!is_integral_v<T>, bool>::type
FloatRangeOK(const Range<T>& r)
{
	return r.first > MIN_VALUE(T) && r.second < MAX_VALUE(T) && r.first < r.second;
}

template <class T>
class RndUniformOperator : public AbstrRndUniformOperator
{
	typedef DataArray<T>      ResultType;
	typedef Unit<T>           Arg3Type; // values of result

public:
	RndUniformOperator()
		:	AbstrRndUniformOperator(
				ResultType::GetStaticClass()
			,	Arg3Type::GetStaticClass()
			)
	{}

	void Calculate(DataWriteLock& res, boost::mt19937::result_type seed, tile_id te, const AbstrUnit* arg3) const override
	{
		Range<T> range = const_unit_cast<T>(arg3)->GetRange();
		if (!(range.first < range.second) || !FloatRangeOK(range))
			throwErrorD("rnd_uniform", "The values unit provided as 3rd argument has an invalid range, use the range function to define a values unit with a valid range");

		ResultType* result = mutable_array_cast<T>(res);

		typedef uniform_engine<T> uniform_engine_t;
		uniform_engine_t rndEngine(seed, range.first, range.second);

		for (tile_id t=0; t!=te; ++t)
		{
			auto resultData = result->GetWritableTile(t);
			auto
				rb = resultData.begin(),
				re = resultData.end();

			while (rb != re)
				Assign(*rb++, rndEngine() );
		}
	}
};

// *****************************************************************************
//											AbstrRndPermutationOperator
// *****************************************************************************

namespace {
	CommonOperGroup cogRndPermutation("rnd_permutation");
}

class AbstrRndPermutationOperator : public BinaryOperator
{
	typedef DataArray<UInt32> Arg1Type; // Random Seed

public:
	AbstrRndPermutationOperator(const Class* resultType, const Class* resDomainCls)
		:	BinaryOperator(&cogRndPermutation
			,	resultType
			,	Arg1Type::GetStaticClass()
			,	resDomainCls
			)
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 2);

		const AbstrUnit* arg2 = AsUnit(args[1]);
		dms_assert(arg2);

		MG_CHECK(const_unit_dynacast<Void>(AsDataItem(args[0])->GetAbstrDomainUnit()));

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(arg2, arg2);

		if (mustCalc)
		{
			boost::mt19937::result_type seed = GetTheCurrValue<UInt32>(args[0]) + 1; // we don't want zero seed

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res);

			Calculate(resLock, seed, arg2);

			resLock.Commit();
		}
		return true;
	}
	virtual void Calculate(DataWriteLock& res, boost::mt19937::result_type seed, const AbstrUnit* arg2) const =0;
};

typedef boost::mt19937 uniform_engine_t;

SizeT selectRnd(uniform_engine_t& engine, SizeT first, SizeT last)
{
	return boost::uniform_smallint<SizeT>(first, last)(engine); // REMOVE TODO COMMENT: this is biased a bit to first elems, which can be filtered out later
}

template <class E>
class RndPermutationOperator : public AbstrRndPermutationOperator
{
	typedef DataArray<E> ResultType;
	typedef Unit<E>      Arg2Type;
public:
	RndPermutationOperator()
		:	AbstrRndPermutationOperator( ResultType::GetStaticClass(), Arg2Type::GetStaticClass() )
	{}

	void Calculate(DataWriteLock& res, boost::mt19937::result_type seed, const AbstrUnit* domainA) const override
	{
		ResultType* result = mutable_array_cast<E>(res);
		const Unit<E>* domain = const_unit_cast<E>(domainA);

		Range<E> rangeT = domain->GetRange();
		SizeT elem_index = Cardinality(rangeT), nrTminus1 = elem_index - 1;

		uniform_engine_t rndEngine(seed);

		auto resultData = result->GetDataWrite(no_tile, dms_rw_mode::write_only_all);

		while (elem_index--)
		{
			SizeT rndElem = selectRnd(rndEngine, elem_index, nrTminus1);
			dms_assert(rndElem >= elem_index);
			dms_assert(rndElem <= nrTminus1);

			if (elem_index < rndElem) // avoid triggering reading uninitialized memory
				resultData[elem_index] = resultData[rndElem]; // could be the default initialized if i==rndElem
			resultData[rndElem] = Range_GetValue_naked(rangeT, elem_index);
		}
	}
};


// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

namespace
{
	RndUniformOperator<Float32> rndUniF32;
	RndUniformOperator<Float64> rndUniF64;
	RndUniformOperator<UInt32>  rndUniU32;
	RndUniformOperator<UInt8>   rndUniU8;

	tl_oper::inst_tuple_templ<typelists::domain_int_objects, RndPermutationOperator > rndPermOperators;
}
