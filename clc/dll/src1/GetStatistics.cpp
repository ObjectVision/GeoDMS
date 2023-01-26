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

void AccumulateData(FormattedOutStream& os, f64_accumulator& accu, bin_count_type& binCounts, const AbstrDataItem* di)
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
			[&os, di, t, &accu] <typename V> (const Unit<V>*)
			{
				auto tileData = const_array_cast<V>(di)->GetTile(t);
#if defined(MG_DEBUG)
				os << mySSPrintF("\nTile %d starts at address %x and size %d", t, SizeT(data_ptr_traits<typename sequence_traits<V>::cseq_t>::begin(tileData)), tileData.size());
#endif // defined(MG_DEBUG)
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

void WriteAccuData(FormattedOutStream& os, f64_accumulator& accu, const AbstrDataItem* di)
{
	auto vu = di->GetAbstrValuesUnit();
	auto metricPtr = vu->GetMetric();
	os << "\nFormal Range " << vu->GetName().c_str() << ":" << di->GetAbstrValuesUnit()->GetRangeAsStr();
	if (metricPtr)
		os << metricPtr->AsString(FormattingFlags::ThousandSeparator);

	os << "\nMaximum  : " << accu.max;
	os << "\nMinimum  : " << accu.min;
	os << "\nSum      : " << accu.s;

	if (accu.d) // there is actual data?
	{
		Float64
			mean = accu.s / accu.d,
			sumXtimesErr = Max<Float64>(accu.s2 - accu.s * mean, 0.0);


		os << "\nAverage  : " << mean;
		os << "\nVariance : " << sumXtimesErr / accu.d;

		if (accu.d > 1)
			os << "\nStdDev   : " << sqrt(sumXtimesErr / (accu.d - 1));
	}
}
void WriteBinData(FormattedOutStream& os, const bin_count_type& binCounts, const AbstrDataItem* di)
{
	if (binCounts.size())
	{
		os << "\n\nValue counts:";
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

				os << "\n" << DisplayValue(vu, i, useMetric, ipHolder, maxLen, guiLock) << ": " << binCounts[i];
		}
	}
	else
	{
		os << "\n\nTo see all unique values and their frequency, you can open this attribute in a TableView and press the GroupBy button in the toolbar.";
	}
}
CLC1_CALL CharPtr DMS_CONV DMS_NumericDataItem_GetStatistics(const TreeItem* item, bool* donePtr)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(item, TreeItem::GetStaticClass(), "DMS_NumericDataItem_GetStatistics");

		dms_assert(IsMainThread());
		static ExternalVectorOutStreamBuff::VectorType statisticsBuffer;
		static const TreeItem*  s_LastItem = 0;
		static TimeStamp        s_LastChangeTS=0;

		dms_assert(!SuspendTrigger::DidSuspend()); // Precondition?

		bool      itemIsValid      = DMS_TreeItem_GetProgressState(item) >= PS_Validated;
		TimeStamp itemLastChangeTS = item->GetLastChangeTS();

		dms_assert(item->HasInterest()); // PRECONDITION

		if (item != s_LastItem || (itemIsValid ? itemLastChangeTS : TimeStamp(0)) != s_LastChangeTS)
		{
			bool bSuspended = false;
			s_LastItem = 0; // invalidate cache contents during processing	
			vector_clear(statisticsBuffer);

			ExternalVectorOutStreamBuff outStreamBuff(statisticsBuffer);
			FormattedOutStream os(&outStreamBuff, FormattingFlags::ThousandSeparator);

			try {

				//os << "Statistics for " << item->GetFullName() << ":\n";

				if (item->InTemplate())
				{
					os << 
						"\nNot available since the DataItem is part of a calculation scheme template."
						"\nGo to its instantiations to see actual data";

					goto finally;
				}

				SharedDataItemInterestPtr di = AsDynamicDataItem(item);
				if (!di)
				{
					os << "Not available since this is not a DataItem";
					goto finally;
				}
				dms_assert(di->Was(PS_MetaInfo)); // PRECONDITION
				SharedUnitInterestPtr vu = SharedPtr<const AbstrUnit>(di->GetAbstrValuesUnit());
				dms_assert(vu);
				dms_assert(vu->Was(PS_MetaInfo)); // follows from PRECONDITION
				bool isReady = vu->PrepareData(); // GetRange needed later
				auto vt = di->GetAbstrValuesUnit()->GetValueType();
				
				SharedStr metricStr = vu->GetCurrMetricStr(os.GetFormattingFlags());
				if (!metricStr.empty())
				os << "\nValuesMetric :" << metricStr;
				os << "\nValuesType   :" << vt->GetID();

				dms_assert(!SuspendTrigger::DidSuspend() || !isReady);
				if (isReady) isReady = di->PrepareData();
				if (!isReady && !donePtr && !di->IsFailed(FR_Data))
				{
					SuspendTrigger::Resume();
					vu->PrepareDataUsage(DrlType::Certain);
					di->PrepareDataUsage(DrlType::Certain);
					auto oc = GetOperationContext(di->GetCurrUltimateItem());
					if (oc)
						oc->Join();
					isReady = true;
				}
				if (di->IsFailed(FR_Data))
				{
					auto fr = di->GetFailReason();
					if (fr)
						os << "\nProcessing statistics failed because:\n\n" << *fr;
					goto finally;
				}
				dms_assert(!(isReady && SuspendTrigger::DidSuspend())); // PRECONDITION: PostCondition of PrepareDataUsage?
				if (!isReady || !(AsDataItem(di->GetCurrUltimateItem())->m_DataObject))
				{
					dms_assert(donePtr); // !donePtr implies lock or failure
					dms_assert(SuspendTrigger::DidSuspend());
					os << "\nProcessing ...";
					bSuspended = true;  // keep cache contents invalidated
					if(donePtr)         // should always be the case
						*donePtr = false;
					goto finally;
				}

				dms_assert(!SuspendTrigger::DidSuspend()); // PostCondition of PrepareDataUsage?

				SizeT n = di->GetAbstrDomainUnit()->GetCount();;
				const AbstrUnit* domain = di->GetAbstrDomainUnit(); tile_id tn = domain->GetNrTiles();
				os << "\n\nCount    : " << n;
				if (tn != 1)
					os << "\n#Tiles   : " << tn;

				if (!n)
					goto finally;

				bin_count_type binCounts;

				SizeT d=0;
				DataReadLock lock(di);
				if (!vt->IsNumeric() && vt->GetValueClassID() != VT_Bool)
				{
					d = n - di->GetRefObj()->GetNrNulls();
				}
				else
				{	
					f64_accumulator accu;
					AccumulateData(os, accu, binCounts, di);
					WriteAccuData(os, accu, di);
					d = accu.d;
				}
				os << "\nNr Nulls : " << (n - d);
				os << "\nNr Values: " << d;

				if (di->GetAbstrValuesUnit()->GetValueType()->GetValueClassID() == VT_String)
				{
					auto da = const_array_cast<SharedStr>(di)->GetDataRead();
					os << "\nNumber of actual   bytes in elements: " << da.get_sa().actual_data_size();
					os << "\nNumber of reserved bytes in elements: " << da.get_sa().data_size();
				}
				if (di->GetValueComposition() != ValueComposition::Single)
				{
					visit<typelists::sequence_fields>(di->GetAbstrValuesUnit(),
						[di, &os] <typename V> (const Unit<V>*) 
						{
							using a_seq = sequence_traits<V>::container_type;

							auto da = const_array_cast<a_seq>(di)->GetDataRead();
							os << "\nNumber of actual   bytes in elements: " << da.get_sa().actual_data_size();
							os << "\nNumber of reserved bytes in elements: " << da.get_sa().data_size();
						}
					);
				}
				if (d)
					WriteBinData(os, binCounts, di);
			}
			catch (...)
			{
				auto err = catchException(true);
				if (err)
					os << *err;
			}
		finally:
			os << "\n" << char(0);

			if (!bSuspended)
			{
				s_LastItem     = item;
				s_LastChangeTS = itemLastChangeTS;
			}
		}
		return &*statisticsBuffer.begin();

	DMS_CALL_END
 	return "Error during the derivation of statistics";
}
