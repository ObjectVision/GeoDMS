// Copyright (C) 1998-2026 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined( __STX_PRODCONFIG_H)
#define __STX_PRODCONFIG_H

#include "TicBase.h"

#include "ptr/SharedPtr.h"
#include "ptr/WeakPtr.h"
#include "sym/Token.h"

#include "DataBlockProd.h"
#include "ExprProd.h"

#include <tuple>
#include <vector>


enum class SignatureType {
	Undefined,
	TreeItem,
	Template,
	Function,
	Unit,
	Attribute,
	Parameter,
	MetaRef    // 'item x' function parameter: the argument binds as a raw item reference
};


struct ConfigProd : AbstrDataBlockProd, AbstrContextHandle, FunctionLiteralSink
{
	EmptyExprProd m_ExprProd = {};

	ConfigProd(TreeItem* context, bool rootIsFirstItem);
	~ConfigProd();

//	§5.11 tier B: lambda lifting -- function literals in parenthesized expression positions
	void OnExprFunctionLiteral(CharPtr first, CharPtr last) override;
	void OnWidenFunctionLiteral(CharPtr first, CharPtr last) override;

//	impl AbstrContextHandle
	bool HasItemContext() const override { return m_pCurrent != nullptr;  }
	auto ItemAsStr() const->SharedStr override 
	{ 
		assert(HasItemContext());  return m_pCurrent->GetSourceName(); 
	}
	bool Describe(FormattedOutStream& fos) override
	{
		auto item_name = HasItemContext() ? ItemAsStr() : SharedStr("");
		fos << "while parsing item " << item_name
			<< "\nin config file " << m_CurrFileName;
		return true;
	}

	TreeItem*  ParseFile  (CharPtr fileName);
	TreeItem*  ParseString(CharPtr configString);

	void DoInclude();

	void DoBeginBlock();
	void DoEndBlock();
	void OnItemDecl();

	void DoItemHeading(iterator_t first, iterator_t last);

	void SetSignature(SignatureType type);
	void DoEntitySignature();
	void DoAttrSignature();

	void DoBasicType();
	void DoUnitIdentifier();

	void DoEntityParam();
	void SetVC (ValueComposition    vc);

	void DoAnyProp();
	void DoExprProp(iterator_t first, iterator_t last);
	void DoStorageProp();
	void DoFileType();
	void DoUsingProp();
	void DoUnitRangeProp(bool isCategorical);
	void DoNrOfRowsProp();

//	name:type declaration style (single or comma-separated multi-name)
	void StartPendingNames();
	void AddPendingName();
	void DoRefTypeSignature();
	void DoColonItemHeading(iterator_t first, iterator_t last);

//	type aliases: alias = type;
	void OnAliasName();
	void DoAliasDecl(iterator_t first, iterator_t last);
	void CloneAliasRefinement(); // copy an exemplar's IntegrityCheck onto m_pCurrent (with name substitution)

//	function declarations (and function-signature type aliases)
	void OnTypeVarName();
	void OnTypeVarConstraint();
	void SetPendingNameLoc(iterator_t loc) { m_PendingNameLoc = loc; }
	void OnVariantSetHeading();
	void OnVariantSetEnd();
	void OnFunctionHeading(iterator_t first);
	void OnFunctionSigHeading(iterator_t first);
	void OnEndFunctionParams();
	void OnFunctionParamDecl();
	void OnRestParamDecl(iterator_t first); // '...x': variadic rest parameter (must be last)
	void ValidateFunctionArityVsOperator(const TreeItem* func); // arity-aware operator-name coexistence
	void DoFunctionUsing();
	void DoFunctionResultName();
	void OnFunctionResultSig();
	void OnFunctionResultIsFunction(); // §5.10 '-> function'
	void OnAnonResultFunction(); // §5.11: ':= function(params) ...' -- anonymous result-position function
	void OnBareExprHeading(iterator_t first); // §5.12: 'name := expr;' -- auto-typed plain item
	void OnAnonSigParam(iterator_t first); // §5.10 Stage 2: anonymous signature-alias parameter
	void OnParamSigTypeArg(); // §5.10 Stage 2: type application 'sig<V, D>' argument
	void OnFunctionResultExpr(iterator_t first, iterator_t last);
	void OnFunctionDeclEnd(iterator_t first);

	void DoArrayAssignment   () override;
	void DataBlockCompleted(iterator_t first, iterator_t last);

	void DoItemName();

	void ProdIdentifier(iterator_t first, iterator_t last);
	void ProdQuotedIdentifier();

	TreeItem* GetContextItem() const;
	TreeItem* GetContextOrRootItem(TokenID& nameID) const;
	bool      CurrentIsRoot()  const { return m_stackContexts.empty(); }

	MG_DEBUGCODE(
	bool      CurrentIsTop ()  const { return CurrentIsRoot() || m_stackContexts.size() == 1 && md_ContextWasGiven; }
	)

private:
	ConfigProd(const ConfigProd& ); // hide

	void CreateItem(TokenID nameID, const iterator_t& loc);
	void CreateDataItem (TokenID nameID, TokenID domainUnit, TokenID valuesUnit);
	void CreateContainer(TokenID nameID);
	void CreateTemplate (TokenID nameID);
	void CreateFunction (TokenID nameID);
	void CreateUnit     (TokenID nameID);
	void CreateAttribute(TokenID nameID);
	void CreateParameter(TokenID nameID);

void    ClearSignature();
void                ClearPropData();
	TokenID             RetrieveEntity();

	[[noreturn]] virtual void throwSemanticError(CharPtr msg) override;

	using TreeItemRef = SharedMutableTreeItem; // std::shared_ptr cursor; owns a parentless config root (sole owner) during parse

	MG_DEBUGCODE(bool                md_ContextWasGiven; )

	TreeItemRef                      m_pCurrent;
	quick_replace_stack<TreeItemRef> m_stackContexts;
	bool                             m_MergeIntoExisting = false;

	TokenID                          m_ItemNameID;
	TokenID                          m_strIdentifierID;

	// Signature support
	SignatureType                    m_eSignatureType = SignatureType::Undefined;
	WeakPtr<const ValueClass>        m_eValueClass;        // signature of unit definition
	TokenID                          m_pSignatureUnit;

	// param support
	TokenID                          m_pParamEntity;
	ValueComposition                 m_eParamVC = ValueComposition::Unknown;

	// name:type declaration support
	std::vector<TokenID>             m_PendingNames;
	std::vector<SharedMutableTreeItem> m_LastDeclSiblings; // multi-name decl: all created items except the last (m_pCurrent)
	UInt32                           m_LastDeclNameCount = 1;

	// type alias support
	const TreeItem* ResolveTypeRef(TokenID refID) const; // parse-time, declared-before-use, no using-directives
	TokenID                          m_AliasNameID;
	iterator_t                       m_PendingNameLoc; // source location captured at a function/variant-set name
	const TreeItem*                  m_PendingFunctionParamSig = nullptr; // function-signature exemplar awaiting item creation
	std::vector<TokenID>             m_PendingTypeArgs; // WP4.1: 'sig<V, D>' type-application arguments of the pending typeref
	const TreeItem*                  m_PendingTypeExemplar = nullptr; // type-by-example / alias source, for refinement (IntegrityCheck) cloning

	// generic type variable support: function f<V: numerics>(...)
	TokenID FindActiveTypeVarConstraint(TokenID varName) const; // empty when varName is no active type variable
	TokenID ConsumeGenericParamMarker(); // detects unit<V>/attribute<V> declarations; adjusts the signature for generic units
	bool    IsTopLevelFunctionParam() const; // m_pCurrent is a direct parameter of the function (not a member of a structured parameter)
	TokenID                          m_PendingTypeVarName;
	std::vector<std::pair<TokenID, TokenID>> m_PendingTypeVars;   // (var, constraint) collected before the function item exists
	TokenID                          m_PendingGenericUnitVar;     // set by DoBasicType for unit<V>

	// §5.11 tier B lambda-lifting support: pending literal extents within the rule
	// text under capture (outermost only; nested literals are re-lifted by the
	// enclosing literal's own declaration parse)
	struct PendingLambda { CharPtr spliceFirst, spliceLast, litFirst, litLast; };
	std::vector<PendingLambda>       m_PendingLambdas;
	UInt32                           m_LambdaCounter = 0;
	SharedStr MaterializePendingLambdas(CharPtr first, CharPtr last); // hoist + splice; returns the stored rule text
	SharedStr HoistFunctionLiteral(CharPtr litFirst, CharPtr litLast); // nested declaration parse; returns the synthesized name
	void      ParseNestedDeclaration(CharPtr declString); // in ConfigParse.cpp (needs the grammar)

	// function declaration support (stack: function declarations may nest in bodies)
	struct FuncProdState
	{
		SharedMutableTreeItem     funcItem;
		UInt32                    paramCount = 0;
		bool                      signatureOnly = false; // function-signature type alias: no result expression, no body
		bool                      resultIsFunction = false; // §5.10 '-> function': result is a nested function
		const TreeItem*           resultSigExemplar = nullptr; // '-> sigAlias<...>' result-signature exemplar (for the config dump)
		std::vector<TokenID>      resultSigTypeArgs;        // its type-application args
		bool                      inParamList = false;
		TokenID                   resultName;
		SignatureType             resultSig = SignatureType::Undefined;
		WeakPtr<const ValueClass> resultValueClass;
		TokenID                   resultSignatureUnit;
		TokenID                   resultParamEntity;
		ValueComposition          resultVC = ValueComposition::Unknown;
		SharedStr                 resultExpr;
		std::vector<std::tuple<UInt32, const TreeItem*, std::vector<TokenID>>> paramSigs; // (param index, signature exemplar, type-application args)
		std::vector<std::pair<UInt32, const TreeItem*>> paramExemplars; // K11a by-example: (param index, UNIT type exemplar) -- its declared members type the parameter
		std::vector<std::pair<TokenID, TokenID>> typeVars;         // (var, constraint)
		std::vector<std::tuple<UInt32, TokenID, TokenID, bool>> genericParams; // (param index, var, constraint, isDomainVar)
		std::vector<UInt32> metaRefParams;                         // 'item x' parameters (raw item-reference binding)
		bool hasRestParam = false;                                 // '...x' rest parameter (must be the last param)
	};
	std::vector<FuncProdState>       m_FuncStates;

	// property support
	TokenID                          m_sPropFileTypeID;
	bool                             m_ResultCommitted;
	SharedStr                        m_CurrFileName;
};

#endif
