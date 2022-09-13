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

#if !defined(__TIC_TILELOCS_H)
#define __TIC_TILELOCS_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "ptr/WeakPtr.h"
#include "ser/FileCreationMode.h"
//REMOVE class AbstrDataObject;

//----------------------------------------------------------------------
// class  : locked_sequence
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

