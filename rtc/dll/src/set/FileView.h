// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__RTC_SET_FILEVIEW_H)
#define __RTC_SET_FILEVIEW_H

#include "geo/Conversions.h"
#include "geo/Undefined.h"
#include "geo/SizeCalculator.h"
#include "ser/FileMapHandle.h"

//----------------------------------------------------------------------
// Section      : file_view_base
//----------------------------------------------------------------------

template <typename T>
struct file_view_base : FileMapHandle
{
	typedef typename sequence_traits<T>::pointer         iterator;
	typedef typename sequence_traits<T>::const_pointer   const_iterator;
	typedef typename sequence_traits<T>::reference       reference;
	typedef typename sequence_traits<T>::const_reference const_reference;

	const_iterator begin() const { return iter_creator<T>()( DataBegin(), 0 ); }
	const_iterator end()   const { return iter_creator<T>()( DataBegin(), m_NrElems); }

	SizeT filed_size() const
	{
		dms_assert(!IsUsable() || begin() + m_NrElems == end());
		return m_NrElems;
	}
	SizeT filed_capacity() const
	{
		SizeT cap = capacity_calculator<T>().Byte2Size(GetFileSize());
		dms_assert(filed_size() <= cap);
		return cap;
	}
	SizeT max_size() const { return SizeT(-1) / sizeof(T); }

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

protected:
	file_view_base() : m_NrElems( 0 ) {}
	SizeT m_NrElems;
};

//----------------------------------------------------------------------
// Section      : const_file_view
//----------------------------------------------------------------------

const SizeT useExistingSize = UNDEFINED_VALUE(SizeT);

template <typename T>
struct const_file_view : file_view_base<T>
{
	using base_type = file_view_base<T>;

	const_file_view() {}
	const_file_view(WeakStr fileName, SafeFileWriterArray* sfwa, SizeT nrElems = useExistingSize, bool throwOnError = true )
	{
		Open(fileName, sfwa, nrElems, throwOnError);
	}

	void Open(WeakStr fileName, SafeFileWriterArray* sfwa, SizeT nrElems, bool throwOnError = true )
	{
		this->OpenForRead(fileName, sfwa, throwOnError, true);
		if (this->IsOpen())
		{
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
	}
};

//----------------------------------------------------------------------
// Section      : rw_file_view
//----------------------------------------------------------------------

template <typename T>
struct rw_file_view : file_view_base<T>
{
	using base_type = file_view_base<T>;

	//	somehow the following is neccesary for ProdConfig.cpp to avoid confusion with std:iterator
	//	maybe somewhere there is a using std:: ??
	using typename base_type::iterator;
	using typename base_type::const_iterator;
	using typename base_type::reference;
	using typename base_type::const_reference;
	using base_type::DataBegin;
	using base_type::m_NrElems;

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

	void reserve(SizeT nrElem, WeakStr handleName, SafeFileWriterArray* sfwa)
	{
		dms_assert(nrElem <= this->max_size());
		this->realloc(nrElem * sizeof(T), handleName, sfwa );
	}

	void resize(SizeT newNrElems)
	{
		if (newNrElems > this->filed_capacity())
			throwErrorD("rw_file_view", "cannot grow a FileMapping");
//		m_FileSize = size_calculator<T>().nr_bytes(newNrElems);
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


#endif //!defined(__RTC_SET_FILEVIEW_H)
