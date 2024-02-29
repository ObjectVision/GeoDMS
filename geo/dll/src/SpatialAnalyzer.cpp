// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "SpatialAnalyzer.h"

#include "dbg/debug.h"
#include "geo/Conversions.h"
#include "geo/PointOrder.h"
#include "mth/Mathlib.h"

// *****************************************************************************
//											TForm
// *****************************************************************************

void TForm::Init(RadiusType radius, bool isCircle)
{ 
	DBG_START("TForm", "Init", true);

	assert(radius <= MAX_VALUE(FormType));
	m_Defined	= false; 
	m_Radius	= radius;
	m_IsCircle	= isCircle;
	Float64 sqrRadiusAsF64 = Float64(m_Radius)* Float64(m_Radius);

	if (isCircle)
	{
		m_CirclePoint.resize(SizeT(m_Radius) + 1);

		for (RadiusType i = 0; i <= radius; i++)
			m_CirclePoint[i] = RadiusType( sqrt(sqrRadiusAsF64 - Float64(i)*Float64(i)) );
	}
	
	DBG_TRACE(("m_Radius %d, m_IsCircle %d", m_Radius, m_IsCircle));
}

bool TForm::Contains (const UGridPoint& p)
{
	return ContainsCentered(Convert<FormPoint>(p - m_Center)); 
}

bool TForm::ContainsCentered(const FormPoint& p)	
{ 
	if (abs(p.Row()) > m_Radius) return false;
	if (abs(p.Col()) > m_Radius) return false;

	if (!m_IsCircle) return true;

	if (abs(p.Row()) > m_CirclePoint[abs(p.Row())]) return false;
	if (abs(p.Col()) > m_CirclePoint[abs(p.Col())]) return false;

	return true;
}

bool TForm::GetOtherCoordinateCentered(FormType p1, FormType& p2)
{
	RadiusType up1 = abs(p1);
	bool res = up1 <= m_Radius;
	if (res) 
		p2	= m_IsCircle ? m_CirclePoint[up1] : m_Radius;
	return res;
}


TQuadrant TForm::Quadrant(const FormPoint& p)
{ 
	if (p.Col() >= 0)
		if (p.Row() >= 0)
			return q_1;
		else
			return q_4;
	else
		if (p.Row() >= 0)
			return q_2;
		else
			return q_3;
}

TOctant	TForm::Octant(const FormPoint& p)
{ 
	switch (Quadrant(p))
	{
		case	q_1	:	return abs(p.Col()) <= abs(p.Row()) ? o_1_1 : o_1_2;
		case	q_2	:	return abs(p.Col()) <= abs(p.Row()) ? o_2_1 : o_2_2;
		case	q_3	:	return abs(p.Col()) <= abs(p.Row()) ? o_3_1 : o_3_2;
		case	q_4	:	return abs(p.Col()) <= abs(p.Row()) ? o_4_1 : o_4_2;
		default:	return o_0;
	}
}

FormPoint TForm::Translate(const FormPoint& p1, TOctant o)
{ 
	switch (o)
	{
		case o_1_1:	return	FormPoint( p1.Row(), p1.Col());
		case o_1_2:	return	FormPoint( p1.Col(), p1.Row());

		case o_2_1:	return	FormPoint( p1.Row(),-p1.Col());
		case o_2_2:	return	FormPoint(-p1.Col(), p1.Row());

		case o_3_1:	return	FormPoint(-p1.Row(),-p1.Col());
		case o_3_2:	return	FormPoint(-p1.Col(),-p1.Row());

		case o_4_1:	return	FormPoint(-p1.Row(), p1.Col());
		case o_4_2:	return	FormPoint( p1.Col(),-p1.Row());
	}
	return FormPoint();
}

bool TForm::NextContainedPoint(IGridPoint& p)
{
	if (! m_Defined)
	{
		m_CurrentPoint.Row() = -m_Radius-1;

	} else
		m_CurrentPoint.Col()++;

	if (!m_Defined || ! ContainsCentered(m_CurrentPoint))
	{
		m_CurrentPoint.Row()++;

		FormType colCoord;
		if (m_Defined = GetOtherCoordinateCentered(m_CurrentPoint.Row(), colCoord))
			m_CurrentPoint.Col() = -colCoord; 
	}

	if (m_Defined)
		p = m_Center + Convert<UGridPoint>(m_CurrentPoint);

	return m_Defined;
}

inline bool TForm::NextBorderPoint(IGridPoint& p, TTranslation t)
{
	if (! m_Defined)
		m_CurrentPoint.Col() = -m_Radius;
	else
		m_CurrentPoint.Col()++;
	
	if (m_Defined = GetOtherCoordinateCentered(m_CurrentPoint.Col(), m_CurrentPoint.Row()))
	{
		p = m_Center;
		switch (t)
		{
			case tIncRow: p += Convert<IGridPoint>(Translate(m_CurrentPoint, o_1_1) ); break;
			case tDecRow: p += Convert<IGridPoint>(Translate(m_CurrentPoint, o_4_1) ); break;
			case tDecCol: p += Convert<IGridPoint>(Translate(m_CurrentPoint, o_4_2) ); break;
			case tIncCol: p += Convert<IGridPoint>(Translate(m_CurrentPoint, o_1_2) ); break;
		}
	}
	return m_Defined;
}

// *****************************************************************************
//											SpatialAnalyzer
// *****************************************************************************

template <typename T>
void SpatialAnalyzer<T>::Init(const DataGridType& input)
{
	DBG_START("SpatialAnalyzer", "Init", false);

	m_Input		  = input;
	m_Rectangle	  = IGridRect(IGridPoint(0, 0), Convert<IGridPoint>(input.GetSize()));


	ICoordType nrCols = input.GetSize().Col();
	MG_CHECK(nrCols>0);

	m_NrCols = nrCols;
	m_Size   = Cardinality(input.GetSize());

	DBG_TRACE(("Size() %d", GridSize()));

	MG_CHECK(m_Size>0);
}

template <typename T>
void DiversityCalculator<T>::Init(const DataGridType& input, RadiusType radius, bool isCircle, const DivCountGridType& output)
{
	dms_assert(input.GetSize() == output.GetSize()); // PRECONDITIOON;
	m_Form.Init(radius, isCircle);
	m_DivOutput = output;
	Init(input);
}

template <typename T, typename DistrIdType>
void Districter<T, DistrIdType>::GetDistricts(const UGrid<const T>& input, const UGrid<DistrIdType>& output, DistrIdType* resNrDistricts, bool rule8)
{
	m_DistrOutput = output;

	assert(input.GetSize() == output.GetSize()); // PRECONDITION;
	this->Init(input);

	auto point = this->m_Rectangle.first;

	fast_fill(
		m_DistrOutput.GetDataPtr(),
		m_DistrOutput.GetDataPtr() + this->GridSize(), 
		UNDEFINED_VALUE(DistrIdType)
	);
	m_Processed = DistrSelVecType(this->GridSize(), false);
//	vector_zero_n(m_Processed, GridSize());

	for (; FindFirstNotProcessedPoint(point); ++*resNrDistricts)
	{
		if (!resNrDistricts)
			throwErrorF("district", "number of found districts exceeds the maximum of the chosen district operator that stores only %d bytes per cell", sizeof(DistrIdType));

		GetDistrict(point, *resNrDistricts, rule8);
	}
}

template <typename T, typename DistrIdType>
void Districter<T, DistrIdType>::GetDistrict(const DataGridType& input, const DistrSelSeqType& output, const IGridPoint& seedPoint, IGridRect& resRect)
{
	m_DistrBoolOutput = output;
	this->m_ResRect = IGridRect();

	assert(input.size() == output.size()); // PRECONDITION
	Init(input);

	m_Processed = DistrSelVecType(this->GridSize(), false);
//	vector_zero_n(m_Processed, GridSize());

	GetDistrict(seedPoint, true, false);

	resRect = this->m_ResRect;
}

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


inline void UpdateRect(IGridRect& resRect, const IGridPoint& point, const DistrIdType* dummy)
{
	// nop
}

inline void UpdateRect(IGridRect& resRect, const IGridPoint& point, const bool* dummy)
{
	resRect |= point;
}

template <typename T, typename D>
void Districter<T, D>::ConsiderPoint(IGridPoint seedPoint, D districtId, DataGridValType val, std::vector<IGridPoint>& stack)
{
	SizeType pos = Pos(seedPoint);
	if (Bool(m_Processed[pos])) return;
	if (m_Input.GetDataPtr()[pos] != val) return;

	SetDistrictId(pos, districtId);
	UpdateRect(m_ResRect, seedPoint, &districtId);
	stack.push_back(seedPoint);
}


template <typename T, typename D>
void Districter<T, D>::GetDistrict(IGridPoint seedPoint, D districtId, bool rule8)
{
	assert(IsIncluding(m_Rectangle, seedPoint));

	SizeType pos = Pos(seedPoint);
	DataGridValType val = m_Input.GetDataPtr()[Pos(seedPoint)];
	SetDistrictId(pos, districtId);
	UpdateRect(m_ResRect, seedPoint, &districtId);

	IGridRect reducedRect = Deflate(m_Rectangle, IGridPoint(1,1) );

	std::vector<IGridPoint> stack;
	while(true) {
		bool nonTop    = seedPoint.Row()   > m_Rectangle.first .Row();
		bool nonBottom = seedPoint.Row()+1 < m_Rectangle.second.Row();
		bool nonLeft   = seedPoint.Col()   > m_Rectangle.first .Col();
		bool nonRight  = seedPoint.Col()+1 < m_Rectangle.second.Col();

		if (nonTop   ) ConsiderPoint(IGridPoint(seedPoint.Row()-1, seedPoint.Col()), districtId, val, stack);
		if (nonBottom) ConsiderPoint(IGridPoint(seedPoint.Row()+1, seedPoint.Col()), districtId, val, stack);
		if (nonLeft  ) ConsiderPoint(IGridPoint(seedPoint.Row(), seedPoint.Col()-1), districtId, val, stack);
		if (nonRight ) ConsiderPoint(IGridPoint(seedPoint.Row(), seedPoint.Col()+1), districtId, val, stack);


		if (rule8)
		{
			if (nonTop    && nonLeft ) ConsiderPoint(IGridPoint(seedPoint.Row()-1, seedPoint.Col()-1), districtId, val, stack);
			if (nonTop    && nonRight) ConsiderPoint(IGridPoint(seedPoint.Row()-1, seedPoint.Col()+1), districtId, val, stack);
			if (nonBottom && nonLeft ) ConsiderPoint(IGridPoint(seedPoint.Row()+1, seedPoint.Col()-1), districtId, val, stack);
			if (nonBottom && nonRight) ConsiderPoint(IGridPoint(seedPoint.Row()+1, seedPoint.Col()+1), districtId, val, stack);
		}
		if (stack.empty())
			return;
		seedPoint = stack.back();
		stack.pop_back();
	}
}

template <typename T, typename D>
void Districter<T, D>::FindFirstNotProcessedPoint(IGridPoint& point)
{
	typename sequence_traits<T>::const_pointer inputData = m_Input.GetDataPtr();
	SizeType pos = Pos(point);
	SizeType end = GridSize();
	dms_assert(pos < end);
	while (Bool(m_Processed[pos]) || !IsDefined(inputData[pos]))
		if (++pos == end)
			return false;
	dms_assert(pos < end);
	point = GetPoint(pos);
	return true;
}

template <typename T>
void DiversityCalculator<T>::GetDiversity(
		const DataGridType& input, 
		DataGridValType inputUpperBound, 
		RadiusType radius, bool isCircle, 
		const DivCountGridType& output)
{
	DBG_START("SpatialAnalyzer", "GetDiversity", true);
	m_InputUpperBound = inputUpperBound;
	Init(input, radius, isCircle, output);

	GetDiversity1();
}

template <typename T>
void DiversityCalculator<T>::GetDiversity1()
{
	DBG_START("DiversityCalculator", "GetDiversity1", false);

	DivVectorType divVector(m_InputUpperBound, DivCountType(0));

	ICoordType rowBegin= Top   (this->m_Rectangle);
	ICoordType rowEnd  = Bottom(this->m_Rectangle);
	ICoordType colBegin= Left  (this->m_Rectangle);
	ICoordType colEnd  = Right (this->m_Rectangle);
	for (ICoordType row = rowBegin; row != rowEnd; row++)
	{
		bool
			isFirstRow = (row == rowBegin),
			right      = Even(row);

		if (right)
		{
			for (ICoordType col = colBegin; col != colEnd; col++)
				DiversityCountIncremental(shp2dms_order(col, row), divVector, isFirstRow, col == colBegin, true);
		}
		else
		{
			for (ICoordType col = colEnd - 1; col >= colBegin; col--)
				DiversityCountIncremental(shp2dms_order(col, row), divVector, isFirstRow, col == colEnd - 1, false);
		}
	}
}

template <typename T>
void DiversityCalculator<T>::GetDiversity2()
{
	DBG_START("DiversityCalculator", "GetDiversity2", true);

	DivVectorType divVector(DivCountType(0), m_InputUpperBound);
	for (ICoordType row = this->m_Rectangle.first.Row(), rowEnd = this->m_Rectangle.second.Row(); row < rowEnd; ++row)
		for (ICoordType col = this->m_Rectangle.first.Col(), colEnd =  this->m_Rectangle.second.Col(); col < colEnd; ++col)
		{
			vector_fill_n(divVector, DivCountType(0), m_InputUpperBound);
			m_DivOutput->m_Data[Pos(shp2dms_order(col, row))] = DiversityCountAll(shp2dms_order(col, row), divVector);
		}
} // SpatialAnalyzer::GetDiversity2

template <typename T>
void DiversityCalculator<T>::DiversityCountIncremental(
	const IGridPoint& point, 
	DivVectorType& divVector, 
	bool isFirstRow, 
	bool isFirstCol, 
	bool right)
{
	if (isFirstCol)
		DiversityCountVertical(point, divVector, isFirstRow);
	else
		DiversityCountHorizontal(point, divVector, right ? tIncCol : tDecCol);
} // SpatialAnalyzer::DiversityCount

template <typename T>
void DiversityCalculator<T>::DiversityCountVertical(
	const IGridPoint& point, 
	DivVectorType& divVector, 
	bool isFirstRow)
{
	DBG_START("SpatialAnalyzer", "DiversityCountVertical", false);

	IGridPoint prevPoint = point; prevPoint.Row()--;

	DBG_TRACE(("isFirstRow %d", isFirstRow));

	m_DivOutput.GetDataPtr()[Pos(point)] =
		(
			isFirstRow
			?
			DiversityCountAll(prevPoint, divVector)
			:
			(
				m_DivOutput.GetDataPtr()[Pos(prevPoint)] 
				- 
				DiversityDifference(prevPoint, divVector, tIncRow, false)
			)
		)
		+	
		DiversityDifference(point, divVector, tIncRow, true);
} // SpatialAnalyzer::DiversityCountVertical

template <typename T>
void DiversityCalculator<T>::DiversityCountHorizontal(const IGridPoint& point, DivVectorType& divVector, TTranslation trans)
{
	IGridPoint prevPoint = point;
	if (trans == tIncCol) --prevPoint.Col();
	if (trans == tDecCol ) ++prevPoint.Col();

	dms_assert(IsIncluding(m_Rectangle, prevPoint));
	m_DivOutput.GetDataPtr()[Pos(point)]	=	m_DivOutput.GetDataPtr()[Pos(prevPoint)]
												-
												DiversityDifference(prevPoint, divVector, trans, false)
												+
												DiversityDifference(point, divVector, trans, true);
} // SpatialAnalyzer::DiversityCountHorizontal

template <typename T>
typename DiversityCalculator<T>::DivCountType
DiversityCalculator<T>::DiversityCountAll(const IGridPoint& center, DivVectorType& divVector)
{
	IGridPoint point;
	DivCountType divCount = 0;

	m_Form.SetCenter(center);
	dms_assert(m_InputUpperBound == divVector.size());

	while (m_Form.NextContainedPoint(point))
	{
		if (IsIncluding(m_Rectangle, point))
		{
			SizeType val = m_Input.GetDataPtr()[Pos(point)];

			if (val < m_InputUpperBound)
			{
				if (divVector[val] == 0)
					divCount++;
			
				divVector[val]++;
			}
		}
	}
	return	divCount;
} // SpatialAnalyzer::DiversityCount

template <typename T>
typename DiversityCalculator<T>::DivCountType
DiversityCalculator<T>::DiversityDifference(const IGridPoint& center, DivVectorType& divVector, TTranslation trans, bool add)
{
	IGridPoint    point;
	DivCountType divCount = 0;

	m_Form.SetCenter(center);
	dms_assert(m_InputUpperBound == divVector.size());

	if (add)
	{
		while (NextBorderPoint(point, trans, true))
		{
			if (IsIncluding(m_Rectangle, point))
			{
				SizeType val = m_Input.GetDataPtr()[Pos(point)];
				if (val < m_InputUpperBound)
				{
					if (++divVector[val] == 1)
						divCount++;
				}
			}
		}
	} else
	{
		while (NextBorderPoint(point, trans, false))
		{
			if (IsIncluding(m_Rectangle, point))
			{
				SizeType val = m_Input.GetDataPtr()[Pos(point)];
				if (val < m_InputUpperBound)
				{
					if (--divVector[val] == 0)
						divCount++;
				}
			}
		}
	}

	return	divCount;
} // SpatialAnalyzer::DiversityDifference

template <typename T>
bool DiversityCalculator<T>::NextBorderPoint(IGridPoint& point, TTranslation trans, bool add)
{
	switch (trans)
	{
		case tDecCol: return m_Form.NextBorderPoint(point, add ? tDecCol : tIncCol);
		case tIncCol: return m_Form.NextBorderPoint(point, add ? tIncCol : tDecCol);
		case tIncRow: return m_Form.NextBorderPoint(point, add ? tIncRow : tDecRow);
		case tDecRow: return m_Form.NextBorderPoint(point, add ? tDecRow : tIncRow);
	}
	return false;
} // SpatialAnalyzer::NextBorderPoint

// *****************************************************************************
//	INTERFACE FUNCTIONS: Districting
// *****************************************************************************

extern "C" {

void MDL_DistrictingI32(const UCInt32Grid& input, const UUInt32Grid& output, SizeType* resNrDistricts)
{
	Districter<Int32, UInt32>().GetDistricts(input, output, resNrDistricts, false);
}

void MDL_DistrictingUI32(const UCUInt32Grid& input, const UUInt32Grid& output, SizeType* resNrDistricts)
{
	Districter<UInt32>().GetDistricts(input, output, resNrDistricts, false);
}

void MDL_DistrictingUI8(const UCUInt8Grid& input, const UUInt32Grid& output, SizeType* resNrDistricts)
{
	Districter<UInt8>().GetDistricts(input, output, resNrDistricts, false);
}

void MDL_DistrictingBool(const UCBoolGrid& input, const UUInt32Grid& output, SizeType* resNrDistricts)
{
	Districter<Bool>().GetDistricts(input, output, resNrDistricts, false);
}

// *****************************************************************************
//	INTERFACE FUNCTIONS: Select 1 District
// *****************************************************************************

GEO_CALL void DMS_CONV MDL_DistrictUI32(
	const UCUInt32Grid&                input, 
	const sequence_traits<Bool>::seq_t output,
	const IGridPoint&                   seedPoint,
	IGridRect&                          resRect
)
{
	Districter<UInt32>().GetDistrict(input, output, seedPoint, resRect);
}

GEO_CALL void DMS_CONV MDL_DistrictUI16(
	const UCUInt16Grid& input,
	const sequence_traits<Bool>::seq_t output,
	const IGridPoint& seedPoint,
	IGridRect& resRect
)
{
	Districter<UInt16>().GetDistrict(input, output, seedPoint, resRect);
}

GEO_CALL void DMS_CONV MDL_DistrictUI8(
	const UCUInt8Grid&                 input, 
	const sequence_traits<Bool>::seq_t output,
	const IGridPoint&                   seedPoint,
	IGridRect&                          resRect
)
{
	Districter<UInt8>().GetDistrict(input, output, seedPoint, resRect);
}

GEO_CALL void DMS_CONV MDL_DistrictBool(
	const UCBoolGrid&                  input, 
	const sequence_traits<Bool>::seq_t output,
	const IGridPoint&                  seedPoint,
	IGridRect&                         resRect
)
{
	Districter<Bool>().GetDistrict(input, output, seedPoint, resRect);
}

// *****************************************************************************
//	INTERFACE FUNCTIONS: Diversity
// *****************************************************************************

void MDL_DiversityUI32(const UCUInt32Grid& input, UInt32 inputUpperBound, RadiusType radius, bool isCircle, const UUInt32Grid& divOutput)
{
	DiversityCalculator<UInt32>().GetDiversity(input, inputUpperBound, radius, isCircle, divOutput);
}

void MDL_DiversityUI8(const UCUInt8Grid& input, UInt8 inputUpperBound, RadiusType radius, bool isCircle, const UUInt8Grid& divOutput)
{
	DiversityCalculator<UInt8>().GetDiversity(input, inputUpperBound, radius, isCircle, divOutput);
}

} // extern "C"

