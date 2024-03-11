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

	void Init(RadiusType radius, bool isCircle);
	void SetCenter(UGridPoint p) { m_Center = p; m_Defined = false; }
	bool Contains (UGridPoint p);

	bool NextContainedPoint(UGridPoint&);	// gives all points of form in Bitmap order (=lexicographic (row, col))
	bool NextBorderPoint(UGridPoint& p, TTranslation t); // all points in Translated order

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

	bool ContainsCentered(FormPoint);	
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

	SpatialAnalyzer(DataGridType input);

protected:
	DataGridType m_Input;
	SizeT        m_Size = 0;
	UCoordType   m_NrCols = 0;

	SizeT GridSize() const { return m_Size; }
	SizeType Pos(UGridPoint p)   
	{ 
		assert(p.Col() >= 0);
		assert(p.Row() >= 0);
		assert(p.Col() < m_Input.GetSize().Col());
		assert(p.Row() < m_Input.GetSize().Row());
		return m_NrCols * p.Row()+ p.Col();
	}
	UGridPoint GetPoint(SizeType pos) { return rowcol2dms_order<UCoordType>(pos / m_NrCols, pos % m_NrCols); }
};

template <typename T, typename D = SizeType>
struct Districter : SpatialAnalyzer<T>
{
	using DistrSelSeqType = sequence_traits<Bool>::seq_t;
	using DistrSelVecType = sequence_traits<Bool>::container_type;
	using DistrDataPtr = typename sequence_traits<D>::seq_t::iterator;

	using DistrIdGridType = UGrid<D>;
	using typename SpatialAnalyzer<T>::DataGridType;
	using typename SpatialAnalyzer<T>::DataGridValType;

	Districter(DataGridType input)
		: SpatialAnalyzer<T>(input)
	{}

	D GetDistricts(UGrid<D> output, bool rule8);
	void GetDistrict(DistrDataPtr output, UGridPoint seedPoint, UGridRect& resRect); // only for T==Bool?

private:
	void ConsiderPoint(DistrDataPtr output, UGridPoint seedPoint, D districtId, DataGridValType val, std::vector<IGridPoint>& stack);
	void GetDistrict  (DistrDataPtr output, UGridPoint seedPoint, D districtId, bool rule8);
	void SetDistrictId(DistrDataPtr output, SizeT pos, D districtId);
	bool FindFirstNotProcessedPoint(UGridPoint& foundPoint);

	UGridRect       m_ResRect;
	DistrSelVecType m_Processed;
};

template <typename T>
struct DiversityCalculator : SpatialAnalyzer<T>
{
	using DivCountType = T;
	using DivCountGridType = UGrid<DivCountType>;
	using typename SpatialAnalyzer<T>::DataGridType;
	using typename SpatialAnalyzer<T>::DataGridValType;

	DiversityCalculator(DataGridType input, DataGridValType inputUpperBound, RadiusType radius, bool isCircle);

	void GetDiversity(DivCountGridType output);

private:
	void DiversityCountIncremental(DivCountGridType output, IGridPoint, DivVectorType&, bool isFirstRow, bool isFirstCol, bool right);
	void DiversityCountVertical   (DivCountGridType output, IGridPoint, DivVectorType&, bool isFirstRow);
	void DiversityCountHorizontal (DivCountGridType output, IGridPoint, DivVectorType&, TTranslation);

	DivCountType DiversityDifference      (UGridPoint, DivVectorType&, TTranslation, bool doAdd);
	DivCountType DiversityCountAll        (UGridPoint, DivVectorType&);

	bool         NextBorderPoint(UGridPoint&, TTranslation, bool forward);

	TForm  m_Form;
	T      m_InputUpperBound = 0;
};

//==================================================== SpatialAnalyzer: template member functions

template <typename T>
SpatialAnalyzer<T>::SpatialAnalyzer(DataGridType input)
	: m_Input( input)
{
	m_Size = Cardinality(input.GetSize());
	MG_CHECK(m_Size > 0);

	m_NrCols = input.GetSize().Col();
	MG_CHECK(m_NrCols > 0);
}

//==================================================== Districter: template member functions

template <typename T, typename DistrIdType>
void Districter<T, DistrIdType>::SetDistrictId(DistrDataPtr output, SizeT pos, DistrIdType districtId)
{
	output[pos] = districtId;
	m_Processed[pos] = true;
}

template <typename T, typename D>
void Districter<T, D>::ConsiderPoint(DistrDataPtr output, UGridPoint seedPoint, D districtId, DataGridValType val, std::vector<IGridPoint>& stack)
{
	auto pos = this->Pos(seedPoint);
	if (Bool(m_Processed[pos])) return;
	if (this->m_Input.GetDataPtr()[pos] != val) return;

	SetDistrictId(output, pos, districtId);
	m_ResRect |= seedPoint;
	stack.push_back(seedPoint);
}

template <typename T, typename D>
bool Districter<T, D>::FindFirstNotProcessedPoint(UGridPoint& point)
{
	typename sequence_traits<T>::const_pointer inputData = this->m_Input.GetDataPtr();
	auto pos = this->Pos(point);
	auto end = this->GridSize();
	assert(pos < end);
	while (Bool(m_Processed[pos]) || !IsDefined(inputData[pos]))
		if (++pos == end)
			return false;
	assert(pos < end);
	point = this->GetPoint(pos);
	return true;
}

template <typename T, typename D>
void Districter<T, D>::GetDistrict(DistrDataPtr output, UGridPoint seedPoint, D districtId, bool rule8)
{
	assert(IsStrictlyLower(seedPoint, this->m_Input.GetSize()));

	auto pos = this->Pos(seedPoint);
	auto val = this->m_Input.GetDataPtr()[pos];
	SetDistrictId(output, pos, districtId);
	m_ResRect |= seedPoint;

	std::vector<IGridPoint> stack;
	while (true) {
		bool nonTop    = seedPoint.Row() > 0;
		bool nonBottom = seedPoint.Row() + 1 < this->m_Input.GetSize().Row();
		bool nonLeft   = seedPoint.Col() > 0;
		bool nonRight  = seedPoint.Col() + 1 < this->m_Input.GetSize().Col();

		if (nonTop   ) ConsiderPoint(output, rowcol2dms_order<ICoordType>(seedPoint.Row() - 1, seedPoint.Col()), districtId, val, stack);
		if (nonBottom) ConsiderPoint(output, rowcol2dms_order<ICoordType>(seedPoint.Row() + 1, seedPoint.Col()), districtId, val, stack);
		if (nonLeft  ) ConsiderPoint(output, rowcol2dms_order<ICoordType>(seedPoint.Row(), seedPoint.Col() - 1), districtId, val, stack);
		if (nonRight ) ConsiderPoint(output, rowcol2dms_order<ICoordType>(seedPoint.Row(), seedPoint.Col() + 1), districtId, val, stack);

		if (rule8)
		{
			if (nonTop    && nonLeft ) ConsiderPoint(output, rowcol2dms_order<ICoordType>(seedPoint.Row() - 1, seedPoint.Col() - 1), districtId, val, stack);
			if (nonTop    && nonRight) ConsiderPoint(output, rowcol2dms_order<ICoordType>(seedPoint.Row() - 1, seedPoint.Col() + 1), districtId, val, stack);
			if (nonBottom && nonLeft ) ConsiderPoint(output, rowcol2dms_order<ICoordType>(seedPoint.Row() + 1, seedPoint.Col() - 1), districtId, val, stack);
			if (nonBottom && nonRight) ConsiderPoint(output, rowcol2dms_order<ICoordType>(seedPoint.Row() + 1, seedPoint.Col() + 1), districtId, val, stack);
		}
		if (stack.empty())
			return;
		seedPoint = stack.back();
		stack.pop_back();
	}
}

template <typename T, typename D>
void Districter<T, D>::GetDistrict(DistrDataPtr output, UGridPoint seedPoint, UGridRect& resRect)
{
	m_Processed = DistrSelVecType(this->GridSize(), false);
	//	vector_zero_n(m_Processed, GridSize());

	m_ResRect = UGridRect();

	GetDistrict(output, seedPoint, true, false);

	resRect = m_ResRect;
}

template <typename T, typename D>
D Districter<T, D>::GetDistricts(UGrid<D> output, bool rule8)
{
	assert(this->m_Input.GetSize() == output.GetSize()); // PRECONDITION;

	auto point = UGridPoint(0, 0);

	fast_undefine(output.begin(), output.end());

	m_Processed = DistrSelVecType(this->GridSize(), false);
	//	vector_zero_n(m_Processed, GridSize());
	
	D resNrDistricts = 0; bool isFirstDistrict = true;
	for (; FindFirstNotProcessedPoint(point); ++resNrDistricts)
	{
		if (!resNrDistricts && !isFirstDistrict)
			throwErrorF("district", "number of found districts exceeds the maximum of the chosen district operator that stores only %d bytes per cell", sizeof(D));

		GetDistrict(output.GetDataPtr(), point, resNrDistricts, rule8);
		isFirstDistrict = false;
	}
	return resNrDistricts;
}

//==================================================== dispatching functions

template <typename ZoneType, typename ResultType>
ResultType Districting(UGrid<const ZoneType> input, UGrid<ResultType> output, bool rule8)
{ 
	return Districter<ZoneType, ResultType>(input).GetDistricts(output, rule8);
}

template <typename ZoneType>
void Diversity(UGrid<const ZoneType> input, ZoneType inputUpperBound, RadiusType radius, bool isCircle, UGrid<ZoneType> divOutput)
{ 
	// TODO: LowerBound
	DiversityCalculator<ZoneType>(input, inputUpperBound, radius, isCircle).GetDiversity(divOutput);
}

// *****************************************************************************

#endif //!defined(__GEO_SPATIALANALYZER_H)
