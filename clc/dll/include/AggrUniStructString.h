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

#if !defined(__CLC_AGGRUNISTRUCTSTRING_H)
#define __CLC_AGGRUNISTRUCTSTRING_H

#include "dbg/Debug.h"
#include "ser/SequenceArrayStream.h"
#include "ser/MoreStreamBuff.h"

#include "AggrUniStruct.h"
#include "UnitCreators.h"

/*****************************************************************************/
//											string aggregation functions
/*****************************************************************************/

// A SerFunc operates on a streamBuffer
template <typename TSerFunc> 
struct unary_assign_string_total_accumulation: unary_total_accumulation<SharedStr, typename TSerFunc::arg1_type>
{
	using base_type = unary_total_accumulation<SharedStr, typename TSerFunc::arg1_type>;

	template <typename R>
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return TSerFunc::template unit_creator<R>(gr, args); }

	typedef	typename base_type::assignee_type accumulation_type;
	typedef	typename base_type::assignee_ref  accumulation_ref;

	unary_assign_string_total_accumulation(TSerFunc f= TSerFunc())
	:	m_SerFunc(f) 
	{}

	void Init(accumulation_ref output) const 
	{
		output.clear();
	}

	void operator()(accumulation_ref output, typename unary_assign_string_total_accumulation::value_cseq1 input, bool hasUndefinedValues) const
	{
		DBG_START("unary_assign_string_total_accumulation", "operator()", false);

		SizeT sz = output.size();

		InfiniteNullOutStreamBuff lengthFinderStreamBuff;
		lengthFinderStreamBuff.m_CurrPos += sz;

		aggr1_total_best(lengthFinderStreamBuff, input.begin(), input.end(), hasUndefinedValues, m_SerFunc);

		output.resize_uninitialized(lengthFinderStreamBuff.CurrPos());

		ThrowingMemoOutStreamBuff writerStreamBuff(begin_ptr( output ), end_ptr( output ));
		writerStreamBuff.m_Curr += sz;
		
		aggr1_total_best(writerStreamBuff, input.begin(), input.end(), hasUndefinedValues, m_SerFunc);
	}

	TSerFunc m_SerFunc;
};

struct length_finder_array : std::vector<InfiniteNullOutStreamBuff>
{
	length_finder_array(UInt32 n) : std::vector<InfiniteNullOutStreamBuff>(n) {}
	SizeT GetTotalLength() const
	{
		SizeT result = 0;
		const_iterator i = begin(), e = end();
		while (i != e)
			result += (i++)->CurrPos();
		return result;
	}
};

void InitOutput(
		sequence_traits<SharedStr>::seq_t& outputs, 
		const length_finder_array&      lengthFinderArray
);

typedef std::vector<ThrowingMemoOutStreamBuff> memo_out_stream_array;

template <typename TSerFunc>
struct unary_assign_string_partial_accumulation : unary_partial_accumulation<SharedStr, typename TSerFunc::arg1_type>
{
	template <typename R>
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return TSerFunc::template unit_creator<R>(gr, args); }

	unary_assign_string_partial_accumulation(const TSerFunc& assignFunc = TSerFunc())
	:	m_AssignFunc(assignFunc) {}


	void InspectData(length_finder_array& lengthFinderArray, typename unary_assign_string_partial_accumulation::value_cseq1 input, const IndexGetter* indices, bool hasUndefinedValues) const
	{ 
		aggr_fw_best_partial(lengthFinderArray.begin(), input.begin(), input.end(), indices, hasUndefinedValues, m_AssignFunc);

	}

	void ReserveData(memo_out_stream_array& outStreamArray, typename unary_assign_string_partial_accumulation::accumulation_seq outputs) const
	{ 
		outStreamArray.reserve(outputs.size());
		for (auto resultSequence: outputs)
			outStreamArray.push_back(ThrowingMemoOutStreamBuff(begin_ptr(resultSequence), end_ptr(resultSequence)));
	}

	void ProcessTileData(memo_out_stream_array& outStreamArray, typename unary_assign_string_partial_accumulation::value_cseq1 input, const IndexGetter* indices, bool hasUndefinedValues) const
	{ 
		// re-write everything into the allocated buffers.
		aggr_fw_best_partial(outStreamArray.begin(), input.begin(), input.end(), indices, hasUndefinedValues, m_AssignFunc);
	}
private:
	TSerFunc m_AssignFunc;
};

/*****************************************************************************/
//											asexprlist
/*****************************************************************************/

template <typename T> 
struct unary_ser_asexprlist : unary_assign<OutStreamBuff, T>
{
	template <typename R> static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return default_unit_creator<R>(); }

	void operator()(typename unary_ser_asexprlist::assignee_ref assignee, typename unary_ser_asexprlist::arg1_cref arg) const
	{ 
		FormattedOutStream os(&assignee, FormattingFlags::None);
		if (assignee.CurrPos())
			os << ";";
		WriteDataString(os, arg);
	}

	typedef SharedStr dms_result_type;
};

// total: 
template <typename T> 
struct asexprlist_total : unary_assign_string_total_accumulation<unary_ser_asexprlist<T> >
{
	typedef SharedStr dms_result_type;
};

// partial: generates Strings based on index_container<PartId>
template <typename T> 
struct asexprlist_partial : unary_assign_string_partial_accumulation< unary_ser_asexprlist<T> >
{
	typedef SharedStr dms_result_type;
};

/*****************************************************************************/
//											aslist
/*****************************************************************************/

struct unary_ser_aslist: unary_assign<OutStreamBuff, SharedStr>
{
	template <typename R> static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return default_unit_creator<R>(); }

	typedef SharedStr dms_result_type;

	unary_ser_aslist(cref<SharedStr>::type separator) : m_Separator(separator) {}

	void operator()(OutStreamBuff& assignee, cref<SharedStr>::type arg) const	
	{ 
		if (assignee.CurrPos() && arg.size())
			assignee.WriteBytes(begin_ptr( m_Separator ), m_Separator.size());
		assignee.WriteBytes(begin_ptr( arg ), arg.size());
	}

private:
	cref<SharedStr>::type m_Separator;
};

// total: 
struct aslist_total : unary_assign_string_total_accumulation<unary_ser_aslist > 
{
	typedef SharedStr dms_result_type;

	aslist_total(unary_ser_aslist::arg1_cref separator)
		:	unary_assign_string_total_accumulation<unary_ser_aslist >(
				unary_ser_aslist(separator)
			) 
	{}
};

// partial: generates Strings based on index_container<PartId>
struct aslist_partial : unary_assign_string_partial_accumulation< unary_ser_aslist>
{
	typedef SharedStr dms_result_type;

	aslist_partial(unary_ser_aslist::arg1_cref separator)
		:	unary_assign_string_partial_accumulation<unary_ser_aslist>(
				unary_ser_aslist(separator)
			)
	{}
};

/*****************************************************************************/
//											asitemlist
/*****************************************************************************/

template <typename T> 
struct unary_ser_asitemlist
:	unary_assign<OutStreamBuff, SharedStr>
{
	template<typename R> static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return default_unit_creator<R>(); }

	typedef SharedStr dms_result_type;

	void operator()(assignee_ref assignee, arg1_cref arg) const	
	{ 
		if (assignee.CurrPos() && arg.size())
			assignee.WriteByte(',');
		assignee.WriteBytes(begin_ptr( arg ), arg.size());
	}
};

// total: 
template <typename T> 
struct asitemlist_total : unary_assign_string_total_accumulation<unary_ser_asitemlist<T> > 
//:	unary_assign_total_accumulation<
//		unary_assign_asitemlist<T>, 
//		assign_no_op<unary_assign_asitemlist<T>::assignee_ref> > 
{
	typedef SharedStr dms_result_type;
};

// partial: generates Strings based on index_container<PartId>
template <typename T> 
struct asitemlist_partial : unary_assign_string_partial_accumulation< unary_ser_asitemlist<T> >
{
	typedef SharedStr dms_result_type;
};


#endif

