// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__GEO_SPATIALANALYZER_H)
#define  __GEO_SPATIALANALYZER_H

#include "SpatialBase.h"

// *****************************************************************************
//											CLASSES
// *****************************************************************************

enum TTranslation { tInit, tDecCol, tIncCol, tDecRow, tIncRow };
enum TQuadrant    { q_0, q_1, q_2, q_3, q_4 };
enum TOctant      { o_0, o_1_1, o_1_2, o_2_1, o_2_2, o_3_1, o_3_2, o_4_1, o_4_2 };

using CirclePointsType = std::vector<RadiusType>;
using DivVectorType = std::vector<UInt32>;

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
	UGridPoint m_Center;
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

template <typename T>
struct SpatialAnalyzer
{
	using DataGridValType = T;
	using DataGridType = UGrid<const DataGridValType>;

	void Init(const DataGridType& input);

protected:
	DataGridType     m_Input;

	SizeType m_Size = 0;
	SizeType m_NrCols = 0;
	IGridRect m_Rectangle;
	IGridRect m_ResRect;


	SizeType GridSize() { return m_Size; }
	SizeType Pos(const UGridPoint& p)   
	{ 
		assert(p.Col() >= 0);
		assert(p.Row() >= 0);
		assert(p.Col() < m_Input.GetSize().Col());
		assert(p.Row() < m_Input.GetSize().Row());
		return m_NrCols* p.Row()+ p.Col();
	}
	UGridPoint GetPoint(SizeType pos) { return UGridPoint(pos / m_NrCols, pos % m_NrCols); }
};

template <typename T, typename DistrIdType = SizeType>
struct Districter : SpatialAnalyzer<T>
{
	using DistrSelSeqType = sequence_traits<Bool>::seq_t;
	using DistrSelVecType = sequence_traits<Bool>::container_type;

	using DistrIdGridType = UGrid<DistrIdType>;
	using typename SpatialAnalyzer<T>::DataGridType;
	using typename SpatialAnalyzer<T>::DataGridValType;

	void GetDistricts(const DataGridType& input, const UGrid<DistrIdType>& output, DistrIdType* resNrDistricts, bool rule8);
	void GetDistrict(const DataGridType& input, const DistrSelSeqType& output, const IGridPoint& seedPoint, IGridRect& resRect); // only for T==Bool?

private:
	void ConsiderPoint(IGridPoint seedPoint, DistrIdType districtId, DataGridValType val, std::vector<IGridPoint>& stack);
	void GetDistrict(IGridPoint seedPoint, DistrIdType districtId, bool rule8);
	bool FindFirstNotProcessedPoint(IGridPoint& foundPoint);
	void SetDistrictId(SizeType pos, DistrIdType districtId);
	void SetDistrictId(SizeType pos, bool        districtId);


	DistrIdGridType m_DistrOutput;
	DistrSelSeqType m_DistrBoolOutput;
	DistrSelVecType m_Processed;
};

template <typename T>
struct DiversityCalculator : SpatialAnalyzer<T>
{
	using DivCountType = T;
	using DivCountGridType = UGrid<DivCountType>;
	using typename SpatialAnalyzer<T>::DataGridType;
	using typename SpatialAnalyzer<T>::DataGridValType;

	void Init(const DataGridType& input, RadiusType radius, bool isCircle, const DivCountGridType& distrOutput);

	void GetDiversity(const DataGridType& input, DataGridValType inputUpperBound, RadiusType radius, bool isCircle, const DivCountGridType& output);

private:
	void GetDiversity1();
	void GetDiversity2();

	void         DiversityCountIncremental(const IGridPoint&, DivVectorType&, bool isFirstRow, bool isFirstCol, bool right);
	void         DiversityCountVertical(const IGridPoint&, DivVectorType&, bool);
	void         DiversityCountHorizontal(const IGridPoint&, DivVectorType&, TTranslation);
	DivCountType DiversityCountAll(const IGridPoint&, DivVectorType&);
	DivCountType DiversityDifference(const IGridPoint&, DivVectorType&, TTranslation, bool);
	bool         NextBorderPoint(IGridPoint&, TTranslation, bool forward);

	TForm  m_Form;
	T      m_InputUpperBound = T();
	DivCountGridType m_DivOutput;
};

//====================================================

template <typename ZoneType, typename ResultType>
void Districting(const UGrid<const ZoneType>& input, const UGrid<ResultType>& output, ResultType* resNrDistricts, bool rule8)
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
