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
#include "set/Token.h"

#include "DataBlockProd.h"
#include "ExprProd.h"


enum class SignatureType {
	Undefined,
	TreeItem,
	Template,
	Function,
	Unit,
	Attribute,
	Parameter
};


struct ConfigProd : AbstrDataBlockProd, AbstrContextHandle
{
	EmptyExprProd m_ExprProd = {};

	ConfigProd(TreeItem* context, bool rootIsFirstItem);
	~ConfigProd();

//	impl AbstrContextHandle
	bool HasItemContext() const override { return m_pCurrent;  }
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
	void CreateUnit     (TokenID nameID);
	void CreateAttribute(TokenID nameID);
	void CreateParameter(TokenID nameID);

MG_DEBUGCODE(	void    ClearSignature(); )
	void                ClearPropData();
	TokenID             RetrieveEntity();

	[[noreturn]] virtual void throwSemanticError(CharPtr msg) override;

	using TreeItemRef = SharedPtr<TreeItem>;

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

	// property support
	TokenID                          m_sPropFileTypeID;
	bool                             m_ResultCommitted;
	SharedStr                        m_CurrFileName;
};

#endif
