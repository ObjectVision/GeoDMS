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

#if !defined(__RTC_MEM_MAPPEDSEQUENCEPROVIDER_H)
#define __RTC_MEM_MAPPEDSEQUENCEPROVIDER_H

#include "mem/AbstrSequenceProvider.h"
#include "ser/SafeFileWriter.h"
#include "set/FileView.h"


//----------------------------------------------------------------------
// allocators for vectors
//----------------------------------------------------------------------

template <typename V>
class mappable_sequence : public abstr_sequence_provider<V>
{
public:
	mappable_sequence(WeakStr fileName)
		:	m_FileName(fileName)
	{}

	~mappable_sequence()
	{
		dms_assert(!IsOpen());
	}
	using alloc_t = typename abstr_sequence_provider<V>::alloc_t;
//	override abstr_sequence_provider
	void Destroy() override { delete this; }

	SizeT Size    (const alloc_t& seq) const override { return m_FileView.filed_size(); }
	SizeT Capacity(const alloc_t& seq) const override { return m_FileView.filed_capacity(); }

	void reserve(alloc_t& seq, SizeT newSize MG_DEBUG_ALLOCATOR_SRC_ARG) override
	{ 
		Check(seq); 
		m_FileView.reserve(newSize, m_FileName, m_SFWA); 
		GetSeq(seq); 
		Check(seq); 
	}
	void resizeSP(alloc_t& seq, SizeT newSize, bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG) override
	{ 
		Check(seq); 
		if (newSize > seq.m_Capacity)
			reserve(seq, Max<SizeT>(newSize, seq.m_Capacity + Min<SizeT>(seq.m_Capacity, 1024*1024)) MG_DEBUG_ALLOCATOR_SRC_PARAM);

		dms_assert(newSize <= seq.m_Capacity);

		auto newEnd = seq.begin() + newSize;
		if (newSize < seq.size())
			destroy_range(newEnd, seq.end());
		m_FileView.resize( newSize );
		if (newSize > seq.size())
			raw_awake_or_init(seq.end(), newEnd, mustClear);

		GetSeq(seq);
		dms_assert (seq.size() == newSize);
		Check(seq);
	}
	void SetSize(alloc_t& seq, SizeT newSize) override
	{
		Check(seq);
		m_FileView.resize(newSize);
		GetSeq(seq);
		Check(seq);
	}
	void cut(alloc_t& seq, SizeT newSize) override { dms_assert(newSize <= seq.size());  destroy_range(seq.begin() + newSize, seq.end()); SetSize(seq, newSize);  }
	void clear  (alloc_t& seq) override  { cut(seq, 0); }
	void free   (alloc_t& seq) override  { if (IsOpen()) Drop(seq); } 

	bool CanWrite() const override { return true; }

	bool IsOpen  () const override { return m_FileView.IsOpen  (); }
	abstr_sequence_provider<IndexRange<SizeT> >* CloneForSeqs() const override
	{
		return new mappable_sequence< IndexRange<SizeT> >(m_FileName + SEQ_SUFFIX);
	}

	void Open(alloc_t& seq, SizeT nrElem, dms_rw_mode rwMode, bool isTmp, SafeFileWriterArray* sfwa MG_DEBUG_ALLOCATOR_SRC_ARG) override
	{ 
		dms_assert(!IsOpen()); 

		dms_assert(sfwa);
		dms_assert(!isTmp || !sfwa->FindExisting(m_FileName.c_str()));
		m_SFWA  = isTmp ? nullptr : sfwa;

		m_FileView.Open(m_FileName, m_SFWA, nrElem, rwMode, isTmp); 
		dms_assert( IsOpen());
		dms_assert(!m_FileView.IsMapped());
	}
	void Lock  (alloc_t& seq, dms_rw_mode rwMode) override { m_FileView.Map(rwMode, m_FileName, m_SFWA); dms_assert(  m_FileView.IsUsable() ); GetSeq(seq);}
	void UnLock(alloc_t& seq)                     override 
	{ 
		m_FileView.resize(seq.size()); 

#if defined(DMS_CLOSING_UNMAP)
		m_FileView.SetFileSize( size_calculator<V>().nr_bytes(seq.size()) );
#endif
		m_FileView.Unmap();
		dms_assert( !m_FileView.IsMapped() );
		seq = alloc_t();
	}
	void Close (alloc_t& seq)                     override { m_FileView.CloseWFV();                      dms_assert( !m_FileView.IsOpen  () ); seq = alloc_t();}
	void Drop  (alloc_t& seq)                     override { m_FileView.Drop(m_FileName);                dms_assert( !m_FileView.IsOpen  () ); seq = alloc_t();}

	WeakStr GetFileName() const override { return m_FileName; }

private:
	void Grow(alloc_t& seq, SizeT newSize)
	{
		if (newSize > seq.m_Capacity)
			reserve(seq, Max<UInt32>(seq.m_Capacity*2, newSize));
	}
	void GetSeq(alloc_t& seq)
	{
		seq = alloc_t(m_FileView.begin(), m_FileView.end(), m_FileView.filed_capacity() );
	}
	void Check(alloc_t& seq)
	{
		dms_assert(m_FileView.IsUsable());
		dms_assert(m_FileView.begin() == seq.begin());
		dms_assert(m_FileView.end  () == seq.end  ());
		dms_assert(m_FileView.filed_size()     == seq.size());
		dms_assert(m_FileView.filed_size()     <= m_FileView.filed_capacity());
		dms_assert(m_FileView.filed_capacity() == seq.capacity());
	}
private:
	rw_file_view<V>      m_FileView;
	SharedStr            m_FileName;
	SafeFileWriterArray* m_SFWA = nullptr;
};

template <typename V> V* mutable_iter(const V* ptr) { return const_cast<V*>(ptr); }
template <int N, typename Block> bit_iterator<N, Block> mutable_iter(const bit_iterator<N, const Block>& ptr)
{
	return bit_iterator<N, Block>(const_cast<Block*>(ptr.data_begin()), ptr.nr_elem()); 
}

template <typename V>
struct mappable_const_sequence : abstr_sequence_provider<V>
{
	using alloc_t = typename abstr_sequence_provider<V>::alloc_t;

	mappable_const_sequence(WeakStr fileName)
		: m_FileName(fileName)
	{}

	~mappable_const_sequence()
	{ 
		dms_assert(!IsOpen()); // InterestCounts must have been 
	}

//	override abstr_sequence_provider
	void Destroy()                             override { delete this; }

	SizeT Size    (const alloc_t& seq) const override { return m_FileView.filed_size(); }
	SizeT Capacity(const alloc_t& seq) const override { return m_FileView.filed_capacity(); }
	void SetSize(alloc_t& seq, SizeT newSize) override { throwIllegalAbstract(MG_POS, "mappable_const_sequence.SetSize"); }

	void reserve(alloc_t& seq, SizeT newSize MG_DEBUG_ALLOCATOR_SRC_ARG) override { throwIllegalAbstract(MG_POS, "mappable_const_sequence.reserve"); }
	void resizeSP(alloc_t& seq, SizeT newSize, bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG) override { throwIllegalAbstract(MG_POS, "mappable_const_sequence.resize"); }
	void cut(alloc_t& seq, SizeT newSize) override { throwIllegalAbstract(MG_POS, "mappable_const_sequence.cut"); }
	void clear  (alloc_t& seq)                override { throwIllegalAbstract(MG_POS, "mappable_const_sequence.clear"); }
	void free   (alloc_t& seq)                override { dms_assert(!IsOpen()); if (IsOpen()) Close(seq); } 

	bool CanWrite() const override { return false; }

	bool IsOpen  () const override { return m_FileView.IsOpen  (); }
	abstr_sequence_provider<IndexRange<SizeT> >* CloneForSeqs() const override
	{
		return new mappable_const_sequence<IndexRange<SizeT> >(m_FileName + SEQ_SUFFIX);
	}

	void Open(alloc_t& seq, SizeT nrElem, dms_rw_mode rwMode, bool isTmp, SafeFileWriterArray* sfwa MG_DEBUG_ALLOCATOR_SRC_ARG) override
	{ 
		dms_assert(!IsOpen()); 
		dms_assert(rwMode == dms_rw_mode::read_only || rwMode == dms_rw_mode::check_only); // const sequence
		dms_assert(!isTmp || rwMode != dms_rw_mode::check_only);

		dms_assert(sfwa);
		dms_assert(!isTmp || !sfwa->FindExisting(m_FileName.c_str()));
		m_SFWA  = isTmp ? nullptr : sfwa;

		m_FileView.Open(m_FileName, m_SFWA, nrElem);
		dms_assert((!IsDefined(nrElem)) || nrElem == m_FileView.filed_size());
		dms_assert(!m_FileView.IsMapped());
	}
	void Lock  (alloc_t& seq, dms_rw_mode rwMode) override { dms_assert(rwMode == dms_rw_mode::read_only); m_FileView.Map(rwMode, m_FileName, m_SFWA); dms_assert( m_FileView.IsUsable()); GetSeq(seq); }
	void UnLock(alloc_t& seq)                     override { m_FileView.Unmap();                        dms_assert(!m_FileView.IsMapped()); seq = alloc_t(); }
	void Close (alloc_t& seq)                     override { m_FileView.CloseFVB();                     dms_assert(!m_FileView.IsOpen());   seq = alloc_t(); }
	void Drop  (alloc_t& seq)                     override { m_FileView.Drop (m_FileName);              dms_assert(!m_FileView.IsOpen());   seq = alloc_t(); }

	WeakStr GetFileName() const override { return m_FileName; }

private:
	void GetSeq(alloc_t& seq)
	{
		seq = alloc_t(mutable_iter(m_FileView.begin()), mutable_iter(m_FileView.end()), m_FileView.filed_capacity() );
	}

	const_file_view<V>   m_FileView;
	SharedStr            m_FileName;
	SafeFileWriterArray* m_SFWA = nullptr;
};


#endif // __RTC_MEM_MAPPEDSEQUENCEPROVIDER_H
