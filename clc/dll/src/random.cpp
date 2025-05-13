// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

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

#include <random>

#include <boost/random/uniform_real.hpp>
#include <boost/random/uniform_01.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/random/uniform_smallint.hpp>

// *****************************************************************************
//											CLASSES
// *****************************************************************************

using uniform_engine_t = std::mt19937; //uniform_engine_t
using rnd_seed_t = UInt32;
using uniform_result_t = uniform_engine_t::result_type;

template <typename T>
struct uniform_engine_real
{
	uniform_engine_real(rnd_seed_t seed, T min, T max)
		:	m_Engine( seed )
		,	m_Distr(min, max)
	{}

	uniform_engine_t       m_Engine;
	boost::uniform_real<T> m_Distr;
};


template <typename T>
struct uniform_engine_int
{
	uniform_engine_int(rnd_seed_t seed, T min, T max)
		:	m_Engine(seed)
		,	m_Distr(min, max-1)
	{}


	uniform_engine_t      m_Engine;
	boost::uniform_int<T> m_Distr;
};


template <typename T>
struct uniform_engine_int_f
{
	using type = uniform_engine_int<T>;
};

template <typename T>
struct uniform_engine_real_f
{
	using  type = uniform_engine_real<T>;
};

/* 
template <>
struct uniform_engine_int<UInt8>
{
	typedef boost::uniform_smallint< UInt8> distribution_type;
//	typedef boost::uniform_smallint< uniform_engine_t, T> type;
};
*/

template <typename T>
struct uniform_engine
{
	using base_engine_f = std::conditional_t<std::is_integral<T>::value, uniform_engine_int_f<T>, uniform_engine_real_f<T> >;
	using base_engine = typename base_engine_f::type;

	base_engine m_Base;

	uniform_engine(rnd_seed_t seed, T min, T max)
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
	typedef DataArray<rnd_seed_t> Arg1Type; // Random Seed
	typedef AbstrUnit             Arg2Type; // domain of result
//	typedef Unit<T>               Arg3Type; // values of result

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
		assert(args.size() == 3);

		const AbstrUnit* arg2 = AsUnit(args[1]); assert(arg2);
		const AbstrUnit* arg3 = AsUnit(args[2]); assert(arg3);

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(arg2, arg3);

		if (mustCalc)
		{
			uniform_engine_t::result_type seed = GetTheCurrValue<rnd_seed_t>(args[0]) + 1; // we don't want zero seed

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res);

			Calculate(resLock, seed, arg2->GetNrTiles(), arg3);

			resLock.Commit();
		}
		return true;
	}
	virtual void Calculate(DataWriteLock& res, uniform_engine_t::result_type seed, tile_id te, const AbstrUnit* arg3) const =0;
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

	void Calculate(DataWriteLock& res, uniform_engine_t::result_type seed, tile_id te, const AbstrUnit* arg3) const override
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
//											RndUniformOperator
// *****************************************************************************

#include "UnitProcessor.h"

template <typename V>
class SeededRndUniformOperator : public BinaryOperator
{
	using ResultType = DataArray<V>;        // result type

	using Arg1Type = DataArray<rnd_seed_t>; // Random Seeds with domain
	using Arg2Type = Unit<V>;               // values of result
public:
	SeededRndUniformOperator()
		: BinaryOperator(&cogRndUniform, ResultType::GetStaticClass(), Arg1Type::GetStaticClass(), Arg2Type::GetStaticClass())
	{}

	using rng_engine = std::mt19937;
	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 2);

		auto seedAttr = AsDataItem(args[0]);             assert(seedAttr);
		auto domain   = seedAttr->GetAbstrDomainUnit();  assert(domain);

		auto values = debug_cast<const Unit<V>*>(args[1]);
		assert(values);

		const AbstrUnit* arg3 = AsUnit(args[2]);
		dms_assert(arg3);

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(domain, values);

		if (mustCalc)
		{
			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataReadLock seeedLock(seedAttr);
			auto seedTileArray = const_array_cast<rnd_seed_t>(seedAttr);
			DataWriteLock resLock(res);
			auto resTileArray = mutable_array_cast<V>(resLock);

			auto lowerBound = values->GetRange().first;
			auto upperBound = values->GetRange().second;
			for (tile_id t = 0; t != domain->GetNrTiles(); ++t)
			{
				auto seedData = seedTileArray->GetTile(t);
				auto resData = resTileArray->GetWritableTile(t, dms_rw_mode::write_only_all);
				auto resIter = resData.begin();
				for (auto seed : seedData)
				{
					auto newEngine = uniform_engine<V>(seed, lowerBound, upperBound);
					*resIter++ = newEngine();
				}
				assert(resIter - resData.begin() == seedData.size());
			}

			resLock.Commit();
		}
		return true;
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
	using Arg1Type =DataArray<rnd_seed_t>; // Random Seed

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
			uniform_engine_t::result_type seed = GetTheCurrValue<rnd_seed_t>(args[0]) + 1; // we don't want zero seed

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res);

			Calculate(resLock, seed, arg2);

			resLock.Commit();
		}
		return true;
	}
	virtual void Calculate(DataWriteLock& res, uniform_engine_t::result_type seed, const AbstrUnit* arg2) const =0;
};

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

	void Calculate(DataWriteLock& res, uniform_engine_t::result_type seed, const AbstrUnit* domainA) const override
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
			assert(rndElem >= elem_index);
			assert(rndElem <= nrTminus1);

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
	tl_oper::inst_tuple_templ <typelists::num_objects, SeededRndUniformOperator> rndUniS32;
	RndUniformOperator<Float32> rndUniF32;
	RndUniformOperator<Float64> rndUniF64;
	RndUniformOperator<UInt32>  rndUniU32;
	RndUniformOperator<UInt8>   rndUniU8;

	tl_oper::inst_tuple_templ<typelists::domain_int_objects, RndPermutationOperator > rndPermOperators;
}
