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

#ifndef __RTC_GEO_SELECTPOINT_H
#define __RTC_GEO_SELECTPOINT_H

#include "dbg/Debug.h"
#include "geo/Area.h"
#include "set/VectorFunc.h"
#include "geo/CalcWidth.h"
#include "ser/PointStream.h"
#include "set/IndexCompare.h"

#include "OperRelUni.h"

//----------------------------------------------------------------------
// Section      : SelectRow calculates the row for which the polygon 
//                has exactly <measure> area above it
//----------------------------------------------------------------------

template <typename ScalarType, typename DensityType, typename ConstPointIter>
ScalarType SelectRow(ConstPointIter polyBegin, ConstPointIter polyEnd, DensityType measure)
{
	DBG_START("SelectRow", "", MG_TRACE_LABELPOS);
	DBG_TRACE(("measure %s", AsString(measure).c_str()));

	dms_assert(measure >= 0);
	UInt32 polySize = std::distance(polyBegin, polyEnd);
	if (polySize)
	{
		measure *= 2; // calculating with twice the area is easier

		//	point_index sort on Row ASC
		std::vector<UInt32> pointIndex;
		make_index(pointIndex, polySize, polyBegin);
		auto
			currPointIndexPtr = pointIndex.begin(),
			lastPointIndexPtr = pointIndex.end();

		DensityType doubleAreaSofar = 0;
		ScalarType prevWidth = 0, prevRow = polyBegin[*currPointIndexPtr].Row();

		if (!measure)
			return prevRow;		

		if (currPointIndexPtr != lastPointIndexPtr)
			++currPointIndexPtr;

		for (; currPointIndexPtr != lastPointIndexPtr; ++currPointIndexPtr)
		{
			ScalarType currRow   = polyBegin[*currPointIndexPtr].Row();
			ScalarType currWidth = CalcWidth<ScalarType>(polyBegin, polyEnd,  polyBegin[*currPointIndexPtr]);
			DensityType deltaDoubleArea = DensityType(prevWidth + currWidth) * (DensityType(currRow) - DensityType(prevRow));

			DBG_TRACE(("currPoint  %s", AsString(polyBegin[*currPointIndexPtr]).c_str()));
			DBG_TRACE(("currWidth  %s", AsString(currWidth).c_str()));
			DBG_TRACE(("deltaDArea %s", AsString(deltaDoubleArea).c_str()));

			DensityType newDoubleAreaSofar = doubleAreaSofar + deltaDoubleArea;
			if (newDoubleAreaSofar >= measure)
			{
				//	D dblarea(row) / D row == 2*width(row) == 2*interpolate(row, prevRow, currRow, prevWidth, currWidth)
				//	dblarea(prevRow) == doubleAreaSofar
				// solve: measure == dblarea(row)
				//	interpolate(row, prevRow, currRow, prevWidth, currWith) := [(row - prevRow) * currWidth + (currRow - row) * prevWidth ] / [currRow - prevRow]
				//	== [row * (currWidth- prevWidth) + currRow*prevWidth - prevRow * currWidth ] / [currRow - prevRow]
				// translate in order that prevRow == 0, thus row' := row-prevRow; currRow' := currRow - prevRow
				//	interpolate(row', ...) == row'*(currWidth- prevWidth)/currRow' + prevWidth
				//	0 = dblarea(row) -measure 
				//	0 = row'^2*(currWidth- prevWidth)/currRow' + 2*row' * prevWidth + doubleAreaSofar-measure
				//	= A row'^2 + B*row'+C
				//	with A = (currWidth- prevWidth)/currRow'
				//	     B = 2*prevWidth
				//	     C = doubleAreaSofar - measure
				//	SOLUTION
				//	row' = [ -B plusmin sqrt(B^2-4AC) ]/2A
				//	= [ - 2*prevWidth plusmin sqrt(4*prevWidth^2 - 4* (currWidth- prevWidth)/currRow'*(doubleAreaSofar - measure) ) ] / 2*((currWidth- prevWidth)/currRow')
				//	=	
				//		[ -		currRow'*prevWidth 
				//		plusmin sqrt(
				//					(currRow'*prevWidth)^2 
				//				-	currRow'*(currWidth - prevWidth)*(doubleAreaSofar - measure) ) ] 
				//		/ (currWidth- prevWidth)

				ScalarType deltaWidth = (currWidth - prevWidth);
				measure -= doubleAreaSofar;

				DBG_TRACE(("measure   %s", AsString(measure).c_str()));
				DBG_TRACE(("currWidth %s", AsString(currWidth).c_str()));
				DBG_TRACE(("prevWidth %s", AsString(prevWidth).c_str()));
				DBG_TRACE(("currRow   %s", AsString(currRow).c_str()));
				DBG_TRACE(("prevRow   %s", AsString(prevRow).c_str()));

				//	row' = 
				//		[ -		currRow'*prevWidth 
				//		plusmin sqrt(
				//					(currRow'*prevWidth)^2 
				//				+	currRow'*deltaWidth*measure ) ] 
				//		/ deltaWidth

				if (!measure)
					return prevRow;
				if (!deltaWidth)
					return prevRow + (measure / (2.0 * prevWidth));

				currRow -= prevRow;
				DensityType tmp1 = currRow*prevWidth ;
				DBG_TRACE(("currRow'  %s", AsString(currRow).c_str()));
				DBG_TRACE(("tmp1 %s", AsString(tmp1).c_str()));

				//	row = prevRow +
				//		[plusmin sqrt(
				//					tmp1^2 
				//				+	currRow'*deltaWidth*measure )
				//			-	tmp1
				//		]	/	deltaWidth

				ScalarType row =
					(sqrt( Sqr(tmp1) + currRow*deltaWidth*measure ) - tmp1)
					/	deltaWidth;

				DBG_TRACE(("row %s", AsString(row).c_str()));

				return prevRow + row;
			}


			doubleAreaSofar = newDoubleAreaSofar;
			prevWidth = currWidth;
			prevRow   = currRow;
		}
	}
	return UNDEFINED_VALUE(ScalarType);
}

#include "geo/Area.h"
#include "geo/PointOrder.h"

template <typename ScalarType, typename DensityType, typename ConstPointIter>
Point<ScalarType> AreaPercentile(ConstPointIter polyBegin, ConstPointIter polyEnd, DensityType yMeasure, DensityType xMeasure, ScanPointCalcResource<ScalarType>& calcResource)
{
	DensityType area = Abs( Area<DensityType>(polyBegin, polyEnd) );

	if (area) {
		int nrTries = 3;
		while (nrTries-- > 0)
		{
			dms_assert(yMeasure >= 0.0 && yMeasure < 1.0);
			ScalarType row = SelectRow<ScalarType>(polyBegin, polyEnd, area*yMeasure);
			if (IsDefined(row))
			{
				ScalarType col = GetScanPoint<ScalarType>(polyBegin, polyEnd, row, xMeasure, calcResource);
				if (IsDefined(col))
					return shp2dms_order(col, row);
			}
			yMeasure = 3.6 * yMeasure * (1 - yMeasure);
		}
	}
	return *polyBegin;
}

template <typename ScalarType, typename ConstPointIter>
Point<ScalarType> Mid(ConstPointIter polyBegin, ConstPointIter polyEnd, ScanPointCalcResource<ScalarType>& calcResource)
{
	return AreaPercentile<ScalarType>(polyBegin, polyEnd, 0.5, 0.5, calcResource);
}

#endif // __RTC_GEO_SELECTPOINT_H


