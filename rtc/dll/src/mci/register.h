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
#ifndef __MCI_REGISTER_H
#define __MCI_REGISTER_H

#include <boost/noncopyable.hpp>

#include "geo/SeqVector.h"
#include "set/vectorfunc.h"
#include "set/Token.h"

template <typename ItemType>
struct CompareLtItemIdPtrs
{
	bool operator ()(const ItemType* a, const ItemType* b)
	{
		dms_assert(a && b);
		return a->GetID() < b->GetID();
	}
	bool operator ()(const ItemType* a, TokenID bID)
	{
		dms_assert(a);
		return a->GetID() < bID;
	}
	bool operator ()(TokenID aID, const ItemType* b)
	{
		dms_assert(b);
		return aID < b->GetID();
	}
};

template <typename ItemType>
struct CompareEqItemIdPtrs
{
	bool operator ()(const ItemType* a, const ItemType* b)
	{
		dms_assert(a && b);
		return a->GetID() == b->GetID();
	}
	bool operator ()(const ItemType* a, TokenID bID)
	{
		dms_assert(a);
		return a->GetID() == bID;
	}
	bool operator ()(TokenID aID, const ItemType* b)
	{
		dms_assert(b);
		return aID == b->GetID();
	}
};

struct LtCharPtrCaseSensitive
{
	bool operator ()(CharPtr a, CharPtr b) const
	{
		dms_assert(a);
		dms_assert(b);
		return strcmp(a, b) < 0;
	}

};

struct LtCharPtrCaseIgnorant
{
	bool operator ()(CharPtr a, CharPtr b) const
	{
		dms_assert(a);
		dms_assert(b);
		return stricmp(a, b) < 0;
	}

};

struct EqCharPtrCaseSensitive
{
	bool operator ()(CharPtr a, CharPtr b) const
	{
		dms_assert(a);
		dms_assert(b);
		return strcmp(a, b) == 0;
	}

};

struct EqCharPtrCaseIgnorant
{
	bool operator ()(CharPtr a, CharPtr b) const
	{
		dms_assert(a);
		dms_assert(b);
		return stricmp(a, b) == 0;
	}

};

template <typename ItemType, typename CaseTraits = LtCharPtrCaseIgnorant>
struct CompareLtItemNamePtrs
{
	bool operator ()(const ItemType* a, const ItemType* b)
	{
		dms_assert(a && b);
		return ct(a->GetName(), b->GetName());
	}
	bool operator ()(const ItemType* a, CharPtr bName)
	{
		dms_assert(a);
		return ct(a->GetName(), bName);
	}
	bool operator ()(CharPtr aName, const ItemType* b)
	{
		dms_assert(b);
		return ct(aName, b->GetName());
	}
private:
	CaseTraits ct;
};

template <typename ItemType, typename CaseTraits = EqCharPtrCaseIgnorant>
struct CompareEqItemNamePtrs
{
	bool operator ()(const ItemType* a, const ItemType* b)
	{
		dms_assert(a && b);
		return ct(a->GetName(), b->GetName());
	}
	bool operator ()(const ItemType* a, CharPtr bName)
	{
		dms_assert(a);
		return ct(a->GetName(), bName);
	}
	bool operator ()(CharPtr aName, const ItemType* b)
	{
		dms_assert(b);
		return ct(aName, b->GetName());
	}
private:
	CaseTraits ct;
};

template <typename T, typename Key = CharPtr, typename CompareT = CompareLtItemNamePtrs<T> >
struct StaticRegister : private boost::noncopyable
{
	typedef std::vector<T*> ContainerType;
	typedef typename ContainerType::const_iterator const_iterator;

	void OrderRegister() // static
	{
		if (s_RegisterDirty)
		{
			if (s_Register)
				std::sort(s_Register->begin(), s_Register->end(), s_Cmp);
			s_RegisterDirty = false;
		}
	}

	void Register(T* cls) // static
	{
		if (!s_Register)
		{
			s_Register = new std::vector<T*>;
			s_Register->reserve(256);
		}
		s_Register->push_back(cls);
		s_RegisterDirty = true;
	}

	void Unregister(T* cls) // static
	{
		dms_assert(s_Register);
		vector_erase(*s_Register, cls);
		s_RegisterDirty = true;
		if (s_Register->size() == 0)
		{
			delete s_Register;
			s_Register = 0;
		}
	}
	T* Find(Key key) // static
	{
		OrderRegister();
		
		const_iterator
			b = s_Register->begin(),
			e = s_Register->end();
		b = std::lower_bound(b,e,key, s_Cmp);
		if (b != e && !s_Cmp(key, *b))
			return *b;
		return nullptr; // NYI
	}
	const_iterator Begin() // static
	{
		dms_assert(!Empty());
		OrderRegister();
		return s_Register ? s_Register->begin() : const_iterator();
	}

	const_iterator End() // static
	{
		dms_assert(!Empty());
		OrderRegister();
		return s_Register ? s_Register->end() : const_iterator();
	}
	bool Empty() const
	{
		return !s_Register || s_Register->empty();
	}

	UInt32 Size() const
	{
		return Empty() ? 0 : s_Register->size();
	}

public:
	ContainerType* s_Register;
private:
	bool     s_RegisterDirty;
	CompareT s_Cmp;
}; // struct StaticRegister


#endif // __MCI_REGISTER_H
