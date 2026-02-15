// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__CLC_PARTITIONTYPES_H)
#define __CLC_PARTITIONTYPES_H

#include "geo/RangeIndex.h"

// *****************************************************************************
//                      partition functions
//
// TODO: Don't instantiate unsigned versions for null_checker<range_checker<>>
// *****************************************************************************

template <typename P>
struct ord_partition
{
	using Range = Range<P>;

	using index_value = P;
	using index_ref = typename param_type<P>::type;

	ord_partition(const Range& indexValueRange): m_Domain(indexValueRange) {}

	P GetIndexBase() const { return m_Domain.first; }
	P GetIndex(index_ref p) const { return p; }

	Range m_Domain;
};

template <typename P>
struct crd_partition
{
	using Range = Range<P>;
	typedef DiffT ordinal_t;

	crd_partition(const Range& indexValueRange): m_Domain(indexValueRange) {}

	ordinal_t GetIndexBase() const             { return ordinal_t(m_Domain.first.Row()) * Width(m_Domain) + m_Domain.first.Col(); }
	ordinal_t GetIndex(typename Range::value_cref point) const { return ordinal_t(point.Row()) * Width(m_Domain) + point.Col(); }

	Range m_Domain;
};

// ***************************************************************************** checker adapters of partitioners

template <typename Base>
struct naked_checker: Base
{
	naked_checker(const typename Base::Range& indexValueRange): Base(indexValueRange) {}

	bool Check(typename Base::Range::value_cref) const { return true; }
};

template <typename Base>
struct range_checker: Base
{
	using value_type = typename Base::Range::value_type;

	range_checker(const typename Base::Range& indexValueRange): Base(indexValueRange) {}

	bool Check(typename Base::Range::value_cref p) const { return IsIncluding(this->m_Domain, p); }
};

template <typename Base>
struct null_checker: Base
{
	null_checker(const typename Base::Range& indexValueRange): Base(indexValueRange)
	{
		dms_assert(Base::Check(UNDEFINED_VALUE(typename Base::Range::value_type))); // Optimization requirement (PRECONDITION responsibility of creator ) that this Check is more strict than Base::Check (else Base could have been used).
	}

	bool Check(typename Base::Range::value_cref p) const { return IsDefined(p) && Base::Check(p); }
};

template <typename P>
struct partition_types
{
	using type = typename std::conditional<is_integral<P>::value, ord_partition<P>, crd_partition<P> >::type;

	typedef               naked_checker<type>   naked_checker_t;
	typedef               range_checker<type>   range_checker_t;
	typedef null_checker< naked_checker<type> > null_checker_t;
	typedef null_checker< range_checker<type> > both_checker_t;
};

#endif // !defined(__CLC_PARTITIONTYPES_H)
