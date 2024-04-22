// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_MEM_TILEDATA_H)
#define __RTC_MEM_TILEDATA_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "mem/ManagedAllocData.h"
#include "mem/SequenceObj.h"
#include "ptr/OwningPtrSizedArray.h"
#include "ptr/SharedObj.h"
#include "set/BitVector.h"

//----------------------------------------------------------------------
// class  : tile
//----------------------------------------------------------------------

template <typename V>
struct tile : sequence_traits<V>::tile_container_type, TileBase // TODO G8: replace by OwningArrayPtr
{
	using TileBase::TileBase;
};

template <typename V> struct mapped_file_tile;
template <typename V>
struct file_tile : sequence_traits<V>::polymorph_vec_t, TileBase // TODO G8: replace by OwningArrayPtr
{
	using TileBase::TileBase;

	// Open(tile_offset nrElem, dms_rw_mode rwMode, bool isTmp, SafeFileWriterArray* sfwa)

	std::shared_ptr<mapped_file_tile<V>> get(dms_rw_mode rwMode) const
	{
		std::lock_guard guard(cs_file);
//		dms_assert(this->IsOpen());
		auto result = m_OpenFile.lock();
		if (!result)
		{
			result = std::make_shared<mapped_file_tile<V>>(this, rwMode);
			m_OpenFile = result;
			assert(this->IsLocked());
		}
		return result;
	}

	mutable std::mutex cs_file;
	mutable std::weak_ptr<mapped_file_tile<V>> m_OpenFile;
	mutable UInt32 m_NrMappedFileTiles = 0;
};

template <typename V> struct mapped_file_tile : TileBase
{ 
	const file_tile<V>* m_Info;

	mapped_file_tile(const file_tile<V>* info, dms_rw_mode rwMode)
		: m_Info(info)
	{
		assert(info);
		if (!info->m_NrMappedFileTiles++)
			info->Lock(rwMode);
		dbg_assert(info->IsLocked());
	}

	~mapped_file_tile()
	{
		if (!m_Info)
			return;

		std::lock_guard guard(m_Info->cs_file);

		dbg_assert(m_Info->IsLocked());
		m_Info->m_OpenFile = {}; // free control block, this requires the guard
		if (!--m_Info->m_NrMappedFileTiles)
		{
			m_Info->UnLock();
			dbg_assert(!m_Info->IsLocked());
		}
		m_Info = nullptr;
	}
};


#endif // __RTC_MEM_TILEDATA_H
