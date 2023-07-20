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
#pragma once

#if !defined( __STX_PRODCONFIG_H)
#define __STX_PRODCONFIG_H

#include "TicBase.h"

#include "ptr/PersistentSharedObj.h"
#include "ptr/SharedPtr.h"
#include "ptr/WeakPtr.h"
#include "set/Token.h"

#include "DataBlockProd.h"
#include "ExprProd.h"


enum SignatureType {
	ST_TreeItem,
	ST_Template,
	ST_Function,
	ST_Unit,
	ST_Attribute,
	ST_Parameter
};


struct ConfigProd : AbstrDataBlockProd
{
	EmptyExprProd m_ExprProd = {};

	ConfigProd(TreeItem* context);
	~ConfigProd();

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
	bool      CurrentIsRoot()  const { return m_stackContexts.empty(); }

	MG_DEBUGCODE(
	bool      CurrentIsTop ()  const { return CurrentIsRoot() || m_stackContexts.size() == 1 && md_IsIncludedFile; }
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

	typedef SharedPtr<TreeItem>      TreeItemRef;

	MG_DEBUGCODE(bool                md_IsIncludedFile; )

	TreeItemRef                      m_pCurrent;
	quick_replace_stack<TreeItemRef> m_stackContexts;

	TokenID                          m_ItemNameID;
	TokenID                          m_strIdentifierID;

	// Signature support
	SignatureType                    m_eSignatureType;    // see enum above
	WeakPtr<const ValueClass>        m_eValueClass;        // signature of unit definition
	TokenID                          m_pSignatureUnit;

	// param support
	TokenID                          m_pParamEntity;
	ValueComposition                 m_eParamVC;

	// property support
	TokenID                          m_sPropFileTypeID;
	bool                             m_ResultCommitted;
};

#endif
