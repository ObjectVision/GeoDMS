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
		GRect sr = Convert<GRect>(d.GetTransformation().Apply(wr));
		FillRect(dc, &sr, m_Brush);
	}
	++counter;
	if (counter.MustBreakOrSuspend())
		return true;

	wr = Convert<CrdRect>(Convert<SRect>(wr)); // round-off to integers;
	GRect sr = Convert<GRect>(d.GetTransformation().Apply(wr));

	CrdPoint factor = Abs( d.GetTransformation().Factor() );
	assert(factor.first  > 0);
	assert(factor.second > 0);
	MakeMax(factor, CrdPoint(1, 1));

	GdiObjectSelector<HPEN> penHolder(dc, m_Pen);

	CrdType right  = sr.right  + factor.first;
	CrdType bottom = sr.bottom + factor.second;
	if (counter.Value() == 1)
	{
		for (CrdType i=sr.top;  i <= bottom; i += factor.second)
		{
			MoveToEx(dc, sr.left, i, NULL);
			LineTo  (dc,right,   i);
		}
		++counter;
		if (counter.MustBreakOrSuspend()) 
			return true;
	}
	if (counter.Value() == 2)
	{
		for (CrdType j=sr.left; j <= right;  j += factor.first)
		{
			MoveToEx(dc, j, sr.top, NULL);
			LineTo  (dc, j, bottom);
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
