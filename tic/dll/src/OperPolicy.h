// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

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
	dont_cache_result    = 1, // dont create the result in the data-cache but instantiate in-place (templates, for each, loop, select_many, select_afew).
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
	can_be_rewritten       = 0x8000, // operator-name appears as pattern-head in rewrite list, therefore: try rewriting, NYI, WIP.
	has_annotation        = 0x10000, // operator has an annotation
	better_not_in_meta_scripting = 0x20000, // operator is not suitable for processing meta-scripting
};

inline bool  operator & (oper_policy a, oper_policy b) { return int(a) & int(b); }
inline oper_policy operator | (oper_policy a, oper_policy b) { return oper_policy(int(a) | int(b)); }
inline void operator |= (oper_policy& a, oper_policy b) { *(int*)(&a) |= int(b); }

#endif // __TIC_OPERPOLICY_H
