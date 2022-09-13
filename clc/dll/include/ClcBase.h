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

#ifndef __CLC_CLCBASE_H
#define __CLC_CLCBASE_H

// *****************************************************************************
// Section:     DLL Build settings
// *****************************************************************************

#include "TicBase.h"

#if defined(DMCLC1_EXPORTS)
#	define CLC1_CALL __declspec(dllexport)
#else
#	define CLC1_CALL __declspec(dllimport)
#endif

#if defined(DMCLC2_EXPORTS)
#	define CLC2_CALL __declspec(dllexport)
#else
#	define CLC2_CALL __declspec(dllimport)
#endif

#include "OperGroups.h"

// *****************************************************************************
// oper groups that are used in multiple untis
// *****************************************************************************

// defined in UnitCreators.cpp
extern CLC1_CALL CommonOperGroup cog_mul;
extern CLC1_CALL CommonOperGroup cog_div;
extern CLC1_CALL CommonOperGroup cog_add;
extern CLC1_CALL CommonOperGroup cog_sub;
extern CLC1_CALL CommonOperGroup cog_bitand;
extern CLC1_CALL CommonOperGroup cog_bitor;
extern CLC1_CALL CommonOperGroup cog_pow;
extern CLC1_CALL CommonOperGroup cog_eq;
extern CLC1_CALL CommonOperGroup cog_ne;
extern CLC1_CALL CommonOperGroup cog_substr;

#endif