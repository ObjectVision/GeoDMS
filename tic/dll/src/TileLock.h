// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__TIC_TILELOCS_H)
#define __TIC_TILELOCS_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "ptr/WeakPtr.h"
#include "ser/FileCreationMode.h"

//----------------------------------------------------------------------
// class  : locked_seq
//----------------------------------------------------------------------

template <typename Resource>
struct non_polluting_base {
	non_polluting_base() = default;
	non_polluting_base(Resource&& r)
	:	m_TileHolder(std::move(r))
	{}
	non_polluting_base(const Resource& r)
		: m_TileHolder(r)
	{}

	Resource m_TileHolder;
};

template <typename Seq, typename TileHolderType>
struct locked_seq : non_polluting_base<TileHolderType>, Seq
{
	locked_seq() = default;

	locked_seq(TileHolderType&& lck, Seq seq)
		: non_polluting_base<TileHolderType>(std::move(lck))
		, Seq(std::move(seq))
	{}


	locked_seq(const locked_seq& rhs) = default;
	locked_seq(locked_seq&& rhs) = default;

	locked_seq& operator =(const locked_seq& rhs) = default;
	locked_seq& operator =(locked_seq&& rhs) = default;

	operator bool() const { return this->m_TileHolder.get_ptr() != nullptr; }

	Seq get_view() const { return *this; }
};


#endif //!defined(__TIC_TILELOCS_H)

