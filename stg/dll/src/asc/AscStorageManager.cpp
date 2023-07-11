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
#include "StoragePCH.h"
#pragma hdrstop

// *****************************************************************************
//
// Implementations of - AsciiStorageOutStreamBuff
//                    - AsciiStorageInpStreamBuff
//                    - AsciiStorageManager
//
// *****************************************************************************

#include "AscStorageManager.h"
#include "asc/AsciiImp.h"

#include "act/UpdateMark.h"
#include "act/TriggerOperator.h"
#include "dbg/debug.h"
#include "dbg/SeverityType.h"
#include "mci/ValueClass.h"  
#include "mci/ValueClassID.h"  
#include "ser/BaseStreamBuff.h"  
#include "utl/Environment.h"
#include "utl/mySPrintF.h"

#include "AbstrDataItem.h"
#include "AbstrUnit.h"
#include "DataItemClass.h"
#include "DataStoreManagerCaller.h"
#include "Param.h"
#include "Projection.h"
#include "TreeItemContextHandle.h"
#include "UnitClass.h"
#include "UnitProcessor.h"
#include "stg/StorageClass.h"

#include "ViewPortInfoEx.h"

//==================================================================================

struct GridDataReaderBase : UnitProcessor
{
	template <typename E>
	void VisitImpl(const Unit<E>* inviter) const
	{
		dms_assert(m_VPI);
		dms_assert(m_Res);

		auto resData = mutable_array_cast<E>(m_Res)->GetDataWrite(m_TileID);
		m_Success = m_Imp.ReadCells<E>(resData.begin(), *m_VPI);
	}
public:
	mutable AsciiImp    m_Imp;
	const StgViewPortInfo* m_VPI = nullptr; 
	AbstrDataObject*    m_Res = nullptr;
	tile_id             m_TileID = no_tile;
	mutable bool        m_Success = false;
};

struct GridDataReader :  boost::mpl::fold<typelists::numerics, GridDataReaderBase, VisitorImpl<Unit<boost::mpl::_2>, boost::mpl::_1> >::type
{};

bool AsciiStorageManager::ReadGridData(const StgViewPortInfo& vpi, AbstrDataItem* adi, AbstrDataObject* ado, tile_id t) 
{
	if (!vpi.IsNonScaling())
		throwItemError("Reading asciigrid for a scaled grid not implemented");
	// Pass a stream object
	auto sfwa = DSM::GetSafeFileWriterArray();
	if (!sfwa)
		return false;
	GridDataReader reader;
	if (! reader.m_Imp.OpenForRead(GetNameStr(), sfwa.get()) )
		throwItemError("cannot open file for reading");

	reader.m_VPI    = &vpi;
	reader.m_Res    =  ado;
	reader.m_TileID = t;

	adi->GetAbstrValuesUnit()->InviteUnitProcessor(reader);
	bool result = reader.m_Success;
	if (!result && !SuspendTrigger::DidSuspend())
		throwItemError("Read Error");
	return result;
}

void AsciiStorageManager::ReadGridCounts(const StgViewPortInfo& vpi, AbstrDataObject* borrowedReadResultHolder, tile_id t)
{
	DBG_START("AsciiStorageManager", "ReadGridCounts", false);
	auto sfwa = DSM::GetSafeFileWriterArray(); MG_CHECK(sfwa);

	AsciiImp imp;
	// Pass a stream object
	if (! imp.OpenForRead(GetNameStr(), sfwa.get()) )
		throwItemError("cannot open file for reading");

	const ValueClass* streamType = GetStreamType(borrowedReadResultHolder);
	dms_assert(streamType);

	// Pick up data type
	UInt32 bitsPerPixel = streamType->GetBitSize();

	// read complete rows in one go
	switch (bitsPerPixel)
	{
		case  8: imp.CountData<UInt8 >(vpi, reinterpret_cast<UInt8 *>( borrowedReadResultHolder->GetDataWriteBegin(t).get_ptr() )); return;
		case 16: imp.CountData<UInt16>(vpi, reinterpret_cast<UInt16*>( borrowedReadResultHolder->GetDataWriteBegin(t).get_ptr() )); return;
		case 32: imp.CountData<UInt32>(vpi, reinterpret_cast<UInt32*>( borrowedReadResultHolder->GetDataWriteBegin(t).get_ptr() )); return;
	}
	throwErrorF("ReadGridCounts", "Cannot count AsciiGrid cells as this type of elements");
}

bool AsciiStorageManager::ReadDataItem(StorageMetaInfoPtr smi, AbstrDataObject* borrowedReadResultHolder, tile_id t)
{
	assert( DoesExist(smi->StorageHolder()) );
	//dms_assert(t == no_tile);

	SuspendTrigger::FencedBlocker suspendingInputStreams_NYI;
	if (SuspendTrigger::MustSuspend())
		return false;

	// Collect zoom info
	const GridStorageMetaInfo* gbr = debug_cast<const GridStorageMetaInfo*>(smi.get());
	ViewPortInfoEx vpi = gbr->m_VPIP.value().GetViewportInfoEx(t);
	AbstrDataItem* adi = smi->CurrWD();
	vpi.SetWritability(adi);

	assert(adi->GetDataObjLockCount() < 0);
	if (vpi.GetCountColor() != -1)
		ReadGridCounts(vpi, borrowedReadResultHolder, t);
	else
		ReadGridData(vpi, adi, borrowedReadResultHolder, t);
	return true;
}

//==================================================================================

struct GridDataWriterBase : UnitProcessor
{
	template <typename E>
	void VisitImpl(const Unit<E>* inviter) const
	{
		dms_assert(m_ADI);

		auto data = const_array_cast<E>(m_ADI)->GetDataRead();
		auto sfwa = DSM::GetSafeFileWriterArray();
		if (!sfwa)
			return;
		m_Success = m_Imp.WriteCells<E>(m_FileNameStr, sfwa.get(), data.begin(), m_NrElem);
	}
public:
	mutable AsciiImp     m_Imp;
	SharedStr            m_FileNameStr;
	const AbstrDataItem* m_ADI = 0;
	UInt32               m_NrElem = -1;
	mutable bool         m_Success = false;
};

struct GridDataWriter :  boost::mpl::fold<typelists::numerics, GridDataWriterBase, VisitorImpl<Unit<boost::mpl::_2>, boost::mpl::_1> >::type
{};

Float64 CalcScaledMin(Int32 first, Int32 second, Float64 factor)
{
	return Min<Float64>(first*factor, second*factor);
}

bool AsciiStorageManager::WriteDataItem(StorageMetaInfoPtr&& smiHandle)
{
	auto smi = smiHandle.get();
	StorageWriteHandle hnd(std::move(smiHandle));

	dms_assert( ! m_IsReadOnly ); // guaranteed by AbstrStorageManager
	dms_assert( IsOpen());        // guaranteed by AbstrStorageManager
	dms_assert(smi->CurrRD()->GetDataRefLockCount());

	const AbstrUnit* domainUnit = smi->CurrRD()->GetAbstrDomainUnit();
	dms_assert(domainUnit);

	GridDataWriter writer;

	writer.m_Imp.SetNrOfRows(domainUnit->GetDimSize(0));
	writer.m_Imp.SetNrOfCols(domainUnit->GetDimSize(1));

	// Retrieve real gridsize, and check for conflict
	const UnitProjection* p = domainUnit->GetProjection();
	dms_assert(p);
	if (Abs(p->Factor().Row()) != Abs(p->Factor().Col()))
		domainUnit->throwItemErrorF("AsciiStorageManager %s cannot save non-square grids, but grid-domain '%s' has rowSize %lf and colSize %lf",
			GetNameStr(), domainUnit->GetName(), p->Factor().Row(), p->Factor().Col());

	IRect gridRect = domainUnit->GetRangeAsIRect();
	Float64 unitYLL = CalcScaledMin(gridRect.first.Row(), gridRect.second.Row(), p ? p->Factor().Row() : 1.0); if (p) unitYLL += p->Offset().Row();
	Float64 unitXLL = CalcScaledMin(gridRect.first.Col(), gridRect.second.Col(), p ? p->Factor().Col() : 1.0); if (p) unitXLL += p->Offset().Col();


	writer.m_Imp.SetXLLCorner(unitXLL);
	writer.m_Imp.SetYLLCorner(unitYLL);
	writer.m_Imp.SetCellSize(Abs(p->Factor().Row()));

	writer.m_FileNameStr = GetNameStr();
	writer.m_ADI =  smi->CurrRD();
	writer.m_NrElem = smi->CurrRD()->GetAbstrDomainUnit()->GetCount();
	dms_assert(writer.m_NrElem == writer.m_Imp.NrOfCells()); // was set like this before

	smi->CurrRD()->GetAbstrValuesUnit()->InviteUnitProcessor(writer);
	if (writer.m_Success)
		return true;
	throwItemError("Write Error");
}

bool AsciiStorageManager::ReadUnitRange(const StorageMetaInfo& smi) const
{
	AbstrUnit* gridDataDomain = smi.CurrWU();
	dms_assert(gridDataDomain);
	if (gridDataDomain->GetNrDimensions() != 2)
		return false;

	auto sfwa = DSM::GetSafeFileWriterArray();
	if (!sfwa)
		return false;

	AsciiImp imp;
	if (!imp.OpenForRead(GetNameStr(), sfwa.get()))
		return false;

	dms_assert(gridDataDomain);

	UpdateMarker::ChangeSourceLock changeStamp( gridDataDomain, "ReadUnitRange");
	gridDataDomain->SetRangeAsIPoint(0, 0, imp.NrOfRows(), imp.NrOfCols());

	return true;
}

// Insert params into client tree
void AsciiStorageManager::DoUpdateTree(const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const
{
	AbstrGridStorageManager::DoUpdateTree(storageHolder, curr, sm);

	dms_assert(storageHolder);
	if (storageHolder != curr)
		return;
	if (curr->IsStorable() && curr->HasCalculator())
		return;
	if (!DoesExist(storageHolder))
		return;

	dms_assert(storageHolder == curr);
	AbstrUnit* gridDataDomain = GetGridDataDomainRW(curr);

	const AbstrUnit* uBase = FindProjectionBase(storageHolder, gridDataDomain);
	if (!uBase)
		return;

	StorageReadHandle storageHandle(this, storageHolder, curr, StorageAction::updatetree);
	auto sfwa = DSM::GetSafeFileWriterArray();
	if (!sfwa)
		return;
	AsciiImp imp;
	if (!imp.OpenForRead(GetNameStr(), sfwa.get()))
		return;

	DPoint offset, factor;
	factor.Row() = -imp.CellSize();
	factor.Col() =  imp.CellSize();
	offset.Row() =  imp.YLLCorner() + imp.CellSize() * imp.NrOfRows();
	offset.Col() =  imp.XLLCorner();

	gridDataDomain->SetProjection(new UnitProjection(uBase, offset, factor));
}

// Register
IMPL_DYNC_STORAGECLASS(AsciiStorageManager, "asc")
