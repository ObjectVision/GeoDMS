// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef __TIC_LISPTREETYPE_H
#define __TIC_LISPTREETYPE_H

#include "LispRef.h"
#include "LispList.h"

struct TreeItem;
#include "geo/SequenceArray.h"
#include "mci/ValueWrap.h"

#if defined(MG_DEBUG)
//#define MG_DEBUG_LISP_TREE
#endif //defined(MG_DEBUG)

TIC_CALL LispRef slSubItemCall(LispPtr baseExpr, CharPtrRange relPath);
TIC_CALL LispRef slConvertedLispExpr(LispPtr result, LispPtr vu);

namespace token {
	extern TIC_CALL TokenID add;
	extern TIC_CALL TokenID sub;
	extern TIC_CALL TokenID mul;
	extern TIC_CALL TokenID div;
	extern TIC_CALL TokenID mod;

	extern TIC_CALL TokenID neg;

	extern TIC_CALL TokenID eq;
	extern TIC_CALL TokenID ne;
	extern TIC_CALL TokenID lt;
	extern TIC_CALL TokenID le;
	extern TIC_CALL TokenID gt;
	extern TIC_CALL TokenID ge;

	extern TIC_CALL TokenID id;

	extern TIC_CALL TokenID and_;
	extern TIC_CALL TokenID or_;
	extern TIC_CALL TokenID not_;

	extern TIC_CALL TokenID iif;

	extern TIC_CALL TokenID true_;
	extern TIC_CALL TokenID false_;
	extern TIC_CALL TokenID pi;

	extern TIC_CALL TokenID const_;
	extern TIC_CALL TokenID null_b;
	extern TIC_CALL TokenID null_w;
	extern TIC_CALL TokenID null_u;
	extern TIC_CALL TokenID null_u64;
	extern TIC_CALL TokenID null_c;
	extern TIC_CALL TokenID null_s;
	extern TIC_CALL TokenID null_i;
	extern TIC_CALL TokenID null_i64;
	extern TIC_CALL TokenID null_f;
	extern TIC_CALL TokenID null_d;
	extern TIC_CALL TokenID null_sp;
	extern TIC_CALL TokenID null_wp;
	extern TIC_CALL TokenID null_ip;
	extern TIC_CALL TokenID null_up;
	extern TIC_CALL TokenID null_fp;
	extern TIC_CALL TokenID null_dp;
	extern TIC_CALL TokenID null_str;

	TIC_CALL bool isConst(TokenID t);

	extern TIC_CALL TokenID arrow;
	extern TIC_CALL TokenID lookup;
	extern TIC_CALL TokenID convert;
	extern TIC_CALL TokenID eval;
	extern TIC_CALL TokenID scope;

	extern TIC_CALL TokenID subitem;
	extern TIC_CALL TokenID NrOfRows;
	extern TIC_CALL TokenID range;
	extern TIC_CALL TokenID cat_range;
	extern TIC_CALL TokenID TiledUnit;
	extern TIC_CALL TokenID point;
	extern TIC_CALL TokenID point_xy;

	extern TIC_CALL TokenID BaseUnit;
	extern TIC_CALL TokenID UInt32;
	extern TIC_CALL TokenID left;
	extern TIC_CALL TokenID right;
	extern TIC_CALL TokenID DomainUnit;
	extern TIC_CALL TokenID ValuesUnit;

	extern TIC_CALL TokenID union_data;
	extern TIC_CALL TokenID sourceDescr;
	extern TIC_CALL TokenID container;
	extern TIC_CALL TokenID classify;

//	SELECT section BEGIN
	extern TIC_CALL TokenID select;
	extern TIC_CALL TokenID select_uint8;
	extern TIC_CALL TokenID select_uint16;
	extern TIC_CALL TokenID select_uint32;
	extern TIC_CALL TokenID select_uint64;

	extern TIC_CALL TokenID select_with_org_rel;
	extern TIC_CALL TokenID select_uint8_with_org_rel;
	extern TIC_CALL TokenID select_uint16_with_org_rel;
	extern TIC_CALL TokenID select_uint32_with_org_rel;
	extern TIC_CALL TokenID select_uint64_with_org_rel;

	// DEPRECIATED BEGIN
	extern TIC_CALL TokenID select_unit;
	extern TIC_CALL TokenID select_orgrel;
	extern TIC_CALL TokenID select_unit_uint8;
	extern TIC_CALL TokenID select_orgrel_uint8;
	extern TIC_CALL TokenID select_unit_uint8_org_rel;
	extern TIC_CALL TokenID select_unit_uint16;
	extern TIC_CALL TokenID select_orgrel_uint16;
	extern TIC_CALL TokenID select_unit_uint32;
	extern TIC_CALL TokenID select_orgrel_uint32;
	// DEPRECIATED END

	extern TIC_CALL TokenID select_with_attr_by_cond;
	extern TIC_CALL TokenID select_uint8_with_attr_by_cond;
	extern TIC_CALL TokenID select_uint16_with_attr_by_cond;
	extern TIC_CALL TokenID select_uint32_with_attr_by_cond;
	extern TIC_CALL TokenID select_uint64_with_attr_by_cond;

	extern TIC_CALL TokenID select_with_org_rel_with_attr_by_cond;
	extern TIC_CALL TokenID select_uint8_with_org_rel_with_attr_by_cond;
	extern TIC_CALL TokenID select_uint16_with_org_rel_with_attr_by_cond;
	extern TIC_CALL TokenID select_uint32_with_org_rel_with_attr_by_cond;
	extern TIC_CALL TokenID select_uint64_with_org_rel_with_attr_by_cond;

	extern TIC_CALL TokenID select_with_attr_by_org_rel;
	extern TIC_CALL TokenID select_uint8_with_attr_by_org_rel;
	extern TIC_CALL TokenID select_uint16_with_attr_by_org_rel;
	extern TIC_CALL TokenID select_uint32_with_attr_by_org_rel;
	extern TIC_CALL TokenID select_uint64_with_attr_by_org_rel;

	extern TIC_CALL TokenID select_data;

	extern TIC_CALL TokenID collect_by_cond;
	extern TIC_CALL TokenID collect_by_org_rel; // synonimous with lookup, arrow-operator, and (reversed) array-index operator

	extern TIC_CALL TokenID collect_attr_by_cond;
	extern TIC_CALL TokenID collect_attr_by_org_rel;

	// DEPRECIATED BEGIN
	extern TIC_CALL TokenID select_many;
	extern TIC_CALL TokenID select_afew;
	extern TIC_CALL TokenID select_many_uint8;
	extern TIC_CALL TokenID select_afew_uint8;
	extern TIC_CALL TokenID select_many_uint16;
	extern TIC_CALL TokenID select_afew_uint16;
	extern TIC_CALL TokenID select_many_uint32;
	extern TIC_CALL TokenID select_afew_uint32;
	// DEPRECIATED END

	extern TIC_CALL TokenID recollect_by_cond;
	extern TIC_CALL TokenID recollect_by_org_rel;

//	SELECT section END

	extern TIC_CALL TokenID nr_OrgEntity;
	extern TIC_CALL TokenID polygon_rel;
	extern TIC_CALL TokenID part_rel;
	extern TIC_CALL TokenID arc_rel;
	extern TIC_CALL TokenID sequence_rel;
	extern TIC_CALL TokenID org_rel;
	extern TIC_CALL TokenID first_rel;
	extern TIC_CALL TokenID second_rel;
	extern TIC_CALL TokenID ordinal;
	extern TIC_CALL TokenID integrity_check;

	extern TIC_CALL TokenID map;
	extern TIC_CALL TokenID geometry;
	extern TIC_CALL TokenID FenceContainer;

}

//LispRef CreateLispSubTree(const TreeItem* self, bool inclSubTree);
TIC_CALL LispRef CreateLispTree(const TreeItem* self, bool inclSubTree);
//LispRef CreateLispSign(const TreeItem* self, LispRef tail);

template <typename T, typename Enabled = std::enable_if_t<is_numeric_v<T>>>
LispRef AsLispRef(T v)
{
	return ExprList(ValueWrap<T>::GetStaticClass()->GetID(), LispRef(Number(v)));
}

inline LispRef AsLispRef(double v)
{
	return LispRef(Number(v));
}

template <typename T>
auto AsLispRef(Point<T> p) -> LispRef
{
	return ExprList(token::point_xy	, AsLispRef(p.X()), AsLispRef(p.Y()));
}

template <typename T, typename Enabled = std::enable_if_t<is_numeric_v<T>>>
LispRef AsLispRef(T v, LispPtr valuesUnitKeyExpr)
{
	return ExprList(token::convert, LispRef(Number(v)), valuesUnitKeyExpr);
}

template <bit_size_t N>
LispRef AsLispRef(bit_value<N> v, LispPtr valuesUnitKeyExpr)
{
	return ExprList(ValueWrap<bit_value<N>>::GetStaticClass()->GetID(), LispRef(Number(v)));
}

auto AsLispRef(Bool v, LispPtr valuesUnitKeyExpr) -> LispRef;

template <typename T>
inline auto AsLispRef(Point<T> p, LispPtr valuesUnitKeyExpr) -> LispRef
{
	return ExprList(token::point, AsLispRef(p.first), AsLispRef(p.second), valuesUnitKeyExpr);
}

template <typename T>
auto AsLispRef(SA_ConstReference<T> s, LispPtr valuesUnitKeyExpr) -> LispRef
{
	throwNYI(MG_POS, "AsLispRef(sequence)");
}

inline auto AsLispRef(SA_ConstReference<char> s, LispPtr valuesUnitKeyExpr) -> LispRef
{
	return LispRef(s.begin(), s.end());
}

inline auto AsLispRef(SharedStr s, LispPtr valuesUnitKeyExpr) -> LispRef
{
	return LispRef(s.begin(), s.end());
}

template <typename T>
auto AsLispRef(const Range<T>& range, LispRef&& base, bool asCategorical) -> LispRef
{
	return ExprList(asCategorical ? token::cat_range : token::range, base
		, AsLispRef(range.first)
		, AsLispRef(range.second)
	);
}

LispRef slUnionDataLispExpr(LispPtr values, SizeT sz);

template <typename T>
LispRef AsLispRef(const std::vector<T>& v)
{
	auto valueList = LispRef();
	for (auto b = v.begin(), e = v.end(); b != e;)
		valueList = LispRef(AsLispRef(*--e), valueList);
	return slUnionDataLispExpr(valueList, v.size());
}

template <typename T>
LispRef AsLispRef(const std::vector<T>& v, LispPtr valuesUnitKeyExpr)
{
	throwNYI(MG_POS, "AsLispRef(vector)");
}


#endif // __TIC_LISPTREETYPE_H
