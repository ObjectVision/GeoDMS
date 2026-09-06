// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "TicPCH.h"
#include "utl/StrFormat.h" // mgFormat2SharedStr

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "XmlTreeParser.h"

#include "dbg/debug.h"
#include "dbg/DmsCatch.h"
#include "stg/StorageInterface.h"
#include "utl/IncrementalLock.h"
#include "utl/StrFormat.h"
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

		m_Msg = mgFormat2SharedStr("{0}({1}, {2}): parsing '{3}'"
			, m_Xml->Buffer().FileName() , lineNr , offset
			, (m_Xml->m_CurrItem ? m_Xml->m_CurrItem->GetFullName().c_str() : "XML")
		);
		return m_Msg.c_str();
	}
private:
	SharedStr      m_Msg;
	XmlTreeParser* m_Xml;
};

SharedMutableTreeItem XmlTreeParser::ReadTree(TreeItem* root, bool rootIsFirstItem)
{
	m_CurrItem = root;
	m_EnclosingItems.clear(); // the outermost element pushes root as its enclosing item
	m_RootIsFirstItem = rootIsFirstItem;

	DMS_CALL_BEGIN

		MG_LOCKER_NO_UPDATEMETAINFO

		XmlContextHandle xmlContext(this);

		XmlRead(); 

	DMS_CALL_END
	if (!root)
	{
		// brand-new root: ownership lives in m_RootHolder, hand it to the caller.
		// A null holder (empty / rootless document) yields a null result, which the caller handles.
		assert(!m_RootHolder || (m_RootHolder.get() == m_CurrItem && m_RootHolder.get() == m_CurrItem->GetRoot()));
		return m_RootHolder;
	}
	// appended into an existing tree: that tree's owner keeps it alive (share its existing ownership)
	return make_shared_tree(root, existing_obj{});
}

static StaticTokenID nameTokenID("name");
static StaticTokenID storageTypeID("StorageType");
static StaticTokenID storageNameID("StorageName");

// called when all attributes of elem has been read
void XmlTreeParser::ReadAttrCallback(XmlElement& element)
{
	++m_CurrElemLevel;
	TreeItem* thisItem = nullptr;

	MetaClass* cls = MetaClass::Find(element.m_NameID);
	if (cls)
	{
		// CreateFromXml hands back an owning SharedPtr; a child is already owned by its parent, so the
		// holder may drop. A brand-new root (created with no parent context) has no other owner yet, so
		// retain it in m_RootHolder for the parse lifetime and hand it to ReadTree's caller.
		bool isNewRoot = (m_CurrItem == nullptr);
		std::shared_ptr<Actor> created = cls->CreateFromXml(m_CurrItem, element);
		thisItem = debug_cast<TreeItem*>(created.get());
		if (thisItem && isNewRoot)
			m_RootHolder = std::static_pointer_cast<TreeItem>(created); // capture the std owner of the brand-new root
	}
	if (thisItem)
	{
		CharPtr storageType = element.GetAttrValue(storageTypeID);
		if (*storageType)
		{
			DMS_TreeItem_SetStorageManager(thisItem, element.GetAttrValue(storageNameID), storageType, StorageReadOnlySetting::Default);
		}
		const Class* thisCls = thisItem->GetDynamicClass();
		XmlElement::AttrValuesConstIterator avIter = element.GetAttrValuesBegin();
		XmlElement::AttrValuesConstIterator  avEnd  = element.GetAttrValuesEnd();
		while (avIter != avEnd)
		{
			AbstrPropDef* propDef = thisCls->FindPropDef((*avIter).first);
			if (!propDef)
			{
				throwErrorF("XML", "{}({}, {}): Unknown XML attribute {} for {}: {}",
					Buffer().FileName(), GetLineNr(), GetColNr(), 
					(*avIter).first, 
					thisItem->GetNameID(),
					thisCls->GetNameID()
				);
			}
			else if (propDef->GetXmlMode() == xml_mode::element)
			{
				throwErrorF("XML",
					"{}({}, {}): XML Element property {} seen as attribute for {}: {}",
					Buffer().FileName(), GetLineNr(), GetColNr(), 
					(*avIter).first, 
					thisItem->GetNameID(), 
					thisCls->GetNameID());
			}
			else if (propDef->GetSetMode() > set_mode::construction) // set_optional || set_obligated, not: set_construction || set_none
				propDef->SetValueAsCharRange(thisItem, (*avIter).second.begin(), (*avIter).second.send());
			XmlElement::Inc(avIter);
		}
		// Descend, remembering what to come back to. Pushed here, beside the m_ClientData that
		// makes ReadElemCallback pop, so the two stay in step: a throw earlier in this function
		// leaves both unset, and the parser is abandoned anyway.
		m_EnclosingItems.push_back(m_CurrItem);
		m_CurrItem = thisItem;
		element.m_ClientData = thisItem;
	}
}

// called when all sub-element have been read
bool XmlTreeParser::ReadElemCallback(XmlElement& element)
{
	--m_CurrElemLevel;

	TreeItem* thisItem = reinterpret_cast<TreeItem*>(element.m_ClientData);
	if (thisItem)
	{
		assert(thisItem == m_CurrItem);
		// Pop back to the item that was current when this element opened, which ReadAttrCallback
		// pushed. NOT the item of the parent ELEMENT: the outermost element of a fragment has no
		// parent element, so what its item hangs on is the context of an #include, and an element
		// that maps to no MetaClass creates no item to point back to either. Reading the parent
		// element made both of those look like "no parent", which asserted on every .xml fragment
		// included into a container and, in a headless Debug run, killed the load (#1254).
		assert(!m_EnclosingItems.empty());
		TreeItem* enclosingItem = m_EnclosingItems.back();
		m_EnclosingItems.pop_back();
		assert(enclosingItem == m_CurrItem->GetTreeParent().get());
		if (enclosingItem) // don't loose the root: ReadTree hands m_CurrItem back as the new root
			m_CurrItem = enclosingItem;
		return false;
	}

	// No item of its own: the element names a property of the item its parent ELEMENT created.
	// Still keyed on that element rather than on m_CurrItem, deliberately: a property element
	// directly under the fragment root, or under an element that created no item, is ignored
	// today, and applying it to the enclosing item instead is a separate decision.
	TreeItem* parentItem =0;
	if (element.m_Parent)
		parentItem = reinterpret_cast<TreeItem*>(element.m_Parent->m_ClientData);
	if (parentItem)
	{
		dms_assert(parentItem == m_CurrItem);
		const Class* cls = m_CurrItem->GetDynamicClass();
		AbstrPropDef* propDef = cls->FindPropDef(element.m_NameID);
		if (!propDef)
		{
			throwErrorF("XML", "{}({}, {}): Unknown XML element {} for {}: {}",
				Buffer().FileName(), GetLineNr(), GetColNr(), 
				element.m_NameID, 
				m_CurrItem->GetNameID(), 
				cls->GetNameID()
			);
		}
		else if (propDef->GetXmlMode() == xml_mode::attribute)
		{
			throwErrorF("XML", "{}({}, {}): XML Attribute property {} seen as element for {}: {}",
				Buffer().FileName(), GetLineNr(), GetColNr(), 
				element.m_NameID,
				m_CurrItem->GetNameID(), 
				cls->GetNameID()
			);
		}
		else if (propDef->GetSetMode() > set_mode::construction)
			propDef->SetValueAsCharArray(parentItem, &*element.m_EnclText.begin());
	}
	return false;
}

