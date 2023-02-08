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
#include "TicPCH.h"
#pragma hdrstop

#include "DataArray.ipp"

#include "mem/HeapSequenceProvider.ipp"

#include <memory>

#include "dbg/Debug.h"
#include "dbg/SeverityType.h"
#include "geo/DataPtrTraits.h"
#include "geo/StringArray.h"
#include "mci/ValueClass.h"
#include "mci/ValueWrap.h"
#include "mem/Grid.h"
#include "mem/HeapSequenceProvider.h"
#include "mem/MappedSequenceProvider.h"
#include "mem/RectCopy.h"
#include "mem/SeqLock.h"
#include "mem/TileData.h"
#include "ser/SequenceArrayStream.h"
#include "ser/StreamTraits.h"
#include "utl/splitPath.h"
#include "xml/XmlOut.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "DataCheckMode.h"
#include "DataItemClass.h"
#include "DataLocks.h"
#include "DataStoreManagerCaller.h"
#include "FreeDataManager.h"
#include "LispTreeType.h"
#include "UnitProcessor.h"
#include "TileAccess.h"
#include "TileArrayImpl.h"
#include "TreeItemProps.h"
#include "UnitClass.h"

//----------------------------------------------------------------------
//	Shadow creation 
//----------------------------------------------------------------------

template <typename V>
void CloseMutableShadow(DataArrayBase<V>* sourceTileArray, typename sequence_traits<V>::cseq_t shadowData)
{
	auto trd = sourceTileArray->GetTiledRangeData();
	SizeT nrElem = trd->GetElemCount();
	I64Rect range = trd->GetRangeAsI64Rect();
	dms_assert(Cardinality(range) >= nrElem);
	dms_assert(Cardinality(range) >= shadowData.size());

	U64Grid<const V> shadowGrid(Size(range), shadowData.begin());

	parallel_tileloop(trd->GetNrTiles(), [=, &shadowGrid, &range](tile_id t)
		{
			I64Rect tileRange = trd->GetTileRangeAsI64Rect(t);
			dms_assert(IsIncluding(range, tileRange));
			auto tileData = sourceTileArray->GetWritableTile(t);
			dms_assert(Cardinality(tileRange) == tileData.size());
			RectCopy(U64Grid<V>(Size(tileRange), tileData.begin()), shadowGrid, tileRange.first - range.first);
		}
	);
}

template <typename V>
struct mutable_shadow_tile : tile<V>
{
	using tile<V>::tile;

	~mutable_shadow_tile()
	{
		if (m_SourceTileArray)
			CloseMutableShadow<V>(m_SourceTileArray, GetConstSeq(*this));
	}

	SharedPtr< DataArrayBase<V> > m_SourceTileArray;
};

template <typename V>
struct const_shadow_tile : tile<V>
{
	using tile<V>::tile;
	std::vector< typename DataArrayBase<V>::locked_cseq_t > m_Seqs;
};

template <typename V>
void InitMutableShadow(DataArrayBase<V>* tileFunctor, tile<V>* shadowTilePtr, const AbstrTileRangeData* trd, dms_rw_mode rwMode MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	SizeT nrElem = trd->GetRangeSize();
	auto range = trd->GetRangeAsI64Rect();
	dms_assert(Cardinality(range) == nrElem);

	resizeSO(*shadowTilePtr, nrElem, !trd->IsCovered() MG_DEBUG_ALLOCATOR_SRC_PARAM);

	if (rwMode <= dms_rw_mode::read_write)
	{
		tile_id tn = trd->GetNrTiles();

		U64Grid<V> shadowData(Size(range), shadowTilePtr->begin());

		parallel_tileloop_if_separable<V>(tn, [tileFunctor, trd, &shadowTilePtr, &shadowData, shadowRangeBase = range.first](tile_id t)
		{
			I64Rect tileRange = trd->GetTileRangeAsI64Rect(t);
			auto tileData = tileFunctor->GetWritableTile(t);
			dms_assert(Cardinality(tileRange) == tileData.size());

			RectCopy(shadowData, U64Grid<const V>(Size(tileRange), tileData.begin()), shadowRangeBase - tileRange.first);
		}
		);
	}
}

template <typename V>
void InitConstShadow(const DataArrayBase<V>* tileFunctor, tile<V>* shadowTilePtr, const AbstrTileRangeData* trd MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	SizeT nrElem = trd->GetRangeSize();
	auto range = trd->GetRangeAsI64Rect();
	dms_assert(Cardinality(range) == nrElem);

	resizeSO(*shadowTilePtr, nrElem, !trd->IsCovered() MG_DEBUG_ALLOCATOR_SRC_PARAM);

	tile_id tn = trd->GetNrTiles();

	U64Grid<V> shadowData(Size(range), shadowTilePtr->begin());

	parallel_tileloop_if_separable<V>(tn, [tileFunctor, trd, &shadowData, shadowRangeBase = range.first](tile_id t)
		{
			I64Rect tileRange = trd->GetTileRangeAsI64Rect(t);
			auto tileData = tileFunctor->GetTile(t);
			dms_assert(Cardinality(tileRange) == tileData.size());

			RectCopy(shadowData, U64Grid<const V>(Size(tileRange), tileData.begin()), shadowRangeBase - tileRange.first);
		}
	);
}

template <typename V>
auto MutableShadowTile(DataArrayBase<V>* tileFunctor, dms_rw_mode rwMode MG_DEBUG_ALLOCATOR_SRC_ARG) -> DataArrayBase<V>::locked_seq_t
{
	auto trd = tileFunctor->GetTiledRangeData();
	dms_assert(trd->GetNrTiles() != 1);

	SharedPtr<mutable_shadow_tile<V>> shadowTilePtr = new mutable_shadow_tile<V>;

	InitMutableShadow(tileFunctor, shadowTilePtr.get_ptr(), trd, rwMode MG_DEBUG_ALLOCATOR_SRC_PARAM);
	if (rwMode >= dms_rw_mode::read_write)
		shadowTilePtr->m_SourceTileArray = tileFunctor;
	return { TileRef(shadowTilePtr.get_ptr()), GetSeq(*shadowTilePtr) };
}

template <fixed_elem V>
auto ConstShadowTile(const DataArrayBase<V>* tileFunctor MG_DEBUG_ALLOCATOR_SRC_ARG) -> typename DataArrayBase<V>::locked_cseq_t
{
	auto trd = tileFunctor->GetTiledRangeData();
	assert(trd->GetNrTiles() != 1);

	SharedPtr<tile<V>> shadowTilePtr = new tile<V>; // normal tile

	InitConstShadow<V>(tileFunctor, shadowTilePtr.get_ptr(), trd MG_DEBUG_ALLOCATOR_SRC_PARAM);

	return { TileCRef(shadowTilePtr.get_ptr()), GetConstSeq(*shadowTilePtr) };
}

template <sequence_or_string V>
auto ConstShadowTile(const DataArrayBase<V>* tileFunctor MG_DEBUG_ALLOCATOR_SRC_ARG) -> typename DataArrayBase<V>::locked_cseq_t
{
	auto trd = tileFunctor->GetTiledRangeData();
	auto tn = trd->GetNrTiles();
	assert(tn != 1);

	using element_type = elem_of_t<V>;
	SharedPtr<const_shadow_tile<V>> shadowTilePtr = new const_shadow_tile<V>; // normal tile, but keep using data sequences from original tles
	if (tn != 0)
	{
		shadowTilePtr->m_Seqs.reserve(tn);
		for (tile_id t = 0; t != tn; ++t)
			shadowTilePtr->m_Seqs.emplace_back(tileFunctor->GetTile(t));

		auto minValuePtr = shadowTilePtr->m_Seqs[0].get_sa().data_begin();
		auto maxValuePtr = shadowTilePtr->m_Seqs[0].get_sa().data_end();
		for (tile_id t = 1; t != tn; ++t)
		{
			auto currValueBeginPtr = shadowTilePtr->m_Seqs[t].get_sa().data_begin();
			auto currValueEndPtr = shadowTilePtr->m_Seqs[t].get_sa().data_end();
			MakeMin(minValuePtr, currValueBeginPtr);
			MakeMax(maxValuePtr, currValueEndPtr);

			MG_CHECK((reinterpret_cast<const char*>(currValueBeginPtr) - reinterpret_cast<const char*>(minValuePtr)) % sizeof(element_type) == 0);
			MG_CHECK((reinterpret_cast<const char*>(currValueEndPtr) - reinterpret_cast<const char*>(minValuePtr)) % sizeof(element_type) == 0);
		}
		shadowTilePtr->SetValues(sequence_obj<element_type>(alloc_data<element_type>(const_cast<element_type*>(minValuePtr), const_cast<element_type*>(maxValuePtr), 0)));

		SizeT nrElem = trd->GetRangeSize();
		shadowTilePtr->resizeSO(nrElem, true MG_DEBUG_ALLOCATOR_SRC("ConstShadowSequenceArrayTile "));

		auto range = trd->GetRangeAsI64Rect();
		dms_assert(Cardinality(range) == nrElem);

		U64Grid<IndexRange<SizeT>> shadowData(Size(range), &*shadowTilePtr->index_begin());

		parallel_tileloop_if_separable<V>(tn, [shadowTilePtr, trd, &shadowData, shadowRangeBase = range.first, minValuePtr](tile_id t)
			{
				I64Rect tileRange = trd->GetTileRangeAsI64Rect(t);
				auto tileData = shadowTilePtr->m_Seqs[t];
				dms_assert(Cardinality(tileRange) == tileData.size());

				RectCopyAndAddConst< IndexRange<SizeT>>(shadowData
					, U64Grid<const IndexRange<SizeT>>(Size(tileRange), tileData.get_sa().index_begin())
					, shadowRangeBase - tileRange.first
					, tileData.get_sa().data_begin() - minValuePtr
				);
			}
		);
	}
	return { TileCRef(shadowTilePtr.get_ptr()) , GetConstSeq(*shadowTilePtr) };
}

#if defined(MG_DEBUG)

std::atomic<UInt32> gd_nrActiveLoops;

#endif // defined(MG_DEBUG)

//----------------------------------------------------------------------
// class  : DataArrayBase memberfunc impl
//----------------------------------------------------------------------


template <class V>
AbstrReadableTileData* 
DataArrayBase<V>::CreateReadableTileData(tile_id t) const
{
	return new ReadableTileData<V>(GetDataRead(t));
}

template <class V> 
typename DataArrayBase<V>::locked_cseq_t 
DataArrayBase<V>::GetDataRead(tile_id t) const
{
	DMS_ASyncContinueCheck();

	if (t == no_tile)
	{
		auto tn = GetTiledRangeData()->GetNrTiles();
		if (tn != 1)
			return ConstShadowTile(this MG_DEBUG_ALLOCATOR_SRC("ConstShadowTile this->md_SrcStr"));
		t = 0;
	}
	dms_assert(t < GetTiledRangeData()->GetNrTiles());
	return GetTile(t);
}

template <class V> 
typename DataArrayBase<V>::locked_seq_t 
DataArrayBase<V>::GetDataWrite(tile_id t, dms_rw_mode rwMode)
{
	if (t == no_tile)
	{
		auto tn = GetTiledRangeData()->GetNrTiles();
		if (tn != 1)
			return MutableShadowTile(this, rwMode MG_DEBUG_ALLOCATOR_SRC("MutableShadowTile this->md_SrcStr"));
		t = 0;
	}
	dms_assert(t < GetTiledRangeData()->GetNrTiles());
	return GetWritableTile(t, rwMode);
}

#if defined(MG_DEBUG)
#include "act/TriggerOperator.h"
#endif


template <class V> 
void DumpImpl(OutStreamBase* xmlOutStr, typename DataArrayBase<V>::const_iterator iter,  typename DataArrayBase<V>::const_iterator end)
{
	while (iter!=end)
	{
		*xmlOutStr << ( IsDefined(*iter) ? ::AsDataStr(*iter).c_str() : UNDEFINED_VALUE_STRING);
		++iter;
		if (iter!=end) *xmlOutStr << ",";
	}
}

template <class V> 
void Dump(OutStreamBase* xmlOutStr, bool isColorCode, typename DataArrayBase<V>::const_iterator iter, typename DataArrayBase<V>::const_iterator end)
{
	DumpImpl<V>(xmlOutStr, iter, end);
}

#include "Aspect.h"

template <>
void Dump<UInt32>(OutStreamBase* xmlOutStr, bool isColorCode, DataArrayBase<UInt32>::const_iterator iter, DataArrayBase<UInt32>::const_iterator end)
{
	if  (isColorCode)
		while (iter!=end)
		{
			* xmlOutStr << ( IsDefined(*iter) ? AsRgbStr(*iter).c_str(): UNDEFINED_VALUE_STRING);
			++iter;
			if (iter!=end) * xmlOutStr << ",";
		}
	else
		DumpImpl<UInt32>(xmlOutStr, iter, end);
}

// override Object
template <class V> 
void DataArrayBase<V>::XML_DumpObjData(OutStreamBase* xmlOutStr, const AbstrDataItem* owner) const
{
//	bool isColorCode = IsColorAspectNameID(dialogTypePropDefPtr->GetValue(owner)); Requires IsMetaThread
	bool isColorCode = false;
	for (tile_id t = 0, tn = GetTiledRangeData()->GetNrTiles(); t != tn; ++t)
	{
		auto data = GetDataRead(t);
		const_iterator
			iter = data.begin(),
			end = data.end();
		Dump<V>(xmlOutStr, isColorCode, iter, end);
	}
}

//----------------------------------------------------------------------
// class  : DataArrayBase implementation (move to .ipp)
//----------------------------------------------------------------------

template <typename V>
bool DataArrayBase<V>::CheckValuesUnit(const AbstrUnit* valuesUnit)
{
	return dynamic_cast<const unit_t*>(valuesUnit) != nullptr;
}
/*
template <typename V>
void DataArrayBase<V>::DoCreateWritable(AbstrDataItem* adi, dms_rw_mode rwMode, UInt32 nrElem, tile_id t)
{
	dms_assert(t < GetTiledRangeData()->GetNrTiles());
	dms_assert(rwMode != dms_rw_mode::unspecified);
	dms_assert(rwMode != dms_rw_mode::read_only);

	MGD_CHECKDATA(!IsLocked(t));
	if (m_Seqs[t].CanWrite() && (m_Seqs[t].Size() == nrElem) && (rwMode == dms_rw_mode::read_write))
	{
		if (m_Seqs[t].IsOpen())
			goto exit;
	}
	else
	{
		if (adi && adi->GetTSF(DSF_DSM_Allocated))
		{
			dms_assert(!m_Seqs[t].IsHeapAllocated()); // follows from DSF_DSM_Allocated
			if (m_Seqs[t].IsOpen())
			{
				if (rwMode == dms_rw_mode::read_write)
					m_Seqs[t].Close();
				else
					m_Seqs[t].Drop();
			}
			DoCreateMappedFile(m_Seqs[t].GetFileName(), true, t); // get a non-const FileMapping
		}	
		else 
		{
//			dms_assert(!adi->GetTSF(TSF_DSM_DcKnown));  // else adi->GetTSF(DSF_DSM_Allocated) should have been true
			if (!m_Seqs[t].IsAssigned() )
				m_Seqs[t].Reset( heap_sequence_provider<typename elem_of<V>::type>::CreateProvider() );
			dms_assert(m_Seqs[t].IsHeapAllocated());
		}
	}

	dms_assert(!m_Seqs[t].IsOpen() || !(adi && adi->GetTSF(DSF_DSM_Allocated)));

	m_Seqs[t].Open(
		nrElem
	,	rwMode
	,	!(adi && adi->IsFnKnown())  // try to keep in RAM - FileCache if not persistent 
	,	DSM::GetSafeFileWriterArray(adi)
	);

exit:
	dms_assert(m_Seqs[t].IsOpen());
	dms_assert(m_Seqs[t].CanWrite());
	dms_assert(m_Seqs[t].Size() == nrElem);

	MGD_CHECKDATA(!IsLocked(t));
}

*/

template <typename V>
constexpr bool is_real_numeric_v = is_numeric_v<V> && ! is_bitvalue_v<V>;

template <typename V, typename Iter>
typename std::enable_if< is_real_numeric_v<V> >::type
SplitClasses(Iter c, Iter d, Iter r, Iter e)
{
	while (true)
	{
		if (r == e)
			*--r = d[-1];
		else
			*--r = (d[0] + d[-1]) / 2;
	
		if (--r == --d)
			break;
		*r = *d;
	}
	dms_assert(d == c);
}


template <typename V, typename Iter>
typename std::enable_if< !is_real_numeric_v<V> >::type
SplitClasses(Iter c, Iter d, Iter r, Iter e)
{
	while (true)
	{
		*--r = *--d;
		if (--r == d)
			break;
		*r = *d;
	}
	dms_assert(d == c);
}
/* === NYI: SplitData in tiled domain  
template <typename V>
void FileTileArray<V>::FileTileArray(AbstrDataItem* adi, dms_rw_mode rwMode, const DomainChangeInfo* info, bool dontAlloc)
{
	dms_assert(rwMode == dms_rw_mode::read_write || rwMode == dms_rw_mode::write_only_mustzero || rwMode == dms_rw_mode::write_only_all);
	dms_assert(rwMode == dms_rw_mode::read_write || !info);
	dms_assert(!(dontAlloc && info));
	dms_check_not_debugonly; 

	const AbstrUnit* adu = GetTiledRangeData(); // NOTE THAT: adi may be nullptr, as for example when used in DoOverlay
	dms_assert(CheckCalculatingOrReady(adu->GetCurrRangeItem()));
	SizeT nrElem = (dontAlloc) ? 0 : (info) ? info->newSize : adu->GetCount();
	if (!IsDefined(nrElem))
		throwErrorD("DoCreateMemoryStorage", "Cannot derive cardinality for domain");
//	if (!m_Seqs[0].CanWrite() || !m_Seqs[0].IsOpen())

	tile_id tn = GetTiledRangeData()->GetNrTiles();
	if (tn != m_Seqs.m_NrRealTiles || ta != m_Seqs.m_NrDataTiles)
		m_Seqs = tile_array<V>(tn, ta);


	bool isTiled = GetTiledRangeData()->IsCurrTiled();
	dms_assert(isTiled || t==1);

	if (rwMode == dms_rw_mode::read_write && !(adi && adi->DataAllocated()))
		rwMode = dms_rw_mode::write_only_mustzero; // no data to be read, assume values must be initialized

	if (rwMode > dms_rw_mode::read_write) // thus: dms_write_only_xxx
		if (adi && IsFileable<V>(adi, nrElem) && !adi->IsSdKnown())
		{
			DataStoreManager::Curr()->CreateFileData(adi, rwMode); // calls OpenFileData -> DoCreateMappedFiles
			return;
		}
	while (t--)
	{
		MGD_CHECKDATA( !IsLocked(t) );
		if (info && info->delta < 0)
		{
			// merge and erase 
			dms_assert(rwMode == dms_rw_mode::read_write);
			dms_assert(m_Seqs[t].IsAssigned());
			DoCreateWritable(adi, rwMode, info->OldSize(), t);

			SeqLock<sequence_t> lock(m_Seqs[t], rwMode);

			dms_assert(m_Seqs[t].IsAssigned());
			dms_assert(m_Seqs[t].size() == info->OldSize());
			dms_assert(info->changePos - info->delta <= info->OldSize());

			m_Seqs[t].erase(m_Seqs[t].begin() + info->changePos, m_Seqs[t].begin() + info->changePos - info->delta);
		}

		nrElem = GetTiledRangeData()->GetTileSize(t);

		DoCreateWritable(adi, rwMode, nrElem, t);

		dms_assert(m_Seqs[t].CanWrite() && m_Seqs[t].Size() == nrElem);

		if (info && info->delta > 0)
		{
			// split data with row doubling
			dms_assert(info->changePos + info->delta <= info->OldSize());
			dms_assert(rwMode == dms_rw_mode::read_write);

			SeqLock<sequence_t> lock(m_Seqs[t], rwMode);
			dms_assert(m_Seqs[t].size() == info->newSize);

			iterator
				c = m_Seqs[t].begin()+info->changePos,
				d = c + info->delta,
				e = m_Seqs[t].end(),
				r = fast_move_backward(d, m_Seqs[t].begin()+info->OldSize(), e ); 

			dms_assert(r == d + info->delta);

			SplitClasses<V>(c, d, r, e);
		}
		MGD_CHECKDATA( !IsLocked(t) );
	}
	MGD_CHECKDATA(m_Seqs.size() && m_Seqs[0].IsHeapAllocated() || m_Seqs.m_NrRealTiles == 0); // DEBUG
}
*/

//----------------------------------------------------------------------
//	DataStorage
//----------------------------------------------------------------------


template <typename V>
void DataArrayBase<V>::DoReadData(BinaryInpStream& ar, tile_id t)
{
	auto data = GetWritableTile(t);
	ar >> data;
}

template <typename V>
void DataArrayBase<V>::DoWriteData(BinaryOutStream& ar, tile_id t) const
{
	auto data = GetDataRead(t);
	ar << data;
}

template <class V>
const ValueClass* DataArrayBase<V>::GetValueClass() const 
{ 
	return ValueWrap<V>::GetStaticClass(); 
}

template <class V>
AbstrValue* DataArrayBase<V>::CreateAbstrValue() const
{
	return GetValueClass()->CreateValue();
}

template <class V>
auto DataArrayBase<V>::GetDataReadBegin(tile_id t) const -> data_read_begin_handle
{
	auto drh = GetDataRead(t);
	auto p= ptr<const Byte>(data_ptr_traits<typename sequence_traits<V>::cseq_t>::begin(drh));
	return data_read_begin_handle(std::move(drh.m_TileHolder), p);
}

template <class V>
auto DataArrayBase<V>::GetDataWriteBegin(tile_id t) -> data_write_begin_handle
{
	auto dwh = GetDataWrite(t, dms_rw_mode::read_write);
	auto p = ptr<Byte>(data_ptr_traits<typename sequence_traits<V>::seq_t>::begin(dwh));
	return data_write_begin_handle(std::move(dwh.m_TileHolder), p );
}

template <class V>
void DataArrayBase<V>::GetAbstrValue(SizeT index, AbstrValue& valueHolder) const
{
	CheckPtr(&valueHolder, ValueWrap<V>::GetStaticClass(), "DataArrayBase<V>::GetAbstrValue");
	GuiReadLock lockHolder;
	auto iter = GetIndexedIterator(index, lockHolder);
	if (iter != const_iterator() )
		debug_cast<ValueWrap<V>*>(&valueHolder)->Set( *iter );
	else
		debug_cast<ValueWrap<V>*>(&valueHolder)->Clear();
}

template <class V>
void DataArrayBase<V>::SetAbstrValue   (SizeT index, const AbstrValue& valueHolder)
{
	CheckPtr(&valueHolder, ValueWrap<V>::GetStaticClass(), "DataArrayBase<V>::SetAbstrValue");
	SetIndexedValue(index, debug_cast<const ValueWrap<V>*>(&valueHolder)->Get());
}

template <class V>
void DataArrayBase<V>::SetNull(SizeT index)
{
	tile_loc tl = GetTiledLocation(index);
	auto data = GetWritableTile(tl.first);
	Assign(data[tl.second], Undefined());
}

template <class V>
bool DataArrayBase<V>::IsNull(SizeT index) const
{
	tile_loc tl = GetTiledLocation(index);
	if (!IsDefined(tl.first))
		return true;
	auto data = GetTile(tl.first);
	return tl.second >= data.size() || !IsDefined(data[tl.second]);
}

template <class V>
SizeT DataArrayBase<V>::GetNrNulls() const
{
	SizeT count = 0;
	for (tile_id t=0, tn=GetTiledRangeData()->GetNrTiles(); t!=tn; ++t)
	{
//		ReadableTileLock lock(this, t);
		auto data = GetDataRead(t);
		for (auto i=data.begin(), e=data.end(); i!=e; ++i)
			if (!IsDefined(*i))
				++count;
	}
	return count;
}

template <class V>
auto DataArrayBase<V>::GetWritableTile(tile_id t, dms_rw_mode rwMode) -> locked_seq_t
{
	throwItemErrorF("This type of tile does not allow write access");
}

template <class V>
bool    DataArrayBase<V>::AsCharArray(SizeT index, char* sink, streamsize_t bufLen, GuiReadLock& lockHolder, FormattingFlags ff) const
{
	auto ii = GetIndexedIterator(index, lockHolder);
	if (ii==const_iterator())
		return ::AsCharArray(Undefined(), sink, bufLen, ff);
	return ::AsCharArray(*ii, sink, bufLen, ff);
}

template <class V>
SharedStr  DataArrayBase<V>::AsString(SizeT index, GuiReadLock& lockHolder) const
{
	auto ii = GetIndexedIterator(index, lockHolder);
	if (ii == const_iterator())
		return ::AsString(Undefined());
	return ::AsString(*ii);
}

template <class V>
SizeT  DataArrayBase<V>::AsCharArraySize(SizeT index, streamsize_t maxLen, GuiReadLock& lockHolder, FormattingFlags ff) const
{
	auto ii = GetIndexedIterator(index, lockHolder);
	if (ii == const_iterator())
		return ::AsCharArraySize(Undefined(), maxLen, ff);
	return ::AsCharArraySize(*ii, maxLen, ff);
}

//----------------------------------------------------------------------
// Support for RangedArrays
//----------------------------------------------------------------------

template <typename F>
Range<F>
GetRangeOrVoid(const RangedUnit<F>* ru)
{
	dms_assert(ru);
	dms_assert(ru->HasVarRange());
	return ru->GetRange();
}

template <int N>
Void GetRangeOrVoid(const Unit<bit_value<N> >* ru)
{
	return Void();
}

Undefined GetRangeOrVoid(const Unit<SharedStr>* ru)
{
	return Undefined();
}

template <typename CI, typename F>
DataCheckMode CheckMode(CI b, CI e, Range<F> valueRange, DataCheckMode dcm)
{
	if (dcm & DCM_CheckRange) // REMOVE || (valueRange.empty() && (b!=e)))
		goto containsOutOfRange;
	if (valueRange.is_max())
		return 
			(dcm & DCM_CheckDefined) || !AllDefined(b, e)
				?	DCM_CheckDefined
				:	DCM_None;
		
	if (dcm & DCM_CheckDefined)
		goto containsUndefined;

	while(b!=e)
	{
		if (!IsDefined(*b))
		{
			++b;
			goto containsUndefined;
		}
		if (!IsIncluding(valueRange, *b++))
			goto containsOutOfRange;
	}
	return DCM_None;
containsUndefined: 
	{
		bool allDefinedInRange = 
			ContainsUndefined(valueRange)
				?	IsIncluding(valueRange, b, e)
				:	IsIncludingOrUndefined(valueRange, b, e)
		;
		return allDefinedInRange ? DCM_CheckDefined : DCM_CheckBoth;
	}
containsOutOfRange:
	if (dcm & DCM_CheckDefined) return DCM_CheckBoth;
	if (!AllDefined(b, e)) return DCM_CheckBoth;
	return DCM_CheckRange;
}

template <typename CI>
DataCheckMode CheckMode(CI b, CI e, Void, DataCheckMode dcm)
{
	return DCM_None;
}

template <typename CI>
DataCheckMode CheckMode(CI b, CI e, Undefined, DataCheckMode dcm)
{
	return  !(dcm & DCM_CheckDefined) && AllDefined(b, e)
		?	DCM_None 
		:	DCM_CheckDefined;
}

template <typename V>
DataCheckMode DataArrayBase<V>::DoGetCheckMode() const
{
	dbg_assert(IsMultiThreaded2() || !gd_nrActiveLoops);
	dms_assert(IsMetaThread() || IsMultiThreaded2());

	if constexpr (!has_var_range_field_v<V> || !has_fixed_elem_size_v<V>)
		return has_undefines_v<field_of_t<V>> ? DCM_CheckDefined : DCM_None;
	else
	{
//		auto valuesRange = GetValueRangeData()->GetRange();
//		if (IsIncluding(valuesRange, UNDEFINED_VALUE(field_of_t<V>)))
		return DCM_CheckBoth;
//		return DCM_CheckRange;
	}
}

template <typename V>
DataCheckMode DataArrayBase<V>::DoDetermineCheckMode() const
{
	dbg_assert(IsMultiThreaded2() || !gd_nrActiveLoops);
	dms_assert(IsMetaThread() || IsMultiThreaded2());

	if constexpr (!has_var_range_field_v<V> || !has_fixed_elem_size_v<V>)
		return has_undefines_v<field_of_t<V>> ? DCM_CheckDefined : DCM_None;
	else
	{
		std::atomic<UInt32> dcm = DCM_None;
		typename Unit<field_of_t<V>>::range_t valuesRange = {};
		auto valuesRangeData = GetValueRangeData();
		if (!valuesRangeData)
			dcm = DCM_CheckRange;
		else
			valuesRange = valuesRangeData->GetRange();
		parallel_tileloop(GetTiledRangeData()->GetNrTiles(),
			[&dcm, this, valuesRange](tile_id t)->void
			{
				if (dcm != DCM_CheckBoth)
				{
					auto data = GetDataRead(t);

					DataCheckMode localDcm = CheckMode(data.begin(), data.end(), valuesRange, DataCheckMode(dcm.load()) );

					dcm |= localDcm;
				}
			}
		);
		if (!valuesRangeData)
			dcm &= ~DCM_CheckRange;
		return DataCheckMode(dcm.load());
	}
}

template <typename V>
void DataArrayBase<V>::DoSimplifyCheckMode(DataCheckMode& dcm) const
{
	if constexpr (has_var_range_field_v<V>)
	{
		if (dcm != DCM_CheckBoth)
			return;
		auto vrd = GetValueRangeData();
		if (vrd && !ContainsUndefined(vrd->GetRange()))
			dcm = DCM_CheckRange; // implies excluding UNDEFINED_VALUE()
	}
	else
	{
		dms_assert(dcm == DCM_None || dcm == DCM_CheckDefined);
	}
}

template <typename V>
LispRef DataArrayBase<V>::GetValuesAsKeyArgs(LispPtr valuesUnitKeyExpr) const
{
	// TODO G8: Merge code with slUnionDataLispExpr TODO G8: Consider removing this function from AbstrDataObj.h interface
	LispRef tail;
	SizeT i = m_TileRangeData->GetElemCount();
	while (i)
		tail = LispRef(AsLispRef(GetIndexedValue(--i), valuesUnitKeyExpr), tail);
	return tail;
}

//----------------------------------------------------------------------
// Serialization and rtti
//----------------------------------------------------------------------

// normal code
template <class V> const Class* TileFunctor<V>::GetDynamicClass() const { return GetStaticClass(); }

#include "utl/mySPrintF.h"

template <typename T>
TokenID GetDataItemClassID()
{
	return GetTokenID_st(myArrayPrintF<100>("DataItem<%s>", ValueWrap<T>::GetStaticClass()->GetID().c_str_st()));
}

template <typename T>
const DataItemClass* TileFunctor<T>::GetStaticClass()
{
	static DataItemClass s_Cls(
		nullptr
	,	GetDataItemClassID<T>()
	,	ValueWrap<T>::GetStaticClass()
	);
	return &s_Cls;
}

//----------------------------------------------------------------------
// DataWriteLock helper funcs
//----------------------------------------------------------------------

template<typename V>
TIC_CALL auto CreateHeapTileArray_impl(const AbstrTileRangeData* tdr, bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG)->std::unique_ptr<TileFunctor<V>>
{
	dms_assert(tdr);
	if constexpr (has_fixed_elem_size_v<V>) // TODO G8: Remove this restriction, especially for SharedStr parameters
	{
		if (tdr->GetRangeSize() == 1)
		{
			dms_assert(tdr->GetNrTiles() == 1);
			return std::make_unique<HeapSingleValue<V>>(tdr);
		}
	}
	if (tdr->GetNrTiles() == 1)
	{
		return std::make_unique<HeapSingleArray<V>>(tdr, mustClear);
	}
	if (tdr->GetNrTiles() == 0 && tdr->GetElemCount() != 0)
		throwDmsErrD("CreateHeapTileArray cannot create a DataArray for an unspecified domain");

	auto newTileFunctor = std::make_unique<HeapTileArray<V>>(tdr, mustClear);
#if defined(MG_DEBUG_ALLOCATOR)
	newTileFunctor->md_SrcStr = srcStr;
#endif //defined(MG_DEBUG_ALLOCATOR)
	return std::unique_ptr<TileFunctor<V>>{ newTileFunctor.release() };
}


template<typename V>
TIC_CALL auto CreateHeapTileArrayU(const AbstrTileRangeData* tdr, const Unit<field_of_t<V>>* valuesUnitPtr, bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG)->std::unique_ptr<TileFunctor<V>>
{
	dms_assert(!valuesUnitPtr || valuesUnitPtr->GetCurrRangeItem() == valuesUnitPtr);
	auto newTileFunctor = CreateHeapTileArray_impl<V>(tdr, mustClear MG_DEBUG_ALLOCATOR_SRC_PARAM);

	newTileFunctor->InitValueRangeData(get_range_ptr_of_valuesunit(valuesUnitPtr));
	return newTileFunctor;
}

auto instantiateThis = & CreateHeapTileArrayU<UInt32>;

template<typename V>
TIC_CALL auto CreateHeapTileArrayV(const AbstrTileRangeData* tdr, const range_or_void_data<field_of_t<V>>* valuesRangeDataPtr, bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG) -> std::unique_ptr<TileFunctor<V>>
{
	auto newTileFunctor = CreateHeapTileArray_impl<V>(tdr, mustClear MG_DEBUG_ALLOCATOR_SRC_PARAM);

	if constexpr (has_var_range_v<V>)
		newTileFunctor->InitValueRangeData( valuesRangeDataPtr );
	return newTileFunctor;
}

auto CreateAbstrHeapTileFunctor(const AbstrDataItem* adi, const SharedObj* abstrValuesRangeData, const bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG) -> std::unique_ptr<AbstrDataObject>
{
	MG_CHECK(adi->GetAbstrDomainUnit());
	MG_CHECK(adi->GetAbstrValuesUnit());
	dbg_assert(adi->GetAbstrDomainUnit()->CheckMetaInfoReadyOrPassor());
	dbg_assert(adi->GetAbstrValuesUnit()->CheckMetaInfoReadyOrPassor());
	SharedPtr<const AbstrTileRangeData> currTRD = AsUnit(adi->GetAbstrDomainUnit()->GetCurrRangeItem())->GetTiledRangeData();
	MG_CHECK(currTRD);
	SharedPtr<const AbstrUnit> valuesUnit = AsUnit(adi->GetAbstrValuesUnit()->GetCurrRangeItem());

	// DEBUG: SEVERE TILING
	if (currTRD->GetNrTiles() > 1 && !adi->IsCacheItem())

		reportF(SeverityTypeID::ST_MinorTrace, "CreateAbstrHeapTileFunctor(attribute<%s> %s(%d tiles), ", adi->GetAbstrValuesUnit()->GetValueType()->GetName(), adi->GetFullName().c_str(), currTRD->GetNrTiles());
	if (!abstrValuesRangeData)
		abstrValuesRangeData = AsUnit(adi->GetAbstrValuesUnit()->GetCurrRangeItem())->GetTiledRangeData();

	std::unique_ptr<AbstrDataObject> resultHolder;
	switch (adi->GetValueComposition())
	{
	case ValueComposition::Single:
		visit<typelists::fields>(valuesUnit, 
			[&resultHolder, adi, currTRD, abstrValuesRangeData, mustClear MG_DEBUG_ALLOCATOR_SRC_PARAM] <typename value_type> (const Unit<value_type>* valuesUnitPtr)
			{
				resultHolder.reset(CreateHeapTileArrayV<value_type>(currTRD, dynamic_cast<const range_or_void_data<value_type>*>(abstrValuesRangeData), mustClear MG_DEBUG_ALLOCATOR_SRC_PARAM).release());
			}
		);
		break;
	case ValueComposition::Sequence:
	case ValueComposition::Polygon:
		visit<typelists::sequence_fields>(valuesUnit, 
			[&resultHolder, adi, currTRD, abstrValuesRangeData, mustClear MG_DEBUG_ALLOCATOR_SRC_PARAM] <typename value_type> (const Unit<value_type>*valuesUnitPtr)
			{
				using element_type = typename sequence_traits<value_type>::container_type;

				resultHolder.reset(CreateHeapTileArrayV<element_type>(currTRD, dynamic_cast<const range_or_void_data<value_type>*>(abstrValuesRangeData), mustClear MG_DEBUG_ALLOCATOR_SRC_PARAM).release());
			}
		);
		break;

	}
	return resultHolder;
}

auto CreateFileTileArray(const AbstrDataItem* adi, dms_rw_mode rwMode, SharedStr filenameBase, bool isTmp, SafeFileWriterArray* sfwa) -> std::unique_ptr<AbstrDataObject>
{
	MG_CHECK(adi->GetAbstrDomainUnit());
	MG_CHECK(adi->GetAbstrValuesUnit());

	SharedPtr<const AbstrTileRangeData> currTRD = adi->GetAbstrDomainUnit()->GetTiledRangeData();
	dms_assert(currTRD);

	std::unique_ptr<AbstrDataObject> resultHolder;
	switch (adi->GetValueComposition())
	{
	case ValueComposition::Single:
		visit<typelists::fields>(adi->GetAbstrValuesUnit(), 
			[&resultHolder, adi, currTRD, rwMode, filenameBase, isTmp, sfwa] <typename value_type> (const Unit<value_type>* valuesUnitPtr)
			{
				auto newTileFunctor = std::make_unique<FileTileArray<value_type>>(currTRD, filenameBase, rwMode, isTmp, sfwa);
				newTileFunctor->InitValueRangeData(get_range_ptr_of_valuesunit(valuesUnitPtr));
				resultHolder.reset(newTileFunctor.release());
			}
		);
		break;
	case ValueComposition::Sequence:
	case ValueComposition::Polygon:
		visit<typelists::sequence_fields>(adi->GetAbstrValuesUnit(), 
			[&resultHolder, adi, currTRD, rwMode, filenameBase, isTmp, sfwa] <typename value_type> (const Unit<value_type>* valuesUnitPtr)
			{
				using element_type = typename sequence_traits<value_type>::container_type;
				auto newTileFunctor = std::make_unique<FileTileArray<element_type> >(currTRD, filenameBase, rwMode, isTmp, sfwa);
				newTileFunctor->InitValueRangeData(get_range_ptr_of_valuesunit(valuesUnitPtr));
				resultHolder.reset(newTileFunctor.release());
			}
		);
		break;

	}
	return resultHolder;
}

//----------------------------------------------------------------------
// instantiation and registration
//----------------------------------------------------------------------

#include "rtcTypeLists.h"
#include "utl/TypeListOper.h"
#include "TileArrayImpl.h"

//#include <boost/mpl/for_each.hpp>
//template <class T> struct wrop {};

namespace  {
	/*
	struct Instantiator {
		template <class T> // T must be derived from Object (or at least have GetStaticClass and GetDynamicClass defined)
		void operator ()(wrap<T>) const // deduce T
		{
//			T::GetStaticClass();   // Call to register
			&(HeapTileArray<T>::HeapTileArray); // take address to instantiate
		}
	};

	Instantiator inst;
	boost::mpl::for_each<typelists::value_elements, wrap<_> >(inst);
	*/
	TypeListClassReg<tl::transform<typelists::value_elements, DataArrayBase<_>>> s_DAB;
	TypeListClassReg<tl::transform<typelists::value_elements, TileFunctor<_>>>   s_TFR;
	TypeListClassReg<tl::transform<typelists::value_elements, HeapTileArray<_>>> s_HTA;
	TypeListClassReg<tl::transform<typelists::value_elements, FileTileArray<_>>> s_FTA;
}

// Explicit Template Instantiation; TODO G8: Why?

template SizeT DataArrayBase<Bool>::CountValues(param_t v) const;

//----------------------------------------------------------------------
// DMS interface functions
//----------------------------------------------------------------------

#include "dbg/DmsCatch.h"
#include "TreeItemContextHandle.h"

/*********************************** DMS_##T##AttrGetValue *************************************/
/*********************************** DMS_##T##AttrGetValueArray *************************************/

#define INSTANTIATE(T) \
TIC_CALL api_type<T>::type DMS_CONV DMS_##T##Attr_GetValue(const AbstrDataItem* self, SizeT index) \
{ \
	DMS_CALL_BEGIN \
		TreeItemContextHandle checkPtr(self, DataArray<T>::GetStaticClass(), "DMS_" #T "Attr_GetValue"); \
		dms_assert(self->GetInterestCount()); \
		PreparedDataReadLock lock(self); \
		return const_array_cast<T>(self)->GetIndexedValue(index); \
	DMS_CALL_END \
	return UNDEFINED_OR_ZERO(T); \
} \
 \
TIC_CALL void DMS_CONV DMS_##T##Attr_GetValueArray(const AbstrDataItem* self, SizeT firstRow, SizeT len, api_type<T>::type* valueBuffer) \
{ \
	DMS_CALL_BEGIN \
		TreeItemContextHandle checkPtr(self, DataArray<T>::GetStaticClass(), "DMS_" #T "Attr_GetValueArray"); \
		dms_assert(self->GetInterestCount()); \
		PreparedDataReadLock lock(self); \
		return const_array_cast<T>(self)->GetIndexedValueArray(firstRow, len, valueBuffer); \
	DMS_CALL_END \
}

INSTANTIATE_NUM_ORG
INSTANTIATE(Bool)
INSTANTIATE(SharedStr)

#undef INSTANTIATE

static_assert(sizeof(bool) == 1);

/*********************************** DMS_##T##Attr_SetValue *************************************/
/*********************************** DMS_##T##Attr_SetValueArray *************************************/

#define INSTANTIATE(T) \
TIC_CALL void DMS_CONV DMS_##T##Attr_SetValue(AbstrDataItem* self, SizeT index, api_type<T>::type value) \
{ \
	DMS_CALL_BEGIN \
		TreeItemContextHandle checkPtr(self, DataArray<T>::GetStaticClass(), "DMS_" #T "Attr_SetValue"); \
		DataWriteLock lock(self, dms_rw_mode::read_write); \
		mutable_array_cast<T>(lock)->SetIndexedValue(index, value); \
		lock.Commit(); \
	DMS_CALL_END \
} \
TIC_CALL void DMS_CONV DMS_##T##Attr_SetValueArray(AbstrDataItem* self, SizeT firstRow, SizeT len, const api_type<T>::type* values) \
{ \
	DMS_CALL_BEGIN \
		TreeItemContextHandle checkPtr(self, DataArray<T>::GetStaticClass(), "DMS_" #T "Attr_SetValueArray"); \
		DataWriteLock lock(self, (firstRow > 0) || (len < self->GetAbstrDomainUnit()->GetCount()) ? dms_rw_mode::read_write : dms_rw_mode::write_only_all); \
		mutable_array_cast<T>(lock)->SetIndexedValueArray(firstRow, len, values); \
		lock.Commit(); \
	DMS_CALL_END \
} 

INSTANTIATE_NUM_ORG
INSTANTIATE(Bool)

#undef INSTANTIATE

