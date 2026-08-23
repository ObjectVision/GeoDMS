// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  The function-item specification API: free functions that record and
 *  query the declared parameters, result, generic type variables, variant
 *  sets and strict scoping of user-defined function items (the 'function'
 *  keyword of the typed-HOF language). Used by the parser (stx ConfigProd)
 *  and the calculator (AbstrCalculator); split out of TreeItem.h so that
 *  HOF-spec churn no longer invalidates the Tic/Stg/Clc/Geo PCHs
 *  (header-hygiene-2026-08.md §5C).
 */

#if !defined(__TIC_TREEITEMFUNCTIONSPEC_H)
#define __TIC_TREEITEMFUNCTIONSPEC_H

#include <utility>
#include <vector>

#include "TicBase.h"

// user-defined function items (declared with the 'function' keyword): declared parameter
// count (= the number of leading sub-items that a call binds), the designated result
// sub-item, and optional per-parameter function-signature exemplars.
TIC_CALL void    TreeItem_SetFunctionSpec(const TreeItem* functionItem, UInt32 nrParams, TokenID resultName);
TIC_CALL UInt32  TreeItem_GetFunctionParamCount(const TreeItem* functionItem);
TokenID TreeItem_GetFunctionResultName(const TreeItem* functionItem);
TIC_CALL void    TreeItem_AddFunctionParamSignature(const TreeItem* functionItem, UInt32 paramIndex, const TreeItem* signatureExemplar, std::vector<TokenID> typeArgs = {});
// meta-reference parameters ('item x'): the argument binds as a raw item reference
// (sourceDescr key), like PropValue's item argument in a direct call -- never as the
// argument's calculation/range key
TIC_CALL void    TreeItem_AddFunctionMetaRefParam(const TreeItem* functionItem, UInt32 paramIndex);
bool    TreeItem_IsFunctionMetaRefParam(const TreeItem* functionItem, UInt32 paramIndex);
// '...x' rest parameter (always the LAST param): binds ONE OR MORE trailing arguments;
// in the body it may only be passed on as the trailing argument of a function call
TIC_CALL void    TreeItem_SetFunctionRestParam(const TreeItem* functionItem);
TIC_CALL bool    TreeItem_HasFunctionRestParam(const TreeItem* functionItem);
auto    TreeItem_GetFunctionParamSignature(const TreeItem* functionItem, UInt32 paramIndex) -> SharedTreeItem;
const std::vector<TokenID>* TreeItem_GetFunctionParamSigTypeArgs(const TreeItem* functionItem, UInt32 paramIndex); // WP4.1: 'sig<V, D>' arguments
// K11a by-example: 'p: exemplar' with a UNIT exemplar -- its declared sub-items are the parameter's member block for the definition-time checker
TIC_CALL void    TreeItem_AddFunctionParamTypeExemplar(const TreeItem* functionItem, UInt32 paramIndex, const TreeItem* exemplar);
auto    TreeItem_GetFunctionParamTypeExemplar(const TreeItem* functionItem, UInt32 paramIndex) -> SharedTreeItem;
TIC_CALL void    TreeItem_SetFunctionTypeVars(const TreeItem* functionItem, std::vector<std::pair<TokenID, TokenID>> typeVars); // WP4.1: ordered <var: constraint> list
const std::vector<std::pair<TokenID, TokenID>>* TreeItem_GetFunctionTypeVars(const TreeItem* functionItem);
TIC_CALL void    TreeItem_SetFunctionSignatureOnly(const TreeItem* functionItem); // 'alias = function<...>(...) -> ...;' -- declared type, no body
bool    TreeItem_IsFunctionSignatureOnly(const TreeItem* functionItem);
TIC_CALL void    TreeItem_SetFunctionResultSig(const TreeItem* functionItem, bool resultIsFunction, const TreeItem* resultSigExemplar, std::vector<TokenID> typeArgs = {}); // §5.10: function-valued result
bool    TreeItem_IsFunctionResultFunction(const TreeItem* functionItem);
TIC_CALL void    TreeItem_SetFunctionResultGenericUnit(const TreeItem* functionItem); // '-> unit<V>': concrete UnitClass follows from application
bool    TreeItem_IsFunctionResultGenericUnit(const TreeItem* functionItem);
auto    TreeItem_GetFunctionResultSig(const TreeItem* functionItem) -> SharedTreeItem;
const std::vector<TokenID>* TreeItem_GetFunctionResultSigTypeArgs(const TreeItem* functionItem);
void    TreeItem_CopyFunctionSpec(const TreeItem* dstFunctionItem, const TreeItem* srcFunctionItem);

// generic type variables on function parameters: function f<V: numerics>(... attribute<V> x ...)
class ValueClass;
TIC_CALL void    TreeItem_AddFunctionGenericParam(const TreeItem* functionItem, UInt32 paramIndex, TokenID varName, TokenID constraintName, bool isDomainVar = false);
bool    TreeItem_GetFunctionGenericParam(const TreeItem* functionItem, UInt32 seqNr, UInt32* paramIndex, TokenID* varName, TokenID* constraintName, bool* isDomainVar = nullptr);
bool    TreeItem_IsFunctionDefinitionChecked(const TreeItem* functionItem);
void    TreeItem_SetFunctionDefinitionChecked(const TreeItem* functionItem);
TIC_CALL void    TreeItem_SetFunctionVariantSet(const TreeItem* functionItem);
TIC_CALL bool    TreeItem_IsFunctionVariantSet(const TreeItem* functionItem);
TIC_CALL bool    IsKnownGenericConstraint(TokenID constraintName);
bool    MatchesGenericConstraint(const ValueClass* vc, TokenID constraintName);

// §5.7 v2: variant match sets over the closed value-class universe
bool    TreeItem_VariantMatches(const TreeItem* variant, const std::vector<const ValueClass*>& argVCs);
int     TreeItem_CompareVariantSpecificity(const TreeItem* a, const TreeItem* b); // -1/+1: strictly more specific side; 0: identical; 2: incomparable
TIC_CALL void    TreeItem_CheckVariantSetDisjointness(const TreeItem* setItem); // throws on identical/incomparable overlapping coverage

// enforce strict scoping on a function definition: name resolution from within stops at
// the item (own sub-items + explicit 'using' imports); relative import urls still
// resolve against the definition scope.
TIC_CALL void    TreeItem_MakeStrictScope(TreeItem* functionItem);

#endif // __TIC_TREEITEMFUNCTIONSPEC_H
