// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

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
	mappable_sequence(std::shared_ptr<MappedFileHandle> mfh, tile_id t, tile_offset nrElem)
		: m_FileView(mfh, nrElem)
	{
		this->m_TileId = t;
	}

	using alloc_t = typename abstr_sequence_provider<V>::alloc_t;
//	override abstr_sequence_provider
	void Destroy() override { delete this; }

	SizeT Size    (const alloc_t& seq) const override { return m_FileView.filed_size(); }
	SizeT Capacity(const alloc_t& seq) const override { return m_FileView.filed_capacity(); }

	void reserve(alloc_t& seq, SizeT newSize MG_DEBUG_ALLOCATOR_SRC_ARG) override
	{ 
		// TODO: look in memPageAllocTable for sufficiently large holes.
		Check(seq); 
		m_FileView.reserve(newSize); 
		GetSeq(seq); 
		Check(seq); 
		if (auto mappedFile = m_FileView.GetMappedFile())
			if (auto memPageAllocTable = mappedFile->m_MemPageAllocTable.get())
			{
				tile_id t = this->m_TileId;
				assert(t < memPageAllocTable->filed_size());
				(*memPageAllocTable)[t] = m_FileView.GetViewSpec();
			}
	}
	void resizeSP(alloc_t& seq, SizeT newSize, bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG) override
	{ 
		Check(seq); 
		if (newSize > seq.m_Capacity)
			reserve(seq, Max<SizeT>(newSize, seq.m_Capacity + Min<SizeT>(seq.m_Capacity, 1024*1024)) MG_DEBUG_ALLOCATOR_SRC_PARAM);

		assert(newSize <= seq.m_Capacity);

		auto newEnd = seq.begin() + newSize;
		if (newSize < seq.size())
			destroy_range(newEnd, seq.end());
		m_FileView.resize( newSize );
		if (newSize > seq.size())
			raw_awake_or_init(seq.end(), newEnd, mustClear);

		GetSeq(seq);
		assert (seq.size() == newSize);
		Check(seq);
	}
	void SetSize(alloc_t& seq, SizeT newSize) override
	{
		Check(seq);
		m_FileView.resize(newSize);
		GetSeq(seq);
		Check(seq);
	}
	void cut(alloc_t& seq, SizeT newSize) override { assert(newSize <= seq.size());  destroy_range(seq.begin() + newSize, seq.end()); SetSize(seq, newSize);  }
	void clear  (alloc_t& seq) override  { cut(seq, 0); }
	void free(alloc_t& seq) override { m_FileView = rw_file_view<V>(); }

	bool CanWrite() const override { return true; }

//	bool IsOpen  () const override { return m_FileView.IsOpen  (); }

	abstr_sequence_provider<IndexRange<SizeT> >* CloneForSeqs() const override
	{
		return new mappable_sequence< IndexRange<SizeT> >(m_FileView.GetMappedFile(), this->m_TileId, 0);
	}

	/*

	void Open(alloc_t& seq, SizeT nrElem, dms_rw_mode rwMode, bool isTmp, SafeFileWriterArray* sfwa MG_DEBUG_ALLOCATOR_SRC_ARG) override
	{ 
		assert(!IsOpen()); 

		assert(sfwa);
		assert(!isTmp || !sfwa->FindExisting(m_FileName.c_str()));
		m_SFWA  = isTmp ? nullptr : sfwa;

		m_FileView.Open(m_FileName, m_SFWA, nrElem, rwMode, isTmp); 
		assert( IsOpen());
		assert(!m_FileView.IsMapped());
	}
	*/

	void Lock  (alloc_t& seq, dms_rw_mode rwMode) override { m_FileView.Map(rwMode != dms_rw_mode::read_only); assert(  m_FileView.IsUsable() ); GetSeq(seq);}
	void UnLock(alloc_t& seq)                     override 
	{ 
		m_FileView.resize(seq.size()); 
		m_FileView.Unmap();
		seq = alloc_t();
	}
//	void Close (alloc_t& seq)                     override { m_FileView.CloseWFV();                      assert( !m_FileView.IsOpen  () ); seq = alloc_t();}
//	void Drop  (alloc_t& seq)                     override { m_FileView.Drop(m_FileName);                assert( !m_FileView.IsOpen  () ); seq = alloc_t();}

	SharedStr GetFileName() const override { return m_FileView.GetMappedFile()->GetFileName(); }

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
		assert(m_FileView.IsUsable());
		assert(m_FileView.begin() == seq.begin());
		assert(m_FileView.end  () == seq.end  ());
		assert(m_FileView.filed_size()     == seq.size());
		assert(m_FileView.filed_size()     <= m_FileView.filed_capacity());
		assert(m_FileView.filed_capacity() == seq.capacity());
	}
private:
	tile_id              m_TileId = tile_id(-1);
	rw_file_view<V>      m_FileView;
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

	mappable_const_sequence(std::shared_ptr<ConstMappedFileHandle> cmfh, tile_id t, tile_offset nrElem, dms::filesize_t fileOffset = -1, dms::filesize_t viewSize = -1)
		: m_FileView(cmfh, nrElem, fileOffset, viewSize)
	{
		this->m_TileId = t;
	}

	~mappable_const_sequence()
	{}

//	override abstr_sequence_provider
	void Destroy()                             override { delete this; }

	SizeT Size    (const alloc_t& seq) const override { return m_FileView.filed_size(); }
	SizeT Capacity(const alloc_t& seq) const override { return m_FileView.filed_capacity(); }
	void SetSize  (alloc_t& seq, SizeT newSize) override { throwIllegalAbstract(MG_POS, "mappable_const_sequence.SetSize"); }

	void reserve (alloc_t& seq, SizeT newSize MG_DEBUG_ALLOCATOR_SRC_ARG) override { throwIllegalAbstract(MG_POS, "mappable_const_sequence.reserve"); }
	void resizeSP(alloc_t& seq, SizeT newSize, bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG) override { throwIllegalAbstract(MG_POS, "mappable_const_sequence.resize"); }
	void cut     (alloc_t& seq, SizeT newSize) override { throwIllegalAbstract(MG_POS, "mappable_const_sequence.cut"); }
	void clear   (alloc_t& seq)                override { throwIllegalAbstract(MG_POS, "mappable_const_sequence.clear"); }
	void free    (alloc_t& seq)                override { m_FileView = const_file_view<V>(); }

	bool CanWrite() const override { return false; }

//	bool IsOpen  () const override { return m_FileView.IsOpen  (); }
	abstr_sequence_provider<IndexRange<SizeT> >* CloneForSeqs() const override
	{
		const auto& pageRange = m_FileView.SequenceMemPageAllocTable()->begin()[this->m_TileId];
		return new mappable_const_sequence<IndexRange<SizeT> >(m_FileView.GetMappedFile()
			, this->m_TileId
			, m_FileView.filed_size()
			, m_FileView.GetViewOffset()
			, m_FileView.GetViewSize()
		);
		m_FileView = const_file_view<V>(m_FileView.GetMappedFile(), pageRange.first, pageRange.second);
	}
/*
	void Open(alloc_t& seq, SizeT nrElem, dms_rw_mode rwMode, bool isTmp, SafeFileWriterArray* sfwa MG_DEBUG_ALLOCATOR_SRC_ARG) override
	{ 
		assert(!IsOpen()); 
		assert(rwMode == dms_rw_mode::read_only || rwMode == dms_rw_mode::check_only); // const sequence
		assert(!isTmp || rwMode != dms_rw_mode::check_only);

		assert(sfwa);
		assert(!isTmp || !sfwa->FindExisting(m_FileName.c_str()));
		m_SFWA  = isTmp ? nullptr : sfwa;

		m_FileView.Open(m_FileName, m_SFWA, nrElem);
		assert((!IsDefined(nrElem)) || nrElem == m_FileView.filed_size());
		assert(!m_FileView.IsMapped());
	}
*/
	void Lock  (alloc_t& seq, dms_rw_mode rwMode) override { assert(rwMode == dms_rw_mode::read_only); m_FileView.Map(); assert( m_FileView.IsUsable()); GetSeq(seq); }
	void UnLock(alloc_t& seq)                     override { m_FileView.Unmap(); seq = alloc_t(); }
//	void Close (alloc_t& seq)                     override { m_FileView.CloseFVB();                     assert(!m_FileView.IsOpen());   seq = alloc_t(); }
//	void Drop  (alloc_t& seq)                     override { m_FileView.Drop (m_FileName);              assert(!m_FileView.IsOpen());   seq = alloc_t(); }

	SharedStr GetFileName() const override { return m_FileView.GetMappedFile()->GetFileName(); }

private:
	void GetSeq(alloc_t& seq)
	{
		seq = alloc_t(mutable_iter(m_FileView.begin()), mutable_iter(m_FileView.end()), m_FileView.filed_capacity() );
	}

	tile_id              m_TileId = tile_id(-1);
	mutable const_file_view<V>   m_FileView;
//	SharedStr            m_FileName;
	SafeFileWriterArray* m_SFWA = nullptr;
};


#endif // __RTC_MEM_MAPPEDSEQUENCEPROVIDER_H
