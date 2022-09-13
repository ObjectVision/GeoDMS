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
#if !defined(__SUPPLCACHE_H)
#define __SUPPLCACHE_H

#include "act/Actor.h"
#include "ptr/InterestHolders.h"

#include "ptr/OwningPtrSizedArray.h"

//----------------------------------------------------------------------
// class  : ExplicitSuppliers
//----------------------------------------------------------------------

using ActorCRef = SharedPtr<const Actor> ;
using ActorCRefArray = std::unique_ptr<ActorCRef[]>;

 // CHECK AND OPTIMIZE ON INVARIANT: all configured suppliers are TreeItems; all implied suppliers are AbstrCalculators

struct SupplCache
{
	SupplCache();
	void SetSupplString(WeakStr val);

	const Actor* GetSupplier(UInt32 i) const
	{
		dms_assert(!m_IsDirty);
		dms_assert(i < m_NrConfigured);
		return m_SupplArray[i];
	}
	const ActorCRef* begin(const TreeItem* context) const { Update(context); return m_SupplArray.get(); }
	const ActorCRef* end  (const TreeItem* context) const { Update(context); return m_SupplArray.get() + m_NrConfigured; }
	UInt32              GetNrConfigured(const TreeItem* context) const { Update(context); return m_NrConfigured; }

	WeakStr GetSupplString() const { return m_strConfigured; }

	void Reset();

private:

	void BuildSet(const TreeItem* context) const;

	void Update(const TreeItem* context) const
	{
		if (m_IsDirty) 
			BuildSet(context);
	}

	mutable SharedStr           m_strConfigured;
	mutable ActorCRefArray      m_SupplArray;
	mutable UInt32              m_NrConfigured:31;
	mutable bool                m_IsDirty     : 1;
};

#endif //!defined(__SUPPLCACHE_H)
