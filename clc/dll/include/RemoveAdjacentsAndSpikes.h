// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __CLC_REMOVEADJACENTSANDSPIKES_H
#define __CLC_REMOVEADJACENTSANDSPIKES_H

// *****************************************************************************
//									remove_adjacents_and_spikes
// *****************************************************************************

template <typename Iter>
Iter remove_adjacents_and_spikes(Iter first, Iter last)
{
	assert(last - first >= 0);
	if (last == first)
		return last;

	for (auto iter = first; iter != last; ++iter)
		if (!IsDefined(*iter))
			throwErrorF("remove_adjacents_and_spikes", "unexpected null or separator found in point sequence at pos %d", iter - first);

	last = std::unique(first, last); // remove doubles

	assert(last - first > 0);
	if (last[-1] == first[0])
		--last;
	assert(first == last || last[-1] != *first);

	Iter curr = first, begin = first;

// Remove spikes around begin-end boundary of the form [a b)(a c] => [a)(c] and [a b)(c b] => [a)(b]. Note that no doubles are introduced
/* REMOVE
	while (last - first >= 2 && (last[-1] == first[1] || last[-2] == first[0]))
	{
		--last, ++first;
		assert(last - first < 3 || last[-1] != *first);
	}
#if defined(MG_DEBUG)
	if (last - first > 2) {
		assert(first[0] != last[-1]);
		assert(first[0] != first[1]);
		assert(first[1] != last[-1]);
		assert(first[0] != last[-2]);
	}
#endif
*/

//	Replace [x)(a-b-a] by ->(x-a]. Note that subsequent (x a x] spikes will be removed too.
	while (last - first >= 3)
	{
		assert(first[0] != first[1]);
		assert(first[1] != first[2]);
		if (first[0] == first[2])
		{
			assert(curr - begin < 2 || curr[-1] != curr[-2]);
			if (curr > begin)
				*++first = *--curr;
			else
				*++first = *--last;
			assert(first[0] != first[1]);
			assert(curr == begin || curr[-1] != *first);
			assert(curr != begin || last[-1] != *first);
		}
		else
			*curr++ = *first++;
	}
//*/
	// copy remaining points (max 2, thus not containing a spike)
	while (first != last)
		*curr++ = *first++;
	assert(first == last);

	assert(curr - begin < 3 || curr[-1] != *begin);

	first = begin;
	last = curr;

	// Remove spikes around begin-end boundary of the form [a b)(a c] => [a)(c] and [a b)(c b] => [a)(b]. Note that no doubles are introduced
	while (last - first >= 2 && (last[-1] == first[1] || last[-2] == first[0]))
	{
		--last, ++first;
		assert(last - first < 3 || last[-1] != *first);
	}
#if defined(MG_DEBUG)
	if (last - first > 2) {
		assert(first[0] != last[-1]);
		assert(first[0] != first[1]);
		assert(first[1] != last[-1]);
		assert(first[0] != last[-2]);
		for (Iter c = first + 2; c != last; ++c) {
			assert(c[0] != c[-1]);
			assert(c[0] != c[-2]);
		}
	}
#endif
	if (first != begin)
		curr = fast_copy(first, last, begin);
	//*/


// Check that the desires post condition
#if defined(MG_DEBUG)
	if (curr - begin > 2) {
		assert(begin[0] != curr[-1]);
		assert(begin[0] != begin[1]);
		assert(begin[1] != curr[-1]);
		assert(begin[0] != curr[-2]);
		for (Iter c= begin + 2; c!= curr; ++c) {
			assert(c[0] != c[-1]);
			assert(c[0] != c[-2]);
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
