// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__TIC_USINGCACHE_H)
#define __TIC_USINGCACHE_H 1

#include "TreeItemSet.h"

struct UsingCache
{
	using usings_iterator = TreeItemCRefArray::iterator;
	using const_usings_iterator = TreeItemCRefArray::const_iterator;

	using item_array_iterator = TreeItemCPtrArray::iterator;
	using const_item_array_iterator = TreeItemCPtrArray::const_iterator;

	UsingCache(const TreeItem* context);
	~UsingCache();

	void AddUsingUrl (TokenID url);
	void AddUsingUrls(CharPtr urlBegin, CharPtr urlEnd);

	void AddUsing (const TreeItem* ns);
	void AddUsings(const TreeItem** firstNameSpace, const TreeItem** lastNameSpace);

	void ClearUsings(bool keepParent);

	UInt32 GetNrUsings() const;
	const TreeItem* GetUsing(UInt32 i) const;

	TIC_CALL auto FindItem(TokenID itemID) const -> SharedTreeItem;

	void OnItemAdded  (const TreeItem* child);
	void OnItemRemoved(const TreeItem* child) { SetDirty(); }

	const std::vector<TokenID>& UsingUrls() const { return m_UsingUrls; }

private:
	enum class CacheStateType {
		DIRTY,
		READY,
		BUSY,
		UNDEFINED
	};

	void SetDirty();

	bool AddUsingUrls(CharPtr urlsBegin, CharPtr urlsEnd, SizeT nrSkippedFromEnd);

	void CheckSearchSpace(const TreeItem* nameSpace) const;
	void UpdateUsings() const;
	void UpdateCache()  const;

	void AddIncoming(UsingCache* incoming) const;
	void DelIncoming(UsingCache* incoming) const;
	void AddParent();

	SharedTreeItem FindNamespace(TokenID url) const;
	bool AddUsingInternal(const TreeItem* ns) const;
	bool IsDirty() const { return m_CacheState == CacheStateType::DIRTY; }
	bool IsReady() const { return m_CacheState == CacheStateType::READY; }
	static void Update(const TreeItem* item);

	mutable CacheStateType               m_CacheState = CacheStateType::DIRTY;

#if defined(MG_DEBUG_DATA)
	mutable CacheStateType               md_PrevState = CacheStateType::UNDEFINED;
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
