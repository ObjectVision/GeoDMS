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

#include "act/ActorVisitor.h"
#include "geo/Transform.h"
#include "geo/Conversions.h"

#include "Param.h"
#include "TreeItemClass.h"

#include "ShvUtils.h"

#include "CounterStacks.h"
#include "GraphicGrid.h"
#include "GraphVisitor.h"


GraphicGrid::GraphicGrid(ScalableObject* owner)
	:	base_type(owner)
	,	m_Brush( CreateSolidBrush(      RGB(0xFF, 0xFF, 0x80)), "GraphicGrid::CreateSolidBrush" )
	,	m_Pen  ( CreatePen(PS_SOLID, 0, RGB(0x0,  0x0,  0xFF)), "GraphicGrid::CreatePen" )
{
}

void GraphicGrid::Sync(TreeItem* viewContext, ShvSyncMode sm)
{
	base_type::Sync(viewContext, sm);
	SyncRef(m_Source_TL, viewContext, GetTokenID_mt("TL"), sm);
	SyncRef(m_Source_BR, viewContext, GetTokenID_mt("BR"), sm);
}


//	override GraphicObject virtuals for size & display of GraphicObjects
void GraphicGrid::DoUpdateView()
{
	dms_assert(m_Source_TL->GetAbstrDomainUnit()->GetCount() == 1);
	dms_assert(m_Source_BR->GetAbstrDomainUnit()->GetCount() == 1);

	PreparedDataReadLock tlLock(m_Source_TL.get_ptr());
	PreparedDataReadLock brLock(m_Source_BR.get_ptr());

	SetWorldClientRect(
		CrdRect(
			m_Source_TL->GetRefObj()->GetValueAsDPoint(0), 
			m_Source_BR->GetRefObj()->GetValueAsDPoint(0)
		)
	);
}

bool GraphicGrid::Draw(GraphDrawer& d) const
{
	HDC dc = d.GetDC();
	CrdRect wr = CalcWorldClientRect();

	wr &= d.GetWorldClipRect();

	ResumableCounter counter(d.GetCounterStacks(), false);

	if (counter.Value() == 0)
	{
		GRect sr = TRect2GRect(Convert<TRect>(d.GetTransformation().Apply(wr)));
		FillRect(dc, &sr, m_Brush);
	}
	++counter;
	if (counter.MustBreakOrSuspend())
		return true;

	wr = Convert<CrdRect>(Convert<SRect>(wr)); // round-off to integers;
	TRect sr = Convert<TRect>(d.GetTransformation().Apply(wr));

	CrdPoint factor = Abs( d.GetTransformation().Factor() );
	dms_assert(factor.first  > 0);
	dms_assert(factor.second > 0);
	MakeMax(factor, CrdPoint(1, 1));

	GdiObjectSelector<HPEN> penHolder(dc, m_Pen);

	CrdType right  = sr.second.first  + factor.first;
	CrdType bottom = sr.second.second + factor.second;
	if (counter.Value() == 1)
	{
		for (CrdType i=sr.first.second;  i <= bottom; i += factor.first)
		{
			MoveToEx(dc, TType2GType(sr.first.first), TType2GType(i), NULL);
			LineTo  (dc, TType2GType(right),   TType2GType(i));
		}
		++counter;
		if (counter.MustBreakOrSuspend()) 
			return true;
	}
	if (counter.Value() == 2)
	{
		for (CrdType j=sr.first.second; j <= right;  j += factor.second)
		{
			MoveToEx(dc, TType2GType(j), TType2GType(sr.first.second), NULL);
			LineTo  (dc, TType2GType(j), TType2GType(bottom));
		}
	}
	counter.Close();
	return false;
}


//	override virtual of Actor
ActorVisitState GraphicGrid::VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const
{
	if (visitor.Visit(m_Source_TL.get_ptr()) == AVS_SuspendedOrFailed) return AVS_SuspendedOrFailed;
	if (visitor.Visit(m_Source_BR.get_ptr()) == AVS_SuspendedOrFailed) return AVS_SuspendedOrFailed;

	return base_type::VisitSuppliers(svf, visitor);
}

#include "LayerClass.h"

IMPL_DYNC_SHVCLASS(GraphicGrid,ScalableObject)
