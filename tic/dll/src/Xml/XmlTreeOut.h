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
BestItemRef TreeItem_GetErrorSource(const TreeItem* src); // impl in TicInterface.cpp

/********** defines                                        **********/

#define MAX_TEXTOUT_SIZE 400

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
	XML_ItemBody(OutStreamBase& out, const TreeItem* item, bool showFullName = false);
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


		void WriteCellData(CharPtr value)
		{
			OutStream().WriteValueN(value, MAX_TEXTOUT_SIZE - 3, "...");
		}

		void ValueCell(CharPtr value)
		{
			Cell xmlElemTD(*this);
				WriteCellData(value);
		}

		void ClickableCell(CharPtr value, CharPtr hRef)
		{
			Cell xmlElemTD(*this);
				XML_hRef xmlElemA(OutStream(), hRef);
					WriteCellData(value);
		}
		void EditablePropCell(CharPtr propName, CharPtr propLabel = "", const TreeItem* item = 0)
		{
			if (!*propLabel) 
				propLabel = propName;
			if (item && item->IsCacheItem())
				ValueCell(propLabel);
			else
			{
				SharedStr editUrl = (item)
					?	mySSPrintF("dms:edit!%s:%s", propName, item->GetFullName().c_str())
					:	mySSPrintF("dms:edit!%s", propName);
				ClickableCell(propLabel, editUrl.c_str());
			}
		}
		void ItemCell(const TreeItem* item)
		{
			ClickableCell(item->GetFullName().c_str(), ItemUrl(item).c_str());
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

	void	LinedRow(UInt32 colSpan = 2)
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
