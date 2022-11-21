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

#if !defined(__TIC_USINGCACHE_H)
#define __TIC_USINGCACHE_H 1

#include "TreeItemSet.h"

struct UsingCache
{
	typedef TreeItemCRefArray::iterator       usings_iterator;
	typedef TreeItemCRefArray::const_iterator const_usings_iterator;

	typedef TreeItemCPtrArray::iterator       item_array_iterator;
	typedef TreeItemCPtrArray::const_iterator const_item_array_iterator;

	UsingCache(const TreeItem* context);
	~UsingCache();

	void AddUsingUrl (TokenID url);
	void AddUsingUrls(CharPtr urlBegin, CharPtr urlEnd);

	void AddUsing (const TreeItem* ns);
	void AddUsings(const TreeItem** firstNameSpace, const TreeItem** lastNameSpace);

	void ClearUsings(bool keepParent);

	UInt32 GetNrUsings() const;
	const TreeItem* GetUsing(UInt32 i) const;

	TIC_CALL const TreeItem* FindItem(TokenID itemID) const;

	void OnItemAdded  (const TreeItem* child);
	void OnItemRemoved(const TreeItem* child) { SetDirty(); }

	const std::vector<TokenID>& UsingUrls() const { return m_UsingUrls; }

private:
	void SetDirty();

	bool AddUsingUrls(CharPtr urlsBegin, CharPtr urlsEnd, SizeT nrSkippedFromEnd);

	void CheckSearchSpace(const TreeItem* nameSpace) const;
	void UpdateUsings() const;
	void UpdateCache()  const;

	void AddIncoming(UsingCache* incoming) const;
	void DelIncoming(UsingCache* incoming) const;
	void AddParent();

	const TreeItem* FindNamespace(TokenID url) const;
	bool AddUsingInternal(const TreeItem* ns) const;
	bool IsDirty() const { return m_CacheState == CS_DIRTY; }
	bool IsReady() const { return m_CacheState == CS_READY; }
	static void Update(const TreeItem* item);

	mutable enum CacheStateType { 
			CS_DIRTY,
			CS_READY,
			CS_BUSY
	}                                    m_CacheState;

#if defined(MG_DEBUG_DATA)
	mutable enum CacheStateType          md_PrevState;
#endif

	const TreeItem*                      m_Context;

	mutable std::vector<TokenID>         m_UsingUrls;
	mutable TreeItemCRefArray            m_Usings;
	mutable TreeItemCPtrArray            m_SortedItemCache;
	mutable std::vector<UsingCache*>     m_Incoming;

#if defined(MG_DEBUG_DATA)
	SizeT md_SeqNr;
#endif

	friend void MergeCacheIntoArray(const UsingCache* uc, TreeItemCPtrArray& tmp1);
};


#endif //__TIC_USINGCACHE_H
