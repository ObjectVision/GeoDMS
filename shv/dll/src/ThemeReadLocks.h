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

#ifndef __SHV_THEMEREADLOCKS_H
#define __SHV_THEMEREADLOCKS_H

#include "DataLockContainers.h"
class Theme;
struct ThemeSet;
class GraphicObject;

//----------------------------------------------------------------------
// class  : ThemeReadLocks
//----------------------------------------------------------------------

struct ThemeReadLocks : DataReadLockContainer
{
	ThemeReadLocks()	: m_FailedTheme(0) {}
	~ThemeReadLocks();

	using DataReadLockContainer::push_back;

	bool push_back(const ThemeSet* ts, DrlType drlType);
 	bool push_back(const Theme* theme, DrlType type);

	bool ProcessFailOrSuspend(const GraphicObject* go) const;

private:
	bool push_back(const Theme* theme, const AbstrDataItem* adi, DrlType type);
	const Theme* m_FailedTheme;

	std::set<const Theme*> m_Themes;
};


#endif // __SHV_THEMEREADLOCKS_H

