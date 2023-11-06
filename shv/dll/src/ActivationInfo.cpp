//<HEADER> // Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "ActivationInfo.h"
#include "MovableObject.h"

//----------------------------------------------------------------------
// struct ActivationInfo
//----------------------------------------------------------------------

sharedPtrMovableObject ActivationInfo::ActiveChild()
{
	while ((*this) && !(*this)->AllVisible())
	{
		dms_assert((*this)->IsActive());
		(*this)->SetActive(false);
		sharedPtrMovableObject::operator=(
			(*this)->GetOwner().lock()
		);
	}
	return (*this);
}

bool ActivationInfo::OnKeyDown(UInt32 virtKey)
{
	sharedPtrMovableObject curr = ActiveChild();
	while (curr)
	{
		if (curr->OnKeyDown(virtKey))
			return true;
		curr = curr->GetOwner().lock();
	}
	return false;
}

