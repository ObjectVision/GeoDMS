// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_SET_FILEVIEW_H)
#define __RTC_SET_FILEVIEW_H

#include "geo/Conversions.h"
#include "geo/Undefined.h"
#include "geo/SizeCalculator.h"
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

	file_view_base(std::shared_ptr<mapped_file_type> mfh, SizeT nrElem, dms::filesize_t fileOffset = -1, dms::filesize_t fileViewSize = -1)
		: FVH(std::move(mfh), fileOffset, fileViewSize)
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
		SizeT cap = capacity_calculator<T>().Byte2Size(this->GetViewSize());
		assert(this->GetViewSize() <= cap);
		return cap;
	}
	SizeT max_size() const { return SizeT(-1) / sizeof(T); }

	/*
	void CloseFVB()
	{
		FileMapHandle::CloseFMH();
		m_NrElems = 0;
	}

	void Drop(WeakStr fileName)
	{
		FileMapHandle::Drop(fileName);
		m_NrElems = 0;
	}
	*/

protected:
	file_view_base() {}
	SizeT m_NrElems = 0;
};

//----------------------------------------------------------------------
// Section      : const_file_view
//----------------------------------------------------------------------

const SizeT useExistingSize = UNDEFINED_VALUE(SizeT);

template <typename T>
struct const_file_view : file_view_base<T, ConstFileViewHandle>
{
	using base_type = file_view_base<T, ConstFileViewHandle>;
	using typename base_type::const_iterator;
	using typename base_type::const_reference;
	using file_view_base<T, ConstFileViewHandle>::file_view_base; // inherit ctors

	void Open(WeakStr fileName, SafeFileWriterArray* sfwa, tile_id nrElems, bool throwOnError = true )
	{
		this->OpenForRead(fileName, sfwa, throwOnError, true);
		if (!this->IsOpen())
			return;

		if (!IsDefined(nrElems))
		{
			dms::filesize_t fileSize = this->GetFileSize();
			nrElems = IsDefined(fileSize) ? size_calculator<T>().max_elems(fileSize) : 0;
		}
		if (this->GetFileSize() != size_calculator<T>().nr_bytes(nrElems))
			throwErrorF("const_file_view", "FileSize of %u expected but file %s has a size of %u bytes"
			,	size_calculator<T>().nr_bytes(nrElems)
			,	fileName.c_str()
			, this->GetFileSize()
			);
		this->m_NrElems = nrElems;
	}
	const mempage_file_view* SequenceMemPageAllocTable() const
	{
		return this->m_MappedFile->m_MemPageAllocTable.get();
	}
};

//----------------------------------------------------------------------
// Section      : rw_file_view
//----------------------------------------------------------------------

template <typename T>
struct rw_file_view : file_view_base<T, FileViewHandle>
{
	using base_type = file_view_base<T, FileViewHandle>;

	//	somehow the following is neccesary for ProdConfig.cpp to avoid confusion with std:iterator
	//	maybe somewhere there is a using std:: ??
	using typename base_type::const_iterator;
	using typename base_type::const_reference;

	using iterator = typename sequence_traits<T>::pointer;
	using reference = typename sequence_traits<T>::reference;

	using base_type::DataBegin;
	using base_type::m_NrElems;

	using file_view_base<T, FileViewHandle>::file_view_base; // inherit ctors
	using file_view_base<T, FileViewHandle>::operator =;

	void Open(WeakStr fileName, SafeFileWriterArray* sfwa, SizeT nrElems, dms_rw_mode rwMode, bool isTmp)
	{
		dms_assert(rwMode != dms_rw_mode::unspecified);
		dms_assert(rwMode != dms_rw_mode::check_only);

		if (rwMode != dms_rw_mode::read_only)
			this->OpenRw(fileName, sfwa
			, IsDefined(nrElems) ? size_calculator<T>().nr_bytes(nrElems) : UNDEFINED_FILE_SIZE
			, rwMode, isTmp
			);
		else
			this->OpenForRead(fileName, sfwa, true, true);

		if (!IsDefined(nrElems))
			nrElems = size_calculator<T>().max_elems(this->GetFileSize());
		m_NrElems = nrElems;

		dms_assert(this->GetFileSize() % sizeof(sequence_traits<T>::block_type) == 0);

		if (this->GetFileSize() != size_calculator<T>().nr_bytes(nrElems)
			|| size_calculator<T>().nr_blocks(nrElems) != this->GetFileSize() / sizeof(sequence_traits<T>::block_type)) // catches overflow error on nrBytes calculation
		{
			file_view_base<T>::CloseFVB();
			throwErrorF("rw_file_view", "FileSize of %u expected but file %s has a size of %u for %u elements"
			,	size_calculator<T>().nr_bytes(nrElems)
			,	fileName.c_str()
			, this->GetFileSize()
			,	nrElems
			);
		}
	}
	void CloseWFV()
	{
		this->SetFileSize( size_calculator<T>().nr_bytes(m_NrElems) );
		file_view_base<T>::CloseFVB();
	}

	void reserve(SizeT nrElem)
	{
		assert(nrElem <= this->max_size());
		MG_CHECK(nrElem < SizeT(-1) / sizeof(T));
		this->realloc(nrElem * sizeof(T));
	}

	void resize(SizeT newNrElems)
	{
		if (newNrElems > this->filed_capacity())
			throwErrorD("rw_file_view", "cannot grow a FileMapping");
		m_NrElems = newNrElems;
	}

	void grow(SizeT newNrElems)
	{
		if (this->filed_size() + newNrElems > this->filed_capacity())
			throwErrorD("rw_file_view", "cannot grow a FileMapping");
//		m_FileSize = size_calculator<T>().nr_bytes(newNrElems);
		m_NrElems += newNrElems;
	}
	iterator       begin()       { return iter_creator<T>()( DataBegin(), 0 ); }
	iterator       end()         { return iter_creator<T>()( DataBegin(), m_NrElems); }
	const_iterator begin() const { return iter_creator<T>()( DataBegin(), 0 ); }
	const_iterator end()   const { return iter_creator<T>()( DataBegin(), m_NrElems); }
};

struct mempage_file_view : rw_file_view < IndexRange<SizeT> >
{
	using rw_file_view < IndexRange<SizeT> >::rw_file_view; // inherit ctors
};

#endif //!defined(__RTC_SET_FILEVIEW_H)
