// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPCH.h"
#pragma hdrstop

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

using bin_count_type = std::vector<SizeT>;

void AccumulateData(f64_accumulator& accu, bin_count_type& binCounts, const AbstrDataItem* di)
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
		ReadableTileLock tileLck(di->GetRefObj(), t);


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

void WriteAccuData(XML_Table& table, f64_accumulator& accu, const AbstrDataItem* di)
{
	auto vu = di->GetAbstrValuesUnit();
	auto metricPtr = vu->GetMetric();
	table.NameValueRow(mySSPrintF("Formal Range %s",  vu->GetName().c_str()).c_str(), di->GetAbstrValuesUnit()->GetRangeAsStr().c_str());
	if (metricPtr)
		table.NameValueRow("Metric Units", metricPtr->AsString(FormattingFlags::ThousandSeparator).c_str());

	table.NameValueRow("Maximum", AsString(accu.max).c_str());
	table.NameValueRow("Minimum", AsString(accu.min).c_str());
	table.NameValueRow("Sum", AsString(accu.s).c_str());

	if (accu.d) // there is actual data?
	{
		Float64
			mean = accu.s / accu.d,
			sumXtimesErr = Max<Float64>(accu.s2 - accu.s * mean, 0.0);


		table.NameValueRow("Average", AsString(mean).c_str());
		table.NameValueRow("Variance", AsString(sumXtimesErr / accu.d).c_str());

		if (accu.d > 1)
			table.NameValueRow("StdDev", AsString(sqrt(sumXtimesErr / (accu.d - 1))).c_str());
	}
}

void WriteBinData(OutStreamBase& os, const bin_count_type& binCounts, const AbstrDataItem* di)
{
	if (binCounts.size())
	{
		XML_Table table(os);
		table.NameValueRow("Value", "Count");
		auto vu = di->GetAbstrValuesUnit();
//		auto valuesRange = vu->GetRangeAsFloat64();
//		int lb = valuesRange.first;
		bool useMetric = false;
		SharedDataItemInterestPtr ipHolder;
		streamsize_t maxLen = 33;
		GuiReadLock guiLock;

		for (Int32 i = 0; i != binCounts.size(); ++i)
		{
			if (binCounts[i])
				table.NameValueRow(DisplayValue(vu, i, useMetric, ipHolder, maxLen, guiLock).c_str()
					, AsString(binCounts[i]).c_str()
				);
		}
	}
	else
	{
		os << CharPtr(u8"\n\nTo see all unique values and their frequency, you can open this attribute in a TableView, select the column header, and press the ∑ (GroupBy) button in the toolbar.");
	}
}

#include "xml/XmlTreeOut.h"

CLC_CALL bool NumericDataItem_GetStatistics(const TreeItem* item, vos_buffer_type& statisticsBuffer)
{
	assert(!SuspendTrigger::DidSuspend()); // Precondition?
	assert(item->HasInterest()); // PRECONDITION

	statisticsBuffer.clear();

	ExternalVectorOutStreamBuff outStreamBuff(statisticsBuffer);
//	FormattedOutStream os(&outStreamBuff, FormattingFlags::ThousandSeparator);

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
		assert(di->Was(PS_MetaInfo)); // PRECONDITION
		SharedUnitInterestPtr vu = SharedPtr<const AbstrUnit>(di->GetAbstrValuesUnit());
		assert(vu);
		assert(vu->Was(PS_MetaInfo)); // follows from PRECONDITION
		bool isReady = vu->PrepareData(); // GetRange needed later

		SizeT d = 0;
		bin_count_type binCounts;

		auto vt = di->GetAbstrValuesUnit()->GetValueType();
		{
			XML_Table table(os);
			SharedStr metricStr = vu->GetCurrMetricStr(os.GetFormattingFlags());
			if (!metricStr.empty())
				table.NameValueRow("ValuesMetric", metricStr.c_str());
			table.NameValueRow("ValuesType", vt->GetID().GetStr().c_str());

			assert(!SuspendTrigger::DidSuspend() || !isReady);
			if (isReady)
				isReady = di->PrepareData();

			if (di->IsFailed(FR_Data))
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

			SizeT n = di->GetAbstrDomainUnit()->GetCount();;
			const AbstrUnit* domain = di->GetAbstrDomainUnit(); tile_id tn = domain->GetNrTiles();
			table.NameValueRow("Count", AsString(n).c_str());
			if (tn != 1)
				table.NameValueRow("#Tiles", AsString(tn).c_str());

			if (!n)
				return isReady;

			DataReadLock lock(di);
			if (!vt->IsNumeric() && vt->GetValueClassID() != ValueClassID::VT_Bool)
			{
				d = n - di->GetRefObj()->GetNrNulls();
			}
			else
			{
				f64_accumulator accu;
				AccumulateData(accu, binCounts, di);
				WriteAccuData(table, accu, di);
				d = accu.d;
			}
			table.NameValueRow("#Nulls", AsString(n - d).c_str());
			table.NameValueRow("#Values", AsString(d).c_str());

			if (di->GetAbstrValuesUnit()->GetValueType()->GetValueClassID() == ValueClassID::VT_String)
			{
				auto da = const_array_cast<SharedStr>(di)->GetDataRead();
				table.NameValueRow("# actual   bytes in elements", AsString(da.get_sa().actual_data_size()).c_str());
				table.NameValueRow("# reserved bytes in elements", AsString(da.get_sa().data_size()).c_str());
			}
			if (di->GetValueComposition() != ValueComposition::Single)
			{
				visit<typelists::sequence_fields>(di->GetAbstrValuesUnit(),
					[&table, di, &os] <typename V> (const Unit<V>*)
				{
					using a_seq = sequence_traits<V>::container_type;

					auto da = const_array_cast<a_seq>(di)->GetDataRead();
					table.NameValueRow("# actual   bytes in elements", AsString(da.get_sa().actual_data_size()).c_str());
					table.NameValueRow("# reserved bytes in elements", AsString(da.get_sa().data_size()).c_str());
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

		dms_assert(IsMainThread());
		static ExternalVectorOutStreamBuff::VectorType statisticsBuffer;
		static const TreeItem*  s_LastItem = 0;
		static TimeStamp        s_LastChangeTS=0;

		bool      itemIsValid = DMS_TreeItem_GetProgressState(item) >= PS_Validated;
		TimeStamp itemLastChangeTS = item->GetLastChangeTS();

		assert(item->HasInterest()); // PRECONDITION

		assert(!donePtr || *donePtr);
		bool cacheReady = item == s_LastItem && (itemIsValid ? itemLastChangeTS : TimeStamp(0)) == s_LastChangeTS;
		while (!cacheReady)
		{
			s_LastItem = nullptr; // invalidate cache contents during processing	

			cacheReady = NumericDataItem_GetStatistics(item, statisticsBuffer);
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
