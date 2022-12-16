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
// SheetVisualTestView.h : interface of the DataView class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(__SHV_CLASSBREAKS_H)
#define __SHV_CLASSBREAKS_H

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------
#define MG_DEBUG_CLASSBREAKS false

#include "cpc/types.h"
#include "ClcBase.h"

#include <vector>

//----------------------------------------------------------------------
// typedefs
//----------------------------------------------------------------------

using ClassBreakValueType = Float64;
using break_array = std::vector<ClassBreakValueType>;

typedef SizeT   CountType;
typedef std::pair<ClassBreakValueType, CountType> ValueCountPair;
typedef std::vector<ClassBreakValueType>          LimitsContainer;
using ValueCountPairContainerBase = std::vector < ValueCountPair, my_allocator < ValueCountPair> >;

struct ValueCountPairContainer : ValueCountPairContainerBase
{
	ValueCountPairContainer() : m_Total(0) {}

	ValueCountPairContainer(ValueCountPairContainer&& rhs) noexcept
		: ValueCountPairContainerBase(std::move(rhs))
		, m_Total(rhs.m_Total)
	{
		dms_assert(rhs.empty());
		rhs.m_Total = 0;
	}
	template<typename Iter>
	ValueCountPairContainer(Iter first, Iter last)
		: ValueCountPairContainerBase(first, last)
	{
		for (const auto& vcp : *this)
			m_Total += vcp.second;
	}

	void operator = (ValueCountPairContainer&& rhs)  noexcept
	{
		ValueCountPairContainerBase::operator =((ValueCountPairContainerBase&&)rhs);
		std::swap(m_Total, rhs.m_Total);
	}


	SizeT m_Total = 0;
	MG_DEBUGCODE( void Check(); )

private:
	ValueCountPairContainer(const ValueCountPairContainer&) = delete;
	void operator = (const ValueCountPairContainer&) = delete;
};

const UInt32 MAX_PAIR_COUNT = 4096;

CLC1_CALL ValueCountPairContainer PrepareCounts(const AbstrDataItem* adi, SizeT maxPairCount);
CLC1_CALL ValueCountPairContainer GetCounts    (const AbstrDataItem* adi, SizeT maxPairCount);

//----------------------------------------------------------------------
// class break functions
//----------------------------------------------------------------------

CLC1_CALL void ClassifyLogInterval(break_array& faLimits, SizeT k, const ValueCountPairContainer& vcpc);

CLC1_CALL break_array ClassifyJenksFisher(const ValueCountPairContainer& vcpc, SizeT kk, bool separateZero);
CLC1_CALL void FillBreakAttrFromArray(AbstrDataItem* breakAttr, const break_array& data, const SharedObj* abstrValuesRangeData);

//----------------------------------------------------------------------
// breakAttr functions
//----------------------------------------------------------------------

CLC1_CALL break_array ClassifyUniqueValues (AbstrDataItem* breakAttr, const ValueCountPairContainer& vcpc);
CLC1_CALL break_array ClassifyEqualCount   (AbstrDataItem* breakAttr, const ValueCountPairContainer& vcpc);
CLC1_CALL break_array ClassifyEqualInterval(AbstrDataItem* breakAttr, const ValueCountPairContainer& vcpc);
CLC1_CALL break_array ClassifyLogInterval  (AbstrDataItem* breakAttr, const ValueCountPairContainer& vcpc);
CLC1_CALL break_array ClassifyJenksFisher  (AbstrDataItem* breakAttr, const ValueCountPairContainer& vcpc, bool separateZero);
inline break_array ClassifyNZJenksFisher(AbstrDataItem* breakAttr, const ValueCountPairContainer& vcpc) { return ClassifyJenksFisher(breakAttr, vcpc, true ); }
inline break_array ClassifyCRJenksFisher(AbstrDataItem* breakAttr, const ValueCountPairContainer& vcpc) { return ClassifyJenksFisher(breakAttr, vcpc, false); }


using ClassBreakFunc = break_array (*)(AbstrDataItem* breakAttr, const ValueCountPairContainer& vcpc);

#endif // !defined(__SHV_CLASSBREAKS_H)
