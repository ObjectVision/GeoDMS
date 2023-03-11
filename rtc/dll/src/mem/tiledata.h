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
// struct TileUseMode
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// class  : tile
//----------------------------------------------------------------------

template <typename V>
struct tile : sequence_traits<V>::tile_container_type, TileBase // TODO G8: replace by OwningArrayPtr
{
	using TileBase::TileBase;
};


template <typename V> struct mapped_file;
template <typename V>
struct file : sequence_traits<V>::polymorph_vec_t, TileBase // TODO G8: replace by OwningArrayPtr
{
	using TileBase::TileBase;

	// Open(tile_offset nrElem, dms_rw_mode rwMode, bool isTmp, SafeFileWriterArray* sfwa)

	std::shared_ptr<mapped_file<V>> get(dms_rw_mode rwMode)
	{
		auto lock = std::lock_guard(cs_file);
		dms_assert(this->IsOpen());
		auto result = m_OpenFile.lock();
		if (!result)
		{
			result = std::make_shared<mapped_file<V>>(this, rwMode);
			m_OpenFile = result;
		}
		return result;
	}

	~file()
	{
		this->Close();
	}

	std::mutex cs_file;
	std::weak_ptr<mapped_file<V>> m_OpenFile;
};

template <typename V> struct mapped_file : TileBase
{ 
	SharedPtr< file<V>  > m_Info;
//	bool                  m_IsTmp = false;

	mapped_file(file<V>* info, dms_rw_mode rwMode)
		: m_Info(info)
	{
		dms_assert(info);
		dms_assert(info->IsOpen());
		dbg_assert(!info->IsLocked());
		info->Lock(rwMode);
		dbg_assert(!info->IsLocked());
	}
	~mapped_file()
	{
		if (!m_Info)
			return;

		auto lock = std::lock_guard(m_Info->cs_file);

		m_Info->m_OpenFile = {}; // free control block.
		m_Info->UnLock();
	}
};

#endif // __RTC_MEM_TILEDATA_H
