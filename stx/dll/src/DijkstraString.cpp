// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "StxPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "DijkstraFlags.h"
#include "SpiritTools.h"

#include "ser/MoreStreamBuff.h"
#include "ser/FormattedStream.h"

auto AssignFlags(DijkstraFlag& result, DijkstraFlag extra)
{
	return [&result, extra](auto, auto) { result = DijkstraFlag(result | extra);  };
}

DijkstraFlag ParseDijkstraString(CharPtr str)
{
	DijkstraFlag result = DijkstraFlag();

	using boost::spirit::strlit;
	using boost::spirit::chlit;

	boost::spirit::chlit<> COMMA(','), LBRACE('('), RBRACE(')'), COLON(':');

	boost::spirit::rule<> dirRule =
		  strlit<>("bidirectional(link_flag)")[ AssignFlags(result, DijkstraFlag::BidirFlag) ]
		| strlit<>("bidirectional")[ AssignFlags(result, DijkstraFlag::Bidirectional) ]
		| strlit<>("directed");

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

	boost::spirit::rule<>  impCutRule =
		strlit<>("cut(OrgZone_max_imp)")[AssignFlags(result, DijkstraFlag::ImpCut)];

	boost::spirit::rule<>  dstLimitRule =
		strlit<>("limit(OrgZone_max_mass,DstZone_mass)")[AssignFlags(result, DijkstraFlag::DstLimit)];

	boost::spirit::rule<>  dstEuclidicRule =
		strlit<>("euclid(maxSqrDist)")[AssignFlags(result, DijkstraFlag::UseEuclidicFilter)];

	
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

	boost::spirit::rule<>  orgMinRule =
		strlit<>("OrgZone_min")[AssignFlags(result, DijkstraFlag::OrgMinImp)];

	boost::spirit::rule<>  dstMinRule =
		strlit<>("DstZone_min")[AssignFlags(result, DijkstraFlag::DstMinImp)];

	boost::spirit::rule<>  orgAlphaRule =
		strlit<>("OrgZone_alpha")[AssignFlags(result, DijkstraFlag::InteractionAlpha)];

	boost::spirit::rule<>  distDecayRule = 
		strlit<>("dist_decay")[AssignFlags(result, DijkstraFlag::DistDecay)];

	boost::spirit::rule<>  distLogitRule =
	strlit<>("dist_logit(alpha,beta,gamma)")[AssignFlags(result, DijkstraFlag::DistLogit)];

	boost::spirit::rule<>  interactionRule =
		strlit<>("interaction")
		>> LBRACE
			>> !(orgMinRule >> COMMA)
			>> !(dstMinRule >> COMMA)
			>> "v_i,w_j"
			>> !(COMMA >> distDecayRule)
			>> !(COMMA >> distLogitRule)
			>> !(COMMA >> orgAlphaRule)
		>> RBRACE 
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
		
	boost::spirit::rule<>  tbRule = strlit<>("node:TraceBack")[AssignFlags(result, DijkstraFlag::ProdTraceBack)];

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

	boost::spirit::rule<>  verboseLoggingRule = strlit<>("verboseLogging")[AssignFlags(result, DijkstraFlag::VerboseLogging)];

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

	auto info = boost::spirit::parse(str, paramRule);
	if (!info.full)
	{
		SharedStr strAtProblemLoc = problemlocAsString(str, str+StrLen(str), info.stop);

		throwErrorF("parse dijkstra options",
			"syntax error at\n%s", 
			strAtProblemLoc.c_str()
		);
	}

	return result;
}

