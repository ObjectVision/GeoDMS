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
#pragma once

#ifndef __SHV_GRIDFILL_H
#define __SHV_GRIDFILL_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "cpc/EndianConversions.h"
#include "geo/Conversions.h"

#include "GeoTypes.h"
#include "GridDrawer.h"
#include "GridCoord.h"
#include "IndexCollector.h"

//----------------------------------------------------------------------
// Helper stuff
//----------------------------------------------------------------------

typedef UInt8 MaxIndirectPixelType;
const UInt32 MaxNrIndirectPixelBits = nrbits_of_v<MaxIndirectPixelType>; // 8
const UInt32 MaxPaletteSize         = (1 << MaxNrIndirectPixelBits);

// use template specialisation (struct) to variate pixel_type
template <typename T> struct pixel_type : std::conditional<nrbits_of_v<T> <= MaxNrIndirectPixelBits, T, MaxIndirectPixelType > {};
template <> struct pixel_type<UInt2> { typedef UInt4 type; };

template <typename T> using pixel_type_t = pixel_type<T>::type;

template <typename Iter>
UInt32 GetAligned4Size(Iter ptr, UInt32 n)
{
	ptr += n;
	while (!IsAligned4(ptr)) { ++ptr; ++n; }
	return n;
}

//----------------------------------------------------------------------
// operator : ValueAdjuster
//----------------------------------------------------------------------

template <typename PixelType>
struct ValueAdjuster
{
	ValueAdjuster(const GridColorPalette* colorPalette)
		:	m_LastValue ( colorPalette->GetPaletteCount() ? colorPalette->GetPaletteCount()-1 : MaxValue<PixelType>())
	{}

	void operator ()(PixelType& result) const
	{
		MakeMin(result, m_LastValue);
	}

	PixelType GetUndefValue() const { return m_LastValue; } // last palette entry contains the UNDEF color

private:
	PixelType m_LastValue;
};

template <bit_size_t N>
struct ValueAdjuster<bit_value<N> >
{
	ValueAdjuster(const GridColorPalette* colorPalette)
	{}

	void operator ()(bit_value<N>& result) const { /* NOP */ }
	bit_value<N> GetUndefValue() const { return 0; } // bit_value<N>::mask; }
};

template <>
struct ValueAdjuster<DmsColor>
{
	ValueAdjuster(const GridColorPalette* colorPalette)
		:	m_UndefColor( Convert<RGBQUAD>( STG_Bmp_GetDefaultColor(CI_NODATA) ) )
	{
		dms_assert(!colorPalette || ! colorPalette->GetPaletteCount() );
	}

	void operator ()(DmsColor& result) const { /* NOP */ }

	DmsColor GetUndefValue() const { return *reinterpret_cast<const DmsColor*>(&m_UndefColor); } // last palette entry contains the UNDEF color

private:
	RGBQUAD m_UndefColor;
};

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

template <typename ClassIdType>
void PostProcessLine(ClassIdType b, ClassIdType e)
{}

template <typename Block>
void PostProcessLine(bit_iterator<1, Block> b, bit_iterator<1, Block> e)
{
	dms_assert(b.nr_elem() == 0);
	dms_assert(e.nr_elem() == 0);
	BitSwap(b.data_begin(), e.data_begin());
}

template <typename Block>
void PostProcessLine(bit_iterator<2, Block> b, bit_iterator<2, Block> e)
{
	dms_assert(b.nr_elem() == 0);
	dms_assert(e.nr_elem() == 0);
	TwipSwap(b.data_begin(), e.data_begin());
}

template <typename Block>
void PostProcessLine(bit_iterator<4, Block> b, bit_iterator<4, Block> e)
{
	dms_assert(b.nr_elem() == 0);
	dms_assert(e.nr_elem() == 0);
	NibbleSwap(b.data_begin(), e.data_begin());
}

//----------------------------------------------------------------------
// func : GridFill
//----------------------------------------------------------------------

template <typename ClassIdType, typename PixelType, typename ValuesFunctor>
void GridFill(
		const GridDrawer*                                    gridDrawer
	,	typename sequence_traits<ClassIdType>::const_pointer classIdArray, SizeT classIdArraySize
	,	const ValuesFunctor&                                 applicator
	,	Range<SizeT>                                         tileIndexRange
	,   bool                                                 isLastRun
	)
{
	typename sequence_traits<PixelType>::pointer resultingClassIds = iter_creator<PixelType>()(gridDrawer->m_pvBits, 0);

	dms_assert(gridDrawer);
	dms_assert(IsAligned4(resultingClassIds) );
	dms_assert(!gridDrawer->m_SelValues || IsDefined( gridDrawer->m_SelValues->m_Rect.first.Row() ) );

	SizeT tileIndexRangeSize = Cardinality(tileIndexRange);

//	Prepare loop
	GType viewColBegin = gridDrawer->m_sViewRect.left;
	GType viewColEnd   = gridDrawer->m_sViewRect.right;
	assert(viewColBegin < viewColEnd);

	UInt32 viewColSize = GetAligned4Size(resultingClassIds, viewColEnd - viewColBegin);

	GType currViewRow  = gridDrawer->m_sViewRect.bottom;
	GType viewRowBegin = gridDrawer->m_sViewRect.top;
	dms_assert(currViewRow > viewRowBegin);

	IPoint gridSize = gridDrawer->m_SelValues 
		?	Size(gridDrawer->m_SelValues->m_Rect)
		:	Size(gridDrawer->m_TileRect);
	assert(gridSize.first  >= 0);
	assert(gridSize.second >= 0);

	const grid_rowcol_id* currGridRowPtr = gridDrawer->m_GridCoords->GetGridRowPtr(currViewRow, true);
	MG_DEBUGCODE(const grid_rowcol_id* gridRowBeginPtr = currGridRowPtr; )
	const grid_rowcol_id* gridColBeginPtr = gridDrawer->m_GridCoords->GetGridColPtr(viewColBegin, true);

	ValueAdjuster<PixelType> valueAdjuster(gridDrawer->m_ColorPalette);
	UInt32 rowOffset = (gridDrawer->m_SelValues) 
		?	gridDrawer->m_SelValues->m_Rect.first.Row()
		:	gridDrawer->m_TileRect.first.Row();

	while (currViewRow != viewRowBegin)
	{
		grid_rowcol_id currGridRow = *--currGridRowPtr;
		--currViewRow;
		assert(gridDrawer->m_sViewRect.bottom - currViewRow == gridRowBeginPtr - currGridRowPtr );

		if (IsDefined(currGridRow))
		{
			if (currGridRow < rowOffset)
				continue;

			typename sequence_traits<PixelType>::pointer resultingClassIdRowBegin = resultingClassIds;
			currGridRow -= rowOffset;

			if (currGridRow >= grid_rowcol_id(gridSize.Row()))
				continue;

			const grid_rowcol_id* currGridColPtr = gridColBeginPtr;

			SizeT currGridRowBegin = CheckedMul<SizeT>(currGridRow, gridSize.Col(), false);

			GType currViewCol = viewColBegin;
			PixelType result;

			while (true)
			{
				assert(currViewCol - viewColBegin == currGridColPtr - gridColBeginPtr);

				grid_rowcol_id currGridCol = *currGridColPtr;
				if (IsDefined(currGridCol))
				{
					grid_rowcol_id colOffset = (gridDrawer->m_SelValues)
						? gridDrawer->m_SelValues->m_Rect.first.Col()
						: gridDrawer->m_TileRect.first.Col();
					if (currGridCol >= colOffset)
					{
						currGridCol -= colOffset;
						if (currGridCol < grid_rowcol_id(gridSize.Col()))
						{
							SizeT currGridNr = currGridRowBegin + currGridCol;
							if (gridDrawer->m_EntityIndex)
							{
								entity_id e = gridDrawer->m_EntityIndex.GetEntityIndex(currGridNr);
								if (!IsDefined(e))
									goto assignUndefined;
								currGridNr = e - tileIndexRange.first;
								if (currGridNr >= tileIndexRangeSize)
									goto skipResult;
							}
							assert(classIdArray);
							assert(currGridNr < classIdArraySize);
							result = Convert<PixelType>(applicator(classIdArray[currGridNr]));
							valueAdjuster(result);
							goto applyResult;
						}
					}
				}
			assignUndefined:
				Assign(result, valueAdjuster.GetUndefValue());

			applyResult:
				// copy duplicate columns
				do {
					*resultingClassIds = result; ++resultingClassIds;
					++currViewCol; if (currViewCol == viewColEnd) goto end_of_line;
				} while (currGridCol == *++currGridColPtr);
				continue; // go to nextGridCol
			skipResult:
				// skip duplicate columns
				do {
					++resultingClassIds;
					++currViewCol; if (currViewCol == viewColEnd) goto end_of_line;
				} while (currGridCol == *++currGridColPtr);
			}

		end_of_line:
			resultingClassIds = resultingClassIdRowBegin + viewColSize;

			typename sequence_traits<PixelType>::pointer
				resultingClassIdRowEnd = resultingClassIds;

			if (isLastRun)
				PostProcessLine(resultingClassIdRowBegin, resultingClassIdRowEnd);

			// copy duplicate lines
			while (true)
			{
				if (currViewRow == viewRowBegin)
					return;
				if (currGridRow != currGridRowPtr[-1])
					break;

				if (isLastRun)
					fast_copy(
						resultingClassIdRowBegin,
						resultingClassIdRowEnd,
						resultingClassIds
					);
				resultingClassIds += (resultingClassIdRowEnd - resultingClassIdRowBegin);

				--currViewRow;
				--currGridRowPtr;
			}
		}
		else
		{
			typename sequence_traits<PixelType>::pointer currResultingClassIds = resultingClassIds;
			resultingClassIds += viewColSize;
			if (isLastRun)
				fast_fill(
					currResultingClassIds,
					resultingClassIds,
					valueAdjuster.GetUndefValue()
				);
		}
	}
}


#endif // __SHV_GRIDFILL_H

