// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_SET_FILEVIEW_H)
#define __RTC_SET_FILEVIEW_H

#include "vt/Conversions.h"
#include "vt/Undefined.h"
#include "vt/SizeCalculator.h"
#include "ser/FileMapHandle.h"

//----------------------------------------------------------------------
// Section      : file_view_base
//----------------------------------------------------------------------

template <typename T, typename FVH>
struct file_view_base : FVH
{
	using const_iterator = typename sequence_traits<T>::const_pointer;
	using const_reference = typename sequence_traits<T>::const_reference;
	using mapped_file_type = typename FVH::mapped_file_type;

	file_view_base(std::shared_ptr<mapped_file_type> mfh, SizeT nrElem, dms::filesize_t fileOffset = -1, dms::filesize_t fileViewCapacity = -1)
		: FVH(std::move(mfh), fileOffset
			, nrElem  == SizeT(-1) ? nrElem : size_calculator<T>().nr_bytes(nrElem)
			, fileViewCapacity == UNDEFINED_FILE_SIZE ? size_calculator<T>().nr_bytes(nrElem) : fileViewCapacity
		)
		, m_NrElems(nrElem)
	{}

	file_view_base(file_view_base&&) = default;
	file_view_base& operator = (file_view_base&&) = default;

	const_iterator begin() const { return iter_creator<T>()(this->DataBegin(), 0 ); }
	const_iterator end()   const { return iter_creator<T>()(this->DataBegin(), m_NrElems); }

	SizeT filed_size() const
	{
		assert(!this->IsUsable() || begin() + m_NrElems == end());
		return m_NrElems;
	}
	SizeT filed_capacity() const
	{
		SizeT cap = capacity_calculator<T>::Byte2Size(this->GetViewCapacity());
		assert(m_NrElems <= cap);
		return cap;
	}
	SizeT max_size() const { return SizeT(-1) / sizeof(T); }

	void SetNrElemsWithoutUpdatingMemPageAllocTable(SizeT newNrElems)
	{
		m_NrElems = newNrElems;
		this->m_ViewSpec.size = size_calculator<T>().nr_bytes(newNrElems);
	}
	void SetNrElems(SizeT newNrElems)
	{
		assert(m_TileID != no_tile);

		SetNrElemsWithoutUpdatingMemPageAllocTable(newNrElems);

		auto mappedFile = this->m_MappedFile.get();
		assert(mappedFile);
		auto memPageAllocTable = this->m_MappedFile->m_MemPageAllocTable.get();
		assert(memPageAllocTable || m_TileID == 0);
		if (memPageAllocTable)
			(*memPageAllocTable)[m_TileID].size = this->m_ViewSpec.size;
	}

	file_view_base() {}
	SizeT m_NrElems = 0;
	tile_id m_TileID = no_tile;
};

//----------------------------------------------------------------------
// Section      : const_file_view
//----------------------------------------------------------------------

template <typename T>
struct const_file_view : file_view_base<T, ConstFileViewHandle>
{
	using base_type = file_view_base<T, ConstFileViewHandle>;
	using typename base_type::const_iterator;
	using typename base_type::const_reference;
	using file_view_base<T, ConstFileViewHandle>::file_view_base; // inherit ctors
};

//----------------------------------------------------------------------
// Section      : rw_file_view
//----------------------------------------------------------------------

template <typename T>
struct rw_file_view : file_view_base<T, FileViewHandle>
{
	using base_type = file_view_base<T, FileViewHandle>;

	// These re-declarations stop the names resolving to std::iterator / std::reference in some TU.
	// The TU that needed it (ProdConfig.cpp) no longer exists, so this may now be removable --
	// verify against all build flavours before dropping it.
	using typename base_type::const_iterator;
	using typename base_type::const_reference;

	using iterator = typename sequence_traits<T>::pointer;
	using reference = typename sequence_traits<T>::reference;

	using base_type::DataBegin;
	using base_type::m_NrElems;

	using file_view_base<T, FileViewHandle>::file_view_base; // inherit ctors
	using file_view_base<T, FileViewHandle>::operator =;

	void ReserveAndMapElems(SizeT nrReservedElem)
	{
		assert(nrReservedElem <= this->max_size());
		MG_CHECK(nrReservedElem < SizeT(-1) / sizeof(T));
		MG_CHECK(size_calculator<T>().nr_bytes(m_NrElems) == this->m_ViewSpec.size);
		this->AllocAndMapFile(size_calculator<T>().nr_bytes(nrReservedElem));
	}

	void resize(SizeT newNrElems)
	{
		if (newNrElems > this->filed_capacity())
			throwErrorD("rw_file_view", "cannot grow a FileMapping");
		this->SetNrElems(newNrElems);
	}
	iterator       begin()       { return iter_creator<T>()( DataBegin(), 0 ); }
	iterator       end()         { return iter_creator<T>()( DataBegin(), m_NrElems); }
	const_iterator begin() const { return iter_creator<T>()( DataBegin(), 0 ); }
	const_iterator end()   const { return iter_creator<T>()( DataBegin(), m_NrElems); }

	      T& operator[](SizeT i)       { return *(begin() + i); }
	const T& operator[](SizeT i) const { return *(begin() + i); }
};

#endif //!defined(__RTC_SET_FILEVIEW_H)
