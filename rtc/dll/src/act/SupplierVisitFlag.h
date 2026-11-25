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
	ScanSupplTree = 0x0800,
	ExportInfo = 0x10000, // the bits 0x1000 to 0x8000 are TemplateOrg, CDF, DIALOGDATA and ImplSuppliers, which are defined further down

	Calc = DataController | DcArgs,
	//	Meta    = 0x0002, // Explicit Suppliers, FuncFC args that don't require delayed updating, such as TemplDC args, and ImplSupplFromIndirectProps
	//	Calc    = 0x0002, // Data processing and reading, Domain +Values Unit

	Update = DomainValues | ExplicitSuppliers | SourceData | NamedSuppliers | ExportInfo,
	UpdateForDataPrep = Calc,
	UpdateForCommit = ExportInfo,
	UpdateForValidation = Update & ~Calc & ~ExportInfo,

	UpdateSupplMetaInfo = Parent | Update | ScanSupplTree,
	DetermineState = (UpdateSupplMetaInfo & ~ExportInfo) | Calc | Checker | ReadyDcsToo | DetermineCalc, // == old ForDataPrep(Calc) | ForValidation(UpdateSupplMetaInfo&~Calc&~ExportInfo) | Calc | Checker | ReadyDcsToo | DetermineCalc

	Explain = NamedSuppliers | SourceData,

	//	UpdateMetaInfo = 0x0010,
	Inspect = Explain | Checker | DetermineCalc,


	//	Check  = 0x0004, // IntegrityCheck
//	Commit = 0x0008, // Specific additional items such as PaletteData, 

	CalcAll = Calc | Explain | Update | ReadyDcsToo,

	InspectAll = CalcAll | Inspect,

	// #1224: WITHOUT ReadyDcsToo, deliberately. The fence scan asks which suppliers can put an item
	// behind a phase fence, and the static argument policy already answers that: FuncDC::VisitSuppliers
	// skips an argument the engine never calculates (MustCalcArg false, i.e. calc_never -- the container
	// of SubItem_PropValues, the source of PhaseContainer itself), but ONLY when ReadyDcsToo is off.
	// With it on, such an argument is visited anyway and its fenced phase folds into the maximum, so an
	// item that merely reads the STRUCTURE of a fenced result inherits that result's phase and every
	// indirect expression over it is then rejected by EvaluateExpr.
	//
	// Reading the structure needs no fenced calculation: PhaseContainer materialises its mirror tree as
	// meta-info before the phase runs. An argument whose value the engine never computes therefore
	// cannot enter the fence, and must not raise the phase number. Arguments that ARE calculated
	// (calc_as_result / calc_always) are still visited, so #1199 keeps rejecting the case it was
	// written for: an indirect expression that reads a VALUE from behind the fence.
	FenceNumberScan = (CalcAll & ~ReadyDcsToo) | ScanSupplTree,

	TemplateOrg = 0x1000, // use to visit also the template origin
	CDF = 0x2000, // use to visit the cdf source item and its palette
	DIALOGDATA = 0x4000,
	ImplSuppliers = 0x8000, // implicit suppliers

	CalcErrorSearch = Update | ImplSuppliers | Checker | ScanSupplTree,

	MetaAll = Signature | TemplateOrg | CDF | DIALOGDATA | ImplSuppliers | NamedSuppliers,
	All = CalcAll | MetaAll,

	IntegrityChecked = (All | DetermineCalc) & ~CDF & ~DIALOGDATA,
	IntegrityCheckedForDataPrep = Calc,
	IntegrityCheckedForCommit = ExportInfo,
	IntegrityCheckedForValidation = IntegrityChecked & (~IntegrityCheckedForDataPrep) & (~IntegrityCheckedForCommit),

	StartSupplInterest = (DetermineState | ExportInfo) & ~Signature // Signature Already explicitly done by StartInterest function
};

// Each single-bit enumerator must have a bit of its own, or a test of one of them also answers for
// another. Their sum equals their bitwise or exactly when no two of them share a bit. A new single
// bit is added to the list below.
template <SupplierVisitFlag... Bits>
inline constexpr bool SupplierVisitFlag_AreDistinctBits =
	((unsigned(Bits) != 0 && (unsigned(Bits) & (unsigned(Bits) - 1)) == 0) && ...)
	&& ((unsigned(Bits) + ...) == (unsigned(Bits) | ...));

static_assert(SupplierVisitFlag_AreDistinctBits<
		SupplierVisitFlag::Parent, SupplierVisitFlag::Domain, SupplierVisitFlag::Values
	,	SupplierVisitFlag::ExplicitSuppliers, SupplierVisitFlag::DetermineCalc, SupplierVisitFlag::NamedSuppliers
	,	SupplierVisitFlag::DataController, SupplierVisitFlag::DcArgs, SupplierVisitFlag::SourceData
	,	SupplierVisitFlag::Checker, SupplierVisitFlag::ReadyDcsToo, SupplierVisitFlag::ScanSupplTree
	,	SupplierVisitFlag::ExportInfo, SupplierVisitFlag::TemplateOrg, SupplierVisitFlag::CDF
	,	SupplierVisitFlag::DIALOGDATA, SupplierVisitFlag::ImplSuppliers
	>
,	"two single-bit enumerators of SupplierVisitFlag share a bit, or one of them is not a single bit"
);

#endif // __RTC_ACT_SUPPLIERVISITFLAG_H
