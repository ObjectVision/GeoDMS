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

#if !defined(__PTR_PERSISTENTSHAREDOBJ_H)
#define __PTR_PERSISTENTSHAREDOBJ_H

#include "ptr/SharedObj.h"

class PersistentSharedObj : public SharedObj
{
public:
	RTC_CALL virtual const PersistentSharedObj* GetParent() const;

	RTC_CALL SharedStr GetSourceName() const override;

	// NON VIRTUAL ROUTINES BASED ON THE ABOVE INTERFACE
	RTC_CALL SharedStr GetFullName() const;
	RTC_CALL SharedStr GetPrefixedFullName() const;

	RTC_CALL const PersistentSharedObj* GetRoot() const;
	RTC_CALL bool DoesContain(const PersistentSharedObj* subItemCandidate) const;

	RTC_CALL const SourceLocation* GetLocation() const override ;

	RTC_CALL SharedStr GetRelativeName(const PersistentSharedObj* context) const;
	RTC_CALL SharedStr GetFindableName(const PersistentSharedObj* subItem) const;

	DECL_ABSTR(RTC_CALL, Class);
};

#endif // __PTR_PERSISTENTSHAREDOBJ_H`
