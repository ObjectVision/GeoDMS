// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if !defined(__TIC_XML_TREE_OUT_H)
#define __TIC_XML_TREE_OUT_H

struct SafeFileWriterArray;

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "ptr/SharedStr.h"
#include "utl/mySPrintF.h"
#include "xml/xmlOut.h"
#include "xct/ErrMsg.h"

#include "TreeItem.h"
TIC_CALL BestItemRef TreeItem_GetErrorSource(const TreeItem* src, bool tryCalcSuppliers); // impl in TicInterface.cpp

/********** defines                                        **********/

#define CLR_BODY "#DDD2D0"
#define CLR_HROW "#FFFFE0"

// *****************************************************************************
// ********** XML structs                                             **********
// *****************************************************************************


inline SharedStr ItemUrl(CharPtr itemName)
{
	return mySSPrintF("dms:dp.general:%s", *itemName ? itemName : "/");
}

inline SharedStr ItemUrl(const TreeItem* item)
{
	return ItemUrl(item->GetFullName().c_str());
}

// ********** XML_ItemBody                                            **********

struct XML_ItemBody : XML_OutElement
{
	XML_ItemBody(OutStreamBase& out, CharPtr caption, CharPtr subText, const TreeItem* item, bool showFullName = false);
};

void NewLine(OutStreamBase& out);

struct XML_Table : XML_OutElement
{
	XML_Table(OutStreamBase& out): XML_OutElement(out, "TABLE") {}

	struct Row : XML_OutElement
	{
		Row(XML_Table& table): XML_OutElement(table.OutStream(), "TR") {}

		struct Cell : XML_OutElement
		{
			Cell(Row& row): XML_OutElement(row.OutStream(), "TD") {}
		};

		void ValueCell(CharPtr value)
		{
			Cell xmlElemTD(*this);
			OutStream().WriteTrimmed(value);
		}

		TIC_CALL void ClickableCell(CharPtr value, CharPtr hRef, bool bold = false);
		TIC_CALL void EditablePropCell(CharPtr propName, CharPtr propLabel = "", const TreeItem* item = nullptr);

		void ItemCell(const TreeItem* item, bool bold = false)
		{
			ClickableCell(item->GetFullName().c_str(), ItemUrl(item).c_str(), bold);
		}
	};

	void NameValueRow(CharPtr propName, CharPtr propValue)
	{
		Row row(*this);
			row.ValueCell(propName);
			row.ValueCell(propValue);
	}
	TIC_CALL void NameErrRow(CharPtr propName, const ErrMsg& err, const TreeItem* self);
	void NamedItemRow(CharPtr role, const TreeItem* item)
	{
		XML_Table::Row row(*this);
			row.ValueCell(role);
			row.ItemCell(item);
	}
	void EmptyRow(UInt32 colSpan = 2)
	{
		Row row(*this);
			Row::Cell xmlElemTD1(row);
				OutStream().WriteAttr("COLSPAN", colSpan);
					NewLine(OutStream());
	}

	void LinedRow(UInt32 colSpan = 2)
	{
		Row row(*this);
			Row::Cell xmlElemTD(row);
				OutStream().WriteAttr("COLSPAN", colSpan);
					XML_OutElement hr(OutStream(), "HR", "", false);
	}

	void EditableNameValueRow(CharPtr propName, CharPtr propValue, const TreeItem* context = 0)
	{
		Row row(*this);
			row.EditablePropCell(propName, propName, context);
			row.ValueCell(propValue);
	}
	void EditableNameItemRow(CharPtr propName, const TreeItem* item)
	{
		Row row(*this);
		OutStream().WriteAttr("bgcolor", CLR_HROW);
			row.EditablePropCell(propName);
			row.ItemCell(item);
	}
};

/********** helper funcs, implemented in XmlTreeOut.cpp    **********/

void GetExprOrSourceDescr(OutStreamBase& stream, const TreeItem* ti);
void GetExprOrSourceDescrRow(XML_Table& xmlTable, const TreeItem* ti);
void IncludeFileSave(const TreeItem* self, CharPtr fileName, SafeFileWriterArray* sfwa);

extern TIC_CALL void(*s_AnnotateExprFunc)(OutStreamBase& outStream, const TreeItem* searchContext, SharedStr expr);


#endif // __TIC_XML_TREE_OUT_H
