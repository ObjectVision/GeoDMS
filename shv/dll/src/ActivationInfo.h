// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#ifndef __MG_SHV_ACTIVATIONINFO_H
#define __MG_SHV_ACTIVATIONINFO_H


//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "ptr/OwningPtr.h"
#include "ptr/SharedPtr.h"

#include "ShvUtils.h"

#include "MovableObject.h"
enum  ToolButtonID;

//----------------------------------------------------------------------
// struct ActivationInfo
//----------------------------------------------------------------------
typedef std::shared_ptr<MovableObject> sharedPtrMovableObject;

struct ActivationInfo : sharedPtrMovableObject
{
	ActivationInfo() {}
	ActivationInfo(MovableObject* obj)
		: sharedPtrMovableObject(obj->shared_from_this())
	{}

	sharedPtrMovableObject ActiveChild();
	bool OnKeyDown(UInt32 nVirtKey);
};


#endif // __MG_SHV_ACTIVATIONINFO_H

