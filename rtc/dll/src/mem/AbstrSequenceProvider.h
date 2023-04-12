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

#if !defined(__RTC_MEM_ABSTRSEQUENCEPROVIDER_H)
#define __RTC_MEM_ABSTRSEQUENCEPROVIDER_H

#include "mem/AllocData.h"
#include "mem/FixedAlloc.h"
#include "ptr/SharedStr.h"
#include "ser/FileCreationMode.h"

struct SafeFileWriterArray;

//----------------------------------------------------------------------
// interfaces to abstr_sequence_provider
//----------------------------------------------------------------------
 typedef UInt32 SaSizeT;

template <typename V>
class abstr_sequence_provider : private boost::noncopyable
{
public:
	typedef typename sequence_traits<V>::value_type    value_type;

	typedef typename sequence_traits<V>::seq_t         seq_t;
	typedef typename sequence_traits<V>::cseq_t        cseq_t;

	typedef typename seq_t::iterator                   iterator;
	typedef typename cseq_t::const_iterator            const_iterator;

	typedef typename param_type<value_type>::type      param_t;
	typedef alloc_data<V>                              alloc_t;

	virtual ~abstr_sequence_provider() {}
	virtual void Destroy() = 0;

	virtual SizeT Size(const alloc_t& seq) const = 0;
	virtual SizeT Capacity(const alloc_t& seq) const = 0;

	virtual bool CanWrite() const = 0;
	virtual bool IsOpen()   const = 0;
	virtual bool IsHeapAllocated() const { return false; }

	virtual abstr_sequence_provider<IndexRange<SizeT> >* CloneForSeqs() const = 0;
	virtual void Open(alloc_t& seq, SizeT nrElem, dms_rw_mode rwMode, bool isTmp, SafeFileWriterArray* sfwa MG_DEBUG_ALLOCATOR_SRC_ARG) { throwIllegalAbstract(MG_POS, "Open"); }
	virtual void Close(alloc_t& seq) { throwIllegalAbstract(MG_POS, "Close"); }
	virtual void Lock(alloc_t& seq, dms_rw_mode rwMode) { }
	virtual void UnLock(alloc_t& seq) { }
	virtual void Drop(alloc_t& seq) { throwIllegalAbstract(MG_POS, "Drop"); }
	virtual WeakStr GetFileName() const { throwIllegalAbstract(MG_POS, "GetFileName"); }

	// the following lower case named functions require IsOpen() and Lock()
	virtual void SetSize(alloc_t& seq, SizeT newSize) = 0;
	virtual void reserve(alloc_t& seq, SizeT newSize MG_DEBUG_ALLOCATOR_SRC_ARG) = 0; // reserve space
	virtual void resizeSP(alloc_t& seq, SizeT newSize, bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG) = 0;
	virtual void cut(alloc_t& seq, SizeT newSize) = 0;
	virtual void clear(alloc_t& seq) = 0; // make sequence empty
	virtual void free(alloc_t& seq) = 0; // discard reserved space

	void grow(alloc_t& seq, SizeT n, bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG) // grow sequence, reserve extra space if neccesary; maintain old sequence is maintained.
	{
		resizeSP(seq, seq.size() + n, mustClear MG_DEBUG_ALLOCATOR_SRC_PARAM);
	} 
};


#endif // __RTC_MEM_ABSTRSEQUENCEPROVIDER_H
