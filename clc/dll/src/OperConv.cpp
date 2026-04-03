// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

// OperConv.cpp - Core conversion operator definitions
// Template instantiations are split into separate files for parallel compilation:
// - OperConvNumeric.cpp   : Numeric type conversions
// - OperConvPoint.cpp     : Point type conversions
// - OperConvSequence.cpp  : Sequence type conversions
// - OperConvMapping.cpp   : Mapping operators
// - OperConvTransform.cpp : RD2LatLong transformations

#include "ClcPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "OperConv.h"
#include "xml/XMLOut.h"

#include <mutex>

// *****************************************************************************
//			Exported CommonOperGroups (for convert operators)
// *****************************************************************************

CommonOperGroup cog_Convert(token::convert);
CommonOperGroup cog_RoundedConvert(token::rounded_convert);

// *****************************************************************************
//			ConstAttrOperator
// *****************************************************************************

CommonOperGroup cog_const(token::const_);

template <typename TR>
class ConstAttrOperator : public AbstrConstOperator
{
	typedef DataArray<TR> ResultType;
	typedef DataArray<TR> Arg1Type;

public:
	ConstAttrOperator()
		: AbstrConstOperator(&cog_const, ResultType::GetStaticClass(), composition_of_v<TR>)
	{
	}

	SharedPtr<const AbstrDataObject> CreateConstFunctor(const AbstrDataItem* arg1A, const AbstrUnit* arg2U MG_DEBUG_ALLOCATOR_SRC(SharedStr srcStr)) const override
	{
		auto tileDataRange = arg2U->GetTiledRangeData();
		auto maxTileSize = tileDataRange->GetMaxTileSize();

		auto valuesUnitA = AsUnit(arg1A->GetAbstrValuesUnit()->GetCurrRangeItem());
		auto valuesUnit = debug_cast<const Unit<field_of_t<TR>>*>(valuesUnitA);

		auto constTileFunctor = make_unique_ConstTileFunctor<TR>(tileDataRange.get(), valuesUnit->m_RangeDataPtr, maxTileSize, const_array_cast<TR>(arg1A)->GetIndexedValue(0) MG_DEBUG_ALLOCATOR_SRC_PARAM);
		return constTileFunctor.release();
	}

	void Calculate(AbstrDataObject* borrowedDataHandle, const AbstrDataItem* arg1A, tile_id t) const override
	{
		auto  argData = const_array_cast  <TR>(arg1A)->GetDataRead();
		auto resData = mutable_array_cast<TR>(borrowedDataHandle)->GetDataWrite(t, dms_rw_mode::write_only_all);

		assert(argData.size() == 1);

		typename ResultType::iterator
			i = resData.begin(),
			e = resData.end();
		for (; i != e; ++i)
			Assign(*i, argData[0]);
	}
};

// *****************************************************************************
//			OGR helpers
// *****************************************************************************

void OGRCheck(OGRSpatialReference* ref, OGRErr result, CharPtr format, const AbstrUnit* au)
{
	assert(ref);
	ValidateSpatialReferenceFromWkt(ref, format);

	assert(ref);
	if (result == OGRERR_NONE)
		return;

	CharPtr errMsg;
	switch (result) {
	case OGRERR_NOT_ENOUGH_DATA: errMsg = "Not enough Data";                       break;
	case OGRERR_NOT_ENOUGH_MEMORY: errMsg = "Not enough Memory";                     break;
	case OGRERR_UNSUPPORTED_GEOMETRY_TYPE: errMsg = "Unsupported Geometry Type";             break;
	case OGRERR_UNSUPPORTED_OPERATION: errMsg = "Unsupported Operation";                 break;
	case OGRERR_CORRUPT_DATA: errMsg = "Corrupt Data";                          break;
	case OGRERR_FAILURE: errMsg = "Failure";                               break;
	case OGRERR_UNSUPPORTED_SRS: errMsg = "Unsupported Spatial Reference System";  break;
	case OGRERR_INVALID_HANDLE: errMsg = "Invalid Handle";                        break;
	default: errMsg = "Error code not defined in ogr_code.h";                                break;
	}
	auto errMsgTxt = mySSPrintF("Cannot make a SpatialRefBlock from %s. OGRCheck code %d: %s"
		, format
		, result
		, errMsg);

	auto projErrStr = GDAL_ErrorFrame::GetProjectionContextErrorString();
	if (!projErrStr.empty())
	{
		errMsgTxt += "\n";
		errMsgTxt += projErrStr;
	}
	au->Fail(errMsgTxt, FailType::Data);
	au->ThrowFail();
}

const AbstrUnit* CompositeBase(const AbstrUnit* proj)
{
	const AbstrUnit* res = proj->GetCurrProjection()->GetCompositeBase();
	if (!res)
		return proj;
	return res;
}

bool ProjectionIsColFirst(OGRSpatialReference& srs)
{
	OGRAxisOrientation PROJCS_peOrientation;
	OGRAxisOrientation GEOGCS_peOrientation;
	srs.GetAxis("PROJCS", 0, &PROJCS_peOrientation);
	srs.GetAxis("GEOGCS", 0, &GEOGCS_peOrientation);
	bool projection_is_col_first = PROJCS_peOrientation == OGRAxisOrientation::OAO_East || GEOGCS_peOrientation == OGRAxisOrientation::OAO_East || PROJCS_peOrientation == OGRAxisOrientation::OAO_West || GEOGCS_peOrientation == OGRAxisOrientation::OAO_West;
	return projection_is_col_first;
}

leveled_critical_section cs_SpatialRefBlockCreation(item_level_type(0), ord_level_type::SpecificOperator, "SpatialRefBlock");

static std::mutex s_projMutex;

SpatialRefBlock::SpatialRefBlock()
{
	std::lock_guard guard(s_projMutex);
	m_ProjCtx = proj_context_create();
}

SpatialRefBlock::~SpatialRefBlock()
{
	if (m_Transformer)
		OGRCoordinateTransformation::DestroyCT(m_Transformer);

	std::lock_guard guard(s_projMutex);
	proj_context_destroy(m_ProjCtx);
}

void SpatialRefBlock::CreateTransformer()
{
	assert(!m_Transformer);
	OGRCoordinateTransformationOptions options;
	CPLSetConfigOption("OGR_CT_OP_SELECTION", "BEST_ACCURACY");
	m_Transformer = OGRCreateCoordinateTransformation(&m_Src, &m_Dst, options);
}

// *****************************************************************************
//			AsDataStr OPERATOR
// *****************************************************************************

#include "xml/XMLOut.h"

class AsDataStringOperator : public UnaryOperator
{
	typedef AbstrDataItem     Arg1Type;
	typedef DataArray<SharedStr> ResultType;

public:
	AsDataStringOperator(AbstrOperGroup* gr)
		: UnaryOperator(gr
			, ResultType::GetStaticClass()
			, Arg1Type::GetStaticClass()
		)
	{
	}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 1);

		const Arg1Type* arg1 = AsDataItem(args[0]);

		dms_assert(arg1);

		if (!resultHolder)
			resultHolder = CreateCacheParam<SharedStr>();

		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1);

			VectorOutStreamBuff vout;
			OutStream_DMS xout(&vout, nullptr);
			arg1->XML_DumpData(&xout);
			CharPtr dataBegin = vout.GetData();

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res);
			ResultType* result = mutable_array_cast<SharedStr>(resLock);

			Assign(result->GetDataWrite(no_tile, dms_rw_mode::write_only_all)[0], SharedStr(CharPtrRange(dataBegin, dataBegin + vout.CurrPos())));

			resLock.Commit();
		}
		return true;
	}
};

// *****************************************************************************
//			UnitGroups initialization
// *****************************************************************************

namespace {
	template <typename ...TL>
	auto GetUnitGroups()
	{
		int a[] = { ((void)GetUnitGroup<TL>(), 0)... };
		return 0;
	}

	int rrr = GetUnitGroups<Void, SPoint, WPoint, IPoint, UPoint, FPoint, DPoint, SharedStr>();
}

// *****************************************************************************
//			INSTANTIATION - Core operators only
// *****************************************************************************

namespace
{
	// String conversions (small)
	convertAndCastOpers<typelists::strings>::apply_TA<SharedStr> stringConvertAndCastOpers;

	// ConstAttrOperator
	tl_oper::inst_tuple_templ<typelists::fields, ConstAttrOperator> constAttrOpers;

	// Constant parameters
#define PI 3.14159265358979323846

	CommonOperGroup cog_true(token::true_);
	CommonOperGroup cog_false(token::false_);
	CommonOperGroup cog_greekpi(token::pi);
	CommonOperGroup cog_funcpi("pi");

	CommonOperGroup cog_null_b(token::null_b);
	CommonOperGroup cog_null_w(token::null_w);
	CommonOperGroup cog_null_u(token::null_u);
	CommonOperGroup cog_null_u64(token::null_u64);
	CommonOperGroup cog_null_c(token::null_c);
	CommonOperGroup cog_null_s(token::null_s);
	CommonOperGroup cog_null_i(token::null_i);
	CommonOperGroup cog_null_i64(token::null_i64);
	CommonOperGroup cog_null_f(token::null_f);
	CommonOperGroup cog_null_d(token::null_d);
	CommonOperGroup cog_null_sp(token::null_sp);
	CommonOperGroup cog_null_wp(token::null_wp);
	CommonOperGroup cog_null_ip(token::null_ip);
	CommonOperGroup cog_null_up(token::null_up);
	CommonOperGroup cog_null_fp(token::null_fp);
	CommonOperGroup cog_null_dp(token::null_dp);
	CommonOperGroup cog_null_str(token::null_str);

	ConstParamOperator<Bool>    cpt(&cog_true, true);
	ConstParamOperator<Bool>    cpf(&cog_false, false);
	ConstParamOperator<Float64> cppig(&cog_greekpi, PI);
	ConstParamOperator<Float64> cppif(&cog_funcpi, PI);

	ConstParamOperator<UInt8> cpp_null_b(&cog_null_b, UNDEFINED_VALUE(UInt8));
	ConstParamOperator<UInt16> cpp_null_w(&cog_null_w, UNDEFINED_VALUE(UInt16));
	ConstParamOperator<UInt32> cpp_null_u(&cog_null_u, UNDEFINED_VALUE(UInt32));
	ConstParamOperator<UInt64> cpp_null_u64(&cog_null_u64, UNDEFINED_VALUE(UInt64));
	ConstParamOperator<Int8> cpp_null_c(&cog_null_c, UNDEFINED_VALUE(Int8));
	ConstParamOperator<Int16> cpp_null_s(&cog_null_s, UNDEFINED_VALUE(Int16));
	ConstParamOperator<Int32> cpp_null_i(&cog_null_i, UNDEFINED_VALUE(Int32));
	ConstParamOperator<Int64> cpp_null_i64(&cog_null_i64, UNDEFINED_VALUE(Int64));

	ConstParamOperator<Float32> cpp_null_f(&cog_null_f, UNDEFINED_VALUE(Float32));
	ConstParamOperator<Float64> cpp_null_d(&cog_null_d, UNDEFINED_VALUE(Float64));

	ConstParamOperator<SPoint> cpp_null_sp(&cog_null_sp, UNDEFINED_VALUE(SPoint));
	ConstParamOperator<WPoint> cpp_null_wp(&cog_null_wp, UNDEFINED_VALUE(WPoint));
	ConstParamOperator<IPoint> cpp_null_ip(&cog_null_ip, UNDEFINED_VALUE(IPoint));
	ConstParamOperator<UPoint> cpp_null_up(&cog_null_up, UNDEFINED_VALUE(UPoint));
	ConstParamOperator<FPoint> cpp_null_fp(&cog_null_fp, UNDEFINED_VALUE(FPoint));
	ConstParamOperator<DPoint> cpp_null_dp(&cog_null_dp, UNDEFINED_VALUE(DPoint));
	ConstParamOperator<SharedStr> cpp_null_str(&cog_null_str, UNDEFINED_VALUE(SharedStr));

	CommonOperGroup cog_asDataString("asDataString");
	AsDataStringOperator g_AsDataString(&cog_asDataString);

} // end anonymous namespace
