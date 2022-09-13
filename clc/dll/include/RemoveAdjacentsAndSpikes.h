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

#ifndef __CLC_REMOVEADJACENTSANDSPIKES_H
#define __CLC_REMOVEADJACENTSANDSPIKES_H

// *****************************************************************************
//									remove_adjacents_and_spikes
// *****************************************************************************

template <typename Iter>
Iter remove_adjacents_and_spikes(Iter first, Iter last)
{
	dms_assert(last - first >= 0);
	if (last == first)
		return last;

	last = std::unique(first, last); // remove doubles
	dms_assert(last - first > 0);
	if (last[-1] == first[0])
		--last;
	dms_assert(first == last || last[-1] != *first);

	Iter curr = first, begin = first;

// Remove spikes around begin-end boundary of the form [a b)(a c] => [a)(c] and [a b)(c b] => [a)(b]. Note that no doubles are introduced
/* REMOVE
	while (last - first >= 2 && (last[-1] == first[1] || last[-2] == first[0]))
	{
		--last, ++first;
		dms_assert(last - first < 3 || last[-1] != *first);
	}
#if defined(MG_DEBUG)
	if (last - first > 2) {
		dms_assert(first[0] != last[-1]);
		dms_assert(first[0] != first[1]);
		dms_assert(first[1] != last[-1]);
		dms_assert(first[0] != last[-2]);
	}
#endif
*/

//	Replace [x)(a-b-a] by ->(x-a]. Note that subsequent (x a x] spikes will be removed too.
	while (last - first >= 3)
	{
		dms_assert(first[0] != first[1]);
		dms_assert(first[1] != first[2]);
		if (first[0] == first[2])
		{
			dms_assert(curr - begin < 2 || curr[-1] != curr[-2]);
			if (curr > begin)
				*++first = *--curr;
			else
				*++first = *--last;
			dms_assert(first[0] != first[1]);
			dms_assert(curr == begin || curr[-1] != *first);
			dms_assert(curr != begin || last[-1] != *first);
		}
		else
			*curr++ = *first++;
	}
//*/
	// copy remaining points (max 2, thus not containing a spike)
	while (first != last)
		*curr++ = *first++;
	dms_assert(first == last);

	dms_assert(curr - begin < 3 || curr[-1] != *begin);

	first = begin;
	last = curr;

	// Remove spikes around begin-end boundary of the form [a b)(a c] => [a)(c] and [a b)(c b] => [a)(b]. Note that no doubles are introduced
	while (last - first >= 2 && (last[-1] == first[1] || last[-2] == first[0]))
	{
		--last, ++first;
		dms_assert(last - first < 3 || last[-1] != *first);
	}
#if defined(MG_DEBUG)
	if (last - first > 2) {
		dms_assert(first[0] != last[-1]);
		dms_assert(first[0] != first[1]);
		dms_assert(first[1] != last[-1]);
		dms_assert(first[0] != last[-2]);
		for (Iter c = first + 2; c != last; ++c) {
			dms_assert(c[0] != c[-1]);
			dms_assert(c[0] != c[-2]);
		}
	}
#endif
	if (first != begin)
		curr = fast_copy(first, last, begin);
	//*/


// Check that the desires post condition
#if defined(MG_DEBUG)
	if (curr - begin > 2) {
		dms_assert(begin[0] != curr[-1]);
		dms_assert(begin[0] != begin[1]);
		dms_assert(begin[1] != curr[-1]);
		dms_assert(begin[0] != curr[-2]);
		for (Iter c= begin + 2; c!= curr; ++c) {
			dms_assert(c[0] != c[-1]);
			dms_assert(c[0] != c[-2]);
		}
	}
#endif

	return curr;
}

template <typename Vec>
void remove_adjacents_and_spikes(Vec& points)
{
	points.erase(
		remove_adjacents_and_spikes(points.begin(), points.end()),
		points.end()
	);
}

template <typename Vec>
void remove_adjacents(Vec& points)
{
	points.erase(
		std::unique(points.begin(), points.end()),
		points.end()
	);
}


#endif // __CLC_REMOVEADJACENTSANDSPIKES_H
