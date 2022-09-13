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

#if !defined(__TIC_TILEITER_H)
#define __TIC_TILEITER_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "ptr/WeakPtr.h"
class AbstrDataObject;

//----------------------------------------------------------------------
// TileIter (for reading data; see write_tile_channel)
//----------------------------------------------------------------------

template <typename V>
struct ConstTileIter
{
	ConstTileIter(const DataArray<V>* dataObj, SizeT i = 0)
		:	m_DataObj(dataObj)
		,	m_CSeq(dataObj->GetLockedDataRead(dataObj->GetAbstrDomainUnit()->GetTileID(i)))
		,	m_Ptr(m_CSeq.begin() + i)
	{}
	ConstTileIter(ConstTileIter&&) = default;
	ConstTileIter(const ConstTileIter&) = default;

	typedef std::random_access_iterator_tag               iterator_category;
	typedef          V                                    value_type;
	typedef          SizeT                                size_type;
	typedef          DiffT                                difference_type;
	typedef typename sequence_traits<V>::cseq_t           cseq_t;
	typedef typename locked_seq<cseq_t, ReadableTileLock> locked_cseq_t;
	typedef typename sequence_traits<V>::const_pointer    const_pointer;
	typedef typename sequence_traits<V>::const_reference  const_reference;
	typedef typename sequence_traits<V>::const_pointer    pointer;
	typedef typename sequence_traits<V>::const_reference  reference;

	V operator [](SizeT i) const
	{
		return *(*this+i);
	}
	const_reference operator *() const
	{
		return *m_Ptr;
	}
	ConstTileIter operator +(difference_type i) const
	{
		ConstTileIter result = *this;
		result += i;
		return result;
	}
	void operator +=(size_type i)
	{
		dms_assert(i >= 0);
		dms_assert(m_Ptr >= m_CSeq.begin());
		dms_assert(m_Ptr <  m_CSeq.end  ());

		SizeT rest = m_CSeq.end() - m_Ptr;
		if (rest > i)
		{
			m_Ptr += i;
			return;
		}
		i -= rest;
		tile_id t = m_CSeq.GetTileID();
	retry:
		rest = m_DataObj->GetAbstrDomainUnit()->GetTileSize(++t);
		if (rest > i)
		{
			m_CSeq = m_DataObj->GetLockedDataRead(t);
			dms_assert(m_CSeq.size() == rest);
			m_Ptr = m_CSeq.begin() + i;
			return;
		}
		i -= rest;
		goto retry;
	}
	void operator +=(difference_type i)
	{
		if (i>=0)
		{
			operator +=(size_type(i));
			return;
		}
		SizeT rest = m_Ptr - m_CSeq.begin();
		if (i + rest >= 0)
		{
			m_Ptr += i;
			return;
		}
		i += rest;
		tile_id t = m_CSeq.GetTileID();
retry:
		dms_assert(t>0);
		rest = m_DataObj->GetAbstrDomainUnit()->GetTileSize(--t);
		if (i + rest >= 0)
		{
			m_CSeq = m_DataObj->GetLockedDataRead(t);
			dms_assert(m_CSeq.size() == rest);
			m_Ptr = m_CSeq.end() + i;
		}
		i += rest;
		goto retry;
	}
	void operator -=(difference_type i)
	{
		operator +=(-i);
	}

	ConstTileIter& operator ++()
	{
		this->operator +=(1);
		return *this;
	}
	ConstTileIter operator ++(int)
	{
		ConstTileIter tmp = *this;
		this->operator ++();
		return tmp;
	}

	WeakPtr<const DataArray<V> > m_DataObj;
	locked_cseq_t                m_CSeq;
	const_pointer                m_Ptr;
};

#endif //!defined(__TIC_TILEITER_H)

