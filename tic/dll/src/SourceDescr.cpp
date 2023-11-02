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
#include "TicPCH.h"
#pragma hdrstop

#include "act/ActorVisitor.h"
#include "act/SupplierVisitFlag.h"
#include "dbg/DmsCatch.h"
#include "utl/Environment.h"
#include "utl/IncrementalLock.h"
#include "geo/SequenceArray.h"
#include "ser/MoreStreamBuff.h"
#include "stg/AbstrStorageManager.h"

#include "AbstrCalculator.h"
#include "TreeItemProps.h"
#include "TreeItemClass.h"
#include "TreeItemContextHandle.h"

//  -----------------------------------------------------------------------
//  SourceDescr
//  -----------------------------------------------------------------------
namespace { // local defs

	typedef SizeT source_seq_index;
	typedef SizeT source_seq_size;

	struct SourceCalculator
	{

		typedef TokenID                                  source_t;
		typedef std::vector<TokenID>                     source_seq_t;
		typedef sequence_vector<TokenID>                 source_seq_array_t;
		typedef std::map<const Actor*, source_seq_index> source_seq_index_map_t;

		SourceCalculator(SourceDescrMode sdm, bool bShowHidden);

		auto GetDescr(const TreeItem* ti) -> SharedStr;
		auto GetSourceSequence(const TreeItem* ti) -> SA_Reference<TokenID>;
		auto GetFirstErrorItem() -> SharedStr;
		source_seq_index GetOrCalcSourceSeqIndex(const Actor* ti, bool alsoSubItems);
		auto GetSourceSecArray() -> source_seq_array_t* { return &m_SourceSecArray; };

	private:
		source_seq_index Assign0();
		source_seq_index CalcSourceSeqIndex(const Actor* ti, bool alsoSubItems);

		SourceDescrMode        m_SDM;
		bool                   m_bShowHidden;
		bool                   m_hasError = false;

		source_seq_array_t     m_SourceSecArray;
		source_seq_index_map_t m_IndexMap;
		SharedPtr<const TreeItem> m_FirstErrorItem;
	};


	source_seq_index SourceCalculator::Assign0()
	{
		auto result = m_SourceSecArray.size();
		m_SourceSecArray.push_back(Undefined());
		return result;
	}

	SourceCalculator::SourceCalculator(SourceDescrMode sdm, bool bShowHidden)
		:	m_SDM(sdm)
		,	m_bShowHidden(bShowHidden)
		,	m_SourceSecArray(1 MG_DEBUG_ALLOCATOR_SRC_SA)
	{
		m_SourceSecArray.push_back(Undefined());
		assert(m_SourceSecArray[0].size() == 0);
	}


	TokenID CalcSourceToken(const Actor* act, bool showHidden, SourceDescrMode sdm)
	{
		const TreeItem* ti = dynamic_cast<const TreeItem*>(act);
		SharedStr v;
		if (ti)
		{
			if (!showHidden && ti->GetTSF(TSF_InHidden))
				return TokenID(Undefined());
			if (ti->InTemplate())
				return TokenID(Undefined());

			dms_assert(sourceDescrPropDefPtr);

			switch (sdm) {

			case SourceDescrMode::Configured:
				v = TreeItemPropertyValue(ti, sourceDescrPropDefPtr);
				break;

			case SourceDescrMode::ReadOnly:
			case SourceDescrMode::WriteOnly:
				if (storageReadOnlyPropDefPtr->GetValue(ti) != (sdm == SourceDescrMode::ReadOnly))
					break;
				[[fallthrough]];
			case SourceDescrMode::All:
				if (ti->HasStorageManager())
					v += "name=" + DoubleQuote(ti->GetStorageManager()->GetNameStr().c_str());
				if (sqlStringPropDefPtr->HasNonDefaultValue(ti))
				{
					if (!v.empty())
						v += ";";
					v += "SqlString=" + DoubleQuote(sqlStringPropDefPtr->GetValue(ti).c_str());
				}
				if ((sdm == SourceDescrMode::All) && !v.empty())
					v += ";ReadOnly=" + AsDataStr(Bool(storageReadOnlyPropDefPtr->GetValue(ti)));
			}
		}
		return GetTokenID_mt(v.c_str());
	}

	source_seq_index SourceCalculator::CalcSourceSeqIndex(const Actor* act, bool alsoSubItems)
	{
		// now fill resultingTokenSequence as needed.
		source_seq_t  resultingTokenSequence;
		
		const TreeItem* ti = dynamic_cast<const TreeItem*>(act);
		if (ti)
		{
			ti->UpdateMetaInfo();
			TokenID sourceTokenID = CalcSourceToken(act, m_bShowHidden, m_SDM);
			if (!IsDefined(sourceTokenID))
				return Assign0();

			if (sourceTokenID)
				resultingTokenSequence.push_back(sourceTokenID);
		}

		source_seq_t  tmpTokenSequence;

		auto merger = [&](const Actor* supplier)
		{
			if (supplier->IsPassorOrChecked())
				return;
			tmpTokenSequence.swap(resultingTokenSequence);
			resultingTokenSequence.clear();
			sequence_vector<TokenID>::const_reference supplTokenRef = m_SourceSecArray[GetOrCalcSourceSeqIndex(supplier, false)];
			std::set_union(
				tmpTokenSequence.begin(),
				tmpTokenSequence.end(),
				supplTokenRef.begin(),
				supplTokenRef.end(),
				std::back_inserter(resultingTokenSequence)
			);
		};

		VisitSupplProcImpl(act, SupplierVisitFlag::InspectAll, merger);

		if (alsoSubItems && ti)
		{
			auto curr = ti;
			while (curr = ti->WalkConstSubTree(curr))
				merger(curr);
		}


		auto result = m_SourceSecArray.size();
		m_SourceSecArray.push_back_seq(begin_ptr(resultingTokenSequence), end_ptr(resultingTokenSequence));
		return result;
	}

	source_seq_index SourceCalculator::GetOrCalcSourceSeqIndex(const Actor* act, bool alsoSubItems)
	{
		dms_assert(act);
		source_seq_index_map_t::iterator mi = m_IndexMap.lower_bound(act);
		if (mi == m_IndexMap.end() || act != mi->first)
		{
			mi = m_IndexMap.insert(mi, source_seq_index_map_t::value_type(act, 0) );
			try {
				mi->second = CalcSourceSeqIndex(act, alsoSubItems);
			}
			catch (...)
			{
				mi->second = Assign0();
				m_hasError = true;
			}
			if (m_hasError && !m_FirstErrorItem)
				m_FirstErrorItem = dynamic_cast<const TreeItem*>(act);
		}
		return mi->second;
	}

	auto SourceCalculator::GetSourceSequence(const TreeItem* ti) -> SA_Reference<TokenID>
	{
		return m_SourceSecArray[GetOrCalcSourceSeqIndex(ti, true)];
	}

	auto SourceCalculator::GetFirstErrorItem() -> SharedStr
	{
		VectorOutStreamBuff vout;
		FormattedOutStream fout(&vout, FormattingFlags::ThousandSeparator);
		if (!m_hasError)
			return {};

		fout << "\nList may be incomplete due to errors.";
		if (m_FirstErrorItem)
			fout << " First error at:\n" << m_FirstErrorItem->GetSourceName();

		CharPtr first = vout.GetData();
		return SharedStr(first, first + vout.CurrPos());
	}

	SharedStr SourceCalculator::GetDescr(TreeItem const * ti)
	{
		VectorOutStreamBuff vout;
		FormattedOutStream fout(&vout, FormattingFlags::ThousandSeparator);
		/*fout << "<BODY bgcolor = '#DDD2D0'>";
		switch (m_SDM) {
		case SourceDescrMode::Configured: fout << "<h1>Configured Source Descriptions</h1>\n"; break;
		case SourceDescrMode::ReadOnly:   fout << "<h1>Read Only Storage Managers</h1>\n"; break;
		case SourceDescrMode::WriteOnly:  fout << "<h1>Non-Read Only Storage Managers</h1>\n"; break;
		case SourceDescrMode::All:        fout << "<h1>Utilized Storage Managers</h1>\n"; break;
		}
		fout << "For item: \n" << ti->GetSourceName() << "\n";
		if (DMS_Appl_GetExeType() == exe_type::geodms_qt_gui)
			fout << "(other specs can be obtaint by re-pressing the (S) button in the DetailPage toolbar)";
		else
			fout << "(other specs can be selected at View->Source Descr variant)";
		*/

		SA_Reference<TokenID> sourceSequence = m_SourceSecArray[GetOrCalcSourceSeqIndex(ti, true)];
		if (IsDefined(sourceSequence))
			for (auto tokenPtr = sourceSequence.begin(), tokenEnd = sourceSequence.end(); tokenPtr != tokenEnd; ++tokenPtr)
				fout << "\n- " << *tokenPtr;

		auto first_error_item = GetFirstErrorItem();
		if (!first_error_item.empty())
			fout << first_error_item;

		/*if (m_hasError) {
			fout << "\nList may be incomplete due to errors.";
			if (m_FirstErrorItem)
				fout << " First error at:\n" << m_FirstErrorItem->GetSourceName();
		}*/

		//fout << "</BODY>";

		CharPtr first = vout.GetData();
		return SharedStr(first, first+vout.CurrPos());
	}

} // end of anonymous namespace

SharedStr TreeItem_GetSourceDescr(const TreeItem* studyObject, SourceDescrMode sdm, bool bShowHidden)
{
	dms_assert(IsMainThread());
	TreeItemContextHandle hnd(studyObject, TreeItem::GetStaticClass(), "DMS_TreeItem_GetSourceDescr");
	
	auto source_description_string = SourceCalculator(sdm, bShowHidden).GetDescr(studyObject);

	return source_description_string;
}

#include "xml/XMLOut.h"
TIC_CALL void TreeItem_DumpSourceCalculator(const TreeItem* studyObject, SourceDescrMode sdm, bool bShowHidden, OutStreamBase* xmlOutStrPtr)
{
	assert(xmlOutStrPtr);
	auto source_calculator = SourceCalculator(sdm, bShowHidden);// .GetDescr(studyObject);
	auto source_seq_array = source_calculator.GetSourceSecArray();
	if (source_seq_array->empty())
		return;

	SA_Reference<TokenID> source_sequence = (*source_seq_array)[source_calculator.GetOrCalcSourceSeqIndex(studyObject, true)];
	if (!IsDefined(source_sequence))
		return;

	auto ordered_list = XML_OutElement(*xmlOutStrPtr, "ul");
	for (auto tokenPtr = source_sequence.begin(), tokenEnd = source_sequence.end(); tokenPtr != tokenEnd; ++tokenPtr)
	{
		auto list_item = XML_OutElement(*xmlOutStrPtr, "li");
		*xmlOutStrPtr << SharedStr(*tokenPtr).c_str();
	}
		/*out.WriteAttr("bgcolor", CLR_BODY);

		{
			XML_OutElement page_title_table(out, "TABLE");
			out.WriteAttr("width", "100%");
			//out.WriteAttr("style", "font-size:200%");
			XML_OutElement table_row(out, "TR");
			out.WriteAttr("bgcolor", "#89CFF0");
			XML_OutElement table_col(out, "TD");
			{
				XML_OutElement font_size(out, "font");
				out.WriteAttr("size", "4");
				out << caption;
*/

	
	return ;
}

//  -----------------------------------------------------------------------
//  extern "C" interface functions
//  -----------------------------------------------------------------------

extern "C"

TIC_CALL CharPtr DMS_CONV DMS_TreeItem_GetSourceDescr(const TreeItem* studyObject, SourceDescrMode sdm, bool bShowHidden)
{
	DMS_CALL_BEGIN

		static SharedStr result;
		result = TreeItem_GetSourceDescr(studyObject, sdm, bShowHidden);
		return result.c_str();

	DMS_CALL_END
	return "Error occured during GetSourceDescr";
}
