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

template <typename T, typename D = SizeType>
struct Districter : SpatialAnalyzer<T>
{
	using DistrSelSeqType = sequence_traits<Bool>::seq_t;
	using DistrSelVecType = sequence_traits<Bool>::container_type;

	using DistrIdGridType = UGrid<D>;
	using typename SpatialAnalyzer<T>::DataGridType;
	using typename SpatialAnalyzer<T>::DataGridValType;

	D GetDistricts(const DataGridType& input, const UGrid<D>& output, bool rule8);
	void GetDistrict(const DataGridType& input, const DistrSelSeqType& output, const IGridPoint& seedPoint, IGridRect& resRect); // only for T==Bool?

private:
	void ConsiderPoint(IGridPoint seedPoint, D districtId, DataGridValType val, std::vector<IGridPoint>& stack);
	void GetDistrict(IGridPoint seedPoint, D districtId, bool rule8);
	bool FindFirstNotProcessedPoint(IGridPoint& foundPoint);
	void SetDistrictId(SizeType pos, D districtId);
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

	void InitFrom(const DataGridType& input, RadiusType radius, bool isCircle, const DivCountGridType& distrOutput);

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

//==================================================== SpatialAnalyzer: template member functions

template <typename T>
void SpatialAnalyzer<T>::Init(const DataGridType& input)
{
	DBG_START("SpatialAnalyzer", "Init", false);

	m_Input = input;
	m_Rectangle = IGridRect(IGridPoint(0, 0), Convert<IGridPoint>(input.GetSize()));


	ICoordType nrCols = input.GetSize().Col();
	MG_CHECK(nrCols > 0);

	m_NrCols = nrCols;
	m_Size = Cardinality(input.GetSize());

	DBG_TRACE(("Size() %d", GridSize()));

	MG_CHECK(m_Size > 0);
}

//==================================================== Districter: template member functions

template <typename T, typename DistrIdType>
void Districter<T, DistrIdType>::SetDistrictId(SizeType pos, DistrIdType districtId)
{
	m_DistrOutput.GetDataPtr()[pos] = districtId;
	m_Processed[pos] = true;
}

template <typename T, typename DistrIdType>
void Districter<T, DistrIdType>::SetDistrictId(SizeType pos, bool districtId)
{
	m_DistrBoolOutput[pos] = districtId;
	m_Processed[pos] = true;
}


template <typename T, typename D>
void Districter<T, D>::ConsiderPoint(IGridPoint seedPoint, D districtId, DataGridValType val, std::vector<IGridPoint>& stack)
{
	SizeType pos = this->Pos(seedPoint);
	if (Bool(m_Processed[pos])) return;
	if (this->m_Input.GetDataPtr()[pos] != val) return;

	SetDistrictId(pos, districtId);
	this->m_ResRect |= seedPoint;
	stack.push_back(seedPoint);
}

template <typename T, typename D>
bool Districter<T, D>::FindFirstNotProcessedPoint(IGridPoint& point)
{
	typename sequence_traits<T>::const_pointer inputData = this->m_Input.GetDataPtr();
	SizeType pos = this->Pos(point);
	SizeType end = this->GridSize();
	assert(pos < end);
	while (Bool(m_Processed[pos]) || !IsDefined(inputData[pos]))
		if (++pos == end)
			return false;
	assert(pos < end);
	point = this->GetPoint(pos);
	return true;
}

template <typename T, typename D>
void Districter<T, D>::GetDistrict(IGridPoint seedPoint, D districtId, bool rule8)
{
	assert(IsIncluding(this->m_Rectangle, seedPoint));

	auto pos = this->Pos(seedPoint);
	auto val = this->m_Input.GetDataPtr()[pos];
	SetDistrictId(pos, districtId);
	this->m_ResRect |= seedPoint;

	IGridRect reducedRect = Deflate(this->m_Rectangle, IGridPoint(1, 1));

	std::vector<IGridPoint> stack;
	while (true) {
		bool nonTop    = seedPoint.Row() > this->m_Rectangle.first.Row();
		bool nonBottom = seedPoint.Row() + 1 < this->m_Rectangle.second.Row();
		bool nonLeft   = seedPoint.Col() > this->m_Rectangle.first.Col();
		bool nonRight  = seedPoint.Col() + 1 < this->m_Rectangle.second.Col();

		if (nonTop   ) ConsiderPoint(rowcol2dms_order< ICoordType>(seedPoint.Row() - 1, seedPoint.Col()), districtId, val, stack);
		if (nonBottom) ConsiderPoint(rowcol2dms_order< ICoordType>(seedPoint.Row() + 1, seedPoint.Col()), districtId, val, stack);
		if (nonLeft  ) ConsiderPoint(rowcol2dms_order< ICoordType>(seedPoint.Row(), seedPoint.Col() - 1), districtId, val, stack);
		if (nonRight ) ConsiderPoint(rowcol2dms_order< ICoordType>(seedPoint.Row(), seedPoint.Col() + 1), districtId, val, stack);


		if (rule8)
		{
			if (nonTop    && nonLeft ) ConsiderPoint(rowcol2dms_order< ICoordType>(seedPoint.Row() - 1, seedPoint.Col() - 1), districtId, val, stack);
			if (nonTop    && nonRight) ConsiderPoint(rowcol2dms_order< ICoordType>(seedPoint.Row() - 1, seedPoint.Col() + 1), districtId, val, stack);
			if (nonBottom && nonLeft ) ConsiderPoint(rowcol2dms_order< ICoordType>(seedPoint.Row() + 1, seedPoint.Col() - 1), districtId, val, stack);
			if (nonBottom && nonRight) ConsiderPoint(rowcol2dms_order< ICoordType>(seedPoint.Row() + 1, seedPoint.Col() + 1), districtId, val, stack);
		}
		if (stack.empty())
			return;
		seedPoint = stack.back();
		stack.pop_back();
	}
}

template <typename T, typename D>
void Districter<T, D>::GetDistrict(const DataGridType& input, const DistrSelSeqType& output, const IGridPoint& seedPoint, IGridRect& resRect)
{
	m_DistrBoolOutput = output;
	this->m_ResRect = IGridRect();

	assert(input.size() == output.size()); // PRECONDITION
	this->Init(input);

	m_Processed = DistrSelVecType(this->GridSize(), false);
	//	vector_zero_n(m_Processed, GridSize());

	GetDistrict(seedPoint, true, false);

	resRect = this->m_ResRect;
}

template <typename T, typename D>
D Districter<T, D>::GetDistricts(const DataGridType& input, const UGrid<D>& output, bool rule8)
{
	m_DistrOutput = output;

	assert(input.GetSize() == output.GetSize()); // PRECONDITION;
	this->Init(input);

	auto point = this->m_Rectangle.first;

	fast_fill(
		m_DistrOutput.GetDataPtr(),
		m_DistrOutput.GetDataPtr() + this->GridSize(),
		UNDEFINED_VALUE(D)
	);
	m_Processed = DistrSelVecType(this->GridSize(), false);
	//	vector_zero_n(m_Processed, GridSize());
	
	D resNrDistricts = 0; bool isFirstDistrict = true;
	for (; FindFirstNotProcessedPoint(point); ++resNrDistricts)
	{
		if (!resNrDistricts && !isFirstDistrict)
			throwErrorF("district", "number of found districts exceeds the maximum of the chosen district operator that stores only %d bytes per cell", sizeof(D));

		GetDistrict(point, resNrDistricts, rule8);
		isFirstDistrict = false;
	}
	return resNrDistricts;
}

//==================================================== dispatching functions

template <typename ZoneType, typename ResultType>
ResultType Districting(const UGrid<const ZoneType>& input, const UGrid<ResultType>& output, bool rule8)
{ 
	return Districter<ZoneType, ResultType>().GetDistricts(input, output, rule8);
}

template <typename ZoneType>
void Diversity(const UGrid<const ZoneType>& input, ZoneType inputUpperBound, RadiusType radius, bool isCircle, UGrid<ZoneType>& divOutput)
{ 
	DiversityCalculator<ZoneType>().GetDiversity(input, inputUpperBound, radius, isCircle, divOutput);
}

// *****************************************************************************

#endif //!defined(__GEO_SPATIALANALYZER_H)
