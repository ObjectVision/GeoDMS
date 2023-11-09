// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__RTC_ACT_SUPPLIERVISITFLAG_H)
#define __RTC_ACT_SUPPLIERVISITFLAG_H

#include "act/ActorEnums.h"

enum class SupplierVisitFlag
{
	Parent = 0x0001,
	Domain = 0x0002,
	Values = 0x0004,
	DomainValues = Domain | Values,
	Signature = Parent | DomainValues,
	ExplicitSuppliers = 0x0008,
	DetermineCalc = 0x0010,
	NamedSuppliers = 0x0020,
	DataController = 0x0040, // AbstrCalculator -> MainController; all subsequent controllers are done automatically.
	DcArgs = 0x0080,
	SourceData = 0x0100, // result of a SubItem main-expression or main-reference
	Checker = 0x0200,
	ReadyDcsToo = 0x0400,

	Calc = DataController | DcArgs,

	//	Meta    = 0x0002, // Explicit Suppliers, FuncFC args that don't require delayed updating, such as TemplDC args, and ImplSupplFromIndirectProps
	//	Calc    = 0x0002, // Data processing and reading, Domain +Values Unit

	Update = Signature | ExplicitSuppliers | SourceData | NamedSuppliers,
	DetermineState = Update | Calc | Checker | ReadyDcsToo | DetermineCalc,

	Explain = NamedSuppliers | SourceData,

//	UpdateMetaInfo = 0x0010,
	Inspect = Explain | Checker | DetermineCalc,

	//	Check  = 0x0004, // IntegrityCheck
//	Commit = 0x0008, // Specific additional items such as PaletteData, 

	CalcAll = Calc | Explain | Update | ReadyDcsToo,
	InspectAll = CalcAll | Inspect,

	TemplateOrg = 0x1000, // use to visit also the template origin
	CDF         = 0x2000, // use to visit the cdf source item and its palette
	DIALOGDATA  = 0x4000,
	ImplSuppliers = 0x8000, // implicit suppliers

	CalcErrorSearch   = Update | ImplSuppliers,

	MetaAll     = Signature | TemplateOrg | CDF | DIALOGDATA | ImplSuppliers | NamedSuppliers,
	All         = CalcAll | MetaAll,
	
	StartSupplInterest = DetermineState & ~Signature // Signature Already explicitly done by StartInterest function
};

#endif // __RTC_ACT_SUPPLIERVISITFLAG_H
