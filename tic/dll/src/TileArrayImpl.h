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
#include "mem/TileData.h"

template <typename V>
struct HeapTileArray : GeneratedTileFunctor<V>
{
	using typename TileFunctor<V>::locked_cseq_t;
	using typename TileFunctor<V>::locked_seq_t;

	using tiles_t = OwningPtrSizedArray<SharedPtr<tile<V>>>;

	HeapTileArray(const AbstrTileRangeData* trd, bool mustClear); // create heap stuff

	bool IsMemoryObject() const override { return true; }

	auto GetWritableTile(tile_id t, dms_rw_mode rwMode) ->locked_seq_t override;
	auto GetTile(tile_id t) const ->locked_cseq_t override;
	mutable tiles_t m_Seqs;
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

	FileTileArray(const AbstrTileRangeData* domain, SharedStr filenameBase, dms_rw_mode rwMode, bool isTmp, SafeFileWriterArray* sfwa);

	locked_seq_t GetWritableTile(tile_id t, dms_rw_mode rwMode) override;
	locked_cseq_t GetTile(tile_id t) const override;

	SharedStr m_CacheFileName;
	files_t m_Files;
	bool    m_IsTmp;
	SafeFileWriterArray* m_SFWA;
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
/*
	for (tile_id t = 0; t != tn; ++t)
	{
		seqs[t] = new tile<V>;
		auto tileSize = trd->GetTileSize(t);
		reallocSO(*seqs[t], tileSize, mustClear MG_DEBUG_ALLOCATOR_SRC("HeapTileArray<V>::ctor"));
	}
*/
	m_Seqs = std::move(seqs);
}

extern std::mutex s_mutableTileRecSection;

template <typename V>
void InitTile(SharedPtr<tile<V>>& tilePtr, const AbstrTileRangeData* trd, tile_id t, bool mustClear)
{
	auto sectionLokc = std::unique_lock(s_mutableTileRecSection);
	if (!tilePtr)
	{
		tilePtr = new tile<V>;
		reallocSO(*tilePtr, trd->GetTileSize(t), mustClear MG_DEBUG_ALLOCATOR_SRC("HeapTileArray<V>::ctor"));
	}
	assert(tilePtr);
	assert(tilePtr->size() == trd->GetTileSize(t));
}

template <typename V>
auto HeapTileArray<V>::GetWritableTile(tile_id t, dms_rw_mode rwMode) -> locked_seq_t
{
	assert(t < this->GetTiledRangeData()->GetNrTiles());

	auto& tilePtr = m_Seqs[t];
	InitTile(tilePtr, this->GetTiledRangeData(), t, rwMode != dms_rw_mode::write_only_all);

	return locked_seq_t(TileRef(tilePtr.get_ptr()), GetSeq(*tilePtr));
}

template <typename V>
auto HeapTileArray<V>::GetTile(tile_id t) const -> locked_cseq_t
{
	assert(t < this->GetTiledRangeData()->GetNrTiles());

	auto& tilePtr = m_Seqs[t];
	InitTile(tilePtr, this->GetTiledRangeData(), t, true);

	return locked_cseq_t(TileCRef(tilePtr.get_ptr()), GetConstSeq(*tilePtr));
}

//----------------------------------------------------------------------
// HeapSingleArray
//----------------------------------------------------------------------

template <typename V>
HeapSingleArray<V>::HeapSingleArray(const AbstrTileRangeData* trd, bool mustClear)
{
	dms_assert(trd);
	dms_assert(trd->GetNrTiles() == 1); // PRECONDITION

	this->m_TileRangeData = trd;
	auto tileSize = trd->GetTileSize(0);
	reallocSO(m_Seq, tileSize, mustClear MG_DEBUG_ALLOCATOR_SRC("HeapSingleArray<V>::ctor"));
}

template <typename V>
auto HeapSingleArray<V>::GetWritableTile(tile_id t, dms_rw_mode rwMode) -> locked_seq_t
{
	dms_assert(t == 0); // PRECONDITION

	auto tileSize = this->GetTiledRangeData()->GetTileSize(0);
	dms_assert(m_Seq.size() == this->GetTiledRangeData()->GetTileSize(0));

	return locked_seq_t(TileRef(this), GetSeq(m_Seq));
}

template <typename V>
auto HeapSingleArray<V>::GetTile(tile_id t) const -> locked_cseq_t
{
	dms_assert(t == 0); // PRECONDITION
	dms_assert(m_Seq.size() == this->GetTiledRangeData()->GetTileSize(0));

	return locked_cseq_t(TileCRef(this), GetConstSeq(m_Seq));
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
	dms_assert(t == 0); // PRECONDITION

	dms_assert(this->GetTiledRangeData()->GetTileSize(t) == 1);

	if constexpr (is_bitvalue_v<V>)
	{
		typename sequence_traits<V>::seq_t span(&m_Value, 1);
		return locked_seq_t(TileRef(nullptr), span);
	}
	else
	{
		typename sequence_traits<V>::seq_t span(&m_Value, &m_Value);
		++span.second;
		return locked_seq_t(TileRef(nullptr), span);
	}
}

template <typename V>
auto HeapSingleValue<V>::GetTile(tile_id t) const -> locked_cseq_t
{
	dms_assert(t == 0); // PRECONDITION

	dms_assert(this->GetTiledRangeData()->GetTileSize(t) == 1);

	if constexpr (is_bitvalue_v<V>)
	{
		typename sequence_traits<V>::cseq_t span(&m_Value, 1);
		return locked_cseq_t(TileCRef(this), span);
	}
	else
	{
		typename sequence_traits<V>::cseq_t span(&m_Value, &m_Value);
		++span.second;
		return locked_cseq_t(TileCRef(this), span);
	}
}

//----------------------------------------------------------------------
// FileTileArray
//----------------------------------------------------------------------

#include "mem/MappedSequenceProvider.h"
#include "geo/mpf.h"
#include "utl/splitPath.h"
#include "utl/mySPrintF.h"

SharedStr TileSuffix(tile_id t)
{
	SharedStr result;
	dms_assert(t > 0);
	while (t)
	{
		result += mySSPrintF("/t%x.", t % 0x100);
		t /= 0x100;
	}
	return result;
}

template <typename V> SizeT MinimalNrMemPages(const AbstrTileRangeData* trd);

template <fixed_elem V>
SizeT MinimalNrMemPages(const AbstrTileRangeData* trd)
{
	assert(trd);
	return trd->GetNrMemPages(mpf::log2_v<sizeof(V)>);
}

template <sequence_or_string V>
SizeT MinimalNrMemPages(const AbstrTileRangeData* trd)
{
	assert(trd);
	using seq_t = typename sequence_traits<V>::polymorph_vec_t::seq_t;
	return trd->GetNrMemPages(mpf::log2_v<sizeof seq_t>);
}

SizeT NrAllocTableMemPages(const AbstrTileRangeData* trd)
{
	auto nrTiles = trd->GetNrTiles();
	auto tileFileChuncSize = SizeT(nrTiles) * sizeof FileChunckSpec;
	return NrMemPages(tileFileChuncSize);
}

template <fixed_elem V>
SizeT MinimalFileSize(const AbstrTileRangeData* trd)
{
	tile_id tn = trd->GetNrTiles();
	SizeT rawSize = 0;
	if (tn > 1)
	{
		constexpr auto log2BytesPerElem = mpf::log2_v<sizeof(V)>;
		auto tileSizeInBytes = SizeT(trd->GetTileSize(tn - 1)) << log2BytesPerElem;
		rawSize = MinimalNrMemPages<V>(trd) - NrMemPages(tileSizeInBytes);
		rawSize <<= GetLog2AllocationGrannularity();
	}
	if (tn > 0)
	{
		rawSize += trd->GetTileSize(tn - 1) * sizeof(V);
	}
	return rawSize;
}

template <sequence_or_string V>
SizeT MinimalFileSize(const AbstrTileRangeData* trd)
{
	using seq_t = IndexRange<SizeT>; // typename sequence_traits<V>::polymorph_vec_t::seq_t;
	return MinimalFileSize<seq_t>(trd);
}

template <typename V>
FileTileArray<V>::FileTileArray(const AbstrTileRangeData* trd, SharedStr filenameBase, dms_rw_mode rwMode, bool isTmp, SafeFileWriterArray* sfwa)
	: m_CacheFileName(filenameBase)
	, m_SFWA(sfwa)
	, m_IsTmp(isTmp)
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
			cmfh = std::make_shared<ConstMappedFileHandle>(fullFileName, sfwa, true, false)
		,	cmfh_sequences;
		if constexpr (!has_fixed_elem_size_v<V>)
		{
			cmfh->m_MemPageAllocTable.reset( new mempage_file_view(cmfh, trd->GetNrTiles(), 0, trd->GetNrTiles() * sizeof(IndexRange<SizeT>)) );
			cmfh_sequences = std::make_shared<ConstMappedFileHandle>(fullFileName + ".seq", sfwa);
		}
		for (tile_id t = 0; t != tn; ++t)
		{
			if constexpr (has_fixed_elem_size_v<V>)
				seqs[t].ResetAllocator(new mappable_const_sequence<elem_of_t<V>>(cmfh, t, trd->GetTileSize(t)));
			else
			{
				auto ms_index = std::make_unique<mappable_const_sequence<IndexRange<SizeT>>>(cmfh, t, trd->GetTileSize(t));
				auto ms_values = std::make_unique<mappable_const_sequence<elem_of_t<V>> >(cmfh_sequences, t, trd->GetTileSize(t));
				seqs[t].ResetAllocators(ms_index.release(), ms_values.release());
			}
			MGD_CHECKDATA(!seqs[t].IsLocked());
		}
	}
	else
	{
		std::shared_ptr<MappedFileHandle> mfh = std::make_shared<MappedFileHandle>();
		std::shared_ptr<MappedFileHandle> mfh_sequences;
		mfh->OpenRw(fullFileName, sfwa, MinimalFileSize<V>(trd), rwMode, isTmp);

		if constexpr (!has_fixed_elem_size_v<V>)
		{
			mfh_sequences = std::make_shared<MappedFileHandle>();

			mfh_sequences->OpenRw(fullFileName+".seq", sfwa, MinimalNrMemPages<V>(trd) << GetLog2AllocationGrannularity(), rwMode, isTmp);
			mfh_sequences->m_MemPageAllocTable.reset( new mempage_file_view(mfh_sequences, trd->GetNrTiles()) );
		}
		for (tile_id t = 0; t != tn; ++t)
		{
			if constexpr (has_fixed_elem_size_v<V>)
				seqs[t].ResetAllocator(new mappable_sequence<elem_of_t<V>>(mfh, t, trd->GetTileSize(t)));
			else
			{
				auto ms_index  = std::make_unique<mappable_sequence<IndexRange<SizeT>>>(mfh, t, trd->GetTileSize(t));
				auto ms_values = std::make_unique<mappable_sequence<elem_of_t<V>>>(mfh_sequences, t, trd->GetTileSize(t));
				seqs[t].ResetAllocators(ms_index.release(), ms_values.release());
			}

			MGD_CHECKDATA(!seqs[t].IsLocked());
		}
	}
	m_Files = std::move(seqs);
}

template <typename V>
auto FileTileArray<V>::GetWritableTile(tile_id t, dms_rw_mode rwMode) -> locked_seq_t
{
	assert(t < this->GetTiledRangeData()->GetNrTiles());

	auto& file = m_Files[t];
	auto fileMapHandle = file.get(rwMode);
	//		fileRef->resizeSO(tileSize, rwMode == dms_rw_mode::write_only_mustzero);
	assert(file.size() == this->GetTiledRangeData()->GetTileSize(t));
	return locked_seq_t(make_SharedThing(std::move(fileMapHandle)), GetSeq(file));
}

template <typename V>
auto FileTileArray<V>::GetTile(tile_id t) const -> locked_cseq_t
{
	assert(t < this->GetTiledRangeData()->GetNrTiles());

	const auto& file = m_Files[t];
	auto fileMapHandle = file.get(dms_rw_mode::read_only);
	assert(file.size() == this->GetTiledRangeData()->GetTileSize(t));
	return locked_cseq_t(TileCRef(make_SharedThing( std::move(fileMapHandle) )), GetConstSeq(file));
}

#endif //!defined(__TIC_TILEARRAYIMPL_H)


