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
#if !defined(__TIC_OPERPOLICY_H)
#define __TIC_OPERPOLICY_H

//----------------------------------------------------------------------
// oper_policy
//----------------------------------------------------------------------

// TODO G8: Reconsider: 
// dont_cache_result -> CompoundDC; 
// usage of calc_requires_metainfo

enum class oper_policy
{
	none                 = 0,
	dont_cache_result    = 1, // dont create the result in the data-cache but instantiate in-place (templates, for each, loop).
	                          // Requires application as MetaFunc.
	existing             = 2, // can result to an existing item, thus result is a supplier (SubItem, DomainUnit, ValuesUnit, GetProjectionBase, Generate).
	has_external_effects = 4,

	allow_extra_args     = 8,
	is_template_call     = 0x0010,
	has_template_arg     = 0x0020, // there is one argument which is required to be a template
	has_tree_args        = 0x0040, // some args are taken as tree and more than just their related data is used
	is_transient         = 0x0080, // result may be different between different sessions (due to change of cache location or other config setting changes or version number)
	                                  // therefore, results and their dependents cannot be restored from the disc-cache (Create as tmp)
    dynamic_argument_policies = 0x0100, // 1st arg is a string that influences the policies of the other args.
	dynamic_result_class   = 0x0200, // MakeResult must be called to find the result class as required for finding a signature
	calc_requires_metainfo = 0x0400, // after result creation, data creation still requires meta-info and therefore data generation has to be done in the main thread.
	is_dynamic_group       = 0x0800,

	can_explain_value      = 0x1000, // Calc can be called to epxlain value
	depreciated            = 0x2000, // warn when used; mention preferred alternative
	obsolete               = 0x4000, // error when used; instruct preferred alternative
	always_preprocess      = 0x8000, // Don't expect nor allow actual registered operators as this operator must be preprocesed, such as select_many_x, select_afew_x, arrow

	can_be_rewritten       = 0x00010000, // operator-name appears as pattern-head in rewrite list, therefore: try rewriting, NYI, WIP.
};

inline bool  operator & (oper_policy a, oper_policy b) { return int(a) & int(b); }
inline oper_policy operator | (oper_policy a, oper_policy b) { return oper_policy(int(a) | int(b)); }

#endif // __TIC_OPERPOLICY_H
