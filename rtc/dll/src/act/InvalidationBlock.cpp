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
#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "act/InvalidationBlock.h"
#include "act/Actor.h"

//----------------------------------------------------------------------
// class  : InvalidationBlock
//----------------------------------------------------------------------

InvalidationBlock::InvalidationBlock(const Actor* self)
	:	m_ChangingObj(self)
	,	m_OldBlockState(self->m_State.HasInvalidationBlock())
{
	dms_assert(self);
	self->DetermineState();  // let other changes of suppliers such as ROI invalidate the ViewPort if changes were made prior to the locked change
	self->m_State.SetInvalidationBlock(); // block invalidation for the current change
}

void InvalidationBlock::ProcessChange() // request to Invalidate Local Changes 
{
	dms_assert(m_ChangingObj);
	m_ChangingObj->GetLastChangeTS(); // process the changes while actor_flag_set::AF_BlockChange is set
}

InvalidationBlock::~InvalidationBlock()
{
	dms_assert(m_ChangingObj);
	dms_assert(m_ChangingObj->m_State.HasInvalidationBlock());
	if (!m_OldBlockState)
		m_ChangingObj->m_State.ClearInvalidationBlock();// go to original state
}

