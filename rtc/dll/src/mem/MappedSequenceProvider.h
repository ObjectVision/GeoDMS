// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_MEM_MAPPEDSEQUENCEPROVIDER_H)
#define __RTC_MEM_MAPPEDSEQUENCEPROVIDER_H

#include "mem/AbstrSequenceProvider.h"
#include "set/FileView.h"

//----------------------------------------------------------------------
// allocators for vectors
//----------------------------------------------------------------------

template <typename V>
class mappable_sequence : public abstr_sequence_provider<V>
{
public:
	mappable_sequence(std::shared_ptr<MappedFileHandle> mfh, tile_id t, SizeT nrElem, dms::filesize_t offset = -1, dms::filesize_t capacity = -1)
		: m_FileView(mfh, nrElem, offset, capacity)
	{
		m_FileView.m_TileID = t;
	}

	using alloc_t = typename abstr_sequence_provider<V>::alloc_t;
//	override abstr_sequence_provider
	void Destroy() override { delete this; }

	SizeT Size    (const alloc_t& seq) const override { return m_FileView.filed_size(); }
	SizeT Capacity(const alloc_t& seq) const override { return m_FileView.filed_capacity(); }

	void reserve(alloc_t& seq, SizeT newCapacity MG_DEBUG_ALLOCATOR_SRC_ARG) override
	{ 
		// TODO: look in memPageAllocTable for sufficiently large holes.
		// TODO: consider growing in-place when no other chunks are allocated beyond this one
 		Check(seq); 
		auto mappedFile = m_FileView.GetMappedFile();
		MG_CHECK(mappedFile);
		if (m_FileView.m_TileID != no_tile)

		if (m_FileView.m_ViewSpec.offset + m_FileView.m_ViewSpec.capacity == mappedFile->m_AllocatedSize)
		{
			m_FileView.UnmapView(); // just save on virtual address space while one can
			mappedFile->m_AllocatedSize -= m_FileView.m_ViewSpec.capacity;
			m_FileView.m_ViewSpec = mappedFile->allocAtEnd(m_FileView.m_ViewSpec.size, newCapacity);
			m_FileView.MapView(true);
		}
		else
		{
			auto oldView = std::move(m_FileView);
			m_FileView = rw_file_view<V>(mappedFile, m_FileView.filed_size(), -1, newCapacity);
			m_FileView.m_TileID = oldView.m_TileID;
			m_FileView.MapView(true);
			fast_copy(oldView.begin(), oldView.end(), m_FileView.begin());
//			oldView.UnmapView();
		}

		GetSeq(seq); 
		Check(seq); 

		if (auto memPageAllocTable = mappedFile->m_MemPageAllocTable.get())
		{
			tile_id t = m_FileView.m_TileID;
			assert(t < memPageAllocTable->filed_size());
			(*memPageAllocTable)[t] = m_FileView.GetViewSpec();
			assert((*memPageAllocTable)[t].size <= (*memPageAllocTable)[t].capacity);
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

	abstr_sequence_provider<IndexRange<SizeT> >* CloneForSeqs() const override
	{
		throwIllegalAbstract(MG_POS, "mappable_sequence.CloneForSeqs");
		/*
		return new mappable_sequence< IndexRange<SizeT> >(m_FileView.GetMappedFile(), this->m_TileId, 0);
		*/
	}

	void Lock  (alloc_t& seq, dms_rw_mode rwMode) override { m_FileView.MapView(rwMode != dms_rw_mode::read_only); assert(  m_FileView.IsUsable() ); GetSeq(seq);}
	void UnLock(alloc_t& seq)                     override 
	{ 
//		m_FileView.resize(seq.size()); 
		m_FileView.UnmapView();
		seq = alloc_t();
	}

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
	rw_file_view<V> m_FileView;
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

	mappable_const_sequence(std::shared_ptr<ConstMappedFileHandle> cmfh, tile_id t, SizeT nrElem, dms::filesize_t fileOffset = -1, dms::filesize_t viewCapacity = -1)
		: m_FileView(cmfh, nrElem, fileOffset, viewCapacity)
	{
		m_FileView.m_TileID = t;
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

	abstr_sequence_provider<IndexRange<SizeT> >* CloneForSeqs() const override
	{
		throwIllegalAbstract(MG_POS, "mappable_const_sequence.CloneForSeqs");
	}
	void Lock  (alloc_t& seq, dms_rw_mode rwMode) override { assert(rwMode == dms_rw_mode::read_only); m_FileView.MapView(); assert( m_FileView.IsUsable()); GetSeq(seq); }
	void UnLock(alloc_t& seq)                     override { m_FileView.UnmapView(); seq = alloc_t(); }

	SharedStr GetFileName() const override { return m_FileView.GetMappedFile()->GetFileName(); }

private:
	void GetSeq(alloc_t& seq)
	{
		seq = alloc_t(mutable_iter(m_FileView.begin()), mutable_iter(m_FileView.end()), m_FileView.filed_capacity() );
	}

	mutable const_file_view<V>   m_FileView;
};


#endif // __RTC_MEM_MAPPEDSEQUENCEPROVIDER_H
