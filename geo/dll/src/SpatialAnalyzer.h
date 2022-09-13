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

#if !defined(__GEO_SPATIALANALYZER_H)
#define  __GEO_SPATIALANALYZER_H

#include "SpatialBase.h"

// *****************************************************************************
//											CLASSES
// *****************************************************************************

enum	TTranslation	{ tInit, tDecCol, tIncCol, tDecRow, tIncRow };
enum	TQuadrant		{ q_0, q_1, q_2, q_3, q_4 };
enum	TOctant			{ o_0, o_1_1, o_1_2, o_2_1, o_2_2, o_3_1, o_3_2, o_4_1, o_4_2 };

typedef std::vector<RadiusType>   CirclePointsType;
typedef std::vector<UInt32>       DivVectorType;

class TForm
{
public:
	TForm()                                 { Init(0, false); }
	TForm(RadiusType radius, bool isCircle) { Init(radius, isCircle); }	

	void             Init(RadiusType radius, bool isCircle);
	void             SetCenter(const UGridPoint& p) { m_Center = p; m_Defined = false; }
	bool             Contains (const UGridPoint& p);

	bool             NextContainedPoint(IGridPoint&);	// gives all points of form in Bitmap order (=lexicographic (row, col))
	bool             NextBorderPoint(IGridPoint& p, TTranslation t); // all points in Translated order

private:
	UGridPoint  m_Center;
	FormPoint  m_CurrentPoint;
	RadiusType m_Radius;
	bool       m_IsCircle,
		       m_Defined;
	CirclePointsType m_CirclePoint;

	static TQuadrant Quadrant (const FormPoint&);
	static TOctant   Octant   (const FormPoint&);
	static FormPoint Translate(const FormPoint& p) { return Translate(p, Octant(p)); }
	static FormPoint Translate(const FormPoint&, TOctant);

	bool ContainsCentered(const FormPoint&);	
	bool GetOtherCoordinateCentered(FormType, FormType&);
};


// *****************************************************************************
//	SpatialAnalyzer for Districting and Diversity
// *****************************************************************************

typedef SizeType                              DistrIdType;
typedef UGrid<DistrIdType>                    DistrIdGridType;
typedef sequence_traits<Bool>::seq_t          DistrSelSeqType;
typedef sequence_traits<Bool>::container_type DistrSelVecType;

template <typename T>
class SpatialAnalyzer
{
public:
	typedef T        DataGridValType;
	typedef T        DivCountType;
	typedef UGrid<const DataGridValType> DataGridType;
	typedef UGrid<      DivCountType   > DivCountGridType;

	void GetDistricts(const DataGridType& input, const DistrIdGridType& output, DistrIdType* resNrDistricts, bool rule8);
	void GetDistrict (const DataGridType& input, const DistrSelSeqType& output, const IGridPoint& seedPoint, IGridRect& resRect); // only for T==Bool?
	void GetDiversity(const DataGridType& input, DataGridValType inputUpperBound, RadiusType radius, bool isCircle, const DivCountGridType& output);

private:
	// general
	DataGridType     m_Input;
	DivCountGridType m_DivOutput;
	DistrIdGridType  m_DistrOutput;
	DistrSelSeqType  m_DistrBoolOutput;

	SizeType m_Size;
	SizeType m_NrCols;
	IGridRect m_Rectangle;
	IGridRect m_ResRect;

	void Init(const DataGridType& input);
	void Init(const DataGridType& input, RadiusType radius, bool isCircle, const DivCountGridType& distrOutput);

	SizeType GridSize() { return m_Size; }
	SizeType Pos(const UGridPoint& p)   
	{ 
		dms_assert(p.Col() >= 0);
		dms_assert(p.Row() >= 0);
		dms_assert(p.Col() < m_Input.GetSize().Col());
		dms_assert(p.Row() < m_Input.GetSize().Row());
		return m_NrCols* p.Row()+ p.Col();
	}
	UGridPoint GetPoint(SizeType pos) { return UGridPoint(pos / m_NrCols, pos % m_NrCols); }
	
	// districting
	DistrSelVecType m_Processed;

	template <typename D> void ConsiderPoint(IGridPoint seedPoint, D districtId, DataGridValType val, std::vector<IGridPoint>& stack);
	template <typename D> void GetDistrict  (IGridPoint seedPoint, D districtId, bool rule8);

	bool FindFirstNotProcessedPoint(IGridPoint& foundPoint);
	void SetDistrictId(SizeType pos, DistrIdType districtId);
	void SetDistrictId(SizeType pos, bool        districtId);


	// diversity
	TForm  m_Form;
	T      m_InputUpperBound;

	void GetDiversity1();
	void GetDiversity2();

	void         DiversityCountIncremental(const IGridPoint&, DivVectorType&, bool isFirstRow, bool isFirstCol, bool right);
	void         DiversityCountVertical   (const IGridPoint&, DivVectorType&, bool);
	void         DiversityCountHorizontal (const IGridPoint&, DivVectorType&, TTranslation);
	DivCountType DiversityCountAll        (const IGridPoint&, DivVectorType&);
	DivCountType DiversityDifference      (const IGridPoint&, DivVectorType&, TTranslation, bool);
	bool         NextBorderPoint(IGridPoint&, TTranslation, bool forward);
};


//====================================================

template <typename ZoneType>
void Districting(const UGrid<const ZoneType>& input, const UUInt32Grid& output, UInt32* resNrDistricts, bool rule8) 
{ 
	SpatialAnalyzer<ZoneType>().GetDistricts(input, output, resNrDistricts, rule8);
}

template <typename ZoneType>
void Diversity(const UGrid<const ZoneType>& input, ZoneType inputUpperBound, RadiusType radius, bool isCircle, UGrid<ZoneType>& divOutput)
{ 
	SpatialAnalyzer<ZoneType>().GetDiversity(input, inputUpperBound, radius, isCircle, divOutput);
}

// *****************************************************************************

#endif //!defined(__GEO_SPATIALANALYZER_H)
