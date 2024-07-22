// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__RTC_GEO_POINTINDEXBUFFER_H)
#define __RTC_GEO_POINTINDEXBUFFER_H

#include "RtcBase.h"
#include "geo/Range.h"

using index_t       = SizeT;
using index_range_t = Range<index_t>;
using index_range_vector_t = std::vector<index_range_t>;

struct pointIndexBuffer_t : std::vector<index_range_t> 
{
	index_range_vector_t m_StackBuffer;
};

static_assert(std::allocator_traits<pointIndexBuffer_t::allocator_type>::is_always_equal::value);
static_assert(std::allocator_traits<index_range_vector_t::allocator_type>::is_always_equal::value);

template <typename PI>
void fillPointIndexBufferImpl(pointIndexBuffer_t& buf, PI iBase, PI ii, PI ie, bool isClosedRing)
{
	assert(ii <= ie);
	while (ie - ii > 2) // minimum Ring with surface must have 3 disticnt points
	{
		assert(ii[0] == ie[-1] || !isClosedRing); // invariant 
		PI ij = std::find(ii+1, ie, *ii);

		SizeT nrPoints = ij - ii;
		SizeT nrPointsIncLast = nrPoints;

		assert(ij != ie || !isClosedRing);

		if (ij != ie)
		{
			dms_assert(*ii == *ij); // postcondition of std::find when ij != ie
			++nrPointsIncLast; // include 2nd starting point
		}
		assert(nrPointsIncLast <= SizeT(ie-ii));
		index_t offset = ii - iBase;
		if (nrPoints > 2)
			buf.emplace_back(offset, offset + nrPointsIncLast);

		// skip points before 2nd starting point
		ii = ij;

		if (ie - ii < 4)  // minimal Ring with non-zero surface: p0 p1 p2 p0
			break;

		assert(ii != ie);         // follows from not breaking
		assert(ii[0] == ie[-1] || !isClosedRing);  // follows from invariant and postcondition
		ij = ie;

		assert(ij != ii);
		while (ij - ii >= 6) // minimal InnerRing has non-zero surface: SO SI pb pc SI SO
		{
			--ij;
			if (ij[0] == ii[0] && ij[-1]==ii[1]) // SO S1 ... SI SO ...
			{
				assert(ij < ie); // follows from ij = ie followed by decrement
				if (ij - ii >= 5) 
					buf.m_StackBuffer.emplace_back(ii+1-iBase, ij-iBase);
				ii = ij;
				ij = ie;
				assert(ii < ij);  // follows from previous assert
				assert(ii[0] == ij[-1] || !isClosedRing);
			}
		}
		assert(ii == ie || ii[0] == ie[-1] || !isClosedRing); // invariant ?
	}
}

template <typename PI>
void fillPointIndexBuffer(pointIndexBuffer_t& buf, PI ii, PI ie)
{
	buf.resize(0);
	buf.m_StackBuffer.resize(0);
	bool isClosedRing = (ii == ie || ii[0] == ie[-1]);
	fillPointIndexBufferImpl<PI>(buf, ii, ii, ie, isClosedRing);
	while (!buf.m_StackBuffer.empty())
	{
		index_range_t currRing = buf.m_StackBuffer.back(); buf.m_StackBuffer.pop_back();
		fillPointIndexBufferImpl<PI>(buf, ii, ii+currRing.first, ii+currRing.second, true);
	}
}

#endif // __RTC_GEO_POINTINDEXBUFFER_H
