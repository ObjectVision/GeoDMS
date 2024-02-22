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

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "mci/CompositeCast.h"

#include "DataArray.h"
#include "DataItemClass.h"
#include "TreeItemClass.h"
#include "AbstrUnit.h"
#include "UnitClass.h"

#include <set>

typedef UInt32            PointIndex;
typedef Point<PointIndex> PointIndexPair;

typedef std::pair<DPoint, PointIndex> WaveFrontElem;

struct Edge
{
	PointIndex 
		m_LeftPoint, 
		m_RightPoint;
	double fx, fy, fc;

};

struct HalfEdge
{
	PointIndex 
		m_LeftPoint, 
		m_RightPoint;


};

struct WaveFront : std::set<WaveFrontElem>
{
};

enum WaveEventType { WE_AddPoint, WE_Collapse };
struct WaveEvent
{

	WaveEvent(WaveEventType type, PointIndex which, Float64 when)
		:	m_Type (type)
		,	m_Which(which)
		,	m_When (when)
	{}

	WaveEventType Type () const { return m_Type;  }
	PointIndex    Which() const { return m_Which; }
	Float64       When () const { return m_When;  }

	friend bool operator <(const WaveEvent& a, const WaveEvent& b)
	{
		return a.m_When < b.m_When;
	}

private:
	WaveEventType m_Type;
	PointIndex    m_Which;
	Float64       m_When;
};

struct WaveEventQueue : std::vector<WaveEvent>
{
	void AddEvent(const WaveEvent& ev)
	{
		push_back(ev);
		std::push_heap(begin(), end());
	}
	void PopTopEvent()
	{
		dms_assert(!empty());
		std::pop_heap(begin(), end());
		pop_back();
	}
};

template <
	typename ConstPointIter,
	typename PointIndexPairIter
>
PointIndexPairIter triangualize(
	ConstPointIter first, 
	ConstPointIter last,
	PointIndexPairIter outIter
)
{
	WaveEventQueue queue;
	WaveFront      waveFront;

	PointIndex c = 0;
	for (ConstPointIter i = first; 
		i != last; ++i, ++c)
	{
			queue.AddEvent(WaveEvent(WE_AddPoint, c, i->Col() ) );
	}

	while (!queue.empty())
	{
		if (queue.front().Type() == WE_AddPoint)
		{
			PointIndex which = queue.front().Which();
			std::pair<WaveFront::iterator, bool>
				insertResult = 
					waveFront.insert(
						WaveFrontElem(
							DPoint(
								first[which].Row(),
								first[which].Col()
							),
							which
						)
					);
			dms_assert(insertResult.second);
			if (insertResult.first != waveFront.begin())
			{
				//YYY
			}
			++insertResult.first;
			if (insertResult.first != waveFront.end())
			{
			}
			// XXX
		}
		else
		{
			dms_assert(queue.front().Type() == WE_Collapse);
			// XXX
		}
		queue.PopTopEvent();
	}

	return outIter;
}
						
// *****************************************************************************
//									Points2PolygonsOperator
// *****************************************************************************

template <class T>
class TriangualizeOperator : public UnaryOperator
{
	typedef T                      PointType;
	typedef std::vector<PointType>  PolygonType;
	typedef Unit<PointType>        PointUnitType;
	typedef DataArray<PolygonType> ResultType;	

	typedef DataArray<PointType>   Arg1Type;
			
public:
	TriangualizeOperator(AbstrOperGroup* gr)
		:	UnaryOperator(gr, ResultType::GetStaticClass(), 
				Arg1Type::GetStaticClass()
			) 
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 1);

		const AbstrDataItem* arg1A = debug_cast<const AbstrDataItem*>(args[0]);
		dms_assert(arg1A);

		const AbstrUnit* pointEntity = arg1A->GetAbstrDomainUnit();
		const AbstrUnit* valuesUnit  = arg1A->GetAbstrValuesUnit();

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(pointEntity, valuesUnit, ValueComposition::Sequence);
		
		if (mustCalc)
		{
			const Arg1Type* arg1 = const_array_cast<PointType>(arg1A);
			dms_assert(arg1);

			DataReadLock arg1Lock(arg1A);

			auto arg1Data = arg1->GetDataRead();

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res, dms_rw_mode::write_only_mustzero);

			ResultType* result = mutable_array_cast<PolygonType>(resLock);
			auto resData = result->GetDataWrite();

			UInt32 nrPoints = pointEntity->GetCount();

			dms_assert(arg1Data.size() == nrPoints);

			dms_assert(resData.size() == nrPoints);


			// planaire graaf heeft max 3n-6 edges
			//	nrPoints nrTriangles nrLines nrOuters
			//	3        1           3       3 
			//	4        3           6       3
			//	5        5           9       3
			//	nrTriangles*3 = nrLines*2 - nrOuters
			//
			//	een tin is planair

			//	nrPoints nrTriangles nrLines

			UInt32 maxNrTinLines = nrPoints * 3 - 6;


			std::vector<PointIndexPair> tin;
			tin.resize(maxNrTinLines);
			std::vector<PointIndexPair>::iterator
				tinEnd = 
					triangualize(
						arg1Data.begin(),
						arg1Data.end(),
						tin.begin()
					);
			dms_assert(tinEnd >= tin.begin());
			dms_assert(tinEnd <= tin.end()  );
			tin.erase(tinEnd, tin.end());

			// XXX invert indicence table 
			// XXX for each point sort neigbouring points and make polygon

			resLock.Commit();
		}
		return true;
	}
};

// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

#include "utl/TypeListOper.h"
#include "RtcTypeLists.h" 

namespace 
{
	CommonOperGroup cogTR("triangualize");

	tl_oper::inst_tuple_templ<typelists::seq_points, TriangualizeOperator, AbstrOperGroup*> trOPers(&cogTR);
}

/******************************************************************************/

