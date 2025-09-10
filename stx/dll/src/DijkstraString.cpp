// Copyright (C) 1998-2023 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////
// 
// Purpose:
//   Parse a compact option string describing how a Dijkstra / OD / interaction
//   computation should behave and translate it into a bitwise combination of
//   DijkstraFlag values using Boost.Spirit.
//
// High level flow (pseudocode):
//   1. Initialize result flags to empty.
//   2. Define small helper AssignFlags() producing a functor that ORs a flag
//      into 'result' when a rule matches.
//   3. Build Boost.Spirit rules for each semantic group:
//        - dirRule: directionality (bidirectional variants / directed)
//        - startPointtRule: optional start point attributes + optional max_imp
//        - endPointtRule: required endpoint attributes
//        - impCutRule: optional impedance cut
//        - dstLimitRule: destination limiting
//        - dstEuclidicRule: Euclidean filter
//        - altLinkImpRule: alternative link impedance / attributes (+ production outputs)
//        - interactionRule: interaction model inputs (+ production outputs)
//        - tbRule: traceback production
//        - odRule: OD matrix related options + outputs
//        - verboseLoggingRule: enable verbose logging
//        - paramRule: master rule chaining the above in a fixed ordered,
//          optionally-present sequence separated by ';'.
//   4. Parse the incoming C string with paramRule.
//   5. If parsing not fully successful: extract failing substring and throw.
//   6. Return accumulated flags.
//
// Notes:
//   - Ordering is enforced: dirRule first, others optionally preceded by ';'.
//   - Each optional rule uses '!' (Spirit) to mean "try, but do not fail overall".
//   - '%' COMMA constructs comma-separated lists.
//   - Some rules allow nested optional COLON sections for production flags.
//   - This parser is intentionally strict to catch typos early.
//
// Potential pitfalls / maintenance tips:
//   - Adding a new flag: extend enum DijkstraFlag, add rule fragment + AssignFlags.
//   - Keep textual tokens stable; changing them breaks external configurations.
//   - If grammar grows large, consider refactoring into Spirit.Qi (newer) or a hand parser.
//   - startPointtRule / endPointtRule variable names retain legacy spelling (t double) to avoid broad renames.
//
#include "StxPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "DijkstraFlags.h"
#include "SpiritTools.h"

#include "ser/MoreStreamBuff.h"
#include "ser/FormattedStream.h"

// Helper: returns a functor usable by Boost.Spirit semantic actions
// that ORs 'extra' into 'result' when a rule matches.
auto AssignFlags(DijkstraFlag& result, DijkstraFlag extra)
{
	return [&result, extra](auto, auto)
	{
		result = DijkstraFlag(result | extra);
	};
}

// Parse textual Dijkstra options into a DijkstraFlag bitfield.
DijkstraFlag ParseDijkstraString(CharPtr str)
{
	DijkstraFlag result = DijkstraFlag(); // start with no flags set

	using boost::spirit::strlit;
	using boost::spirit::chlit;

	boost::spirit::chlit<> COMMA(','), LBRACE('('), RBRACE(')'), COLON(':');

	// Directionality / high-level mode
	boost::spirit::rule<> dirRule =
    strlit<>("bidirectional(link_flag)")[ AssignFlags(result, DijkstraFlag::BidirFlag) ]
		| strlit<>("bidirectional")[ AssignFlags(result, DijkstraFlag::Bidirectional) ]
		| strlit<>("directed");

	// Optional start point specification & optional max_imp production
	boost::spirit::rule<>  startPointtRule =
		strlit<>("startPoint")
		>>	!(LBRACE
			>>	(	strlit<>("Node_rel"   )[AssignFlags(result, DijkstraFlag::OrgNode)]
				|	strlit<>("impedance"  )[AssignFlags(result, DijkstraFlag::OrgImp )]
				|	strlit<>("OrgZone_rel")[AssignFlags(result, DijkstraFlag::OrgZone)]
				|   strlit<>("OrgZone_loc" )[AssignFlags(result, DijkstraFlag::OrgZoneLoc)]
				)
				%	COMMA
			>> RBRACE)
		>> !(COLON >> strlit<>("max_imp")[AssignFlags(result, DijkstraFlag::ProdOrgMaxImp)]);

	// Optional end point specification
	boost::spirit::rule<>  endPointtRule =
		strlit<>("endPoint")
		>>	LBRACE
		>>	(	strlit<>("Node_rel")[AssignFlags(result, DijkstraFlag::DstNode)]
			|	strlit<>("impedance")[AssignFlags(result, DijkstraFlag::DstImp)]
			|	strlit<>("DstZone_rel")[AssignFlags(result, DijkstraFlag::DstZone)]
			|   strlit<>("DstZone_loc")[AssignFlags(result, DijkstraFlag::DstZoneLoc)]
			)
			%	COMMA
		>> RBRACE;

	// Optional impedance cut
	boost::spirit::rule<>  impCutRule =
		strlit<>("cut(OrgZone_max_imp)")[AssignFlags(result, DijkstraFlag::ImpCut)];

	// Optional destination limiting rule
	boost::spirit::rule<>  dstLimitRule =
		strlit<>("limit(OrgZone_max_mass,DstZone_mass)")[AssignFlags(result, DijkstraFlag::DstLimit)];

	// Optional euclidic spatial filter
	boost::spirit::rule<>  dstEuclidicRule =
		strlit<>("euclid(maxSqrDist)")[AssignFlags(result, DijkstraFlag::UseEuclidicFilter)];

	// Alternative link impedance / attribute usage + optional production outputs
	boost::spirit::rule<>  altLinkImpRule =
		strlit<>("alternative")
		>>	LBRACE
		>>	(	strlit<>("link_imp")[AssignFlags(result, DijkstraFlag::UseAltLinkImp)]
			|	strlit<>("link_attr")[AssignFlags(result, DijkstraFlag::UseLinkAttr)]
			)
			% COMMA
		>> RBRACE
		>> !(COLON >>
			(	strlit<>("alt_imp")[AssignFlags(result, DijkstraFlag::ProdOdAltImpedance)]
			|	strlit<>("link_attr")[AssignFlags(result, DijkstraFlag::ProdOdLinkAttr)]
			)
			% COMMA
			);

	// Interaction model configuration + optional production outputs
	boost::spirit::rule<>  interactionRule =
		strlit<>("interaction")
		>> !(LBRACE >>
			(
				strlit<>("OrgZone_min")[AssignFlags(result, DijkstraFlag::OrgMinImp)]
			|	strlit<>("DstZone_min")[AssignFlags(result, DijkstraFlag::DstMinImp)]
			|	strlit<>("v_i")[AssignFlags(result, DijkstraFlag::InteractionVi)]
			|	strlit<>("w_j")[AssignFlags(result, DijkstraFlag::InteractionWj)]
			|	strlit<>("dist_decay")[AssignFlags(result, DijkstraFlag::DistDecay)]
			|	strlit<>("dist_logit(alpha,beta,gamma)")[AssignFlags(result, DijkstraFlag::DistLogit)]
			|	strlit<>("OrgZone_alpha")[AssignFlags(result, DijkstraFlag::InteractionAlpha)]
			)
			% COMMA
		>> RBRACE)
		>> !(COLON >>
				(
					strlit<>("NrDstZones")[AssignFlags(result, DijkstraFlag::ProdOrgNrDstZones)]
				|	strlit<>("D_i")[AssignFlags(result, DijkstraFlag::ProdOrgFactor)]
				|	strlit<>("M_ix")[AssignFlags(result, DijkstraFlag::ProdOrgDemand)]
				|   strlit<>("SumImp")[AssignFlags(result, DijkstraFlag::ProdOrgSumImp)]
				|	strlit<>("SumLinkAttr")[AssignFlags(result, DijkstraFlag::ProdOrgSumLinkAttr)]
				|	strlit<>("C_j")[AssignFlags(result, DijkstraFlag::ProdDstFactor)]
				|	strlit<>("M_xj")[AssignFlags(result, DijkstraFlag::ProdDstSupply)]
				|	strlit<>("Link_flow"     )[AssignFlags(result, DijkstraFlag::ProdLinkFlow )]
				)
				%	COMMA
			);

	// Trace back production (node path reconstruction)
	boost::spirit::rule<>  tbRule = strlit<>("node:TraceBack")[AssignFlags(result, DijkstraFlag::ProdTraceBack)];

	// OD matrix specification + optional productions
	boost::spirit::rule<>  odRule =
		(	strlit<>("od_uint64")[AssignFlags(result, DijkstraFlag::UInt64_Od)]
		|	strlit<>("od_uint32")
		|	strlit<>("od")
		)
		>> !(LBRACE
			>>	strlit<>("precalculateted_NrDstZones")[AssignFlags(result, DijkstraFlag::PrecalculatedNrDstZones)]
			>> RBRACE
			)
		>> !(COLON >>
				(
					strlit<>("impedance"  )   [AssignFlags(result, DijkstraFlag::ProdOdImpedance  )]
				|	strlit<>("OrgZone_rel")   [AssignFlags(result, DijkstraFlag::ProdOdOrgZone_rel)]
				|	strlit<>("DstZone_rel")   [AssignFlags(result, DijkstraFlag::ProdOdDstZone_rel)]
				|	strlit<>("StartPoint_rel")[AssignFlags(result, DijkstraFlag::ProdOdStartPoint_rel)]
				|	strlit<>("EndPoint_rel")  [AssignFlags(result, DijkstraFlag::ProdOdEndPoint_rel)]
				|	strlit<>("LinkSet")       [AssignFlags(result, DijkstraFlag::ProdOdLinkSet    )]
				)
				%	COMMA
			);

	// Verbose logging control
	boost::spirit::rule<>  verboseLoggingRule = strlit<>("verboseLogging")[AssignFlags(result, DijkstraFlag::VerboseLogging)];

	// Master parameter rule:
	// Enforces order: direction first, then optional semicolon-separated sections.
	boost::spirit::rule<>  paramRule =
		dirRule
		>> !(chlit<>(';') >> startPointtRule)
		>> !(chlit<>(';') >> endPointtRule)
		>> !(chlit<>(';') >> impCutRule)
		>> !(chlit<>(';') >> dstLimitRule)
		>> !(chlit<>(';') >> dstEuclidicRule)
		>> !(chlit<>(';') >> altLinkImpRule)
		>> !(chlit<>(';') >> interactionRule)
		>> !(chlit<>(';') >> tbRule)
		>> !(chlit<>(';') >> odRule)
		>> !(chlit<>(';') >> verboseLoggingRule)
		;

	// Execute parse
	auto info = boost::spirit::parse(str, paramRule);
	if (!info.full)
	{
		// Extract a substring around the failure and throw a descriptive error.
		SharedStr strAtProblemLoc = problemlocAsString(str, str+StrLen(str), info.stop);

		throwErrorF("parse dijkstra options",
			"syntax error at\n%s",
			strAtProblemLoc.c_str()
		);
	}

	return result;
}

