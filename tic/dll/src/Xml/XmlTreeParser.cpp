// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "XmlTreeParser.h"

#include "dbg/debug.h"
#include "dbg/DmsCatch.h"
#include "stg/StorageInterface.h"
#include "utl/IncrementalLock.h"
#include "utl/mySPrintF.h"
#include "xct/DmsException.h"

// *****************************************************************************


// *****************************************************************************

XmlTreeParser::XmlTreeParser(InpStreamBuff* inpBuff)
	: XmlParser(inpBuff)
	, m_CurrItemLevel(0)
	, m_CurrElemLevel(0) 
{}

XmlTreeParser::~XmlTreeParser()
{
	if (m_CurrElemLevel != 0)
	{
		MG_TRACE(("XMLTreeParser did not meet required close tags"));
	}
}


#include "mci/ValueClass.h"
#include "mci/PropDef.h"
#include "mci/PropdefEnums.h"


struct XmlContextHandle : AbstrContextHandle
{
	XmlContextHandle(XmlTreeParser* xml) : m_Xml(xml) {}

	CharPtr GetDescription() override
	{
		int lineNr = m_Xml->GetLineNr();  
		int offset = m_Xml->GetColNr();

		m_Msg = mgFormat2SharedStr("%1%(%2%, %3%): parsing '%4%'"
			, m_Xml->Buffer().FileName() , lineNr , offset
			, (m_Xml->m_CurrItem ? m_Xml->m_CurrItem->GetFullName().c_str() : "XML")
		);
		return m_Msg.c_str();
	}
private:
	SharedStr      m_Msg;
	XmlTreeParser* m_Xml;
};

TreeItem* XmlTreeParser::ReadTree(TreeItem* root, bool rootIsFirstItem)
{
	m_CurrItem = root;
	m_RootIsFirstItem = rootIsFirstItem;

	DMS_CALL_BEGIN

		MG_LOCKER_NO_UPDATEMETAINFO

		XmlContextHandle xmlContext(this);

		XmlRead(); 

	DMS_CALL_END
	if (!root) 
	{
		root = m_CurrItem; // is Root?
		assert(root == m_CurrItem->GetRoot());
	}
	return root;
}

static TokenID nameTokenID = GetTokenID_st("name");
static TokenID storageTypeID = GetTokenID_st("StorageType");
static TokenID storageNameID = GetTokenID_st("StorageName");

// called when all attributes of elem has been read
void XmlTreeParser::ReadAttrCallback(XmlElement& element)
{
	++m_CurrElemLevel;
	TreeItem* thisItem = nullptr;

	MetaClass* cls = MetaClass::Find(element.m_NameID);
	if (cls)
		thisItem = debug_cast<TreeItem*>(cls->CreateFromXml(m_CurrItem, element));
	if (thisItem)
	{
		CharPtr storageType = element.GetAttrValue(storageTypeID);
		if (*storageType)
		{
			DMS_TreeItem_SetStorageManager(thisItem, 
				element.GetAttrValue(storageNameID), 
				storageType, false);
		}
		const Class* thisCls = thisItem->GetDynamicClass();
		XmlElement::AttrValuesConstIterator avIter = element.GetAttrValuesBegin();
		XmlElement::AttrValuesConstIterator  avEnd  = element.GetAttrValuesEnd();
		while (avIter != avEnd)
		{
			AbstrPropDef* propDef = thisCls->FindPropDef((*avIter).first);
			if (!propDef)
			{
				throwErrorF("XML", "%s(%d, %d): Unknown XML attribute %s for %s: %s",
					Buffer().FileName(), GetLineNr(), GetColNr(), 
					GetTokenStr((*avIter).first), 
					thisItem->GetName(),
					thisCls->GetName()
				);
			}
			else if (propDef->GetXmlMode() == xml_mode::element)
			{
				throwErrorF("XML",
					"%s(%d, %d): XML Element property %s seen as attribute for %s: %s",
					Buffer().FileName(), GetLineNr(), GetColNr(), 
					GetTokenStr((*avIter).first), 
					thisItem->GetName(), 
					thisCls->GetName());
			}
			else if (propDef->GetSetMode() > set_mode::construction) // set_optional || set_obligated, not: set_construction || set_none
				propDef->SetValueAsCharRange(thisItem, (*avIter).second.begin(), (*avIter).second.send());
			XmlElement::Inc(avIter);
		}
		m_CurrItem = thisItem;
		element.m_ClientData = thisItem;
	}
}

// called when all sub-element have been read
bool XmlTreeParser::ReadElemCallback(XmlElement& element)
{
	--m_CurrElemLevel;

	TreeItem* thisItem = reinterpret_cast<TreeItem*>(element.m_ClientData);
	TreeItem* parentItem =0;
	if (element.m_Parent)
		parentItem = reinterpret_cast<TreeItem*>(element.m_Parent->m_ClientData);
	if (parentItem && !thisItem)
	{
		dms_assert(parentItem == m_CurrItem);
		const Class* cls = m_CurrItem->GetDynamicClass();
		AbstrPropDef* propDef = cls->FindPropDef(element.m_NameID);
		if (!propDef)
		{
			throwErrorF("XML", "%s(%d, %d): Unknown XML element %s for %s: %s",
				Buffer().FileName(), GetLineNr(), GetColNr(), 
				GetTokenStr(element.m_NameID), 
				m_CurrItem->GetName(), 
				cls->GetName()
			);
		}
		else if (propDef->GetXmlMode() == xml_mode::attribute)
		{
			throwErrorF("XML", "%s(%d, %d): XML Attribute property %s seen as element for %s: %s",
				Buffer().FileName(), GetLineNr(), GetColNr(), 
				GetTokenStr(element.m_NameID),
				m_CurrItem->GetName(), 
				cls->GetName()
			);
		}
		else if (propDef->GetSetMode() > set_mode::construction)
			propDef->SetValueAsCharArray(parentItem, &*element.m_EnclText.begin());
		
	}
	else if (thisItem)
	{
		dms_assert(thisItem == m_CurrItem);
		// Pop m_CurrItem if curr element is related to item.
		dms_assert(parentItem == m_CurrItem->GetTreeParent());
		if (parentItem) // don't loose the root
			m_CurrItem = parentItem;
	}
	return false;
}

