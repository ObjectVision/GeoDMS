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
#include "TicPCH.h"
#pragma hdrstop

#include "utl/MySPrintF.h"
#include "xct/DmsException.h"
#include "stg/StorageInterface.h"

#include "stg/AsmUtil.h"

/*
 * asm_state_string
 *
 * global method for determining a string associated with the ASM_STATE
 *
 * Parameters:
 * state(I): the state to return a string for
 *
 * Returns:
 * (const char*): the string for the state
 */

SharedStr asm_state_string(ASM_STATE state, CharPtr storageName)
{
	CharPtr msg = "unknown error";
	switch(state)
	{
	case ASM_E_FILENOTFOUND:
		msg = "file not found";
		break;
	case ASM_E_PATHNOTFOUND:
		msg = "path not found";
		break;
	case ASM_E_FILENOTOPENED:
		msg = "file not opened";
		break;
	case ASM_E_FILEALREADYEXISTS:
		msg = "Storage Manager: file already exists";
		break;
	case ASM_E_LOCKVIOLATION:
		msg = "lock violation";
		break;
	case ASM_E_SHAREVIOLATION:
		msg = "share violation";
		break;
	case ASM_E_ACCESSDENIED:
		msg = "access denied";
		break;
	case ASM_E_DATAREAD:
		msg = "data read error";
		break;
	case ASM_E_MEDIUMFULL:
		msg = "medium full";
		break;
	case ASM_E_WRITEFAULT:
		msg = "write fault";
		break;
	case ASM_E_INSUFFICIENTMEMORY:
		msg = "insufficient memory";
		break;
	case ASM_E_INVALIDOBJECT:
		msg = "invalid object";
		break;
	case ASM_E_UNKNOWNSTORAGECLASS:
		msg = "unknown storage type";
		break;
	}
	return mySSPrintF("Storage Exception %s: %s", storageName, msg);
}

/*
 * throwStorageError
 *
 * global method which constructs an ASMException with the given ASM_STATE
 *
 * Parameters:
 * state(I): the state to construct an exception for
 *
 * throw:
 * ASMException
 */

[[noreturn]] TIC_CALL void throwStorageError(ASM_STATE state, CharPtr storageName)
{
	throwErrorD("STG", asm_state_string(state, storageName).c_str() );
}

