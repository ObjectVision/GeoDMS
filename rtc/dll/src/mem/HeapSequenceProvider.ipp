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

#include "geo/IndexRange.h"
#include "mem/HeapSequenceProvider.h"
#include "mem/ManagedAllocData.ipp"

// =================================================== class heap_sequence_provider : public abstr_sequence_provider<V>

template <typename V>
SizeT heap_sequence_provider<V>::max_size()
{
	return managed_alloc_data<V>().max_size();
}

template <typename V>
void heap_sequence_provider<V>::reserve(alloc_t& seq, SizeT newSize MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	dms_assert(seq.m_Capacity >= seq.size());
	if (newSize > seq.m_Capacity)
	{
		seq = managed_alloc_data<V>(seq.begin(), seq.end(), newSize MG_DEBUG_ALLOCATOR_SRC_PARAM);
	}
}

template <typename V>
void heap_sequence_provider<V>::resizeSP(alloc_t& seq, SizeT newSize, bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG)
{ 
	Check(seq);
	if (newSize <= seq.size())
		cut(seq, newSize);
	else
	{
		if (newSize > seq.m_Capacity) // ALLOC || !seq.begin())
			reserve(seq, Max<SizeT>(newSize, 2*seq.m_Capacity) MG_DEBUG_ALLOCATOR_SRC_PARAM);

		dms_assert(newSize <= seq.m_Capacity);

		iterator newEnd = seq.begin() + newSize;
		raw_awake_or_init(seq.end(), newEnd, mustClear);

		SetSize(seq, newSize);
	}
}

template <typename V>
void heap_sequence_provider<V>::cut(alloc_t& seq, SizeT newSize)
{ 
	Check(seq);
	dms_assert(newSize <= seq.size());

	iterator newEnd = seq.begin() + newSize;
	destroy_range(newEnd, seq.end());

	SetSize(seq, newSize);
}

template <typename V>
void heap_sequence_provider<V>::clear(alloc_t& seq) 
{ 
	Check(seq); 
	managed_alloc_data<V> oldSeq;
	seq.swap(oldSeq);
}

template <typename V>
abstr_sequence_provider<V>* heap_sequence_provider<V>::CreateProvider()
{
	static heap_sequence_provider<V> theProvider;
	return &theProvider;
}

template <typename V>
void heap_sequence_provider<V>::Destroy()
{
}

template <typename V>
abstr_sequence_provider<IndexRange<SizeT>>* heap_sequence_provider<V>::CloneForSeqs() const
{
	return heap_sequence_provider<IndexRange<SizeT> >::CreateProvider();
}

// =================================================== heap_sequence_provider private implementation

template <typename V>
void heap_sequence_provider<V>::Shrink(alloc_t& seq) 
{
	if (seq.size()< seq.m_Capacity / 4)
	{
		if (!seq.size())
			clear(seq);
		else
		{
			managed_alloc_data<V> newSeq(seq.begin(), seq.end(), seq.size() );
			seq.swap(newSeq);
		}
	}
}
