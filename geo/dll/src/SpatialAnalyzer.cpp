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
#include "geo/Point.h"
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

bool TForm::Contains (UGridPoint p)
{
	return ContainsCentered(FormPoint(p - m_Center)); 
}

bool TForm::ContainsCentered(FormPoint p)	
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
		case o_1_1:	return	rowcol2dms_order<FormType>(p1.Row(), p1.Col());
		case o_1_2:	return	rowcol2dms_order<FormType>(p1.Col(), p1.Row());

		case o_2_1:	return	rowcol2dms_order<FormType>(p1.Row(), -p1.Col());
		case o_2_2:	return	rowcol2dms_order<FormType>(-p1.Col(), p1.Row());

		case o_3_1:	return	rowcol2dms_order<FormType>(-p1.Row(), -p1.Col());
		case o_3_2:	return	rowcol2dms_order<FormType>(-p1.Col(), -p1.Row());

		case o_4_1:	return	rowcol2dms_order<FormType>(-p1.Row(), p1.Col());
		case o_4_2:	return	rowcol2dms_order<FormType>(p1.Col(), -p1.Row());
	}
	return FormPoint();
}

bool TForm::NextContainedPoint(UGridPoint& p)
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
		p = m_Center + m_CurrentPoint;

	return m_Defined;
}

bool TForm::NextBorderPoint(UGridPoint& p, TTranslation t)
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
			case tIncRow: p += UGridPoint(Translate(m_CurrentPoint, o_1_1)); break;
			case tDecRow: p += UGridPoint(Translate(m_CurrentPoint, o_4_1)); break;
			case tDecCol: p += UGridPoint(Translate(m_CurrentPoint, o_4_2)); break;
			case tIncCol: p += UGridPoint(Translate(m_CurrentPoint, o_1_2)); break;
		}
	}
	return m_Defined;
}

// *****************************************************************************
//											DiversityCalculator
// *****************************************************************************

template <typename T>
DiversityCalculator<T>::DiversityCalculator(DataGridType input, DataGridValType inputUpperBound, RadiusType radius, bool isCircle)
	:	SpatialAnalyzer<T>(input)
{
	m_InputUpperBound = inputUpperBound;
	m_Form.Init(radius, isCircle);
}

template <typename T>
void DiversityCalculator<T>::GetDiversity(DivCountGridType output)
{
	DBG_START("SpatialAnalyzer", "GetDiversity", true);
	MG_CHECK(this->m_Input.GetSize() == output.GetSize()); // PRECONDITIOON;

	DivVectorType divVector(m_InputUpperBound, DivCountType(0));

	ICoordType rowBegin = 0;
	ICoordType colBegin = 0;
	ICoordType rowEnd = this->m_Input.GetSize().Row();
	ICoordType colEnd = this->m_Input.GetSize().Col();

	for (ICoordType row = rowBegin; row != rowEnd; row++)
	{
		bool
			isFirstRow = (row == 0),
			right      = Even(row);

		if (right)
		{
			for (ICoordType col = 0; col != colEnd; col++)
				DiversityCountIncremental(output, shp2dms_order(col, row), divVector, isFirstRow, col == colBegin, true);
		}
		else
		{
			for (ICoordType col = colEnd - 1; col >= 0; col--)
				DiversityCountIncremental(output, shp2dms_order(col, row), divVector, isFirstRow, col == colEnd - 1, false);
		}
	}
}

template <typename T>
void DiversityCalculator<T>::DiversityCountIncremental(DivCountGridType output, IGridPoint point, DivVectorType& divVector, bool isFirstRow, bool isFirstCol, bool right)
{
	if (isFirstCol)
		DiversityCountVertical(output, point, divVector, isFirstRow);
	else
		DiversityCountHorizontal(output, point, divVector, right ? tIncCol : tDecCol);
}

template <typename T>
void DiversityCalculator<T>::DiversityCountVertical(DivCountGridType output, IGridPoint point, DivVectorType& divVector, bool isFirstRow)
{
	DBG_START("SpatialAnalyzer", "DiversityCountVertical", false);

	IGridPoint prevPoint = point; prevPoint.Row()--;

	DBG_TRACE(("isFirstRow %d", isFirstRow));

	output.GetDataPtr()[this->Pos(point)] =
		(
			isFirstRow
			?	DiversityCountAll(prevPoint,	divVector)
			:	output.GetDataPtr()[this->Pos(prevPoint)]
				-	DiversityDifference(prevPoint, divVector, tIncRow, false)
		)
		+	DiversityDifference(point, divVector, tIncRow, true);
}

template <typename T>
void DiversityCalculator<T>::DiversityCountHorizontal(DivCountGridType output, IGridPoint point, DivVectorType& divVector, TTranslation trans)
{
	UGridPoint prevPoint = point;
	if (trans == tIncCol) --prevPoint.Col();
	if (trans == tDecCol) ++prevPoint.Col();

	assert(IsStrictlyLower(prevPoint, this->m_Input.GetSize()));
	output.GetDataPtr()[this->Pos(point)] = output.GetDataPtr()[this->Pos(prevPoint)]
		-	DiversityDifference(prevPoint, divVector, trans, false)
		+	DiversityDifference(point, divVector, trans, true);
}

template <typename T>
typename DiversityCalculator<T>::DivCountType
DiversityCalculator<T>::DiversityCountAll(UGridPoint center, DivVectorType& divVector)
{
	UGridPoint point;
	DivCountType divCount = 0;

	m_Form.SetCenter(center);
	dms_assert(m_InputUpperBound == divVector.size());

	while (m_Form.NextContainedPoint(point))
	{
		if (IsStrictlyLower(point, this->m_Input.GetSize()))
		{
			SizeType val = this->m_Input.GetDataPtr()[this->Pos(point)];

			if (val < m_InputUpperBound)
			{
				if (divVector[val] == 0)
					divCount++;
			
				divVector[val]++;
				if (divVector[val] == 0)
					throwErrorF("Diversity", "Numeric overflow in counting diversity around location %s", AsString(point).c_str());
			}
		}
	}
	return	divCount;
}

template <typename T>
typename DiversityCalculator<T>::DivCountType
DiversityCalculator<T>::DiversityDifference(UGridPoint center, DivVectorType& divVector, TTranslation trans, bool doAdd)
{
	UGridPoint   point;
	DivCountType divCount = 0;

	m_Form.SetCenter(center);
	assert(m_InputUpperBound == divVector.size());

	if (doAdd)
	{
		while (NextBorderPoint(point, trans, true))
		{
			if (IsStrictlyLower(point, this->m_Input.GetSize()))
			{
				SizeType val = this->m_Input.GetDataPtr()[this->Pos(point)];
				if (val < m_InputUpperBound)
				{
					if (divVector[val] == 0)
						divCount++;

					divVector[val]++;
					if (divVector[val] == 0)
						throwErrorF("Diversity", "Numeric overflow in counting diversity around location %s", AsString(point).c_str());
				}
			}
		}
	} else
	{
		while (NextBorderPoint(point, trans, false))
		{
			if (IsStrictlyLower(point, this->m_Input.GetSize()))
			{
				SizeType val = this->m_Input.GetDataPtr()[this->Pos(point)];
				if (val < m_InputUpperBound)
				{
					assert(divVector[val]); // logic of caller required divVector was incremented before for this value.
					if (--divVector[val] == 0)
						divCount++;
				}
			}
		}
	}

	return	divCount;
}

template <typename T>
bool DiversityCalculator<T>::NextBorderPoint(UGridPoint& point, TTranslation trans, bool add)
{
	switch (trans)
	{
		case tDecCol: return m_Form.NextBorderPoint(point, add ? tDecCol : tIncCol);
		case tIncCol: return m_Form.NextBorderPoint(point, add ? tIncCol : tDecCol);
		case tIncRow: return m_Form.NextBorderPoint(point, add ? tIncRow : tDecRow);
		case tDecRow: return m_Form.NextBorderPoint(point, add ? tDecRow : tIncRow);
	}
	return false;
}


template DiversityCalculator<UInt32>;
template DiversityCalculator<UInt8>;