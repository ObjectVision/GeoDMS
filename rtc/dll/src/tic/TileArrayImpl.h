// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__TIC_TILEARRAYIMPL_H)
#define __TIC_TILEARRAYIMPL_H

#include "DataArray.h"
//----------------------------------------------------------------------
// class  : HeapTileArray
//----------------------------------------------------------------------

#include "DataLocks.h"
#include "dbg/DebugCast.h"
#include "mem/FixedAlloc.h"
#include "mem/tiledata.h"
#include "ser/FileMapHandle.h"

#include <memory>
#include <mutex>

template <typename V>
struct HeapTileArray : GeneratedTileFunctor<V>
{
	using typename TileFunctor<V>::locked_cseq_t;
	using typename TileFunctor<V>::locked_seq_t;

	using tiles_t = OwningPtrSizedArray<std::shared_ptr<tile<V>>>;

	HeapTileArray(const AbstrTileRangeData* trd, bool mustClear); // create heap stuff

	bool IsMemoryObject() const override { return true; }

	auto GetWritableTile(tile_id t, dms_rw_mode rwMode) ->locked_seq_t override;
	auto GetTile(tile_id t) const ->locked_cseq_t override;
	mutable tiles_t m_Seqs;
	mutable std::unique_ptr<std::once_flag[]> m_TileInitFlags; // one per tile, see InitTile
};

template <typename V>
struct HeapSingleArray : GeneratedTileFunctor<V>
{
	using typename TileFunctor<V>::locked_cseq_t;
	using typename TileFunctor<V>::locked_seq_t;

	using tile_t = typename sequence_traits<V>::tile_container_type;

	HeapSingleArray(const AbstrTileRangeData* trd, bool mustClear);

	bool IsMemoryObject() const override { return true; }

	auto GetWritableTile(tile_id t, dms_rw_mode rwMode)->locked_seq_t override;
	auto GetTile(tile_id t) const->locked_cseq_t override;

	tile_t m_Seq;

#if defined(MG_DEBUG)
	// Whether m_Seq was zeroed when the ctor allocated it, i.e. whether the DataWriteLock that
	// created this object was opened with write_only_mustzero. Diagnostics only -- see the
	// contradiction check in GetWritableTile. Absent from release builds, so sizeof() is unchanged
	// there and AbstrDataObject stays untouched for every other data-object kind.
	bool md_WasZeroed = false;
#endif
};


template <typename T> struct blocktype_of { using type = T; };
template <bit_size_t N> struct blocktype_of<bit_value<N> > { using type = typename sequence_traits<bit_value<N>>::block_type; };

template <typename T> using blocktype_of_t = blocktype_of<T>::type;

template <typename V>
struct HeapSingleValue : GeneratedTileFunctor<V>
{
	using typename TileFunctor<V>::locked_cseq_t;
	using typename TileFunctor<V>::locked_seq_t;
	using blockvalue_t = blocktype_of_t<V>;

	HeapSingleValue(const AbstrTileRangeData* trd); // create heap stuff

	bool IsMemoryObject() const override { return true; }

	auto GetWritableTile(tile_id t, dms_rw_mode rwMode)->locked_seq_t override;
	auto GetTile(tile_id t) const->locked_cseq_t override;

	blockvalue_t m_Value = blockvalue_t();
};


template <typename V>
struct FileTileArray : GeneratedTileFunctor<V>
{
	using typename TileFunctor<V>::locked_cseq_t;
	using typename TileFunctor<V>::locked_seq_t;

	using files_t = OwningPtrSizedArray<file_tile<V>>;

	FileTileArray(const AbstrTileRangeData* domain, SharedStr filenameBase, dms_rw_mode rwMode, bool isTmp);

	locked_seq_t GetWritableTile(tile_id t, dms_rw_mode rwMode) override;
	locked_cseq_t GetTile(tile_id t) const override;

	// The data is retained in the cache file; each file_tile keeps only a weak ref to its mapping,
	// so RAM holds live mappings and a re-request re-maps instead of recomputing.
	materialization GetMaterialization() const override { return materialization::spilled; }

	// Tiles with a live mapping. m_NrMappedFileTiles is the mapping refcount maintained by
	// mapped_file_tile's ctor/dtor, so a plain read tells us residency without touching cs_file and
	// without creating a mapping (which file_tile::get() would). Reading it unsynchronised is
	// deliberate: this feeds a resource estimate, and a tile mapped or unmapped concurrently is
	// exactly as true a moment later. Never calls GetTile().
	tile_id GetNrResidentTilesNow() const override
	{
		auto trd = this->GetTiledRangeData();
		tile_id tn = trd ? trd->GetNrTiles() : 0, resident = 0;
		for (tile_id t = 0; t != tn; ++t)
			if (m_Files[t].m_NrMappedFileTiles)
				++resident;
		return resident;
	}

	void FinishWrite() override;

	SharedStr m_CacheFileName;
	files_t m_Files;
	// No m_IsTmp here: the ctor hands isTmp straight to MappedFileHandle::OpenRw, which owns the
	// delete-on-close semantics (see ser/FileMapHandle.h).

	// The .seq file of a sequence attribute that was opened for writing, which FinishWrite seals (#1280).
	std::shared_ptr<MappedFileHandle> m_SeqFile;

private:
	void SealSequences();
	void ResetSeqProvider(tile_id t, dms::filesize_t offset, SizeT nrElems);
};

//----------------------------------------------------------------------
// HeapTileArray
//----------------------------------------------------------------------
#include "mem/HeapSequenceProvider.h"

template <typename V>
HeapTileArray<V>::HeapTileArray(const AbstrTileRangeData* trd, bool mustClear)
{
	dms_assert(trd);
	this->m_TileRangeData = trd;
	tile_id tn = trd->GetNrTiles();

	tiles_t seqs(tn, value_construct MG_DEBUG_ALLOCATOR_SRC_EMPTY);
	m_Seqs = std::move(seqs);
	m_TileInitFlags = std::make_unique<std::once_flag[]>(tn);
}

// A tile is allocated by its first access and never reset. call_once runs that allocation, and the zeroing, outside
// any shared lock, and its completion makes the pointer safe to read without one. Every access to a tile of any heap
// tile array of the process used to take one std::mutex, and the first one allocated and zeroed under it, so a
// parallel_tileloop that writes a tiled result serialised its tile allocations (TIC-A06). A throwing allocation
// leaves the flag unset, and the next access tries again.
template <typename V>
void InitTile(std::once_flag& initFlag, std::shared_ptr<tile<V>>& tilePtr, const AbstrTileRangeData* trd, tile_id t, bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	std::call_once(initFlag, [&]
		{
			auto newTile = std::make_shared<tile<V>>();
			reallocSO(*newTile, trd->GetTileSize(t), mustClear MG_DEBUG_ALLOCATOR_SRC_PARAM);
			tilePtr = std::move(newTile);
		}
	);
	assert(tilePtr);
	assert(tilePtr->size() == trd->GetTileSize(t));
}

template <typename V>
auto HeapTileArray<V>::GetWritableTile(tile_id t, dms_rw_mode rwMode) -> locked_seq_t
{
	assert(t < this->GetTiledRangeData()->GetNrTiles());
	this->CheckFailure();

	auto& tilePtr = m_Seqs[t];
	InitTile(m_TileInitFlags[t], tilePtr, this->GetTiledRangeData().get(), t, rwMode != dms_rw_mode::write_only_all MG_DEBUG_ALLOCATOR_SRC(this->md_SrcStr.c_str()));

	return locked_seq_t(std::static_pointer_cast<void>(tilePtr), GetSeq(*tilePtr));
}

template <typename V>
auto HeapTileArray<V>::GetTile(tile_id t) const -> locked_cseq_t
{
	assert(t < this->GetTiledRangeData()->GetNrTiles());
	this->CheckFailure();

	auto& tilePtr = m_Seqs[t];
	InitTile(m_TileInitFlags[t], tilePtr, this->GetTiledRangeData().get(), t, true MG_DEBUG_ALLOCATOR_SRC(this->md_SrcStr.c_str()));

	return locked_cseq_t(std::static_pointer_cast<const void>(tilePtr), GetConstSeq(*tilePtr));
}

//----------------------------------------------------------------------
// HeapSingleArray
//----------------------------------------------------------------------

template <typename V>
HeapSingleArray<V>::HeapSingleArray(const AbstrTileRangeData* trd, bool mustClear)
{
	assert(trd);
	assert(trd->GetNrTiles() == 1); // PRECONDITION

	this->m_TileRangeData = trd;
	auto tileSize = trd->GetTileSize(0);
	reallocSO(m_Seq, tileSize, mustClear MG_DEBUG_ALLOCATOR_SRC("HeapSingleArray<V>::ctor"));
	MG_DEBUGCODE(md_WasZeroed = mustClear;)
}

template <typename V>
auto HeapSingleArray<V>::GetWritableTile(tile_id t, dms_rw_mode rwMode) -> locked_seq_t
{
	assert(t == 0); // PRECONDITION
	this->CheckFailure();

	auto tileSize = this->GetTiledRangeData()->GetTileSize(0);
	dms_assert(m_Seq.size() == this->GetTiledRangeData()->GetTileSize(0));

	// write_only_mustzero means "zeroed when the storage was allocated", and for this class the
	// allocation already happened in the ctor, from the mode given to the DataWriteLock. So rwMode
	// cannot zero anything now: a caller asking for a zeroed buffer here while the write session was
	// opened write_only_all is stating two contradictory things about one session, and silently gets
	// uninitialised memory to accumulate into -- that was issue #1169 (perimeter) and the same defect
	// in Dijkstra's org-zone aggregates. Untiled results are the only place this is harmful:
	// HeapTileArray/FileTileArray allocate lazily and so do honour the rwMode of the first request,
	// and HeapSingleValue is always value-initialised. Covering GetWritableTile covers every route,
	// since GetDataWrite, GetWritableTileLock and GetDataWriteBegin all funnel through here.
	//
	// Only for fixed-size elements: skipping the zero-fill leaves those indeterminate (raw_awake is
	// a NOP for raw_constructed types, rangefuncs.h). Sequence/string elements go through
	// raw_construct instead, so they are always constructed and an unhonoured mustzero costs nothing
	// -- points2sequence states the same contradiction harmlessly, and should not be flagged.
	if constexpr (has_fixed_elem_size_v<V>)
		MGD_CHECK_OBJ(md_WasZeroed || rwMode != dms_rw_mode::write_only_mustzero);

	return locked_seq_t(std::make_shared<SharedPtr<AbstrDataObject>>(this), GetSeq(m_Seq));
}

template <typename V>
auto HeapSingleArray<V>::GetTile(tile_id t) const -> locked_cseq_t
{
	assert(t == 0); // PRECONDITION
	assert(m_Seq.size() == this->GetTiledRangeData()->GetTileSize(0));
	this->CheckFailure();

	return locked_cseq_t(std::make_shared<SharedPtr<const AbstrDataObject>>(this), GetConstSeq(m_Seq));
}

//----------------------------------------------------------------------
// HeapSingleValue
//----------------------------------------------------------------------

template <typename V>
HeapSingleValue<V>::HeapSingleValue(const AbstrTileRangeData* trd)
{
	dms_assert(trd);
	this->m_TileRangeData = trd;
	dms_assert(trd->GetNrTiles() == 1); // PRECONDITION
	dms_assert(trd->GetRangeSize() == 1); // PRECONDITION
}

template <typename V>
auto HeapSingleValue<V>::GetWritableTile(tile_id t, dms_rw_mode rwMode) -> locked_seq_t
{
	assert(t == 0); // PRECONDITION
	assert(this->GetTiledRangeData()->GetTileSize(t) == 1);
	this->CheckFailure();

	if constexpr (is_bitvalue_v<V>)
	{
		typename sequence_traits<V>::seq_t span(&m_Value, 1);
		return locked_seq_t(std::make_shared<SharedPtr<AbstrDataObject>>(this), span);
	}
	else
	{
		typename sequence_traits<V>::seq_t span(&m_Value, &m_Value);
		++span.second;
		return locked_seq_t(std::make_shared<SharedPtr<AbstrDataObject>>(this), span);
	}
}

template <typename V>
auto HeapSingleValue<V>::GetTile(tile_id t) const -> locked_cseq_t
{
	assert(t == 0); // PRECONDITION
	assert(this->GetTiledRangeData()->GetTileSize(t) == 1);
	this->CheckFailure();

	if constexpr (is_bitvalue_v<V>)
	{
		typename sequence_traits<V>::cseq_t span(&m_Value, 1);
		return locked_cseq_t(std::make_shared<SharedPtr<const AbstrDataObject>>(this), span);
	}
	else
	{
		typename sequence_traits<V>::cseq_t span(&m_Value, &m_Value);
		++span.second;
		return locked_cseq_t(std::make_shared<SharedPtr<const AbstrDataObject>>(this), span);
	}
}

//----------------------------------------------------------------------
// FileTileArray
//----------------------------------------------------------------------

#include "mem/MappedSequenceProvider.h"
#include "vt/mpf.h"
#include "dbg/debug.h"
#include "dbg/DmsCatch.h"
#include "utl/FileSystem.h"
#include "utl/splitPath.h"
#include "utl/StrFormat.h"

SharedStr TileSuffix(tile_id t)
{
	SharedStr result;
	dms_assert(t > 0);
	while (t)
	{
		result += mySSPrintF("/t{:x}.", t % 0x100);
		t /= 0x100;
	}
	return result;
}

template <typename V> SizeT MinimalNrMemPages(const AbstrTileRangeData* trd);

template <fixed_elem V>
SizeT MinimalNrMemPages(const AbstrTileRangeData* trd)
{
	assert(trd);
	return trd->GetNrMemPages(mpf::log2_v<nrbits_of_v<V>>);
}

template <sequence_or_string V>
SizeT MinimalNrMemPages(const AbstrTileRangeData* trd)
{
	assert(trd);
	using seq_t = typename sequence_traits<V>::polymorph_vec_t::seq_t;
	return trd->GetNrMemPages(mpf::log2_v<nrbits_of_v<seq_t>>);
}

SizeT NrAllocTableMemPages(const AbstrTileRangeData* trd)
{
	auto nrTiles = trd->GetNrTiles();
	auto tileFileChunkSize = safe_size_n<nrbits_of_v<FileChunkSpec>>(nrTiles);
	return NrMemPages(tileFileChunkSize);
}

// the .dat file size (not the sequence file) has mappable tile data, 
// thus each tile starts at a new mapping boundary, but the last tile can end directly after the last byte of its last element.

template <fixed_elem V>
SizeT MinimalDatFileSize(const AbstrTileRangeData* trd)
{
	tile_id tn = trd->GetNrTiles();
	SizeT rawSize = 0;
	if (tn > 1)
	{
		rawSize = trd->GetMemPageIndex(mpf::log2_v<nrbits_of_v<V>>, tn-1);
		rawSize <<= GetLog2MemPageSize();
	}
	if (tn > 0)
	{
		// now add the size of last tile (= tn-1) can be smaller than page size
		rawSize += safe_size_n<nrbits_of_v<V>>(trd->GetTileSize(tn - 1));
	}
	return rawSize;
}

template <sequence_or_string V>
SizeT MinimalDatFileSize(const AbstrTileRangeData* trd)
{
	using seq_t = IndexRange<SizeT>; // typename sequence_traits<V>::polymorph_vec_t::seq_t;
	return MinimalDatFileSize<seq_t>(trd);
}

template <typename V>
FileTileArray<V>::FileTileArray(const AbstrTileRangeData* trd, SharedStr filenameBase, dms_rw_mode rwMode, bool isTmp)
	: m_CacheFileName(filenameBase)
{
	this->m_TileRangeData = trd;
	assert(!m_CacheFileName.empty());

	assert(rwMode <= dms_rw_mode::write_only_all);

	assert(trd);
	tile_id tn = trd->GetNrTiles();

	files_t seqs(tn, value_construct MG_DEBUG_ALLOCATOR_SRC_EMPTY);

	assert(!this->m_CacheFileName.empty());
	SharedStr fullFileName = m_CacheFileName; // +getFileNameExtension(fullFileName.c_str());

	if (rwMode <= dms_rw_mode::read_only)
	{
		std::shared_ptr<ConstMappedFileHandle> 
			cmfh = std::make_shared<ConstMappedFileHandle>(fullFileName, true, false)
		,	cmfh_sequences;

		// The element count of a stored array comes from the domain, never from the file, and the
		// mapping call only notices a file that is too SHORT (as ERROR_ACCESS_DENIED from
		// CreateFileMapping, which names neither the domain nor the size). A file that is too LONG
		// used to be accepted silently: the surplus was dropped and the first N elements were handed
		// out as if they belonged to this domain. Since the store is a keyless positional array, any
		// length difference proves it was written against a different instance of the domain, so
		// every value may be attached to the wrong element -- a wrong answer that looks plausible
		// (issue #1187). The writer sizes this file with the very same MinimalDatFileSize<V>(trd), so
		// equality is the right test, and it is the last line of defence for stores whose dictionary
		// carries no domain restriction (everything written before 20.15.0).
		auto expectedFileSize = dms::filesize_t(MinimalDatFileSize<V>(trd));
		auto actualFileSize = cmfh->GetFileSize();
		if (actualFileSize != expectedFileSize)
			throwErrorF("FileTileArray"
				, "stored array '{}' holds {} bytes, but the domain it is read into requires exactly {} bytes"
				  " ({} elements in {} tile(s)). "
				  "The array was written for a different domain; recreate the storage or restore the domain it was written with."
				, fullFileName.c_str(), actualFileSize, expectedFileSize
				, trd->GetElemCount(), tn
			);
		if constexpr (!has_fixed_elem_size_v<V>)
		{
			cmfh_sequences = std::make_shared<ConstMappedFileHandle>(fullFileName + ".seq");
			if (tn > 1)
				CreateMemPageAllocTable(cmfh_sequences, true, tn);
		}
		mempage_table* memPageAllocTable = nullptr; if (cmfh_sequences) memPageAllocTable = cmfh_sequences->m_MemPageAllocTable.get();
		for (tile_id t = 0; t != tn; ++t)
		{
			if constexpr (has_fixed_elem_size_v<V>)
				seqs[t].ResetAllocator(new mappable_const_sequence<V>(cmfh, t, trd->GetTileSize(t)));
			else
			{
				using elem_type = elem_of_t<V>;
				auto ms_index = std::make_unique<mappable_const_sequence<IndexRange<SizeT>>>(cmfh, t, trd->GetTileSize(t));
				FileChunkSpec pageRange = { 0, dms::filesize_t(-1), dms::filesize_t (-1) };
				if (memPageAllocTable)
				{
					pageRange = (*memPageAllocTable)[t];
					pageRange.size = capacity_calculator<elem_type>::Byte2Size(pageRange.size);
				}
				else 
				{
					pageRange.capacity = cmfh_sequences->GetFileSize();
					pageRange.size = capacity_calculator<elem_type>::Byte2Size( pageRange.capacity );
				}
				assert(pageRange.size <= pageRange.capacity);
				auto ms_values = std::make_unique<mappable_const_sequence<elem_type> >(cmfh_sequences, t, pageRange.size, pageRange.offset, pageRange.capacity);
				seqs[t].ResetAllocators(ms_index.release(), ms_values.release());
			}
			MGD_CHECKDATA(!seqs[t].IsLocked());
		}
	}
	else
	{
		std::shared_ptr<MappedFileHandle> mfh = std::make_shared<MappedFileHandle>();
		std::shared_ptr<MappedFileHandle> mfh_sequences;
		mfh->OpenRw(fullFileName, MinimalDatFileSize<V>(trd), rwMode, isTmp);

		if constexpr (!has_fixed_elem_size_v<V>)
		{
			mfh_sequences = std::make_shared<MappedFileHandle>();

			mfh_sequences->OpenRw(fullFileName+".seq", MinimalSeqFileSize(tn), rwMode, isTmp);
			if (tn > 1)
				CreateMemPageAllocTable(mfh_sequences, false, tn);
		}
		mempage_table* memPageAllocTable = nullptr; if (mfh_sequences) memPageAllocTable = mfh_sequences->m_MemPageAllocTable.get();
		for (tile_id t = 0; t != tn; ++t)
		{
			if constexpr (has_fixed_elem_size_v<V>)
				seqs[t].ResetAllocator(new mappable_sequence<V>(mfh, t, trd->GetTileSize(t)));
			else
			{
				using elem_type = elem_of_t<V>;
				auto ms_index  = std::make_unique<mappable_sequence<IndexRange<SizeT>>>(mfh, t, trd->GetTileSize(t));
				FileChunkSpec pageRange = { 0, 0, 0 };
				if (rwMode == dms_rw_mode::read_write)
				{
					pageRange = { 0, dms::filesize_t(-1), dms::filesize_t(-1) };
					if (memPageAllocTable)
					{
						pageRange = (*memPageAllocTable)[t];
						pageRange.size = capacity_calculator<V>::Byte2Size(pageRange.size);
					}
				}
				else if (memPageAllocTable)
					(*memPageAllocTable)[t] = pageRange;

				assert(pageRange.size <= pageRange.capacity);
				auto ms_values = std::make_unique<mappable_sequence<elem_type>>(mfh_sequences, t, pageRange.size, pageRange.offset, pageRange.capacity);
				seqs[t].ResetAllocators(ms_index.release(), ms_values.release());
			}

			MGD_CHECKDATA(!seqs[t].IsLocked());
		}
		m_SeqFile = std::move(mfh_sequences);
	}
	m_Files = std::move(seqs);
}

template <typename V>
void FileTileArray<V>::FinishWrite()
{
	if constexpr (!has_fixed_elem_size_v<V>)
		if (m_SeqFile)
			SealSequences();
}

// #1280, #1294: a stored pool grows by doubling, and in a domain of more than one tile its chunk also moves
// to a free place in the .seq file when it outgrows its place, leaving the old chunk behind. Once the write
// is done, the file is cut back to what the sequences refer to, whenever it holds even one byte more: a pool
// of a domain of one tile that never moved is truncated, any other file is written afresh, chunk table first
// and then the pools in tile order without the abandoned elements, which also gives the same bytes for the
// same values. A file without waste is left as it is, whatever the order of its chunks and of the sequences
// in them. A failure before the new file is in place keeps the old one, which is complete, and only warns.
template <typename V>
void FileTileArray<V>::SealSequences()
{
	using elem_type = elem_of_t<V>;
	using seq_t = typename file_tile<V>::seq_t;

	tile_id tn = this->GetTiledRangeData()->GetNrTiles();
	if (!tn)
		return;
	if (tn > 1 && !m_SeqFile->m_MemPageAllocTable)
		return; // no chunk table: the Linux allocation, which puts every grown chunk at the end (#1297)

	for (tile_id t = 0; t != tn; ++t)
		if (m_Files[t].m_NrMappedFileTiles)
		{
			reportF(SeverityTypeID::ST_MajorTrace, "{} is not sealed: tile {} is still mapped when its write ends", m_SeqFile->GetFileName().c_str(), t);
			return;
		}

	std::vector<SizeT> nrElems(tn);
	std::vector<bool>  isDirty(tn);
	dms::filesize_t usedBytes = (tn > 1) ? MinimalSeqFileSize(tn) : 0; // a domain of one tile has no chunk table
	for (tile_id t = 0; t != tn; ++t)
	{
		auto mappedTile = m_Files[t].get(this, dms_rw_mode::read_only);
		nrElems[t] = m_Files[t].count_actual_data_size();
		isDirty[t] = (nrElems[t] != m_Files[t].data_size());
		usedBytes += nrElems[t] * sizeof(elem_type);
	}
	if (usedBytes == m_SeqFile->GetFileSize())
		return;

	if (tn == 1 && !isDirty[0] && SingleChunkGrowsInPlace)
	{
		try {
			m_SeqFile->TruncateAndClose(usedBytes);
		}
		catch (...)
		{
			auto err = catchException(true);
			reportF(SeverityTypeID::ST_Warning, "{} is not cut back to the {} bytes its sequences use: {}", m_SeqFile->GetFileName().c_str(), usedBytes, err->Why().c_str());
			return;
		}
		ResetSeqProvider(0, 0, nrElems[0]);
		return;
	}

	std::vector<dms::filesize_t> tileBytes(tn);
	for (tile_id t = 0; t != tn; ++t)
		tileBytes[t] = nrElems[t] * sizeof(elem_type);

	// the new index ranges of a pool with abandoned elements; written only once the new file is in place
	std::vector<std::vector<seq_t>> newIndices(tn);

	auto seqFileName = m_SeqFile->GetFileName();
	auto tmpFileName = seqFileName + ".tmp";
	try {
		WriteCompactSeqFile(tmpFileName, tn, tileBytes.data(), [this, &isDirty, &newIndices](tile_id t, char* dst)
			{
				auto& file = m_Files[t];
				auto mappedTile = file.get(this, dms_rw_mode::read_only);
				auto dstElems = reinterpret_cast<elem_type*>(dst);
				if (isDirty[t])
				{
					newIndices[t].resize(file.size());
					file.compact_to(dstElems, newIndices[t].data());
				}
				else
					fast_copy(file.data_begin(), file.data_end(), dstElems);
			}
		);
		m_SeqFile->ReplaceByAndClose(tmpFileName);
	}
	catch (...)
	{
		auto err = catchException(true);
		reportF(SeverityTypeID::ST_Warning, "{} is not compacted to the {} bytes its sequences use: {}", seqFileName.c_str(), usedBytes, err->Why().c_str());
		KillFileOrDir(tmpFileName, false);
		return;
	}

	dms::filesize_t offset = (tn > 1) ? MinimalSeqFileSize(tn) : 0;
	for (tile_id t = 0; t != tn; ++t)
	{
		ResetSeqProvider(t, offset, nrElems[t]);
		offset += tileBytes[t];
	}
	for (tile_id t = 0; t != tn; ++t)
		if (isDirty[t])
			m_Files[t].set_indices(newIndices[t].data());
}

// the pool of tile t now lies at offset in the sealed .seq file and fills its capacity
template <typename V>
void FileTileArray<V>::ResetSeqProvider(tile_id t, dms::filesize_t offset, SizeT nrElems)
{
	using elem_type = elem_of_t<V>;
	m_Files[t].ResetValuesAllocator(new mappable_sequence<elem_type>(m_SeqFile, t, nrElems, offset, nrElems * sizeof(elem_type)), nrElems);
}

template <typename V>
auto FileTileArray<V>::GetWritableTile(tile_id t, dms_rw_mode rwMode) -> locked_seq_t
{
	assert(t < this->GetTiledRangeData()->GetNrTiles());
	this->CheckFailure();

	auto& file = m_Files[t];
	auto fileMapHandle = file.get(this, rwMode);
	assert(file.size() == this->GetTiledRangeData()->GetTileSize(t));
	return locked_seq_t(std::static_pointer_cast<void>(fileMapHandle), GetSeq(file));
}

template <typename V>
auto FileTileArray<V>::GetTile(tile_id t) const -> locked_cseq_t
{
	assert(t < this->GetTiledRangeData()->GetNrTiles());
	this->CheckFailure();

	const auto& file = m_Files[t];
	auto fileMapHandle = file.get(this, dms_rw_mode::read_only);
	assert(file.size() == this->GetTiledRangeData()->GetTileSize(t));
	return locked_cseq_t(std::static_pointer_cast<const void>(fileMapHandle), GetConstSeq(file));
}

#endif //!defined(__TIC_TILEARRAYIMPL_H)


