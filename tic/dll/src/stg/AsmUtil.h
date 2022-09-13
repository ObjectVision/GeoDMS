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
#ifndef __ASMUTIL_H
#define __ASMUTIL_H 1

/*
 *	ASM_STATE
 *
 *	Abstract storage (error) state definitions
 */
typedef enum
{
	ASM_OK,
	ASM_ERR,
	ASM_E_FILENOTFOUND,
	ASM_E_PATHNOTFOUND,
	ASM_E_FILENOTOPENED,
	ASM_E_FILEALREADYEXISTS,
	ASM_E_LOCKVIOLATION,
	ASM_E_SHAREVIOLATION,
	ASM_E_ACCESSDENIED,
	ASM_E_DATAREAD,
	ASM_E_MEDIUMFULL,
	ASM_E_WRITEFAULT,
	ASM_E_INSUFFICIENTMEMORY,
	ASM_E_INVALIDOBJECT,
	ASM_E_UNKNOWNSTORAGECLASS
} ASM_STATE;


[[noreturn]] TIC_CALL void throwStorageError(ASM_STATE state, const char* storageName);

#endif // __ASMUTIL_H