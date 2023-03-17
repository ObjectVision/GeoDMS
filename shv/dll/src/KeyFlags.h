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
// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(__SHV_KEYFLAGS_H)
#define __SHV_KEYFLAGS_H

//----------------------------------------------------------------------
// class  : KeyDown Flags
//----------------------------------------------------------------------

namespace KeyInfo {

	const UInt32 CharMask  = 0x0000FFFF;
	const UInt32 FlagsMask = 0xFFFF0000;
	
	namespace Flag {
		const UInt32 Char = 0x80000000; // true for WM_(SYS)CHAR , false for WM_(SYS)KEY[DOWN|UP]
		const UInt32 Ctrl = 0x40000000; // Ctrl was pressed
		const UInt32 Menu = 0x20000000; // Alt  was pressed corresponds to context code in lParam ( bit 29 ), implies KF_SYSTEM
		const UInt32 Syst = 0x10000000; // true for WM_SYS*, can also be sent without KF_MENU when curr=Active and no control has key-focus

		const UInt32 CCM = Char|Ctrl|Menu;
	}

	inline UInt32 FlagsOf(UInt32 virtKey) { return virtKey & FlagsMask; }
	inline char   CharOf (UInt32 virtKey) { return virtKey & CharMask; }
	inline UInt32 CcmOf  (UInt32 virtKey) { return virtKey & Flag::CCM; }

	inline bool IsChar   (UInt32 virtKey) { return  FlagsOf(virtKey) == Flag::Char; } // nonsystem normal char
	inline bool IsSpec   (UInt32 virtKey) { return  CcmOf(virtKey) == 0; }
	inline bool IsCtrl   (UInt32 virtKey) { return  CcmOf(virtKey) == Flag::Ctrl; }
	inline bool IsAlt    (UInt32 virtKey) { return  CcmOf(virtKey) == Flag::Menu; }
	inline bool IsCtrlAlt(UInt32 virtKey) { return  CcmOf(virtKey) == (Flag::Ctrl|Flag::Menu); }
}

#endif // !defined(__SHV_KEYFLAGS_H)
