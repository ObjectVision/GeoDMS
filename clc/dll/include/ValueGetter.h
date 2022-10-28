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

#if !defined(DMS_CLC_VALUEGETTER_H)
#define DMS_CLC_VALUEGETTER_H

#include "UnitProcessor.h"

template <typename T>
struct AbstrValueGetter
{
	virtual ~AbstrValueGetter()  {}
	virtual T Get(SizeT t) const = 0;
	virtual bool AllDefined() const { return false; }
};

//=================================== ValueGetter

template <typename T, typename V>
struct ValueGetter : AbstrValueGetter<T>
{
	ValueGetter(const DataArray<V>* ado, tile_id t)
		:	m_Data(ado->GetDataRead(t) )
	{}

	T Get(SizeT t) const override
	{
		dms_assert(t < m_Data.size());
		return Convert<T>(m_Data[t]);
	}

	typename DataArray<V>::locked_cseq_t m_Data;
};

template <typename T>
struct ValueGetterCreatorBase : UnitProcessor
{
	template <typename E>
	void VisitImpl(const Unit<E>* inviter) const
	{
		m_Result = new ValueGetter<T, E>(const_array_cast<E>(m_Adi), m_TileID);
	}
	WeakPtr<const AbstrDataItem>         m_Adi;
	tile_id                              m_TileID = no_tile;
	mutable WeakPtr<AbstrValueGetter<T>> m_Result;
};

template <typename T, typename TL>
struct ValueGetterCreator :  boost::mpl::fold<TL, ValueGetterCreatorBase<T>, VisitorImpl<Unit<_2>, _1> >::type
{
	ValueGetterCreator(const AbstrDataItem* adi, tile_id t)
	{
		this->m_Adi    = adi;
		this->m_TileID = t;
	}
	AbstrValueGetter<T>* Create()
	{
		this->m_Adi->GetAbstrValuesUnit()->InviteUnitProcessor(*this);
		return this->m_Result.get_ptr();
	}
	static AbstrValueGetter<T>* Create(const AbstrDataItem* adi, tile_id t = no_tile)
	{
		return ValueGetterCreator(adi, t).Create();
	}
};


typedef AbstrValueGetter<Float64>                           WeightGetter;
typedef ValueGetterCreator<Float64, typelists::num_objects> WeightGetterCreator;

#endif //!defined(DMS_CLC_VALUEGETTER_H)
