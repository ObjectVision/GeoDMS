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

#include "ShvDllPch.h"

#include "GridLayer.h"

#include "act/InvalidationBlock.h"
#include "act/TriggerOperator.h"
#include "dbg/Debug.h"
#include "geo/Conversions.h"
#include "geo/IsInside.h"
#include "geo/Point.h"
#include "geo/PointOrder.h"
#include "geo/RangeIndex.h"
#include "mci/CompositeCast.h"
#include "mci/ValueClassID.h"
#include "mci/ValueClass.h"
#include "utl/IncrementalLock.h"

#include "AbstrUnit.h"
#include "DataArray.h"
#include "GridStorageManager.h"

#include "StgBase.h"

#include "SpatialInterface.h"

#include "AbstrController.h"
#include "Cmds.h"
#include "Clipboard.h"
#include "CounterStacks.h"
#include "GraphVisitor.h"
#include "GridCoord.h"
#include "GridDrawer.h"
#include "IndexCollector.h"
#include "LayerClass.h"
#include "MouseEventDispatcher.h"
#include "RegionTower.h"
#include "SelCaret.h"
#include "Theme.h"
#include "ThemeReadLocks.h"
#include "ViewPort.h"

const UInt32 FOCUS_BORDER_SIZE       = 12;
const UInt32 FOCUS_BORDER_FRAMEWIDTH =  2;
const  Int32 FOCUS_BORDER_SIZE1      = FOCUS_BORDER_SIZE + FOCUS_BORDER_FRAMEWIDTH;


const UINT CF_CELLVALUES = CF_PRIVATEFIRST;

//----------------------------------------------------------------------
// struct: PasteHandler
//----------------------------------------------------------------------

struct PasteHandler
{
	PasteHandler( HANDLE selValuesData )
//		:	m_SelValuesData(selValuesData)
//		,	m_SelValues    (reinterpret_cast<SelValuesData*>(m_SelValuesData.GetDataPtr()))
	{
		GlobalLockHandle selValuesLock(selValuesData);
		UInt32 size = reinterpret_cast<SelValuesData*>(selValuesLock.GetDataPtr())->m_Size;
		m_SelValuesData.reset( new BYTE[size] );
		const BYTE* dataPtr = reinterpret_cast<BYTE*>(selValuesLock.GetDataPtr());
		fast_copy(dataPtr, dataPtr + size, m_SelValuesData.begin());

		m_SelValues  = reinterpret_cast<SelValuesData*>(m_SelValuesData.begin());
	}

	SelValuesData* GetSelValues() { return m_SelValues; }
	bool           IsHidden    () const { return m_SelValues->IsHidden(); }

private:
//	GlobalLockHandle m_SelValuesData;
	OwningPtrSizedArray<BYTE>  m_SelValuesData;
	SelValuesData*             m_SelValues;
};

//----------------------------------------------------------------------
// class  : GridLayer
//----------------------------------------------------------------------

GridLayer::GridLayer(GraphicObject* owner)
	:	GridLayerBase(owner, GetStaticClass())
{
	m_State.Set(GOF_Exclusive);
}

GridLayer::~GridLayer()
{}

void GridLayer::SelectPoint(CrdPoint pnt, EventID eventID)
{
	CrdTransformation tr = GetGeoTransformation(); // grid to world transformer
	IRect gridRect = GetGeoCrdUnit()->GetRangeAsIRect();

	InvalidationBlock lock1(this); // we guarantee to invalidate relevant part of GridLayer ourselves
	bool changed;
	if ( IsIncluding(tr.Apply( Convert<CrdRect>(gridRect) ), pnt) )
	{
		if (!IsDataReady(GetActiveEntity()->GetCurrRangeItem()))
		{
			reportD(SeverityTypeID::ST_MajorTrace, "GridLayer::SelectPoint cancelled... ActiveEntity not ready.");
			MessageBeep(MB_ICONEXCLAMATION);
			return;
		}
		auto selTheme = CreateSelectionsTheme();
		if (!selTheme)
			return;
		DataWriteLock writeLock(
			const_cast<AbstrDataItem*>(selTheme->GetThemeAttr()),
			CompoundWriteType(eventID)
		);
		IPoint gridLoc = RoundDown<4>( tr.Reverse(pnt) );
		SizeT gridIdx = Range_GetIndex_naked(gridRect, gridLoc);

		changed = SelectFeatureIndex(writeLock, gridIdx, eventID);
		if (changed)
			writeLock.Commit();
	}
	else
		changed = SetFocusEntityIndex( UNDEFINED_VALUE(SizeT), eventID & EID_LBUTTONDBLCLK );
	if (changed)
		lock1.ProcessChange();
}

template <typename T>
struct AbstrRowProcessor {
	using iterator = typename DataArray<T>::iterator;
	virtual void operator ()(Int32 r, Int32 c1, Int32 c2, iterator i, const CrdTransformation& tr) const =0;
};

template <typename T>
void GridLayer::SelectRegion(CrdRect worldRect, const AbstrRowProcessor<T>& rowProcessor, AbstrDataItem* selAttr, EventID eventID)
{
	if (HasEntityIndex())
		throwErrorD("SelectRegion", "Cannot make a region selection on an indirect grid");

	CrdTransformation tr = GetGeoTransformation(); // grid to world transformer
	IRect gridRect = GetGeoCrdUnit()->GetRangeAsIRect();
	if (! IsIntersecting(tr.Apply( Convert<CrdRect>(gridRect) ), worldRect) )
		return;

	IRect selectRect = gridRect & Round<4>( tr.Reverse( worldRect ) );
	if (selectRect.empty())
		return;

	InvalidationBlock viewChangeLock(this);
	InvalidationBlock dataChangeLock(GetEditTheme()->GetThemeAttr()); // REMOVE, MOVE TO DataWriteLock as a Generic facility
	{
		DataWriteLock lock(selAttr, CompoundWriteType(eventID));
		{
			auto selData = mutable_array_cast<T>(lock)->GetDataWrite();

			auto i = selData.begin() + Range_GetIndex_naked(gridRect, shp2dms_order(IPoint(_Left(selectRect), _Top(selectRect))));
			UInt32 gridWidth = _Width(gridRect);
			for (Int32 r = _Top(selectRect); r != _Bottom(selectRect); ++r, i += gridWidth)
				rowProcessor(r, _Left(selectRect), _Right(selectRect), i, tr);
		}
		lock.Commit();
	}
	if (!IsCreateNewEvent(eventID) && !HasEntityIndex())
	{
		dataChangeLock.ProcessChange();
		viewChangeLock.ProcessChange();
		GRect borderExtents(-1, -1, 1, 1);
		InvalidateWorldRect(tr.Apply(Convert<CrdRect>(selectRect)), &borderExtents);
	}
}


template <bool V> struct SetConstOper { typedef SelectionID elem_t; void operator () (sequence_traits<elem_t>::reference selElem) const { selElem = V; } };
typedef SetConstOper< true> AddOper;
typedef SetConstOper<false> OffOper;

//struct OffOper { typedef SelectionID elem_t; void operator () (sequence_traits<elem_t>::reference selElem) const { selElem = false; } };

struct SetValueOper {
	typedef ClassID elem_t;

	SetValueOper(elem_t v) : m_Value(v) {}
	void operator () (sequence_traits<elem_t>::reference selElem) const 
	{
		selElem = m_Value; 
	}

private:
	elem_t m_Value;
};

template <typename Oper> 
struct RectRowProcessor : AbstrRowProcessor<typename Oper::elem_t>
{
	RectRowProcessor(Oper oper = Oper()) : m_Oper(oper)  {}

	using iterator = typename DataArray<typename Oper::elem_t>::iterator;
	void operator () (Int32 r, Int32 cb, Int32 ce, iterator i, const CrdTransformation& tr) const override
	{
		for (; cb != ce; ++cb, ++i)
			m_Oper(*i);
	}
private:
	Oper m_Oper;
};

void GridLayer::SelectRect(CrdRect worldRect, EventID eventID)
{
	ClassID currClassID;
	if (HasEditAttr() && IsDefined(currClassID = GetCurrClassID()))
		SelectRegion(worldRect, RectRowProcessor<SetValueOper>(SetValueOper(currClassID)), GetEditAttr(), EID_SHIFTKEY );
	else if (eventID & EID_CTRLKEY )
		SelectRegion(worldRect, RectRowProcessor<OffOper>(), GetEditAttr(), eventID );
	else
		SelectRegion(worldRect, RectRowProcessor<AddOper>(), GetEditAttr(), eventID );
}

template <typename Oper> 
struct CircleRowProcessor : AbstrRowProcessor<typename Oper::elem_t>
{
	CircleRowProcessor(const CrdPoint& pnt, CrdType radius, Oper oper = Oper()): m_Oper(oper), m_Pnt(pnt), m_Radius(radius) {}

	using iterator = typename DataArray<typename Oper::elem_t>::iterator;
	void operator ()(Int32 r, Int32 cb, Int32 ce, iterator i, const CrdTransformation& tr) const override
	{
		CrdType  radius2 = Sqr(m_Radius);
		CrdPoint pnt     = m_Pnt;
	
		for (; cb != ce; ++cb, ++i)
			if (SqrDist<CrdType>(tr.Apply(shp2dms_order(CrdPoint(cb+0.5, r+0.5))), pnt) < radius2)
				m_Oper(*i);
	}

private:
	CrdPoint m_Pnt;
	CrdType  m_Radius;
	Oper     m_Oper;
};

void GridLayer::SelectCircle(CrdPoint worldPnt, CrdType worldRadius, EventID eventID)
{
	CrdRect worldRect = Inflate(worldPnt, CrdPoint(worldRadius, worldRadius));

	ClassID currClassID;
	if (HasEditAttr() && IsDefined(currClassID = GetCurrClassID()))
		SelectRegion(worldRect, CircleRowProcessor<SetValueOper>(worldPnt, worldRadius, SetValueOper(currClassID)), GetEditAttr(), EID_SHIFTKEY );
	else if (eventID & EID_CTRLKEY )
		SelectRegion(worldRect, CircleRowProcessor<OffOper>(worldPnt, worldRadius), GetEditAttr(), eventID  );
	else
		SelectRegion(worldRect, CircleRowProcessor<AddOper>(worldPnt, worldRadius), GetEditAttr(), eventID  );

}

template <typename Oper> 
struct PolygonRowProcessor : AbstrRowProcessor<typename Oper::elem_t>
{
	PolygonRowProcessor(const CrdPoint* first, const CrdPoint* last, Oper oper = Oper()): m_Oper(oper), m_First(first), m_Last(last) {}

	using iterator = typename DataArray<typename Oper::elem_t>::iterator;
	void operator () (Int32 r, Int32 cb, Int32 ce, iterator i, const CrdTransformation& tr) const override
	{
		const CrdPoint* first = m_First;
		const CrdPoint* last  = m_Last;
	
		for (; cb != ce; ++cb, ++i)
			if (IsInside(first, last, tr.Apply(shp2dms_order(CrdPoint(cb+0.5, r+0.5)))))
				m_Oper(*i);
	}

private:
	const CrdPoint* m_First;
	const CrdPoint* m_Last;
	Oper            m_Oper;
};

void GridLayer::SelectPolygon(const CrdPoint* first, const CrdPoint* last, EventID eventID)
{
	CrdRect rect(first, last, false, true);
	ClassID currClassID;
	if (HasEditAttr() && IsDefined(currClassID = GetCurrClassID()))
		SelectRegion(rect, PolygonRowProcessor<SetValueOper>(first, last, SetValueOper(currClassID)), GetEditAttr(), EID_SHIFTKEY );
	else if (eventID & EID_CTRLKEY )
		SelectRegion(rect, PolygonRowProcessor<OffOper>(first, last), GetEditAttr(), eventID );
	else
		SelectRegion(rect, PolygonRowProcessor<AddOper>(first, last), GetEditAttr(), eventID );
}

typedef sequence_traits<Bool>::const_pointer CBoolIter;

template <typename T>
void assign_if(
	typename sequence_traits<T>::pointer b, 
	typename sequence_traits<T>::pointer e, 
	T v, 
	CBoolIter si,
	CBoolIter se)
{
	dms_assert(e-b == se - si);
	// PreProcessing
	for (; si.nr_elem(); ++b, ++si) 
	{
		if (b==e)
			return;
		if (Bool(*si)) 
			*b = v;
	}

	// PostProcessing
	const CBoolIter::block_type* blockPtr = si.data_begin();
	const CBoolIter::block_type* blockEnd = se.data_begin();
	for (; blockPtr != blockEnd; ++blockPtr)
		if (*blockPtr)
		{
			si = CBoolIter(blockPtr, SizeT(0));
			do {
				if (Bool(*si))
					*b = v;
				++b;
				++si;
			} while (si.nr_elem());
		}
		else
			b += sequence_traits<Bool>::const_pointer::nr_elem_per_block;

	si = CBoolIter(blockPtr	, SizeT(0));

	// PostProcessing
	for (; b != e; ++b, ++si) 
	{
		if (Bool(*si))
			*b = v;
	}
}

void GridLayer::AssignValues(sequence_traits<Bool>::cseq_t selData)
{
	// dms_assert(HasEditAttr()); // PRECONDITION, BUT MUTATING
	ClassID clsID = GetCurrClassID();
	AbstrDataItem* adi = const_cast<AbstrDataItem*>(GetActiveTheme()->GetThemeAttrSource());

	InvalidationBlock dataChangeLock(GetActiveTheme()->GetThemeAttr()); // REMOVE, MOVE TO DataWriteLock as a Generic facility
	DataWriteLock dwl(adi, DmsRwChangeType(false));

	auto editData = mutable_array_cast<ClassID>(dwl)->GetDataWrite();
	dms_assert(editData.size() == selData.size());
	assign_if<ClassID>(editData.begin(), editData.end(), clsID, selData.begin(), selData.end());

	dwl.Commit();
	dataChangeLock.ProcessChange();
}

void GridLayer::AssignSelValues()
{
	dms_assert(m_Themes[AN_Selections]); // PRECONDITION 
	const AbstrDataItem* selAttr= m_Themes[AN_Selections]->GetThemeAttrSource();
	dms_assert(selAttr);                // PRECONDITION

	PreparedDataReadLock drl(selAttr);
	AssignValues(composite_cast<const DataArray<Bool>*>(selAttr)->GetDataRead(no_tile));
}

void District(
	const AbstrDataItem* adi,
	const AbstrDataObject* ado,
	const UGridPoint& size,
	sequence_traits<Bool>::seq_t resData,
	const IPoint&  gridLoc,
	IRect&  changedRect
)
{
	switch (ado->GetValuesType()->GetValueClassID())
	{
		case VT_UInt32:
			MDL_DistrictUI32(
				UCUInt32Grid(
					size,
					const_array_cast<UInt32>(ado)->GetDataRead().begin()
				),
				resData,
				gridLoc,
				changedRect
			);
			return;
		case VT_Int32:
			MDL_DistrictUI32(
				UCUInt32Grid(
					size,
					reinterpret_cast<const UInt32*>(const_array_cast<Int32>(ado)->GetDataRead().begin())
				),
				resData,
				gridLoc,
				changedRect
			);
			return;
		case VT_UInt8:
			MDL_DistrictUI8(
				UCUInt8Grid(
					size,
					const_array_cast<UInt8>(ado)->GetDataRead().begin()
				),
				resData,
				gridLoc,
				changedRect
			);
			return;
		case VT_Int8:
			MDL_DistrictUI8(
				UCUInt8Grid(
					size,
					reinterpret_cast<const UInt8*>(const_array_cast<Int8>(ado)->GetDataRead().begin())
				),
				resData,
				gridLoc,
				changedRect
			);
			return;
		case VT_Bool:
			MDL_DistrictBool(
				UCBoolGrid(
					size,
					const_array_cast<Bool>(ado)->GetDataRead(no_tile).begin()
				),
				resData,
				gridLoc,
				changedRect
			);
			return;
	}
	adi->throwItemError("Districting operation is not implemented for values of this type");
}

void GridLayer::SelectDistrict(CrdPoint pnt, EventID eventID)
{
	DBG_START("GridLayer", "SelectDistrict", MG_DEBUG_DISTRICT);

	IRect gridRect = GetGeoCrdUnit()->GetRangeAsIRect();
	IPoint gridSize = Convert<IGridPoint>(Size(gridRect));
	IPoint gridLoc = RoundDown<4>( GetGeoTransformation().Reverse( pnt ) );

	dms_assert(IsIncluding(gridRect, gridLoc));

	DBG_TRACE(("gridLoc = %s", AsString(gridLoc).c_str()));
	DBG_TRACE(("gridIdx = %d", Range_GetIndex_naked(gridRect, gridLoc)));

	const AbstrDataItem* themeAttr = GetActiveTheme()->GetThemeAttr();
	      AbstrDataItem* selAttr   = GetEditAttr();


	InvalidationBlock viewChangeLock(this);
	InvalidationBlock dataChangeLock(GetEditTheme()->GetThemeAttr()); // REMOVE, MOVE TO DataWriteLock as a Generic facility

	IRect changedRect;
	auto dwlt = DmsRwChangeType(false);
	if (HasEditAttr() && IsDefined(GetCurrClassID()))
	{
		DataWriteLock dwl(selAttr, dwlt);

		AbstrDataObject* selObj = dwl.get();
		sequence_traits<Bool>::container_type  resVector(selObj->GetNrFeaturesNow());

		District(
			selAttr, selObj,
			gridSize,
			sequence_traits<Bool>::seq_t(resVector.begin(), resVector.size()),
			gridLoc,
			changedRect
		);
		AssignValues(sequence_traits<Bool>::cseq_t(resVector.begin(), resVector.size()));
		dwl.Commit();
	}
	else
	{
		dwlt = CompoundWriteType(eventID);
		PreparedDataReadLock  drl(themeAttr);
		DataWriteLock dwl(selAttr, dwlt);

		District(
			themeAttr, themeAttr->GetRefObj(),
			gridSize,
			mutable_array_cast<SelectionID>( dwl )->GetDataWrite(),
			gridLoc,
			changedRect
		);
		dwl.Commit();
	}
	changedRect.second += IPoint(1, 1);

	dataChangeLock.ProcessChange();
	viewChangeLock.ProcessChange();
	if (dwlt == DmsRwChangeType(true))
		InvalidateDraw();
	else if (!changedRect.empty())
	{
		GRect borderExtents(-1, -1, 1, 1);
		InvalidateWorldRect(GetGeoTransformation().Apply(Convert<CrdRect>(changedRect)), &borderExtents);
	}
}

IRect GridLayer::CalcSelectedGeoRect()  const
{
	IRect selectRect;
	if (m_Themes[AN_Selections])
	{
		auto selTheme = CreateSelectionsTheme();
		dms_assert(selTheme);
		const AbstrDataItem* selAttr = selTheme->GetThemeAttr();

		PreparedDataReadLock lock(selAttr);

		auto selData = const_array_cast<SelectionID>(selAttr)->GetDataRead();

		IRect gridRect = GetGeoCrdUnit()->GetRangeAsIRect();
		auto indexCollectorPtr = GetIndexCollector();

		auto sdb = selData.begin();

		Int32 c      = _Left (gridRect);
		Int32 cRight = _Right(gridRect);
		if (indexCollectorPtr)
		{
			SizeT i = 0;
			for (Int32 r = _Top(gridRect), re = _Bottom(gridRect); r != re; ++r)
			{
				while (c < cRight)
				{
					assert(i == Range_GetIndex_naked(gridRect, shp2dms_order(IPoint(c, r))));
					SizeT sdIndex = indexCollectorPtr->GetEntityIndex(i);
					if (SelectionID(sdb[sdIndex]))
						selectRect |= shp2dms_order(IPoint(c, r));
					++c;
					if (c >= selectRect.first.Col() && c < selectRect.second.Col())
						c = selectRect.second.Col();
				}
				assert(c == cRight);
				c -= _Width(gridRect);
			}
		}
		else
		{
			auto sdi = sdb;
			for (Int32 r = _Top(gridRect), re = _Bottom(gridRect); r != re; ++r)
			{
				while (c < cRight)
				{
					assert(Range_GetIndex_naked(gridRect, shp2dms_order(IPoint(c, r))) == sdi - sdb);
					if (sdi.nr_elem() || *sdi.data_begin())
					{
						if (SelectionID(*sdi))
							selectRect |= shp2dms_order(IPoint(c, r));
						++sdi;
						++c;
					}
					else
					{
						sdi.skip_full_block();
						c += sdi.nr_elem_per_block;
					}
					if (c >= selectRect.first.Col() && c < selectRect.second.Col())
					{
						sdi += (selectRect.second.Col() - c);
						c = selectRect.second.Col();
					}
				}
				assert(c >= cRight);
				c -= _Width(gridRect);
			}
		}

		if (!selectRect.empty())
		{
			++selectRect.second.Row();
			++selectRect.second.Col();
		}
	}
	return selectRect;
}

CrdRect GridLayer::CalcSelectedFullWorldRect()  const
{

	IRect selectRect = CalcSelectedGeoRect();

	if (selectRect.empty())
		return CrdRect();

	return AsWorldExtents(Convert<CrdRect>(selectRect), GetGeoCrdUnit());
}

CrdRect GetWorldExtents(const IRect& geoCrdRect, const UnitProjection* proj, feature_id featureIndex)
{
	dms_assert(IsDefined(featureIndex));

	IPoint gridLoc  = Range_GetValue_naked(geoCrdRect, featureIndex);

	return
		AsWorldExtents(
			Convert<CrdRect>(IRect(gridLoc, gridLoc+IPoint(1,1) )),
			proj
		);
}

CrdRect GridLayer::GetWorldExtents(feature_id featureIndex) const
{
	const AbstrUnit* geoCrdUnit = GetGeoCrdUnit();
	dms_assert(geoCrdUnit);
	return ::GetWorldExtents(geoCrdUnit->GetRangeAsIRect(), geoCrdUnit->GetProjection(), featureIndex);
}

void GridLayer::InvalidateFeature(SizeT featureIndex)
{
	dms_assert(IsDefined(featureIndex));

	Int32 focusSize = RoundUp<4>(FOCUS_BORDER_SIZE1* GetDesktopDIP2pixFactor());

	GRect borderExtents(-focusSize, -focusSize, focusSize, focusSize);
	InvalidateWorldRect(GetWorldExtents(featureIndex),	&borderExtents);
}

const AbstrDataItem* GridLayer::GetGridAttr() const
{
	const AbstrDataItem* basisGrid = GetActiveTheme()->GetThemeAttr();
	MG_CHECK(basisGrid);
	if (HasGridDomain(basisGrid))
		return basisGrid;
	else
	{
		dms_assert(m_Themes[AN_Feature]);
		basisGrid = m_Themes[AN_Feature] ? m_Themes[AN_Feature]->GetClassification() : nullptr;
	}
	MG_CHECK(basisGrid && HasGridDomain(basisGrid));
	return basisGrid;
}

const AbstrUnit* GridLayer::GetGeoCrdUnit() const
{
	if (!m_GeoCoordUnit)
	{
		const AbstrDataItem* basisGrid = GetGridAttr();
		dms_assert(basisGrid && HasGridDomain(basisGrid)); // POSTCONDITION of GetGridAttr()
		m_GeoCoordUnit = basisGrid->GetAbstrDomainUnit();
		MG_CHECK(m_GeoCoordUnit && IsGridDomain(m_GeoCoordUnit));
		dms_assert(m_GeoCoordUnit);
	}
	return m_GeoCoordUnit;
}

GraphVisitState GridLayer::InviteGraphVistor(AbstrVisitor& v)
{
	return v.DoGridLayer(this);
}

#include "DataView.h"

void GridLayer::CopySelValues()
{
	ClipBoard clipBoard(false); if (!clipBoard.IsOpen()) return;

	ThemeReadLocks readLocks;

	dms_assert(m_Themes[AN_Selections]); // PRECONDITION 
	readLocks.push_back(m_Themes[AN_Selections].get(), DrlType::Certain);

	const AbstrDataItem* selAttr= m_Themes[AN_Selections]->GetThemeAttr();
	dms_assert(selAttr);                // PRECONDITION

	bit_iterator<1, const UInt32> selArray = const_array_cast<Bool>(selAttr)->GetDataRead().begin();

	Theme* colorTheme   = m_Themes[AN_BrushColor].get();
	Theme* featureTheme = m_Themes[AN_Feature   ].get();
	if (featureTheme)
		throwErrorD("CopySelValues", "Cannot copy from Indirect Grid");

	dms_assert(colorTheme);
	readLocks.push_back(colorTheme, DrlType::Certain);

	const AbstrDataItem*      colorAttr = colorTheme->GetThemeAttr();
	const DataArray<ClassID>* colorData = const_array_cast<ClassID>( colorAttr );
	const ClassID*            colorArray= colorData->GetDataRead().begin();

	IRect gridRange = colorTheme->GetThemeEntityUnit()->GetRangeAsIRect();
	Int32 gridWidth = _Width(gridRange);

	IRect  selRect  = Convert<IRect>( CalcSelectedGeoRect() ); if (selRect.empty()) return;
	Int32  selWidth = _Width(selRect);
	UInt32 selSize  = Cardinality(selRect);

	UInt32 dataSize = sizeof(SelValuesData) + sizeof(ClassID) * selSize;
	GlobalLockHandle dataHandle(dataSize);

	SelValuesData* svd = reinterpret_cast<SelValuesData*>( dataHandle.GetDataPtr() );
	svd->m_Size      = dataSize;
	svd->m_Rect      = selRect;

	dms_assert(IsIncluding(gridRange, selRect));
	ClassID* ptr = svd->m_Data;
	for (Int32 row = selRect.first.Row(); row != selRect.second.Row(); ++row)
	{
		SizeT i = Range_GetIndex_naked(gridRange, shp2dms_order(IPoint(selRect.first.Col(), row)));
		const ClassID* dataPtr = colorArray + i;
		ClassID* oldPtr = ptr;
		ptr = fast_copy(dataPtr, dataPtr + selWidth, ptr);
		undefine_if_not(oldPtr, ptr, selArray + i);
	}

	clipBoard.SetData(CF_CELLVALUES, dataHandle); 

}

void GridLayer::PasteSelValuesDirect()
{
	ClipBoard clipBoard(false); if (!clipBoard.IsOpen()) return;
	if (ClipBoard::GetCurrFormat() != CF_CELLVALUES)
		return;
	HANDLE dataHandle = clipBoard.GetData(CF_CELLVALUES);

	if (m_PasteHandler)
		InvalidatePasteArea();

	m_PasteHandler.assign(new PasteHandler(dataHandle) );

	PasteNow();
	ClearPaste();
}

void GridLayer::PasteSelValues()
{
	ClipBoard clipBoard(false); if (!clipBoard.IsOpen()) return;
	if (ClipBoard::GetCurrFormat() != CF_CELLVALUES)
		return;
	HANDLE dataHandle = clipBoard.GetData(CF_CELLVALUES);

	if (m_PasteHandler)
		InvalidatePasteArea();

	m_PasteHandler.assign(new PasteHandler(dataHandle) );
	
	GetViewPort()->PasteGrid(m_PasteHandler->GetSelValues(), this);
	InvalidatePasteArea();
	// CHECK values
}

void GridLayer::InvalidatePasteArea()
{
	dms_assert(m_PasteHandler);
	if (m_PasteHandler->IsHidden())
		return;

	IRect rect = m_PasteHandler->GetSelValues()->m_Rect;
	GRect borderExtents(-1, -1, 1, 1);
	InvalidateWorldRect(
		AsWorldExtents(
			Convert<CrdRect>( rect ),
			GetGeoCrdUnit()
		),
		&borderExtents
	);
}


template <typename Iter, typename CIter> inline
Iter
copy_defined(CIter first, CIter last, Iter target)
{
	for (; first != last; ++target, ++first)
		if (IsDefined(*first))
			*target = *first;
	return target;
}

void GridLayer::PasteNow()
{
	dms_assert(m_PasteHandler);

	Theme* featureTheme = m_Themes[AN_Feature   ].get();
	if (featureTheme)
		throwErrorD("PasteNow", "Cannot paste to Indirect Grid");

	auto colorTheme = GetActiveTheme();
	dms_assert(colorTheme);
	InvalidationBlock viewChangeLock(this);
	InvalidationBlock dataChangeLock(colorTheme->GetThemeAttr());

	AbstrDataItem*      colorAttr = const_cast<AbstrDataItem*>(colorTheme->GetThemeAttrSource());
	DataWriteLock lock(colorAttr, dms_rw_mode::read_write);
	DataArray<ClassID>* colorData = mutable_array_cast<ClassID>( lock.get() );
	ClassID*            colorArray= colorData->GetDataWrite().begin();

	IRect pselRect = m_PasteHandler->GetSelValues()->m_Rect;
	IRect gridRect = colorAttr->GetAbstrDomainUnit()->GetRangeAsIRect();
	IRect copyRect = pselRect & gridRect;
	if (copyRect.empty())
		return;
	UInt32 nrCols = _Right(copyRect) - _Left(copyRect);
	const ClassID* pselArray = m_PasteHandler->GetSelValues()->m_Data;

	for (Int32 row = _Top(copyRect); row!=_Bottom(copyRect); ++row)
	{
		const ClassID* srcPtr = pselArray + Range_GetIndex_naked(pselRect, shp2dms_order(IPoint(_Left(copyRect),row)));
		copy_defined(
			srcPtr,
			srcPtr + nrCols,
			colorArray + Range_GetIndex_naked(gridRect, shp2dms_order(IPoint(_Left(copyRect),row)))
		);
	}
	lock.Commit();
	dataChangeLock.ProcessChange();
	GRect borderExtents(-1, -1, 1, 1);
	InvalidateWorldRect(GetGeoTransformation().Apply(Convert<CrdRect>(copyRect)), &borderExtents);
	viewChangeLock.ProcessChange();
}

void GridLayer::ClearPaste()
{
	InvalidatePasteArea();
	m_PasteHandler.reset();
}

void GridLayer::CopySelValuesToBitmap()
{
	ClipBoard clipBoard(false);
	if (!clipBoard.IsOpen())
		throwItemError("Cannot open Clipboard");

	ThemeReadLocks readLocks;

	dms_assert(m_Themes[AN_Selections]); // PRECONDITION 
	readLocks.push_back(m_Themes[AN_Selections].get(), DrlType::Certain);

	const AbstrDataItem* selAttr= m_Themes[AN_Selections]->GetThemeAttr();
	dms_assert(selAttr);                // PRECONDITION

	Theme* colorTheme   = m_Themes[AN_BrushColor].get();
	Theme* featureTheme = m_Themes[AN_Feature   ].get();
	dms_assert(colorTheme);
	readLocks.push_back(colorTheme, DrlType::Certain);
	if (featureTheme)
		readLocks.push_back(featureTheme, DrlType::Certain);

	GRect selectRect = Convert<GRect>( CalcSelectedGeoRect() );

	GridCoord mapping(nullptr,
		GetGridCoordKey(GetGeoCrdUnit()), 
		selectRect.Size(), 
		CrdTransformation(-Convert<CrdPoint>(selectRect.TopLeft()), shp2dms_order(CrdPoint(1,1)) ) 
	);
	mapping.Update(1.0);

	GridColorPalette colorPalette(colorTheme);
	dms_assert(colorPalette.IsReady());

	auto dv = GetDataView().lock();
	GridDrawer drawer(
		&mapping
	,	GetIndexCollector()
	,	&colorPalette
	,	nullptr	// selValues
	,	0	// HDC hDC
	,	GRect(GPoint(0,0), GPoint(selectRect.Size())) // viewExtents
	);

	GdiHandle<HBITMAP> 
		hPaletteBitmap = drawer.Apply(),
		hCompatibleBitmap(
			CreateCompatibleBitmap(
				DcHandleBase(dv->GetHWnd()), // memDC,
				selectRect.Width(), 
				selectRect.Height()
			)
		);

	CompatibleDcHandle 
		memPaletteDC(NULL, 0),
		memCompatibleDC(NULL, 0);

	GdiObjectSelector<HBITMAP> 
		selectBitmap1(memPaletteDC,    hPaletteBitmap),
		selectBitmap2(memCompatibleDC, hCompatibleBitmap);

	BitBlt(memCompatibleDC, 
		0, 
		0, 
		selectRect.Width(), selectRect.Height(), 
		memPaletteDC, 0, 0, SRCCOPY
	);

	clipBoard.SetBitmap( hCompatibleBitmap );
}

bool GridLayer::DrawAllRects(GraphDrawer& d, const GridColorPalette& colorPalette) const
{
	DBG_START("GridLayer", "DrawAllRects", MG_DEBUG_REGION);

	// =========== Get DataReadLocks
	if (d.MustBreak())
		return true;

	ThemeReadLocks readLocks;

	const Theme* colorTheme = colorPalette.m_ColorTheme;
	dms_assert(colorTheme);
	if (!readLocks.push_back(colorTheme, DrlType::Suspendible))
	{
		dms_assert(SuspendTrigger::DidSuspend() || colorTheme->WasFailed());
		if (colorTheme->IsFailed())
		{
			Fail(colorTheme);
			return false;
		}
		return true;
	}

	if (m_Themes[AN_Feature] && !readLocks.push_back(m_Themes[AN_Feature].get(), DrlType::Suspendible))
	{
		dms_assert(SuspendTrigger::DidSuspend() || m_Themes[AN_Feature]->WasFailed());
		if (m_Themes[AN_Feature]->IsFailed())
		{
			Fail(m_Themes[AN_Feature].get());
			return false;
		}
		return true;
	}

	// =========== Get Data
	DBG_TRACE(("Region  : %s", d.GetAbsClipRegion().AsString().c_str()));

	GridCoordPtr drawGridCoords = GetGridCoordInfo(d.GetViewPortPtr() );
	drawGridCoords->Update(d.GetSubPixelFactor());
	if (!d.GetDC())
		return false;

	RectArray ra;
	d.GetAbsClipRegion().FillRectArray(ra);

	const AbstrDataItem* grid = GetGridAttr();
	dms_assert(grid);
	const AbstrUnit* gridDomain = grid->GetAbstrDomainUnit();
	dms_assert(gridDomain->GetValueType()->GetNrDims() == 2);

	GPoint viewportOffset = TPoint2GPoint(d.GetClientOffset());
	GRect clippedAbsRect = drawGridCoords->GetClippedRelRect() + viewportOffset;

	ResumableCounter tileCounter(d.GetCounterStacks(), true);
	for (tile_id t=tileCounter.Value(), tn=gridDomain->GetNrTiles(); t!=tn; ++t)
	{
		bool doneAnything = false;
		IRect tileGridRect = gridDomain->GetTileRangeAsIRect(t);
		GRect tileViewRect = drawGridCoords->GetClippedRelRect(tileGridRect);
		for (RectArray::iterator rectPtr = ra.begin(), rectEnd = ra.end(); rectPtr != rectEnd; ++rectPtr)
		{
			*rectPtr &= clippedAbsRect;
			if (rectPtr->empty())
				continue;

			GRect viewRelRect = *rectPtr - viewportOffset;
			GRect tileRelRect = viewRelRect & tileViewRect;
			if (tileRelRect.empty())
				continue;

			IRect gridRect = drawGridCoords->GetClippedGridRect(viewRelRect);
			if (!IsIntersecting(tileGridRect, gridRect))
				continue;

			ReadableTileLock lock(grid->GetRefObj(), t);

			doneAnything = true;
			GridDrawer drawer(
				drawGridCoords.get()
			,	GetIndexCollector()
			,	&colorPalette 
			,	nullptr	// selValues
			,	d.GetDC()
			,	tileRelRect
			,	t
			,	tileGridRect - drawGridCoords->GetGridRect().first // adjusted tileRect
			);
			if (!drawer.empty())
				drawer.CopyDIBSection( drawer.Apply(), viewportOffset, SRCAND );
		}
		++tileCounter; 
		if (tileCounter.MustBreak()) return true;
		if (doneAnything && SuspendTrigger::MustSuspend()) return true;
	}
	tileCounter.Close();
	return false;
}

void GridLayer::DrawPaste(GraphDrawer& d, const GridColorPalette& colorPalette) const
{
	DBG_START("GridLayer", "DrawPaste", MG_DEBUG_REGION);
	dms_assert(m_PasteHandler);

	if (m_PasteHandler->IsHidden())
		return;
	dms_assert(IsDefined( m_PasteHandler->GetSelValues()->m_Rect.first.Row() ) ); // follows from IsHidden
	dms_assert(IsDefined( m_PasteHandler->GetSelValues()->m_Rect.first.Col() ) ); // follows from IsHidden

	// =========== Get DataReadLocks
	ThemeReadLocks readLocks;

	if (m_Themes[AN_Feature])
		readLocks.push_back(m_Themes[AN_Feature].get(), DrlType::Certain);

	// =========== Get Data

	GridCoordPtr drawGridCoords = GetGridCoordInfo(d.GetViewPortPtr() );
	drawGridCoords->Update(1.0);

	static RectArray ra;
	d.GetAbsClipRegion().FillRectArray(ra);

	RectArray::iterator
		rectPtr = ra.begin(),
		rectEnd = ra.end();

	GPoint viewportOffset = TPoint2GPoint(d.GetClientOffset());
	GRect  clippedAbsRect = drawGridCoords->GetClippedRelRect(m_PasteHandler->GetSelValues()->m_Rect) + viewportOffset;

	if (clippedAbsRect.empty())
		return;

	for (; rectPtr != rectEnd; ++rectPtr)
	{
		*rectPtr &= clippedAbsRect;
		if (rectPtr->empty()) continue;

		GridDrawer drawer(
			drawGridCoords.get()
		,	GetIndexCollector()
		,	&colorPalette
		,	m_PasteHandler->GetSelValues()
		,	d.GetDC() 
		,	*rectPtr
		);
		if (!drawer.empty())
			drawer.CopyDIBSection( drawer.Apply(), viewportOffset, SRCCOPY );
	}
}

//	override virtuals of GraphicObject
bool GridLayer::Draw(GraphDrawer& d) const 
{
	if (!VisibleLevel(d))
		return false;

	Theme* colorTheme = m_Themes[AN_BrushColor].get();

	if (colorTheme)
	{
		GridColorPalette colorPalette(colorTheme);
		if (!colorPalette.IsReady())
			return false;

		if (DrawAllRects(d, colorPalette))
			return true;

		if (m_PasteHandler)
			DrawPaste(d, colorPalette);
	}
	if (m_Themes[AN_Selections])
	{
		SuspendTrigger::FencedBlocker dontSuspendAfterDrawingColors;
		CreateSelCaretInfo(); // trigger the creation of the SelCaret
	}

	// Draw Focus
	if (IsActive() && d.GetDC())
	{
		SizeT fe = GetFocusElemIndex();
		if (!IsDefined(fe))
			goto skipDrawFocus;

		Region focusViewRgn, focusBordRgn;
		Float64 subPixelFactor = d.GetSubPixelFactor();
		Int32 focusSize = RoundUp<4>(FOCUS_BORDER_SIZE* subPixelFactor);
		const AbstrUnit* geoCrdUnit = GetGeoCrdUnit();
		IRect geoCrdRect = geoCrdUnit->GetRangeAsIRect();
		const UnitProjection* proj = geoCrdUnit->GetProjection();

		if (HasEntityAggr())
		{
			RegionTower focusBordRgnTower, focusViewRgnTower;
			const IndexCollector* ic = GetIndexCollector();
			for (tile_id t=0, te = ic->GetNrTiles(); t!=te; ++t) 
			{
				IRect tileRect = geoCrdUnit->GetTileRangeAsIRect(t); 

				// SKIP tiles that don't intersect with view area
				GRect clipRect = d.GetAbsClipRect();
				clipRect.Expand(focusSize);
				if (!IsIntersecting(
						DRect2GRect( 
							d.GetTransformation().Apply(
								AsWorldExtents(
									Convert<CrdRect>(tileRect)
								,	proj
								)
							)
						)
					,	clipRect
					)
				)
					continue;

				LockedIndexCollectorPtr indexCollector(GetIndexCollector(), t);

				for (tile_offset minFE = 0, maxFE = geoCrdUnit->GetTileCount(t); minFE!=maxFE; ++minFE)
					if (indexCollector->GetEntityIndex(minFE) == fe)
					{
						CrdRect focusWorldRect = ::GetWorldExtents(tileRect, proj, minFE);

						while (minFE+1!=maxFE && indexCollector->GetEntityIndex(minFE+1) == fe)
						{
							CrdRect nextWorldRect = ::GetWorldExtents(tileRect, proj, minFE+1);
							if	(	nextWorldRect.first .Row() != focusWorldRect.first .Row() 
								||	nextWorldRect.second.Row() != focusWorldRect.second.Row()
								||	nextWorldRect.first.Col() > focusWorldRect.second.Col())
								break;
							focusWorldRect |= nextWorldRect;
							++minFE;							
						}

						GRect focusViewRect = DRect2GRect( d.GetTransformation().Apply(focusWorldRect) );
						GRect focusBordRect = focusViewRect; focusBordRect.Expand(focusSize);
						if (IsIntersecting(focusBordRect, d.GetAbsClipRect()))
						{
							focusViewRgnTower.Add( Region( focusViewRect ) );
							focusBordRgnTower.Add( Region( focusBordRect ) );
						}
					}
			}
			focusViewRgn = focusViewRgnTower.GetResult();
			focusBordRgn = focusBordRgnTower.GetResult();
		}
		else
		{
			fe = Entity2FeatureIndex(fe);
			if (!IsDefined(fe))
				goto skipDrawFocus;

			CrdRect focusWorldRect = ::GetWorldExtents(geoCrdRect, proj, fe);
			GRect focusViewRect = DRect2GRect( d.GetTransformation().Apply(focusWorldRect) );
			GRect focusBordRect = focusViewRect; focusBordRect.Expand(focusSize);

			if (!IsIntersecting(focusBordRect, d.GetAbsClipRect()))
				goto skipDrawFocus;
			focusViewRgn = Region( focusViewRect );
			focusBordRgn = Region( focusBordRect );
		}

		DcMixModeSelector xorMode(d.GetDC());
		FrameRgn(d.GetDC(), focusViewRgn.GetHandle(), GetSysColorBrush(COLOR_HIGHLIGHT), subPixelFactor, subPixelFactor);
		FrameRgn(d.GetDC(), focusBordRgn.GetHandle(), GetSysColorBrush(COLOR_HIGHLIGHT), FOCUS_BORDER_FRAMEWIDTH* subPixelFactor, FOCUS_BORDER_FRAMEWIDTH* subPixelFactor);
		focusBordRgn -= focusViewRgn;
		FillRgn (d.GetDC(), focusBordRgn.GetHandle(), GdiHandle<HBRUSH>( CreateHatchBrush(HS_BDIAGONAL, DmsColor2COLORREF(DmsRed)))); 
	}
skipDrawFocus:
	return false;
}

void GridLayer::DoUpdateView()
{
	const AbstrUnit* geoCrdUnit = GetGeoCrdUnit();
	if (geoCrdUnit && !geoCrdUnit->InTemplate())
	{
		if (PrepareDataOrUpdateViewLater(geoCrdUnit))
			SetWorldClientRect(
				AsWorldExtents( 
					Convert<CrdRect>(geoCrdUnit->GetRangeAsIRect()),
					GetGeoCrdUnit()
				)
			);
	}
}

GRect GridLayer::GetBorderPixelExtents(CrdType subPixelFactor) const
{
	Int32 focusSize = RoundUp<4>(FOCUS_BORDER_SIZE* subPixelFactor);

	return GRect(-focusSize, -focusSize, focusSize, focusSize);  // max rounding error without considering orientation
}

void GridLayer::Zoom1To1(ViewPort* vp)
{
	dms_assert(vp);

	if (!vp->GetWorldCrdUnit())
		return;

	const AbstrUnit* gridDomain   = GetGeoCrdUnit();
	CrdTransformation grid2ViewTr =	::GetGeoTransformation(gridDomain);

	CrdPoint p = Center(vp->GetROI());
	CrdPoint s = grid2ViewTr.Factor() * Convert<CrdPoint>(vp->GetCurrClientSize()) * 0.5;
	vp->SetROI(CrdRect(p-s, p+s));
}

void CheckClassIdAttr(GraphicLayer* self)
{
	if (! self->HasClassIdAttr() )
		self->throwItemError("LayerDAta aren't UInt8 values and therefore cannot be edited");
}

void CheckEditability(GraphicLayer* self)
{
	CheckClassIdAttr(self);
	if (!self->HasEditAttr())
		self->throwItemError("LayerData is not editable");
}

bool GridLayer::OnCommand(ToolButtonID id)
{
	switch (id)
	{
		case TB_CutSel:
			CheckEditability(this);
			CopySelValues ();
			AssignSelValues();
			return true;

		case TB_CopySel:
			CheckClassIdAttr(this);	
			CopySelValues ();
			return true;

		case TB_PasteSelDirect:
			CheckEditability(this);
			PasteSelValuesDirect();
			return true;

		case TB_PasteSel:
			CheckEditability(this);
			PasteSelValues();
			return true;

		case TB_DeleteSel:
			CheckEditability(this);
			AssignSelValues();
			return true;
	}
	return base_type::OnCommand(id);
}

void GridLayer::FillMenu(MouseEventDispatcher& med)
{
	base_type::FillMenu(med);

	if	(!IsActive())
		return;

	//	========== from here only menu options for active layer
	dms_assert(IsVisible());  // implied by IsActive()

	if	(!HasClassIdAttr())
		return;

	//	========== from here only menu options for ClassId layer

	bool hasEditAttr  = HasEditAttr();
	bool hasSelection = bool(m_Themes[AN_Selections]);

	med.m_MenuData.push_back( 
		MenuItem(
			hasEditAttr && IsDefined(GetCurrClassID())
				?	"Fill District with " + GetCurrClassLabel()
				:	SharedStr("Select District"),
			new CmdSelectDistrict( med.m_WorldCrd ), 
			this
		)
	);	

	if	(hasEditAttr && hasSelection)
	{
		dms_assert(IsVisible());
		med.m_MenuData.push_back( 
			MenuItem(
				"Fill Selected Cells with "+ GetCurrClassLabel(),
				make_MembFuncCmd( &GridLayer::AssignSelValues),
				this
			)
		);	
	}
	if	(hasSelection)
	{
		dms_assert(IsVisible());
		med.m_MenuData.push_back( 
			MenuItem(
				SharedStr("Copy Selected Cells to Clipboard"),
				make_MembFuncCmd( &GridLayer::CopySelValues),
				this
			)
		);	
	}
	if (hasEditAttr && ClipBoard::GetCurrFormat() == CF_CELLVALUES)
	{
		dms_assert(IsVisible());
		med.m_MenuData.push_back( 
			MenuItem(
				SharedStr("Paste Selected Cells from Clipboard"),
				make_MembFuncCmd( &GridLayer::PasteSelValues),
				this
			)
		);	
	}
}

GridCoordPtr GridLayerBase::GetGridCoordInfo(ViewPort* vp) const
{
	dms_assert(vp);
	if (m_GridCoord  && m_GridCoord->GetOwner().lock().get() == vp)
		return m_GridCoord;

	GridCoordPtr result = vp->GetOrCreateGridCoord( GetGeoCrdUnit() );
	if (vp == GetViewPort())
		m_GridCoord = result; // cache only if vp owns this GridLayer
	return result;
}

void GridLayerBase::FillLcMenu(MenuData& menuData)
{
	base_type::FillLcMenu(menuData);

	menuData.push_back(
		MenuItem(
			SharedStr("Zoom &1 Grid to 1 Pixel"),
			make_MembFuncCmd(&GridLayerBase::Zoom1To1Caller),
			this,
			IsVisible() ? 0 : MFS_GRAYED
		)
	);
}


void GridLayer::CreateSelCaretInfo()  const
{
	dms_assert( m_Themes[AN_Selections] ); // callers responsibility not to call this when no SelAttr was created
	if (m_SelCaret)
		return;

	m_SelCaret = const_cast<GridLayer*>(this)->GetViewPort()->GetOrCreateSelCaret(
		sel_caret_key(
			m_Themes[AN_Selections]->GetThemeAttrSource()
		,	GetIndexCollector()
		)
	);
}

IMPL_DYNC_LAYERCLASS(GridLayer, ASE_Feature|ASE_BrushColor|ASE_HatchStyle|ASE_Pen|ASE_PixSizes|ASE_Selections, AN_BrushColor, 2)

