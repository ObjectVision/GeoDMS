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
#include "TicPropDefConst.h"    // FUNCTIONSPEC_NAME, DATABLOCK_NAME
#include "TreeItemFunctionSpec.h" // #1261: TreeItem_SetFunctionSpecFromStr
#include "AbstrCalculator.h"      // #1261: ConstructFromDataBlockStr
#include "AbstrDataItem.h"

static StaticTokenID functionSpecID(FUNCTIONSPEC_NAME);
static StaticTokenID dataBlockID(DATABLOCK_NAME);


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
	m_PendingFunctionSpecs.clear();
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
		// #1261: no StorageType/StorageName attribute handling here. Both are xml_mode::element
		// properties, so the attribute loop right below refuses them as "seen as attribute"; the
		// code that used to stand here, calling DMS_TreeItem_SetStorageManager from those two
		// attributes, could therefore never run. They are written and read as child elements, which
		// is what the wiki documents and what the battery covers.
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
		// #1261: the function declaration, now that the parameters and the body are in place. Before
		// the item is popped, so that a failing specification is reported against the function item.
		if (!m_PendingFunctionSpecs.empty() && m_PendingFunctionSpecs.back().first == thisItem)
		{
			auto spec = std::move(m_PendingFunctionSpecs.back());
			m_PendingFunctionSpecs.pop_back();
			TreeItem_SetFunctionSpecFromStr(thisItem, spec.second.c_str());
		}
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
		// #1261: not a property, so not looked up as one: the function declaration of the item this
		// element sits in. Held until that item's element closes, see m_PendingFunctionSpecs.
		if (element.m_NameID == functionSpecID)
		{
			m_PendingFunctionSpecs.emplace_back(parentItem, SharedStr(CharPtrRange(&*element.m_EnclText.begin())));
			return false;
		}
		// #1261: idem for a configured value array. Applied here rather than deferred: it needs
		// nothing but the item it hangs on, and the calculator is built by the .dms side of the
		// engine through the factory that stx installs, since DataBlockTask is not reachable here.
		if (element.m_NameID == dataBlockID)
		{
			if (!IsDataItem(parentItem))
				throwErrorF("XML", "{}({}, {}): a DataBlock element is only allowed on a DATAITEM, not on {}"
					, Buffer().FileName(), GetLineNr(), GetColNr(), parentItem->GetNameID());
			SharedStr dataBlockText(CharPtrRange(&*element.m_EnclText.begin()));
			parentItem->GetOrCreateConfigProperties().mc_Calculator =
				AbstrCalculator::ConstructFromDataBlockStr(AsDataItem(parentItem), dataBlockText);
			return false;
		}
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
		else
		{
			// #1261: a property that is fixed at construction (name, ValueType, DomainUnit,
			// ValuesUnit, ValueComposition) or is read-only (TableType) used to be dropped here
			// without a word, so a configuration that spelled one as a child element loaded with
			// that element having no effect at all. Say so instead; the construction ones belong in
			// the open tag, where CreateFromXml reads them.
			throwErrorF("XML", "{}({}, {}): XML element {} for {}: {} names a property that cannot be set here: it is {}",
				Buffer().FileName(), GetLineNr(), GetColNr(),
				element.m_NameID,
				m_CurrItem->GetNameID(),
				cls->GetNameID(),
				propDef->GetSetMode() == set_mode::construction
					? "set at construction, from an attribute of the open tag"
					: "read-only");
		}
	}
	return false;
}

