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

#ifndef __TIC_LISPTREETYPE_H
#define __TIC_LISPTREETYPE_H

#include "LispRef.h"
#include "LispList.h"

struct TreeItem;
#include "geo/SequenceArray.h"

#if defined(MG_DEBUG)
//#define MG_DEBUG_LISP_TREE
#endif //defined(MG_DEBUG)

LispRef slSubItemCall(LispPtr baseExpr, CharPtrRange relPath);

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

	extern TIC_CALL TokenID arrow;
	extern TIC_CALL TokenID lookup;
	extern TIC_CALL TokenID convert;
	extern TIC_CALL TokenID eval;
	extern TIC_CALL TokenID scope;

	extern TIC_CALL TokenID subitem;
	extern TIC_CALL TokenID NrOfRows;
	extern TIC_CALL TokenID range;
	extern TIC_CALL TokenID TiledUnit;
	extern TIC_CALL TokenID point;

	extern TIC_CALL TokenID BaseUnit;
	extern TIC_CALL TokenID UInt32;
	extern TIC_CALL TokenID left;
	extern TIC_CALL TokenID right;
	extern TIC_CALL TokenID DomainUnit;
	extern TIC_CALL TokenID ValuesUnit;

	extern TIC_CALL TokenID union_data;
	extern TIC_CALL TokenID sourceDescr;
	extern TIC_CALL TokenID container;
	extern TIC_CALL TokenID geometry;
	extern TIC_CALL TokenID nr_OrgEntity;
	extern TIC_CALL TokenID org_rel;
	extern TIC_CALL TokenID first_rel;
	extern TIC_CALL TokenID second_rel;
	extern TIC_CALL TokenID integrity_check;
}

//LispRef CreateLispSubTree(const TreeItem* self, bool inclSubTree);
LispRef CreateLispTree(const TreeItem* self, bool inclSubTree);
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
	return ExprList(token::point, AsLispRef(p.first), AsLispRef(p.second));
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

inline auto AsLispRef(Bool v, LispPtr valuesUnitKeyExpr) -> LispRef
{
	return LispRef(v ? token::true_ : token::false_);
}

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
auto AsLispRef(const Range<T>& range, LispRef&& base) -> LispRef
{
	dms_assert(!(base.IsRealList() && base.Left().IsSymb() && base.Left().GetSymbID() == token::range));
	return List(LispRef(token::range)
		, base
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
