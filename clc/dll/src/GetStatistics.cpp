// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// Statistics text for data items: min/max, counts and value summaries as
// shown in the GUI's statistics pane.

#include "dbg/DmsCatch.h"
#include "geo/DataPtrTraits.h"
#include "mci/ValueClass.h"
#include "mci/ValueClassID.h"
#include "utl/mySPrintF.h"
#include "xct/DmsException.h"

#include "RtcTypeLists.h"

#include "AbstrDataItem.h"
#include "AbstrUnit.h"
#include "DataItemClass.h"
#include "DisplayValue.h"
#include "Metric.h"
#include "OperationContext.h"
#include "StateChangeNotification.h"
#include "TreeItemProps.h"
#include "TreeItemClass.h"
#include "TreeItemContextHandle.h"
#include "UnitProcessor.h"
#include "xml/XmlTreeOut.h"

#include "TicInterface.h"
#include "ClcInterface.h"

#include "pcount.h"

#include <cmath>


SharedStr ReplaceChar(SharedStr src, char ch, char esc)
{
	auto pos = std::find(src.begin(), src.end(), ch);
	if (pos == src.end())
		return src;

	VectorOutStreamBuff buff;
	auto cursor = src.begin();
	while (pos != src.end())
	{
		buff.WriteBytes(cursor, pos - cursor);
		buff.WriteByte('\\');
		buff.WriteByte(esc);
		cursor = pos + 1;
		pos = std::find(cursor, src.end(), ch);
	}
	buff.WriteBytes(cursor, src.end() - cursor);
	return SharedStr{ CharPtrRange(buff.GetData(), buff.GetDataEnd()) };
}

SharedStr Tablelize(SharedStr src)
{
	src = ReplaceChar(src, '\\', '\\');
	src = ReplaceChar(src, '\n', 'n');
	src = ReplaceChar(src, '\t', 't');
	return src;
}

// Collects name-value rows as tab-separated text and, on destruction, writes a
// "copy-to-clipboard-as-tab-separated-values" hyperlink with that text as "clipboard:..." href
// to the html stream; clicking the link in the GUI puts the payload on the clipboard
// (see MainWindow::onInternalLinkClick). "Post": the link is emitted after the content it
// belongs to, when this object goes out of scope.
struct PostLinker
{
	VectorOutStreamBuff buff;
	FormattedOutStream fout = FormattedOutStream(&buff);
	OutStreamBase& os;
	PostLinker(OutStreamBase& os_)
		:	os(os_)
	{
		fout << "clipboard:";
	}
	~PostLinker()
	{
		fout << char(0);
		hRefWithText(os, "copy-to-clipboard-as-tab-separated-values", buff.GetData());
	}

	// adds a row to the clipboard payload only; it does not appear in the html output
	void AddClipboardRow(CharPtr propName, CharPtr propValue)
	{
		fout
			<< Tablelize(SharedStr(propName)) << "\t"
			<< Tablelize(SharedStr(propValue)) << "\n";

	}
};

// A visible html table whose rows are also collected into a copy-to-clipboard link:
// AddRow writes a row to both the table and the clipboard payload; use the inherited
// AddClipboardRow for payload-only rows (e.g. the item path, which the visible page
// already presents in its header). The link lands just below the table: member `table`
// is destructed (closing the Table element) before the ~PostLinker base emits the link.
struct PostLinkedTable : PostLinker
{
	PostLinkedTable(OutStreamBase& os_)
		: PostLinker(os_)
		, table(os_)
	{}

	void AddRow(CharPtr propName, CharPtr propValue)
	{
		table.NameValueRow(propName, propValue);
		AddClipboardRow(propName, propValue);
	}

private:
	XML_Table table;
};

struct f64_accumulator
{
	SizeT d = 0;
	Float64
		s = 0,
		s2 = 0,
		min = MaxValue<Float64>(),
		max = MinValue<Float64>();
	void operator ()(Float64 x)
	{
		++d;
		s += x;
		s2 += x * x;
		MakeMin(min, x);
		MakeMax(max, x);
	}
	void operator +=(const f64_accumulator& rhs)
	{
		d += rhs.d;
		s += rhs.s;
		s2 += rhs.s2;
		MakeMin(min, rhs.min);
		MakeMax(max, rhs.max);
	}
};

struct point64_accumulator
{
	SizeT nrPoints = 0, nrValues = 0;
	DPoint
		s   = DPoint(0, 0),
		min = MaxValue<DPoint>(),
		max = MinValue<DPoint>();

	void operator ()(DPoint xy)
	{
		++nrPoints;
		s += xy;
		MakeLowerBound(min, xy);
		MakeUpperBound(max, xy);
	}
	void operator +=(const point64_accumulator& rhs)
	{
		nrPoints += rhs.nrPoints;
		nrValues += rhs.nrValues;
		s += rhs.s;
		MakeLowerBound(min, rhs.min);
		MakeUpperBound(max, rhs.max);
	}
};

using bin_count_type = std::vector<SizeT>;

void AccumulateNumericData(f64_accumulator& accu, bin_count_type& binCounts, const AbstrDataItem* di)
{
	auto valueBitSize = di->GetRefObj()->GetValuesType()->GetBitSize();
	if (valueBitSize && valueBitSize <= 16)
	{
		binCounts.resize(row_id(1) << valueBitSize);
	}

	auto domain = di->GetAbstrDomainUnit();
	auto vu = di->GetAbstrValuesUnit();

	for (tile_id t = 0, tn = domain->GetNrTiles(); t != tn; ++t)
	{
		ReadableTileLock tileLck(di->GetRefObj().get(), t);

		visit<typelists::numerics>(vu,
			[di, t, &accu] <typename V> (const Unit<V>*)
			{
				auto tileData = const_array_cast<V>(di)->GetTile(t);
				for (auto x : tileData)
				{
					if constexpr (!is_bitvalue_v<V>)
					{
						if (!IsDefined(x))
							continue;
					}
					accu(Convert<Float64>(x));
				}
			}
		);

		if (binCounts.size())
		{
			visit<typelists::ashorts>(vu,
				[di, t, &binCounts] <typename V> (const Unit<V>* vu)
				{
					auto tileData = const_array_cast<V>(di)->GetTile(t);
					//									pcount_container(binCounts, tileData, vu->GetRange(), DataCheckMode::DCM_None, false);
					pcount_best(
						tileData.begin(), tileData.end(),
						begin_ptr(binCounts), binCounts.size(),
						vu->GetRange(), is_bitvalue_v<V> ? DataCheckMode::DCM_None : DataCheckMode::DCM_CheckDefined,
						false
					);
				}
			);
		}
	}
}

void WriteNumericAccuData(PostLinkedTable& table, const f64_accumulator& accu, const AbstrDataItem* di)
{
	auto vu = di->GetAbstrValuesUnit();
	auto metricPtr = vu->GetMetric();
	table.AddRow(mySSPrintF("Formal Range {}",  vu->GetName().c_str()).c_str(), di->GetAbstrValuesUnit()->GetRangeAsStr(FormattingFlags::ThousandSeparator).c_str());
	if (metricPtr)
		table.AddRow("Metric Units", metricPtr->AsString(FormattingFlags::ThousandSeparator).c_str());

	table.AddRow("Minimum", AsString(accu.min).c_str());
	table.AddRow("Maximum", AsString(accu.max).c_str());
	table.AddRow("Sum", AsString(accu.s).c_str());

	if (accu.d) // there is actual data?
	{
		Float64
			mean = accu.s / accu.d,
			sumXtimesErr = Max<Float64>(accu.s2 - accu.s * mean, 0.0);


		table.AddRow("Average", AsString(mean).c_str());
		table.AddRow("Variance", AsString(sumXtimesErr / accu.d).c_str());

		if (accu.d > 1)
			table.AddRow("StdDev", AsString(sqrt(sumXtimesErr / (accu.d - 1))).c_str());
	}
}

void AccumulatePointData(point64_accumulator& accu, const AbstrDataItem* di)
{
	auto domain = di->GetAbstrDomainUnit();
	auto vu = di->GetAbstrValuesUnit();
	auto vc = di->GetValueComposition();
	for (tile_id t = 0, tn = domain->GetNrTiles(); t != tn; ++t)
	{
		ReadableTileLock tileLck(di->GetRefObj().get(), t);

		if (vc == ValueComposition::Single)
		{
			visit<typelists::points>(vu,
				[di, t, &accu] <typename P> (const Unit<P>*)
			{
				auto tileData = const_array_cast<P>(di)->GetTile(t);
				for (auto x : tileData) if (IsDefined(x))
				{
					accu(Convert<DPoint>(x));
					accu.nrValues++;
				}
			}
			);
		}
		else
		{
			visit<typelists::points>(vu,
				[di, t, &accu] <typename P> (const Unit<P>*)
			{
				using SequenceType = sequence_traits<P>::container_type;

				auto tileData = const_array_cast<SequenceType>(di)->GetTile(t);
				for (auto seq : tileData) if (IsDefined(seq))
				{
					for (auto p : seq) if (IsDefined(p))
						accu(Convert<DPoint>(p));
					accu.nrValues++;
				}
			}
			);
		}
	}
}

void WritePointAccuData(PostLinkedTable& table, const point64_accumulator& accu, const AbstrDataItem* di)
{
	auto vu = di->GetAbstrValuesUnit();
	auto metricPtr = vu->GetMetric();
	table.AddRow(mySSPrintF("Formal Range {}", vu->GetName().c_str()).c_str(), di->GetAbstrValuesUnit()->GetRangeAsStr(FormattingFlags::ThousandSeparator).c_str());
	if (metricPtr)
		table.AddRow("Metric Units", metricPtr->AsString(FormattingFlags::ThousandSeparator).c_str());

	table.AddRow("Minimum", AsString(accu.min).c_str());
	table.AddRow("Maximum", AsString(accu.max).c_str());

	if (accu.nrPoints) // there is actual data?
	{
		DPoint mean = accu.s;
		mean.first  /= accu.nrPoints;
		mean.second /= accu.nrPoints;

		table.AddRow("Average", AsString(mean).c_str());
	}
}

void WriteAsTable(OutStreamBase& os, const bin_count_type& binCounts, const AbstrDataItem* di)
{
	PostLinkedTable table(os);
	table.AddClipboardRow("Item name", di->GetFullName().c_str());
	table.AddRow("Value", "Count");

	auto vu = di->GetAbstrValuesUnit();
	bool useMetric = false;
	SharedDataItemInterestPtr ipHolder;
	streamsize_t maxLen = 33;
	GuiReadLock guiLock;

	for (SizeT i = 0; i != binCounts.size(); ++i)
	{
		if (binCounts[i])
			table.AddRow(DisplayValue(vu, i, useMetric, ipHolder, maxLen, guiLock).c_str()
				, AsString(binCounts[i]).c_str()
			);
	}
}

void WriteBinData(OutStreamBase& os, const bin_count_type& binCounts, const AbstrDataItem* di)
{
	if (binCounts.size())
	{
		XML_OutElement extraSpace(os, "BR", "", ClosePolicy::nonPairedElement);

		WriteAsTable(os, binCounts, di);
	}
	else
	{
		os << CharPtr(u8"\n\nTo see all unique values and their frequency, you can open this attribute in a TableView, select the column header, and press the ∑ (GroupBy) button in the toolbar.");
	}
}

CLC_CALL bool NumericDataItem_GetStatistics(const TreeItem* item, vos_buffer_type& statisticsBuffer)
{
	assert(!SuspendTrigger::DidSuspend()); // Precondition?
	assert(item->HasInterest()); // PRECONDITION

	statisticsBuffer.clear();

	ExternalVectorOutStreamBuff outStreamBuff(statisticsBuffer);

	OutStream_HTM os(&outStreamBuff, "html", nullptr);

	try {

		os << "Statistics for " << item->GetFullName().c_str() << ":\n";

		if (item->InTemplate())
		{
			os <<
				"\nNot available since the DataItem is part of a calculation scheme template."
				"\nGo to its instantiations to see actual data";

			return true;
		}

		SharedDataItemInterestPtr di = AsDynamicDataItem(item);
		if (!di)
		{
			os << "Not available since this is not a DataItem";
			return true;
		}
		assert(di->Was(ProgressState::MetaInfo)); // PRECONDITION
		SharedUnitInterestPtr vu = make_shared_tree(di->GetAbstrValuesUnit(), existing_obj{});
		assert(vu);
		assert(vu->Was(ProgressState::MetaInfo)); // follows from PRECONDITION
		bool isReady = vu->PrepareData(); // GetRange needed later

		SizeT d = 0;
		bin_count_type binCounts;

		auto vt = di->GetAbstrValuesUnit()->GetValueType();
		{
			PostLinkedTable table(os);
			table.AddClipboardRow("Item name", item->GetFullName().c_str()); // #1149: the visible header already shows the path
			SharedStr metricStr = vu->GetCurrMetricStr(os.GetFormattingFlags());
			if (!metricStr.empty())
				table.AddRow("ValuesMetric", metricStr.c_str());
			table.AddRow("ValuesType", vt->GetID().GetStr().c_str());

			assert(!SuspendTrigger::DidSuspend() || !isReady);
			if (isReady)
				isReady = di->PrepareData();

			if (di->IsFailed(FailType::Data))
			{
				auto fr = di->GetFailReason();
				if (fr)
					os << "\nProcessing statistics failed because:\n\n" << *fr;
				return true;
			}
			assert(!(isReady && SuspendTrigger::DidSuspend())); // PRECONDITION: PostCondition of PrepareDataUsage?
			if (!isReady || !(AsDataItem(di->GetCurrUltimateItem())->m_DataObject))
			{
				assert(SuspendTrigger::DidSuspend());
				os << "\nProcessing ...";
				return isReady;
			}

			assert(!SuspendTrigger::DidSuspend()); // PostCondition of PrepareDataUsage?

			const AbstrUnit* domain = di->GetAbstrDomainUnit(); 
			SizeT    n = domain->GetCount();
			SizeT   irr_n = n;
			tile_id tn = domain->GetNrTiles();
			table.AddRow("Count", AsString(n).c_str());
			if (tn != 1)
			{
				table.AddRow("#Tiles", AsString(tn).c_str());
			}
			if (!domain->IsCovered())
			{
				auto range_item = domain->GetCurrRangeItem();
				auto si = AsUnit(range_item)->GetTiledRangeData();
				irr_n = si->GetElemCount();
			}

			if (!n)
				return isReady;

			if (irr_n != n)
			{
				table.AddRow("Count inside tiles", AsString(irr_n).c_str());
				table.AddRow("Count outside tiles", AsString(n - irr_n).c_str());
			}

			DataReadLock lock(di);
			SizeT nrPoints = 0;
			if (!vt->IsNumeric() && vt->GetValueClassID() != ValueClassID::VT_Bool)
			{
				if (vt->GetNrDims() == 2)
				{
					point64_accumulator accu;
					AccumulatePointData(accu, di);
					WritePointAccuData(table, accu, di);
					d = accu.nrValues;
					nrPoints = accu.nrPoints;
				}
				else
					d = n - di->GetRefObj()->GetNrNulls();
			}
			else
			{
				f64_accumulator accu;
				AccumulateNumericData(accu, binCounts, di);
				WriteNumericAccuData(table, accu, di);
				d = accu.d;
			}

			if (irr_n == n)
				table.AddRow("#Nulls", AsString(irr_n - d).c_str());
			else
				table.AddRow("#Nulls inside tiles", AsString(irr_n - d).c_str());
			table.AddRow("#Values", AsString(d).c_str());

			if (di->GetAbstrValuesUnit()->GetValueType()->GetValueClassID() == ValueClassID::VT_String)
			{
				auto da = const_array_cast<SharedStr>(di)->GetDataRead();
				table.AddRow("# actual   bytes in strings", AsString(da.get_sa().actual_data_size()).c_str());
				table.AddRow("# reserved bytes in strings", AsString(da.get_sa().data_size()).c_str());
			}
			if (di->GetValueComposition() != ValueComposition::Single)
			{
				visit<typelists::sequence_fields>(di->GetAbstrValuesUnit(),
					[&table, di, &os, nrPoints] <typename V> (const Unit<V>*)
				{
					using a_seq = sequence_traits<V>::container_type;

					auto da = const_array_cast<a_seq>(di)->GetDataRead();
					if (nrPoints)
						table.AddRow("# counted  elements in values", AsString(nrPoints).c_str());
					table.AddRow("# actual   elements in values", AsString(da.get_sa().actual_data_size()).c_str());
					table.AddRow("# reserved elements in values", AsString(da.get_sa().data_size()).c_str());
				}
				);
			}
		}
		if (d)
			WriteBinData(os, binCounts, di);
		return isReady;
	}
	catch (...)
	{
		auto err = catchException(true);
		if (err)
			os << *err;
	}
	return true;
}

CLC_CALL CharPtr DMS_CONV DMS_NumericDataItem_GetStatistics(const TreeItem* item, bool* donePtr)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(item, TreeItem::GetStaticClass(), "DMS_NumericDataItem_GetStatistics");

		dms_assert(IsMetaThread());
		static ExternalVectorOutStreamBuff::VectorType statisticsBuffer;
		static const TreeItem*  s_LastItem = 0;
		static TimeStamp        s_LastChangeTS=0;

		#if defined(MG_DEBUG)
				assert(!DebugOnlyLock::IsLocked()); // PRECONDITION
		#endif
		bool      itemIsValid = TreeItem_GetProgressState(item) >= NC2_Validated;
		TimeStamp itemLastChangeTS = item->GetLastChangeTS();

		assert(item->HasInterest()); // PRECONDITION

		assert(!donePtr || *donePtr);
		bool cacheReady = item == s_LastItem && (itemIsValid ? itemLastChangeTS : TimeStamp(0)) == s_LastChangeTS;
		while (!cacheReady)
		{
			s_LastItem = nullptr; // invalidate cache contents during processing	

			cacheReady = NumericDataItem_GetStatistics(item, statisticsBuffer);
			statisticsBuffer.push_back('\0');
			if (cacheReady)
			{
				s_LastItem = item;
				s_LastChangeTS = itemLastChangeTS;
				break;
			}
			if (donePtr)
			{
				*donePtr = false;
				break;
			}
		}

		return &*statisticsBuffer.begin();

	DMS_CALL_END
 	return "Error during the derivation of statistics";
}
