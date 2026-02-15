// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


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
