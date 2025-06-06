//<HEADER> // Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__MEM_RESIZE_H)
#define __MEM_RESIZE_H

#include "RtcBase.h"

//======================== Decl

template <typename Vector> inline
void resizeSO(Vector& vec, SizeT len, bool mustDefaultInitialize MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	vec.resizeSO(len, mustDefaultInitialize MG_DEBUG_ALLOCATOR_SRC_PARAM);
}

template <typename Vector> inline
void reallocSO(Vector& vec, SizeT len, bool mustDefaultInitialize MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	vec.reallocSO(len, mustDefaultInitialize MG_DEBUG_ALLOCATOR_SRC_PARAM);
}

template <typename V, typename A> inline
void resizeSO(std::vector<V, A>& vec, SizeT len, bool mustDefaultInitialize MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	vec.resize(len);
}

template <typename V, typename A> inline
void reallocSO(std::vector<V, A>& vec, SizeT len, bool mustDefaultInitialize MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	if (vec.capacity() < len)
		vec = std::vector<V, A>(len);
	else
		vec.resize(len);
}


#endif // __MEM_RESIZE_H
