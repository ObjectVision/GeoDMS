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

#include <deque>

using index_t       = SizeT;
using index_range_t = Range<index_t>;
using index_range_vector_t = std::vector<index_range_t>;

template <typename PI>
void fillPointIndexBufferImpl(index_range_vector_t& buf, PI iBase, PI ii, PI ie, bool isClosedRing)
{
	assert(ii <= ie);
	assert(ii == ie || ii[0] == ie[-1] || !isClosedRing);  // follows from invariant and postcondition
	while (ie - ii > 2) // minimum Ring with surface must have 3 distinct points
	{
		assert(ii[0] == ie[-1] || !isClosedRing); // invariant 
		PI ij = std::find(ii+1, ie, *ii);

		SizeT nrPoints = ij - ii; 
		if (nrPoints >= 3)
		{
			SizeT nrPointsIncLast = nrPoints;

			assert(ij != ie || !isClosedRing);

			if (ij != ie)
			{
				assert(*ii == *ij); // postcondition of std::find when ij != ie
				++nrPointsIncLast; // include 2nd starting point
			}
			assert(nrPointsIncLast <= SizeT(ie - ii));
			index_t offset = ii - iBase;

			buf.emplace_back(offset, offset + nrPointsIncLast);
		}

		// skip points before 2nd starting point
	retry:
		ii = ij;
		assert(ii != ie && ii[0] == ie[-1] || !isClosedRing);  // follows from invariant and postcondition

		for (ij = ii + 5; ij < ie; ++ ij)
		{                                          // ii ..  .. ..   .. ij
			if (ij[0] == ii[0] && ij[-1] == ii[1]) // SO S1 .S2 S3.. SI SO ...
			{
				fillPointIndexBufferImpl(buf, iBase, ii + 1, ij, true);
				goto retry;
			}
		}
		assert(ii == ie || ii[0] == ie[-1] || !isClosedRing); // invariant ?
	}
}

template <typename PI>
void fillPointIndexBuffer(index_range_vector_t& buf, PI ii, PI ie)
{
	assert(buf.empty());

	bool isClosedRing = (ii == ie || ii[0] == ie[-1]);
	fillPointIndexBufferImpl(buf, ii, ii, ie, isClosedRing);
}

#endif // __RTC_GEO_POINTINDEXBUFFER_H
