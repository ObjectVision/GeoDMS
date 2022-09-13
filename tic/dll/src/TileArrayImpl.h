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
#pragma once

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

	void Commit() override
	{
		MG_CHECK(m_Seqs.size() == this->m_TileRangeData->GetNrTiles());

		tile_id t = 0;
		for (auto& tile : m_Seqs)
		{
			MG_CHECK(tile->size() == this->m_TileRangeData->GetTileSize(t++));
		}
	}

	tiles_t m_Seqs;
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

	void Commit() override
	{
		MG_CHECK(m_Seq.size() == this->m_TileRangeData->GetTileSize(0));
	}

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

	using files_t = OwningPtrSizedArray<SharedPtr<file<V>>>;

	FileTileArray(const AbstrTileRangeData* domain, SharedStr filenameBase, dms_rw_mode rwMode, bool isTmp, SafeFileWriterArray* sfwa);

	locked_seq_t GetWritableTile(tile_id t, dms_rw_mode rwMode) override;
	locked_cseq_t GetTile(tile_id t) const override;

	void Commit() override
	{
		tile_id t = 0;
		MG_CHECK(m_Files.size() == this->m_TileRangeData->GetNrTiles());
		for (auto& file : m_Files)
		{
			MG_CHECK(file->size() == this->m_TileRangeData->GetTileSize(t++));
		}
	}

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

	tiles_t seqs(tn MG_DEBUG_ALLOCATOR_SRC_EMPTY);

	for (tile_id t = 0; t != tn; ++t)
	{
		// TODO G8: make it non-virtually allocated arrays.
		seqs[t] = new tile<V>;
//		seqs[t]->Reset(heap_sequence_provider<typename elem_of<V>::type>::CreateProvider());
		//		seqs[t]->resizeSO(trd->GetTileSize(t), mustClear);
	}

	m_Seqs = std::move(seqs);
}

template <typename V>
auto HeapTileArray<V>::GetWritableTile(tile_id t, dms_rw_mode rwMode) -> locked_seq_t
{
	dms_assert(t < this->GetTiledRangeData()->GetNrTiles());

	const auto& tilePtr = m_Seqs[t];

	auto tileSize = this->GetTiledRangeData()->GetTileSize(t);
	if (rwMode == dms_rw_mode::read_write)
		resizeSO(*tilePtr, tileSize, true MG_DEBUG_ALLOCATOR_SRC(this->md_SrcStr));
	else
		reallocSO(*tilePtr, tileSize, rwMode == dms_rw_mode::write_only_mustzero MG_DEBUG_ALLOCATOR_SRC(this->md_SrcStr));

	dms_assert(tilePtr->size() == this->GetTiledRangeData()->GetTileSize(t));

	return locked_seq_t(TileRef(tilePtr.get_ptr()), GetSeq(*tilePtr));
}

template <typename V>
auto HeapTileArray<V>::GetTile(tile_id t) const -> locked_cseq_t
{
	dms_assert(t < this->GetTiledRangeData()->GetNrTiles());

	const auto& tilePtr = m_Seqs[t];
	dms_assert(tilePtr->size() == this->GetTiledRangeData()->GetTileSize(t));

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
}

template <typename V>
auto HeapSingleArray<V>::GetWritableTile(tile_id t, dms_rw_mode rwMode) -> locked_seq_t
{
	dms_assert(t == 0); // PRECONDITION

	auto tileSize = this->GetTiledRangeData()->GetTileSize(0);
	if (rwMode == dms_rw_mode::read_write)
		resizeSO(m_Seq, tileSize, true MG_DEBUG_ALLOCATOR_SRC(this->md_SrcStr));
	else
		reallocSO(m_Seq, tileSize, rwMode == dms_rw_mode::write_only_mustzero MG_DEBUG_ALLOCATOR_SRC(this->md_SrcStr));
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

template <typename V>
FileTileArray<V>::FileTileArray(const AbstrTileRangeData* trd, SharedStr filenameBase, dms_rw_mode rwMode, bool isTmp, SafeFileWriterArray* sfwa)
	: m_CacheFileName(filenameBase)
	, m_SFWA(sfwa)
	, m_IsTmp(isTmp)
{
	this->m_TileRangeData = trd;
	dms_assert(!m_CacheFileName.empty());

	dms_assert(rwMode <= dms_rw_mode::write_only_all);

	dms_assert(trd);
	tile_id tn = trd->GetNrTiles();

	files_t seqs(tn MG_DEBUG_ALLOCATOR_SRC_EMPTY);

	for (tile_id t = 0; t != tn; ++t)
	{
		dms_assert(!this->m_CacheFileName.empty());
		SharedStr fullFileName = m_CacheFileName;
		if (tn > 1)

			fullFileName = getFileNameBase(fullFileName.c_str()) + TileSuffix(t) + getFileNameExtension(fullFileName.c_str());

		MGD_CHECKDATA(!seqs[t]->IsLocked());

		if (rwMode <= dms_rw_mode::read_only)
			seqs[t]->Reset(new mappable_const_sequence<typename elem_of<V>::type>(fullFileName));
		else
			seqs[t]->Reset(new mappable_sequence<typename elem_of<V>::type>(fullFileName));

		seqs[t]->Open(trd->GetTileSize(t), rwMode, isTmp, sfwa MG_DEBUG_ALLOCATOR_SRC_EMPTY);


		MGD_CHECKDATA(!seqs[t]->IsLocked());
	}

	m_Files = std::move(seqs);
}

template <typename V>
auto FileTileArray<V>::GetWritableTile(tile_id t, dms_rw_mode rwMode) -> locked_seq_t
{
	dms_assert(t < this->GetTiledRangeData()->GetNrTiles());

	auto filePtr = m_Files[t];
	auto fileMapHandle = filePtr->get(rwMode);
	//		fileRef->resizeSO(tileSize, rwMode == dms_rw_mode::write_only_mustzero);
	dms_assert(filePtr->size() == this->GetTiledRangeData()->GetTileSize(t));
	return locked_seq_t(make_SharedThing(std::move(fileMapHandle)), GetSeq(*filePtr));
}

template <typename V>
auto FileTileArray<V>::GetTile(tile_id t) const -> locked_cseq_t
{
	dms_assert(t < this->GetTiledRangeData()->GetNrTiles());

	auto filePtr = m_Files[t];
	auto fileMapHandle = filePtr->get(dms_rw_mode::read_only);
	dms_assert(filePtr->size() == this->GetTiledRangeData()->GetTileSize(t));
	return locked_cseq_t(TileCRef(make_SharedThing( std::move(fileMapHandle) )), GetConstSeq(*filePtr));
}
/* REMOVE
template <typename V>
void FileTileArray<V>::DropData()
{
	tile_id tn = this->GetTiledRangeData()->GetNrTiles();
	for (tile_id t=0; t!=tn; ++t)
		m_Files[t]->Drop();
}
*/


#endif //!defined(__TIC_TILEARRAYIMPL_H)


