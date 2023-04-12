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
#if !defined(__TIC_OPERARGPOLICY_H)
#define __TIC_OPERARGPOLICY_H

#include "OperPolicy.h"

//----------------------------------------------------------------------
// oper_arg_policy
//----------------------------------------------------------------------

enum class oper_arg_policy {
	calc_never = 0,
	calc_as_result = 1, // only calc arg when result needs to be calculated
	calc_subitem_root = 2,
	calc_always = 3,
	is_templ = 4, // arg is used as template in a for_each situation
	subst_with_subitems = 5,
	calc_at_subitem = 6
};

enum class metainfo_policy_flags {
	subst_allowed  = 0,
	subst_never    = 4,
	suppl_tree     = 8,    // takes whole subtree of arg as suppl (f.e. in template instantiation)
//	templ_suppl_tree = suppl_tree,

//	subst_with_subitems = subst_never | suppl_tree,

	recursive_check= 0x40, // called from integrity check constructor
	subst_may_fail = 0x80,
	is_root_expr   = 0x100, // only set at AbstrCalculator's first call to SubstExpr to allow for template instantiations and non-dc inplace operations (fka CompoundDCs)

	get_as_lispref = subst_may_fail,
	is_metafunc_call = is_root_expr,
};

inline bool operator &(metainfo_policy_flags lhs, metainfo_policy_flags rhs) { return reinterpret_cast<int&>(lhs) & reinterpret_cast<int&>(rhs);  }
inline metainfo_policy_flags operator |(metainfo_policy_flags lhs, metainfo_policy_flags rhs) { return metainfo_policy_flags(reinterpret_cast<int&>(lhs) | reinterpret_cast<int&>(rhs)); }

metainfo_policy_flags arg2metainfo_polcy(oper_arg_policy oap);

#endif // __TIC_OPERARGPOLICY_H
