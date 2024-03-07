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

#if !defined(__RTC_GEO_POINTINDEXBUFFER_H)
#define __RTC_GEO_POINTINDEXBUFFER_H

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
	dms_assert(ii <= ie);
	while (ie - ii > 2) // minimum Ring with surface must have 3 disticnt points
	{
		dms_assert(ii[0] == ie[-1] || !isClosedRing); // invariant 
		PI ij = std::find(ii+1, ie, *ii);

		SizeT nrPoints = ij - ii;
		SizeT nrPointsIncLast = nrPoints;

		dms_assert(ij != ie || !isClosedRing);

		if (ij != ie)
		{
			dms_assert(*ii == *ij); // postcondition of std::find when ij != ie
			++nrPointsIncLast; // include 2nd starting point
		}
		dms_assert(nrPointsIncLast <= SizeT(ie-ii));
		index_t offset = ii - iBase;
		if (nrPoints > 2)
			buf.emplace_back(offset, offset + nrPointsIncLast);

		// skip points before 2nd starting point
		ii = ij;

		if (ie - ii < 4)  // minimal Ring with non-zero surface: p0 p1 p2 p0
			break;

		dms_assert(ii != ie);         // follows from not breaking
		dms_assert(ii[0] == ie[-1] || !isClosedRing);  // follows from invariant and postcondition
		ij = ie;

		dms_assert(ij != ii);
		while (ij - ii >= 6) // minimal InnerRing has non-zero surface: SO SI pb pc SI SO
		{
			--ij;
			if (ij[0] == ii[0] && ij[-1]==ii[1]) // SO S1 ... SI SO ...
			{
				dms_assert(ij < ie); // follows from ij = ie followed by decrement
				if (ij - ii >= 5) 
					buf.m_StackBuffer.emplace_back(ii+1-iBase, ij-iBase);
				ii = ij;
				ij = ie;
				dms_assert(ii < ij);  // follows from previous assert
				dms_assert(ii[0] == ij[-1] || !isClosedRing);
			}
		}
		dms_assert(ii == ie || ii[0] == ie[-1] || !isClosedRing); // invariant ?
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
