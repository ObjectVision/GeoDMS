// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__CLC_MAKE_CULULATIVE_H)
#define __CLC_MAKE_CULULATIVE_H

// C_i := C_(i-1) + V_i = SUM(j=0..i, V_j)

template <typename IterAddable>
void make_cumulative(IterAddable b, IterAddable e)
{
	typedef typename std::iterator_traits<IterAddable>::value_type value_type;
	if (b == e)
		return;
	value_type accumulator = *b;
	while (++b != e)
	{
		accumulator += *b;
		*b = accumulator;
	}
}

// C_i := C_(i-1) + V_(i-1) = SUM(j=0..i-1, V_j)

template <typename IterAddable>
void make_cumulative_base(IterAddable b, IterAddable e)
{
	typedef typename std::iterator_traits<IterAddable>::value_type value_type;
	if (b == e)
		return;
	value_type accumulation_so_far = value_type();
	while (b != e)
	{
		value_type accumulator = accumulation_so_far + *b;
		*b++ = accumulation_so_far;
		accumulation_so_far = accumulator;
	}
}


#endif //!defined(__CLC_MAKE_CULULATIVE_H)
