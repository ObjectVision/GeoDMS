// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#ifndef __TIC_LISPTREETYPE_H
#define __TIC_LISPTREETYPE_H

#include "sym/Token.h" // StaticTokenID (namespace-scope token globals)
#include "LispRef.h"
#include "LispList.h"

struct TreeItem;
#include "vt/SequenceArray.h"
#include "mci/ValueWrap.h"

#if defined(MG_DEBUG)
//#define MG_DEBUG_LISP_TREE
#endif //defined(MG_DEBUG)

TIC_CALL LispRef slSubItemCall(LispPtr baseExpr, CharPtrRange relPath);
TIC_CALL LispRef slConvertedLispExpr(LispPtr result, LispPtr vu);

namespace token {
	extern TIC_CALL StaticTokenID add;
	extern TIC_CALL StaticTokenID sub;
	extern TIC_CALL StaticTokenID mul;
	extern TIC_CALL StaticTokenID div;
	extern TIC_CALL StaticTokenID mod;

	extern TIC_CALL StaticTokenID neg;

	extern TIC_CALL StaticTokenID eq;
	extern TIC_CALL StaticTokenID ne;
	extern TIC_CALL StaticTokenID lt;
	extern TIC_CALL StaticTokenID le;
	extern TIC_CALL StaticTokenID gt;
	extern TIC_CALL StaticTokenID ge;

	extern TIC_CALL StaticTokenID id;

	extern TIC_CALL StaticTokenID and_;
	extern TIC_CALL StaticTokenID or_;
	extern TIC_CALL StaticTokenID not_;

	extern TIC_CALL StaticTokenID iif;

	extern TIC_CALL StaticTokenID true_;
	extern TIC_CALL StaticTokenID false_;
	extern TIC_CALL StaticTokenID pi;

	extern TIC_CALL StaticTokenID const_;
	extern TIC_CALL StaticTokenID null_b;
	extern TIC_CALL StaticTokenID null_w;
	extern TIC_CALL StaticTokenID null_u;
	extern TIC_CALL StaticTokenID null_u64;
	extern TIC_CALL StaticTokenID null_c;
	extern TIC_CALL StaticTokenID null_s;
	extern TIC_CALL StaticTokenID null_i;
	extern TIC_CALL StaticTokenID null_i64;
	extern TIC_CALL StaticTokenID null_f;
	extern TIC_CALL StaticTokenID null_d;
	extern TIC_CALL StaticTokenID null_sp;
	extern TIC_CALL StaticTokenID null_wp;
	extern TIC_CALL StaticTokenID null_ip;
	extern TIC_CALL StaticTokenID null_up;
	extern TIC_CALL StaticTokenID null_fp;
	extern TIC_CALL StaticTokenID null_dp;
	extern TIC_CALL StaticTokenID null_str;

	bool isConst(TokenID t);

	extern TIC_CALL StaticTokenID arrow;
	extern TIC_CALL StaticTokenID lookup;
	extern TIC_CALL StaticTokenID convert;
	extern TIC_CALL StaticTokenID rounded_convert;
	extern TIC_CALL StaticTokenID eval;
	extern TIC_CALL StaticTokenID scope;

	extern TIC_CALL StaticTokenID subitem;
	extern TIC_CALL StaticTokenID NrOfRows;
	extern TIC_CALL StaticTokenID range;
	extern TIC_CALL StaticTokenID cat_range;
	extern TIC_CALL StaticTokenID TiledUnit;
	extern TIC_CALL StaticTokenID point;
	extern StaticTokenID point_xy;

	extern StaticTokenID BaseUnit;
	extern StaticTokenID CrsUnit;
	extern StaticTokenID UInt32;
	extern TIC_CALL StaticTokenID left;
	extern TIC_CALL StaticTokenID right;
	extern TIC_CALL StaticTokenID DomainUnit;
	extern StaticTokenID ValuesUnit;

	extern TIC_CALL StaticTokenID union_data;
	extern TIC_CALL StaticTokenID ordered_union_data;
	extern StaticTokenID sourceDescr;
	extern TIC_CALL StaticTokenID container;
	extern TIC_CALL StaticTokenID classify;

//	SELECT section BEGIN
	extern TIC_CALL StaticTokenID select;
	extern TIC_CALL StaticTokenID select_uint8;
	extern TIC_CALL StaticTokenID select_uint16;
	extern TIC_CALL StaticTokenID select_uint32;
	extern TIC_CALL StaticTokenID select_uint64;

	extern TIC_CALL StaticTokenID select_with_org_rel;
	extern TIC_CALL StaticTokenID select_uint8_with_org_rel;
	extern TIC_CALL StaticTokenID select_uint16_with_org_rel;
	extern TIC_CALL StaticTokenID select_uint32_with_org_rel;
	extern TIC_CALL StaticTokenID select_uint64_with_org_rel;

	extern TIC_CALL StaticTokenID select_with_attr_by_cond;
	extern TIC_CALL StaticTokenID select_uint8_with_attr_by_cond;
	extern TIC_CALL StaticTokenID select_uint16_with_attr_by_cond;
	extern TIC_CALL StaticTokenID select_uint32_with_attr_by_cond;
	extern TIC_CALL StaticTokenID select_uint64_with_attr_by_cond;

	extern TIC_CALL StaticTokenID select_with_org_rel_with_attr_by_cond;
	extern TIC_CALL StaticTokenID select_uint8_with_org_rel_with_attr_by_cond;
	extern TIC_CALL StaticTokenID select_uint16_with_org_rel_with_attr_by_cond;
	extern TIC_CALL StaticTokenID select_uint32_with_org_rel_with_attr_by_cond;
	extern TIC_CALL StaticTokenID select_uint64_with_org_rel_with_attr_by_cond;

	extern TIC_CALL StaticTokenID select_with_attr_by_org_rel;
	extern TIC_CALL StaticTokenID select_uint8_with_attr_by_org_rel;
	extern TIC_CALL StaticTokenID select_uint16_with_attr_by_org_rel;
	extern TIC_CALL StaticTokenID select_uint32_with_attr_by_org_rel;
	extern TIC_CALL StaticTokenID select_uint64_with_attr_by_org_rel;

	extern TIC_CALL StaticTokenID collect_by_cond;
	extern TIC_CALL StaticTokenID collect_by_org_rel; // synonimous with lookup, arrow-operator, and (reversed) array-index operator

	extern TIC_CALL StaticTokenID collect_attr_by_cond;
	extern TIC_CALL StaticTokenID collect_attr_by_org_rel;

	// #337: the spec forms, whose first argument carries the configuration

	extern TIC_CALL StaticTokenID select_spec;
	extern TIC_CALL StaticTokenID collect_spec;
	extern TIC_CALL StaticTokenID table_spec;

	extern TIC_CALL StaticTokenID recollect_by_cond;
	extern StaticTokenID recollect_by_org_rel;

//	SELECT section END

	extern TIC_CALL StaticTokenID nr_OrgEntity;
	extern TIC_CALL StaticTokenID polygon_rel;
	extern TIC_CALL StaticTokenID part_rel;
	extern TIC_CALL StaticTokenID arc_rel;
	extern TIC_CALL StaticTokenID sequence_rel;
	extern TIC_CALL StaticTokenID org_rel;
	extern TIC_CALL StaticTokenID first_rel;
	extern TIC_CALL StaticTokenID second_rel;
	extern TIC_CALL StaticTokenID ordinal;
	extern TIC_CALL StaticTokenID integrity_check;

	extern StaticTokenID map;
	extern TIC_CALL StaticTokenID geometry;
	extern TIC_CALL StaticTokenID geometry_z;
	extern TIC_CALL StaticTokenID geometry_m;
	extern TIC_CALL StaticTokenID spatial_reference;
	extern TIC_CALL StaticTokenID PhaseContainer;

	extern StaticTokenID SubItems;
	extern StaticTokenID Error;
	extern StaticTokenID SigAndSub;

	extern TIC_CALL StaticTokenID direct_index;
	extern TIC_CALL StaticTokenID index;
	extern TIC_CALL StaticTokenID subindex;
}

//LispRef CreateLispSubTree(const TreeItem* self, bool inclSubTree);
TIC_CALL LispRef CreateLispTree(const TreeItem* self, bool inclSubTree);
//LispRef CreateLispSign(const TreeItem* self, LispRef tail);

template <typename T, typename Enabled = std::enable_if_t<is_numeric_v<T>>>
LispRef AsLispRef(T v)
{
	return ExprList(ValueWrap<T>::GetStaticClass()->GetNameID(), LispRef(Number(v)));
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
	return ExprList(ValueWrap<bit_value<N>>::GetStaticClass()->GetNameID(), LispRef(Number(v)));
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

template <typename T>
LispRef AsLispRef(const locked_sequence<T>& v)
{
	auto valueList = LispRef();
	for (auto b = v.begin(), e = v.end(); b != e;)
		valueList = LispRef(AsLispRef(*--e), valueList);
	return slUnionDataLispExpr(valueList, v.size());
}

template <typename T>
LispRef AsLispRef(const my_vector<T>& v)
{
	auto valueList = LispRef();
	for (auto b = v.begin(), e = v.end(); b != e;)
		valueList = LispRef(AsLispRef(*--e), valueList);
	return slUnionDataLispExpr(valueList, v.size());
}

template <typename T>
LispRef AsLispRef(const locked_sequence<T>& v, LispPtr valuesUnitKeyExpr)
{
	throwNYI(MG_POS, "AsLispRef(vector)");
}

template <typename T>
LispRef AsLispRef(const my_vector<T>& v, LispPtr valuesUnitKeyExpr)
{
	throwNYI(MG_POS, "AsLispRef(vector)");
}


#endif // __TIC_LISPTREETYPE_H
