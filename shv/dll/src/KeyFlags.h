// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__SHV_KEYFLAGS_H)
#define __SHV_KEYFLAGS_H

//----------------------------------------------------------------------
// class  : KeyDown Flags
//----------------------------------------------------------------------

namespace KeyInfo {

	const UInt32 CharMask  = 0x07FFFFFF; // See Qt::Key_Left etc.
	const UInt32 FlagsMask = 0xF8000000;
	
	namespace Flag {
		const UInt32 Char = 0x80000000; // true for WM_(SYS)CHAR , false for WM_(SYS)KEY[DOWN|UP]
		const UInt32 Ctrl = 0x40000000; // Ctrl was pressed
		const UInt32 Menu = 0x20000000; // Alt  was pressed corresponds to context code in lParam ( bit 29 ), implies KF_SYSTEM
		const UInt32 Syst = 0x10000000; // true for WM_SYS*, can also be sent without KF_MENU when curr=Active and no control has key-focus
		const UInt32 Shift= 0x08000000; // true for WM_SYS*, can also be sent without KF_MENU when curr=Active and no control has key-focus

		const UInt32 CCM = Char|Ctrl|Menu|Shift;
		const UInt32 IsCharMask = Char | Ctrl | Menu;
	}

	inline UInt32 FlagsOf  (UInt32 virtKey) { return virtKey & FlagsMask; }
	inline UInt32 CharOf   (UInt32 virtKey) { return virtKey & CharMask; }
	inline UInt32 CcmOf    (UInt32 virtKey) { return virtKey & Flag::CCM; }

	inline bool IsChar     (UInt32 virtKey) { return  (virtKey & Flag::IsCharMask) == Flag::Char; } // nonsystem normal char or shifted char
	inline bool IsSpec     (UInt32 virtKey) { return  CcmOf(virtKey) == 0; }
	inline bool IsCtrl     (UInt32 virtKey) { return  CcmOf(virtKey) == Flag::Ctrl; }
	inline bool IsShiftCtrl(UInt32 virtKey) { return  CcmOf(virtKey) == (Flag::Ctrl|Flag::Shift); }
	inline bool IsAlt      (UInt32 virtKey) { return  CcmOf(virtKey) == Flag::Menu; }
	inline bool IsCtrlAlt  (UInt32 virtKey) { return  CcmOf(virtKey) == (Flag::Ctrl|Flag::Menu); }
}

#endif // !defined(__SHV_KEYFLAGS_H)
