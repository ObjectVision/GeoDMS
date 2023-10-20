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
#if !defined(__EXPRCALCULATOR_H)
#define __EXPRCALCULATOR_H

#include "AbstrCalculator.h"

// *****************************************************************************
// Section:     ExprCalculator Interface
// *****************************************************************************

class ExprCalculator : public AbstrCalculator // TODO G8: RENAME TO ParsedExprKey
{
	typedef AbstrCalculator base_type;
public:
	ExprCalculator(const TreeItem* context, WeakStr expr, CalcRole cr);

//	override AbstrCalculator
	LispRef GetLispExprOrg() const override; 

	bool      CheckSyntax() const override;
	SharedStr GetExpr    () const override { return m_Expression; }
	void WriteHtmlExpr(OutStreamBase& outStream) const override;

private:
	SharedStr                 m_Expression;
};

// *****************************************************************************

#endif // __EXPRCALCULATOR_H
