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

#if !defined( __STX_PRODDATABLOCK_H)
#define __STX_PRODDATABLOCK_H

#include "geo/Point.h"
#include "geo/Range.h"
#include "geo/Geometry.h"
#include "mci/ValueClassID.h"
#include "set/StackUtil.h"

#include "DataLocks.h"

#include "StringProd.h"
#include "SpiritTools.h"
#include "TextPosition.h"

struct AbstrDataBlockProd : private boost::noncopyable
{
	virtual ~AbstrDataBlockProd() {}

	void DoFirstIntervalValue();
	void DoSecondIntervalValue();

	// the following virtuals are overridden by both ConfigProduct and DataBlockAssignment
	virtual void DoArrayAssignment()  =0;

	void DoFirstPointValue();
	void DoSecondPointValue();
	void DoRgbValue1(UInt32);
	void DoRgbValue2(UInt32);
	void DoRgbValue3(UInt32);

	void DoFloatValue(const Float64&);
	void DoUInt64(UInt64);
	void DoBoolValue(bool v);
	void DoNullValue();
	void SetValueType(ValueClassID vid) { m_eValueType = vid; }
	void SetSign(bool sign) { m_bSignIsPlus = sign; }

	void ResetDataBlock() { m_nIndexValue = 0; }

	// data collection support
	row_id              m_nIndexValue = 0;

	ValueClassID        m_eAssignmentDomainType = VT_Unknown;     // type of interval or selector value

	Range<Float64>      m_FloatInterval;
	Range<DPoint>       m_DPointInterval;


	// basicValue elements
	ValueClassID        m_eValueType = VT_Unknown;   // type of basicValue
	StringProd          m_StringVal;
	bool                m_BoolVal = false;
	DPoint              m_DPointVal;
	Float64             m_FloatVal = 0.0;
	Int64               m_IntValAsInt64 = 0;  // the read integer or UNDEFINED(Int64)  if out of range(Int32)
	UInt64              m_IntValAsUInt64 = 0; // the read integer or UNDEFINED(UInt64) if out of range(UInt32)
	bool                m_bSignIsPlus = false;

protected:
	[[noreturn]] virtual void throwSemanticError(CharPtr msg)=0;
};


struct DataBlockProd : AbstrDataBlockProd
{
	DataBlockProd(AbstrDataItem* adi, SizeT elemCount);
	~DataBlockProd();

	void DoArrayAssignment() override;
	void Commit();

private:
	DataWriteLock         m_Lock;
	TileRef               m_TileLock;
	SizeT                 m_ElemCount;
	OwningPtr<AbstrValue> m_AbstrValue;

	[[noreturn]] void throwSemanticError(CharPtr msg) override;

	AbstrDataItem* CurrDI  () { return m_Lock.GetItem(); }
};

#endif
