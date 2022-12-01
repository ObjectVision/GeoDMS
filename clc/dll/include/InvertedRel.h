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

#pragma once

#if !defined(__CLC_INVERTREL_H)
#define __CLC_INVERTREL_H

// *****************************************************************************
//									optional n:1 relation
// *****************************************************************************

template <typename CIter, typename V> V LookupOrSame(CIter arrayPtr, V v) { return (arrayPtr) ? arrayPtr[v] : v; }

// *****************************************************************************
//	Invert: (DomType -> ValType)
//		->  First: ValType -> DomType
//			Next: DomType -> DomType
// *****************************************************************************


template <typename DomType>
struct Inverted_rel {

	template <typename ValType> void Init(typename sequence_traits<ValType>::const_pointer value_rel, DomType nrD, ValType nrV)
	{
		dms_assert(nrD == 0 || value_rel);
		InitData(nrD, nrV);
		for (DomType edgeNr = 0; edgeNr != nrD; ++edgeNr, ++value_rel)
			Assign<ValType>(*value_rel, nrV, edgeNr);
	}
	template <typename ValType> void Init(typename sequence_traits<ValType>::const_pointer value_rel, DomType nrD, ValType nrV, DataArray<Bool>::const_iterator bidirFlagIter)
	{
		dms_assert(nrD == 0 || (value_rel && bidirFlagIter));

		InitData(nrD, nrV);
		for (DomType edgeNr = 0; edgeNr != nrD; ++edgeNr, ++value_rel, ++bidirFlagIter)
			if ((Bool)*bidirFlagIter)
				Assign<ValType>(*value_rel, nrV, edgeNr);
	}
	void InitAllToOne( DomType nrD)
	{
		InitData(nrD, 1);
		for (DomType edgeNr = 0; edgeNr != nrD; ++edgeNr)
			Assign<DomType>(0, 1, edgeNr);
	}

	template <typename ValType> DomType First(ValType v) const { dms_assert(m_First_rel); return m_First_rel[v]; }
	DomType FirstOrSame(DomType v) const { return LookupOrSame(m_First_rel.begin(), v); }
	DomType Next(DomType d) const { return m_Next_rel[d]; }
	DomType NextOrNone(DomType d) const { return m_Next_rel ? m_Next_rel[d] : UNDEFINED_VALUE(DomType); }

private:
	void InitData(DomType nrD, SizeT nrV)
	{
		m_First_rel = OwningPtrSizedArray<DomType>(nrV, Undefined() MG_DEBUG_ALLOCATOR_SRC("dijkstra: m_First_rel"));
		m_Next_rel = OwningPtrSizedArray<DomType>(nrD, Undefined() MG_DEBUG_ALLOCATOR_SRC("dijkstra: m_Next_rel"));
	}
	template <typename ValType> void Assign(ValType v, ValType nrV, DomType d)
	{
		if (v < nrV)
		{
			m_Next_rel[d] = m_First_rel[v];
			m_First_rel[v] = d;
		}
	}

	OwningPtrSizedArray<DomType> m_First_rel, m_Next_rel;
};

#endif //!defined(__CLC_INVERTREL_H)
