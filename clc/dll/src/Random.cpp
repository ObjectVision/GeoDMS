// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "OperUnit.h"

#include "vt/Conversions.h"
#include "mci/CompositeCast.h"
#include "set/VectorFunc.h"
#include "utl/Environment.h"

#include "stg/AbstrStorageManager.h"
#include "DataArray.h"
#include "DataItemClass.h"
#include "ParallelTiles.h"
#include "Param.h"
#include "Unit.h"
#include "UnitClass.h"

#include <functional>
#include <iterator>
#include <algorithm>

#include <random>
#include <limits>
#include <type_traits>
#include "DataArrayValue.h"

// *****************************************************************************
//   Local, boost-free reimplementations of the boost::random distributions
//   formerly used here (boost/random/uniform_real, uniform_int,
//   uniform_smallint). The algorithms are reproduced from Boost.Random
//   (Copyright Jens Maurer 2000-2001, Steven Watanabe 2011; Boost Software
//   License 1.0) so that rnd_uniform / rnd_permutation keep producing
//   bit-for-bit identical, cross-platform-reproducible sequences on top of
//   std::mt19937. std::uniform_*_distribution is deliberately NOT used: its
//   output is unspecified and differs across standard-library implementations
//   (MSVC STL vs libstdc++), which would break Windows/Linux reproducibility.
// *****************************************************************************

namespace rnd_detail
{
	// ---- boost/random/detail/signed_unsigned_tools.hpp : subtract (x >= y) ----
	template <class T> std::make_unsigned_t<T> subtract(T x, T y)
	{
		using UT = std::make_unsigned_t<T>;
		if constexpr (std::numeric_limits<T>::is_signed)
		{
			if (y >= 0)   return UT(x) - UT(y);
			if (x >= 0)   return UT(x) + UT(-(y + 1)) + 1;
			return UT(x - y);
		}
		else
			return x - y;
	}

	// ---- boost/random/detail/signed_unsigned_tools.hpp : add (x unsigned) ----
	template <class T1, class T2> T2 add(T1 x, T2 y)
	{
		if constexpr (std::numeric_limits<T2>::is_signed
			&& (std::numeric_limits<T1>::digits >= std::numeric_limits<T2>::digits))
		{
			if (y >= 0) return T2(x) + y;
			if (x > T1(-(y + 1))) return T2(x - T1(-(y + 1)) - 1);
			return T2(x) + y;
		}
		else
			return T2(x) + y;
	}

	// ---- boost/random/uniform_real_distribution.hpp (integral-engine branch) ----
	template <class RealType>
	struct uniform_real
	{
		RealType _min, _max;
		uniform_real(RealType mn, RealType mx) : _min(mn), _max(mx) {}

		template <class Engine>
		RealType operator ()(Engine& eng) const { return generate(eng, _min, _max); }

	private:
		template <class Engine>
		static RealType generate(Engine& eng, RealType min_value, RealType max_value)
		{
			if (max_value / 2 - min_value / 2 > (std::numeric_limits<RealType>::max)() / 2)
				return 2 * generate(eng, RealType(min_value / 2), RealType(max_value / 2));
			using base_result = typename Engine::result_type; // std::mt19937 : integral
			for (;;)
			{
				RealType numerator = static_cast<RealType>(subtract<base_result>(eng(), (eng.min)()));
				RealType divisor = static_cast<RealType>(subtract<base_result>((eng.max)(), (eng.min)())) + 1;
				RealType result = numerator / divisor * (max_value - min_value) + min_value;
				if (result < max_value) return result;
			}
		}
	};

	// ---- boost/random/uniform_int_distribution.hpp (integral-engine branch) ----
	template <class Engine, class T>
	T generate_uniform_int(Engine& eng, T min_value, T max_value)
	{
		using result_type = T;
		using range_type = std::make_unsigned_t<T>;
		using base_result = typename Engine::result_type;
		using base_unsigned = std::make_unsigned_t<base_result>;
		const range_type range = subtract<result_type>(max_value, min_value);
		const base_result bmin = (eng.min)();
		const base_unsigned brange = subtract<base_result>((eng.max)(), (eng.min)());

		if (range == 0)
			return min_value;
		else if (brange == range)
		{
			base_unsigned v = subtract<base_result>(eng(), bmin);
			return add<base_unsigned, result_type>(v, min_value);
		}
		else if (brange < range)
		{
			for (;;)
			{
				range_type limit;
				if (range == (std::numeric_limits<range_type>::max)())
				{
					limit = range / (range_type(brange) + 1);
					if (range % (range_type(brange) + 1) == range_type(brange))
						++limit;
				}
				else
					limit = (range + 1) / (range_type(brange) + 1);

				range_type result = range_type(0);
				range_type mult = range_type(1);
				while (mult <= limit)
				{
					result += static_cast<range_type>(static_cast<range_type>(subtract<base_result>(eng(), bmin)) * mult);
					if (mult * range_type(brange) == range - mult + 1)
						return static_cast<result_type>(result);
					mult *= range_type(brange) + range_type(1);
				}
				range_type result_increment = generate_uniform_int(eng, static_cast<range_type>(0), static_cast<range_type>(range / mult));
				if (std::numeric_limits<range_type>::is_bounded && ((std::numeric_limits<range_type>::max)() / mult < result_increment))
					continue;
				result_increment *= mult;
				result += result_increment;
				if (result < result_increment) continue;
				if (result > range) continue;
				return add<range_type, result_type>(result, min_value);
			}
		}
		else // brange > range
		{
			using mixed_range_type = base_unsigned;
			mixed_range_type bucket_size;
			if (brange == (std::numeric_limits<base_unsigned>::max)())
			{
				bucket_size = static_cast<mixed_range_type>(brange) / (static_cast<mixed_range_type>(range) + 1);
				if (static_cast<mixed_range_type>(brange) % (static_cast<mixed_range_type>(range) + 1) == static_cast<mixed_range_type>(range))
					++bucket_size;
			}
			else
				bucket_size = static_cast<mixed_range_type>(brange + 1) / (static_cast<mixed_range_type>(range) + 1);
			for (;;)
			{
				mixed_range_type result = subtract<base_result>(eng(), bmin);
				result /= bucket_size;
				if (result <= static_cast<mixed_range_type>(range))
					return add<mixed_range_type, result_type>(result, min_value);
			}
		}
	}

	template <class T>
	struct uniform_int
	{
		T _min, _max;
		uniform_int(T mn, T mx) : _min(mn), _max(mx) {}

		template <class Engine>
		T operator ()(Engine& eng) const { return generate_uniform_int(eng, _min, _max); }
	};

	// ---- boost/random/uniform_smallint.hpp (integral-engine branch) ----
	template <class IntType>
	struct uniform_smallint
	{
		IntType _min, _max;
		uniform_smallint(IntType mn, IntType mx) : _min(mn), _max(mx) {}

		template <class Engine>
		IntType operator ()(Engine& eng) const
		{
			using base_result = typename Engine::result_type;
			using base_unsigned = std::make_unsigned_t<base_result>;
			using range_type = std::make_unsigned_t<IntType>;
			using mixed_range_type = base_unsigned;
			range_type range = subtract<IntType>(_max, _min);
			base_unsigned base_range = subtract<base_result>((eng.max)(), (eng.min)());
			base_unsigned val = subtract<base_result>(eng(), (eng.min)());
			if (range >= base_range)
				return add<range_type, IntType>(static_cast<range_type>(val), _min);
			mixed_range_type modulus = static_cast<mixed_range_type>(range) + 1;
			return add<range_type, IntType>(static_cast<mixed_range_type>(val) % modulus, _min);
		}
	};

} // namespace rnd_detail

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

	uniform_engine_real(std::seed_seq& seeds, T min, T max)
		: m_Engine(seeds)
		, m_Distr(min, max)
	{
	}

	uniform_engine_t            m_Engine;
	rnd_detail::uniform_real<T> m_Distr;
};


template <typename T>
struct uniform_engine_int
{
	uniform_engine_int(rnd_seed_t seed, T min, T max)
		:	m_Engine(seed)
		,	m_Distr(min, max-1)
	{}
	uniform_engine_int(std::seed_seq& seeds, T min, T max)
		: m_Engine(seeds)
		, m_Distr(min, max - 1)
	{
	}


	uniform_engine_t           m_Engine;
	rnd_detail::uniform_int<T> m_Distr;
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

template <typename T>
struct uniform_engine
{
	using base_engine_f = std::conditional_t<std::is_integral<T>::value, uniform_engine_int_f<T>, uniform_engine_real_f<T> >;
	using base_engine = typename base_engine_f::type;

	base_engine m_Base;

	uniform_engine(rnd_seed_t seed, T min, T max)
		:	m_Base(seed, min, max)
	{}

	uniform_engine(std::seed_seq& seeds, T min, T max)
		: m_Base(seeds, min, max)
	{
	}
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

template <IntegralValue T>
bool FloatRangeOK(const Range<T>& r)
{
	return true;
}

template <typename T>
	requires (!is_integral_v<T>)
bool FloatRangeOK(const Range<T>& r)
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

		// todo: enable MT3
		parallel_tileloop(te, [result, seed, range](tile_id t)
			{
				using uniform_engine_t = uniform_engine<T> ;
				std::array<UInt32, 2> seedArr = { static_cast<UInt32>(seed), static_cast<UInt32>(t) };
				auto seeds = std::seed_seq(seedArr.begin(), seedArr.end());
				uniform_engine_t rndEngine(seeds, range.first, range.second); // let each tile have its own seed

				auto resultData = result->GetWritableTile(t);
				auto
					rb = resultData.begin(),
					re = resultData.end();

				while (rb != re)
					Assign(*rb++, rndEngine());
			}
		);
	}
};

// *****************************************************************************
//											RndUniformOperator
// *****************************************************************************

#include "UnitProcessor.h"

template <typename V>
class SeededRndUniformOperator : public TernaryOperator
{
	using ResultType = DataArray<V>;        // result type

	using Arg1Type = DataArray<rnd_seed_t>; // Random Seed parameter
	using Arg2Type = DataArray<rnd_seed_t>; // Random Seeds per domain elememt
	using Arg3Type = Unit<V>;               // values of result
public:
	SeededRndUniformOperator()
		: TernaryOperator(&cogRndUniform, ResultType::GetStaticClass(), Arg1Type::GetStaticClass(), Arg2Type::GetStaticClass(), Arg3Type::GetStaticClass())
	{}

	using rng_engine = std::mt19937;
	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 3);

		auto seedParam= AsDataItem(args[0]);             assert(seedParam);
		auto seedAttr = AsDataItem(args[1]);             assert(seedAttr);
		auto domain   = seedAttr->GetAbstrDomainUnit();  assert(domain);

		MG_USERCHECK(seedParam->HasVoidDomainGuarantee() && "The first seed argument must be a parameter or an attribute with void domain.");

		auto values = debug_cast<const Unit<V>*>(args[2]);
		assert(values);

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(domain, values);

		if (mustCalc)
		{
			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			auto seed1 = GetTheCurrValue<rnd_seed_t>(seedParam);

			DataReadLock seeedLock(seedAttr);
			auto seedTileArray = const_array_cast<rnd_seed_t>(seedAttr);
			DataWriteLock resLock(res);
			auto resTileArray = mutable_array_cast<V>(resLock);

			auto range = values->GetRange();

			// todo: enable MT3
			parallel_tileloop(domain->GetNrTiles(), [seed1, seedTileArray, range, resTileArray](tile_id t)
				{
					auto seedData = seedTileArray->GetTile(t);
					auto resData = resTileArray->GetWritableTile(t, dms_rw_mode::write_only_all);
					auto resIter = resData.begin();
					for (auto seed : seedData)
					{
						auto seeds = std::seed_seq{ seed1, seed };
						auto newEngine = uniform_engine<V>(seeds, range.first, range.second);
						*resIter++ = newEngine();
					}
					assert(SizeT(resIter - resData.begin()) == seedData.size());
				}
			);

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
	return rnd_detail::uniform_smallint<SizeT>(first, last)(engine); // NB: biased a bit to first elems, which can be filtered out later
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
