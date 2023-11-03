// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __SHV_ABSTRBOUNDINGBOXCACHE_H
#define __SHV_ABSTRBOUNDINGBOXCACHE_H

#include "RtcBase.h"
#include "ptr/SharedObj.h"
class AbstrDataObject;

//----------------------------------------------------------------------
// struct  : AbstrBoundingBoxCache
//----------------------------------------------------------------------

struct AbstrBoundingBoxCache : SharedObj
{
protected:
	AbstrBoundingBoxCache(const AbstrDataObject* featureData);
	virtual ~AbstrBoundingBoxCache();

public:
	void Register() { m_HasBeenRegistered = true; }

	virtual DRect GetTileBounds(tile_id t) const = 0;
	virtual DRect GetBlockBounds(tile_id t, tile_offset blockNr) const = 0;
	virtual DRect GetBounds(tile_id t, tile_offset featureID) const;

	DRect GetBounds(SizeT featureID) const;

	static const UInt32 c_BlockSize = 256;

protected:
	const AbstrDataObject* m_FeatureData;
	bool m_HasBeenRegistered = false;
};

#endif // __SHV_ABSTRBOUNDINGBOXCACHE_H
