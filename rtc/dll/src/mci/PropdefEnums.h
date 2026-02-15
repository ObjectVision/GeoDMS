// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__RTC_MCI_PROPDEFENUMS_H)
#define __RTC_MCI_PROPDEFENUMS_H

//----------------------------------------------------------------------
// enumeration types for PropDefs
//----------------------------------------------------------------------

enum class xml_mode { none, name,         signature, attribute, element }; // requires 3 bits
enum class set_mode { none, construction, obligated, optional };           // requires 2 bits
enum class cpy_mode { none, expr_noroot,  expr,      all      };           // requires 2 bits
enum class chg_mode { none, eval,         invalidate };                    // requires 2 bits

#endif // __RTC_MCI_PROPDEFENUMS_H
