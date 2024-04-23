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

#if !defined(__RTC_MEM_HEAPSEQUENCEPROVIDER_H)
#define __RTC_MEM_HEAPSEQUENCEPROVIDER_H

#include "AbstrSequenceProvider.h"
#include "Parallel.h"

//----------------------------------------------------------------------
// heap_sequence_provider
//----------------------------------------------------------------------

RTC_CALL void throwInsertError(SizeT seqSize, SizeT n);

template <typename V>
class heap_sequence_provider : public abstr_sequence_provider<V>
{
	using typename abstr_sequence_provider<V>::alloc_t;

	typedef typename sequence_traits<V>::value_type     value_type;
	typedef typename param_type<value_type>::type       param_t;
	typedef typename sequence_traits<V>::container_type container_t; 
	typedef typename sequence_obj<V>::iterator          iterator;

public:
//	override abstr_sequence_provider
	void Destroy() override;

	void Open(alloc_t& seq, SizeT nrElem, dms_rw_mode rwMode, bool isTmp, SafeFileWriterArray* sfwa MG_DEBUG_ALLOCATOR_SRC_ARG) override
	{
		dms_assert(rwMode != dms_rw_mode::unspecified);
		if (rwMode == dms_rw_mode::write_only_mustzero)
			clear(seq);
		if (IsDefined(nrElem))
			resizeSP(seq, nrElem, rwMode != dms_rw_mode::write_only_all MG_DEBUG_ALLOCATOR_SRC_PARAM);
	}
	SizeT Size    (const alloc_t& seq) const override { return seq.size    (); }
	SizeT Capacity(const alloc_t& seq) const override { return seq.capacity(); }

	void reserve (alloc_t& seq, SizeT newSize MG_DEBUG_ALLOCATOR_SRC_ARG) override;
	void resizeSP(alloc_t& seq, SizeT newSize, bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG) override;
	void cut     (alloc_t& seq, SizeT newSize) override;
	void clear   (alloc_t& seq) override;
	void free    (alloc_t& seq) override  { clear(seq); }
	
	bool CanWrite() const override { return true; }
//	bool IsOpen()   const override { return true; }
	bool IsHeapAllocated() const override   { return true; }

	abstr_sequence_provider<IndexRange<SizeT> >* CloneForSeqs() const override;
	static abstr_sequence_provider<V>* CreateProvider();
	static SizeT max_size();

private:
	heap_sequence_provider()  {}
	~heap_sequence_provider() {}
	heap_sequence_provider(const heap_sequence_provider&); // Dont call

	void Shrink(alloc_t& seq);

	void SetSize(alloc_t& seq, SizeT newSize) override
	{
		seq.setsize(newSize);
		dms_assert(seq.size() == newSize);
	}
	void Check(alloc_t& seq)
	{
		dms_assert(seq.size() <= seq.m_Capacity);
	}

	template <typename Iter    > static iterator iter(Iter               i) { return &*i; }
	template <typename T       > static iterator iter(T*                 p) { return   p; }
	template <typename T       > static iterator iter(SA_Iterator<T>     i) { return i; }
	template <int N, typename B> static iterator iter(bit_iterator<N, B> i) { return i; }
};


#endif // __RTC_MEM_HEAPSEQUENCEPROVIDER_H
