// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "geo/CheckedCalc.h"
#include "geo/Conversions.h"
#include "geo/StringArray.h"
#include "geo/PointOrder.h"
#include "geo/GeoSequence.h"
#include "mci/CompositeCast.h"
#include "ser/AsString.h"
#include "ser/StringStream.h"
#include "ser/SequenceArrayStream.h"
#include "ser/RangeStream.h"
#include "set/VectorFunc.h"
#include "stg/AbstrStorageManager.h"
#include "utl/Environment.h"
#include "utl/mySPrintF.h"
#include "LockLevels.h"

#include "Unit.h"
#include "Param.h"
#include "UnitClass.h"
#include "DataItemClass.h"
#include "TreeItemClass.h"
#include "LispTreeType.h"

#include "gdal/gdal_base.h"

#include "ConstOper.h"
#include "Lookup.h"
#include "OperUnit.h"

#include <functional>
#include <iterator>
#include <algorithm>

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
		:	AbstrConstOperator(&cog_const, ResultType::GetStaticClass(), composition_of_v<TR>)
	{}

	// Override Operator
	SharedPtr<const AbstrDataObject> CreateConstFunctor(const AbstrDataItem* arg1A, const AbstrUnit* arg2U MG_DEBUG_ALLOCATOR_SRC_ARG) const override
	{
		auto tileDataRange = arg2U->GetTiledRangeData();
		auto maxTileSize = tileDataRange->GetMaxTileSize();

		auto valuesUnitA = AsUnit(arg1A->GetAbstrValuesUnit()->GetCurrRangeItem());
		auto valuesUnit = debug_cast<const Unit<field_of_t<TR>>*>(valuesUnitA);

		auto constTileFunctor = make_unique_ConstTileFunctor<TR>(tileDataRange, valuesUnit->m_RangeDataPtr, maxTileSize, const_array_cast<TR>(arg1A)->GetIndexedValue(0) MG_DEBUG_ALLOCATOR_SRC_PARAM);
		return constTileFunctor.release();
	}

	void Calculate(AbstrDataObject* borrowedDataHandle, const AbstrDataItem* arg1A, tile_id t) const override
	{
		auto  argData = const_array_cast  <TR>(arg1A)->GetDataRead();
		auto resData = mutable_array_cast<TR>(borrowedDataHandle)->GetDataWrite(t);

		dms_assert(argData.size() == 1); // domain of arg1 was checked to be void
		
		typename ResultType::iterator
			i = resData.begin(),
			e = resData.end();
		for (; i!=e; ++i)
			Assign(*i, argData[0]);
	}
};


// *****************************************************************************
//			COORDINATE CONVERSION FUNCTIONS
// *****************************************************************************

DPoint rd2LatLong(DPoint rd) //shp_order
{
		double
			x	=	rd.first,
			y	=	rd.second;

		if (x < 0 || x > 290000)
			throwErrorD("Argument Error", "RD2LatLong requires x to be between 0 and 290000");
		if (y < 290000 || y > 630000)
			throwErrorD("Argument Error", "RD2LatLong requires y to be between 290000 and 630000");

		const double
			x0  = 155000.000,
			y0  = 463000.000,

			l0 =  5.387638889, // OL (x)
			f0 = 52.156160556, // NB (y)

			 a01=3236.0331637,  b10=5261.3028966,
			 a20= -32.5915821,  b11= 105.9780241,
			 a02=  -0.2472814,  b12=   2.4576469,
			 a21=  -0.8501341,  b30=  -0.8192156,
			 a03=  -0.0655238,  b31=  -0.0560092,
			 a22=  -0.0171137,  b13=   0.0560089,
			 a40=   0.0052771,  b32=  -0.0025614,
			 a23=  -0.0003859,  b14=   0.0012770,
			 a41=   0.0003314,  b50=   0.0002574,
			 a04=   0.0000371,  b33=  -0.0000973,
			 a42=   0.0000143,  b51=   0.0000293,
			 a24=  -0.0000090,  b15=   0.0000291;

		double 
			dx=(x-x0) / 100000, // horizontal distance from RD center in 100 km units
			dy=(y-y0) / 100000; // vertical   distance from RD center in 100 km units

		double
			dx2 = dx*dx,
			dx3 = dx2*dx,
			dx4 = dx2 * dx2,
			dx5 = dx4*dx,

			dy2 = dy *dy,
			dy3 = dy2*dy,
			dy4 = dy2*dy2,
			dy5 = dy4*dy;

		double
			df =	a01*dy 
				+	a20*dx2
				+	a02*dy2
				+	a21*dx2*dy
				+	a03*dy3
				+	a40*dx4
				+	a22*dx2*dy2
				+	a04*dy4
				+	a41*dx4*dy
				+	a23*dx2*dy3
				+	a42*dx4*dy2
				+	a24*dx2*dy4,


			dl	=	b10*dx 
				+	b11*dx*dy 
				+	b30*dx3
				+	b12*dx*dy2
				+	b31*dx3*dy
				+	b13*dx*dy3
				+	b50*dx5 
				+	b32*dx3*dy2 
				+	b14*dx*dy4
				+	b51*dx5*dy
				+	b33*dx3*dy3
				+	b15*dx*dy5;

		return DPoint(l0 + dl/3600, f0 + df/3600); // shp order
}

DPoint rd2LatLongEd50(DPoint rd) //shp_order
{
	DPoint lf = rd2LatLong(rd);
	double
		l = lf.first,  // OL (x)
		f = lf.second; // NB (y) 

	return DPoint(
		l + (+89.120 + 3.708 * (f - 52) - 17.176* (l - 5)) / 100000
	,	f + (-18.00 - 14.723 * (f - 52) - 1.029 * (l - 5)) / 100000
	);
}

DPoint rd2LatLongWgs84(DPoint rd) //shp_order
{
	DPoint lf = rd2LatLong(rd);
	double
		l = lf.first, // OL (x)
		f = lf.second; // NB (y) 

	return DPoint(
		l + (-37.902 +  0.329 * (f - 52) - 14.667 * (l - 5)) / 100000
	,	f + (-96.862 - 11.714 * (f - 52) -  0.125 * (l - 5)) / 100000
	);
}

DPoint latLongWgs842rd(DPoint ll) //shp_order
{
	//Bereken RD coördinaten
	// bron: http://www.gpsgek.nl/informatief/wgs84-rd-script.html
	Float64
		longitude = ll.first,
		latitude  = ll.second;
	Float64
		dL = 0.36 * (longitude - 5.38720621), // OL (x)
		dF = 0.36 * (latitude - 52.15517440); // NB (y)
	Float64
		dF2 = dF * dF,
		dF3 = dF * dF2,
//		dF4 = dF2 * dF2,
		dL2 = dL * dL,
		dL3 = dL * dL2,
		dL4 = dL2 * dL2;
	Float64
		somX
			=  190094.945 * dL
			+ -11832.228 * dF * dL
			+ -144.221 * dF2 * dL 
			+ -32.391 * dL3
			+ -0.705 * dF
			+ -2.340 * dF3 * dL
			+ -0.608 * dF * dL3
			+ -0.008 * dL2
			+ 0.148 * dF2 * dL3,
		somY
			= 309056.544 * dF
			+ 3638.893 * dL2
			+ 73.077 * dF2
			+ -157.984 * dF * dL2
			+ 59.788 * dF3
			+ 0.433 * dL
			+ -6.439 * dF2 * dL2
			+ -0.032 * dF * dL
			+ 0.092 * dL4
			+ -0.054 * dF * dL4;

	return DPoint(155000 + somX, 463000 + somY);
}

DPoint rd2LatLongGE(DPoint rd) //shp_order
{
	//	correctiefactoren voor rd coordinaten voor een betere match met GE
	const double Xc = 17.78369991;
	const double Yc = 40.18290427;
	
	const double Xx = -0.0000749081;
	const double Xy = -0.0000128123;
	const double Yx = -0.0000064425;
	const double Yy = -0.0000846220;

	return rd2LatLongWgs84(
		DPoint(
			rd.first  + Xc + Xx * rd.first + Xy * rd.second
		,	rd.second + Yc + Yx * rd.first + Yy * rd.second
		)
	);
}

// *****************************************************************************
//			COORDINATE CONVERSION FUNCTORS
// *****************************************************************************

template<typename TR, typename TA, DPoint CF(DPoint)>
struct ConvertFunc : unary_func<TR, TA> 
{
	TR operator() (const TA& p) const
	{
		DPoint res = CF(DPoint(p.Col(), p.Row()));
		return shp2dms_order( TR(res.first, res.second) );
	}
};

template<typename TR, typename TA> struct RD2LatLong     : ConvertFunc<TR, TA, rd2LatLong     > { RD2LatLong     (const AbstrUnit* resUnit, const AbstrUnit* srcUnit) { /* check for EPSG compatibility? */ } };
template<typename TR, typename TA> struct RD2LatLongEd50 : ConvertFunc<TR, TA, rd2LatLongEd50 > { RD2LatLongEd50 (const AbstrUnit* resUnit, const AbstrUnit* srcUnit) { /* check for EPSG compatibility? */ } };
template<typename TR, typename TA> struct RD2LatLongWgs84: ConvertFunc<TR, TA, rd2LatLongWgs84> { RD2LatLongWgs84(const AbstrUnit* resUnit, const AbstrUnit* srcUnit) { /* check for EPSG compatibility? */ } };
template<typename TR, typename TA> struct RD2LatLongGE   : ConvertFunc<TR, TA, rd2LatLongGE   > { RD2LatLongGE   (const AbstrUnit* resUnit, const AbstrUnit* srcUnit) { /* check for EPSG compatibility? */ } };
template<typename TR, typename TA> struct LatLongWgs842RD: ConvertFunc<TR, TA, latLongWgs842rd> { LatLongWgs842RD(const AbstrUnit* resUnit, const AbstrUnit* srcUnit) { /* check for EPSG compatibility? */ } };

// *****************************************************************************
//			Generic COORDINATE CONVERSION FUNCTOR
// *****************************************************************************

//#include "ogr_api.h"
#include "ogr_spatialref.h"
#include "geo/Transform.h"
#include "Projection.h"

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
	au->Fail(errMsgTxt, FR_Data);
	au->ThrowFail();
}

const AbstrUnit* CompositeBase(const AbstrUnit* proj)
{
	const AbstrUnit* res = proj->GetCurrProjection()->GetCompositeBase();
	if (!res)
		return proj;
	return res;
}

bool ProjectionIsColFirst(OGRSpatialReference &srs)
{
	OGRAxisOrientation PROJCS_peOrientation;
	OGRAxisOrientation GEOGCS_peOrientation;
	srs.GetAxis("PROJCS", 0, &PROJCS_peOrientation);
	srs.GetAxis("GEOGCS", 0, &GEOGCS_peOrientation);
	bool projection_is_col_first = PROJCS_peOrientation == OGRAxisOrientation::OAO_East || GEOGCS_peOrientation == OGRAxisOrientation::OAO_East || PROJCS_peOrientation == OGRAxisOrientation::OAO_West || GEOGCS_peOrientation == OGRAxisOrientation::OAO_West;
	return projection_is_col_first;
}

static leveled_critical_section cs_SpatialRefBlockCreation(item_level_type(0), ord_level_type::SpecificOperator, "SpatialRefBlock");

#include <gdal_priv.h>
#include <ogr_spatialref.h>
#include <proj.h>

static std::mutex s_projMutex;
struct SpatialRefBlock: SharedBase, gdalComponent 
{
	pj_ctx*                      m_ProjCtx = nullptr;
	OGRSpatialReference          m_Src, m_Dst;  // http://www.gdal.org/ogr/classOGRSpatialReference.html
	OGRCoordinateTransformation* m_Transformer = nullptr; // http://www.gdal.org/ogr/classOGRCoordinateTransformation.html
	SpatialRefBlock() 
	{
		std::lock_guard guard(s_projMutex);
		m_ProjCtx = proj_context_create();
	}

	~SpatialRefBlock()
	{
		if (m_Transformer)
			OGRCoordinateTransformation::DestroyCT(m_Transformer);

		std::lock_guard guard(s_projMutex);
		proj_context_destroy(m_ProjCtx);
	}

	void CreateTransformer()
	{
		assert(!m_Transformer);
		OGRCoordinateTransformationOptions options;
		//options.SetBallparkAllowed(true);
		CPLSetConfigOption("OGR_CT_OP_SELECTION", "BEST_ACCURACY");
		m_Transformer = OGRCreateCoordinateTransformation(&m_Src, &m_Dst, options); // http://www.gdal.org/ogr/ogr__spatialref_8h.html#aae11bd08e45cdb2e71e1d9c31f1e550f
	}

	void Release() const { if (!DecRef()) delete this; }
};

// *****************************************************************************
//			Type Conversion Functor
// *****************************************************************************

template<typename TR, typename TA>
struct Type0DConversion  : unary_func<TR,TA>
{
	Type0DConversion(const AbstrUnit* resUnit, const AbstrUnit* srcUnit)
	{ 
		if (resUnit->GetUnitClass()->CreateDefault() != resUnit) // cast?
		{
			MG_CHECK(!srcUnit->GetMetric    ());
			MG_CHECK(!srcUnit->GetProjection());
		}
	};

	TR operator() (typename sequence_traits<TA>::container_type::const_reference v) const
	{
		return Convert<TR>(v);
	}

	using iterator = typename DataArrayBase<TR>::iterator;
	using const_iterator = typename DataArrayBase<TA>::const_iterator;

	void Dispatch(iterator ri, const_iterator ai, const_iterator ae)
	{
		for (; ai != ae; ++ai, ++ri)
			Assign(*ri, operator ()(*ai) );
	}


	typedef typename sequence_traits<TR>::container_type TPR;
	typedef typename sequence_traits<TPR>::seq_t         pr_seq;
	typedef typename pr_seq::iterator                    seq_iterator;

	typedef typename sequence_traits<TA>::container_type TPA;
	typedef typename sequence_traits<TPA>::cseq_t  pa_cseq;
	typedef typename pa_cseq::const_iterator       cseq_iterator;

	void Dispatch(seq_iterator pri, cseq_iterator pai, cseq_iterator pae)
	{
		for (; pai != pae; ++pri, ++pai)
		{
			pri->resize(pai->size());
			Dispatch(pri->begin(), pai->begin(), pai->end());
		}
	}
};

template<typename TR, typename TA>
void DispatchMapping(Type0DConversion<TR, TA>& functor, typename Type0DConversion<TR, TA>::iterator ri,  typename Unit<TA>::range_t tileRange, SizeT n)
{
	for (SizeT i=0; i!=n; ++ri, ++i)
		Assign(*ri, functor(Range_GetValue_naked(tileRange, i)));
}

// *****************************************************************************
//			Type 1D Conversion Functor
// *****************************************************************************

template<typename TR, typename TA>
TR checked_multiply(TA v, Float64 factor)
{
	return IsDefined(v) ? Convert<TR>(v * factor) : UNDEFINED_OR_ZERO(TR);
}

template<typename TR, bit_size_t N>
TR checked_multiply(bit_value<N> v, Float64 factor)
{
	return Convert<TR>(v * factor);
}

template<typename TR, typename TA>
struct Type1DConversion  : unary_func<TR,TA>
{
	Type1DConversion(const AbstrUnit* resUnit, const AbstrUnit* srcUnit) 
	{ 
		if (!srcUnit->GetCurrMetric() || !resUnit->GetCurrMetric() || srcUnit->GetCurrMetric() == resUnit->GetCurrMetric())
		{
			m_Factor = 1.0;
			return;
		}
		if (!(srcUnit->GetCurrMetric()->m_BaseUnits == resUnit->GetCurrMetric()->m_BaseUnits))
		{
			srcUnit->UnifyValues(resUnit, "Base Units of first argument", "Base Units of cast target as specified by the second argument", UM_Throw);
			dms_assert(0); // if+unify => throw
		}
		m_Factor = srcUnit->GetCurrMetric()->m_Factor / resUnit->GetCurrMetric()->m_Factor;
	};

	TR ApplyDirect(typename sequence_traits<TA>::container_type::const_reference v) const
	{
		assert(m_Factor == 1.0 );

		return Convert<TR>(v);
	}

	TR ApplyScaled(typename sequence_traits<TA>::container_type::const_reference v) const
	{
		assert(m_Factor != 1.0 );
		return checked_multiply<TR>(v, m_Factor);
	}

	using iterator = typename DataArrayBase<TR>::iterator;
	using const_iterator = typename DataArrayBase<TA>::const_iterator;

	void Dispatch(iterator ri, const_iterator ai, const_iterator ae)
	{
		if (m_Factor != 1.0)
			for (; ai != ae; ++ai, ++ri)
				Assign(*ri, ApplyScaled(*ai) );
		else 
			for (; ai != ae; ++ai, ++ri)
				Assign(*ri, ApplyDirect(*ai) );
	}
	typedef typename sequence_traits<TR>::container_type TPR;
	typedef typename sequence_traits<TPR>::seq_t         pr_seq;
	typedef typename pr_seq::iterator                    seq_iterator;

	typedef typename sequence_traits<TA>::container_type TPA;
	typedef typename sequence_traits<TPA>::cseq_t  pa_cseq;
	typedef typename pa_cseq::const_iterator       cseq_iterator;

	void Dispatch(seq_iterator pri, cseq_iterator pai, cseq_iterator pae)
	{
		for (; pai != pae; ++pri, ++pai)
		{
			pri->resize_uninitialized(pai->size());
			Dispatch(pri->begin(), pai->begin(), pai->end());
		}
	}

	Float64 m_Factor;
};

const int PROJ_BLOCK_SIZE = 1024;

template<typename TR, typename TA>
void DispatchMapping(Type1DConversion<TR, TA>& functor, typename Type1DConversion<TR, TA>::iterator ri,  typename Unit<TA>::range_t tileRange, SizeT n)
{
	if (functor.m_Factor != 1.0)
		for (SizeT i=0; i!=n; ++ri, ++i)
			Assign(*ri, functor.ApplyScaled(Range_GetValue_naked(tileRange, i)) );
	else
		for (SizeT i=0; i!=n; ++ri, ++i)
			Assign(*ri, functor.ApplyDirect(Range_GetValue_naked(tileRange, i)));
}

template<typename TR, typename TA>
struct Type2DConversion: unary_func<TR, TA> // http://www.gdal.org/ogr/osr_tutorial.html
{
	Type2DConversion(const AbstrUnit* resUnit, const AbstrUnit* srcUnit)
		:	m_PreRescaler (UnitProjection::GetCompositeTransform(srcUnit->GetCurrProjection()))
		,	m_PostRescaler(UnitProjection::GetCompositeTransform(resUnit->GetCurrProjection()).Inverse())
	{
		TokenID srcFormat = CompositeBase(srcUnit)->GetCurrSpatialReference();
		if (srcFormat)
		{
			TokenID resFormat = CompositeBase(resUnit)->GetCurrSpatialReference();
			if (resFormat && srcFormat != resFormat)
			{
				leveled_critical_section::scoped_lock lock(cs_SpatialRefBlockCreation);
				m_OgrComponentHolder = new SpatialRefBlock;
				GDAL_ErrorFrame frame;
				SharedStr srcFormatStr = SharedStr(srcFormat);
				SharedStr resFormatStr = SharedStr(resFormat);
				
				if (!AuthorityCodeIsValidCrs(srcFormatStr.c_str()))
				{
					reportF(MsgCategory::progress, SeverityTypeID::ST_Error, 
						"The 'src' unit in projection conversion from 'src' unit %s to res unit %s is not a valid projection.",
						srcUnit->GetFullName(), resUnit->GetFullName());
				}

				if (!AuthorityCodeIsValidCrs(resFormatStr.c_str()))
				{
					reportF(MsgCategory::progress, SeverityTypeID::ST_Error,
						"The 'res' unit in projection conversion from src unit %s to 'res' unit %s is not a valid projection.",
						srcUnit->GetFullName(), resUnit->GetFullName());
				}

				OGRCheck(&m_OgrComponentHolder->m_Src, m_OgrComponentHolder->m_Src.SetFromUserInput(srcFormatStr.c_str()), srcFormatStr.c_str(), srcUnit );
				OGRCheck(&m_OgrComponentHolder->m_Dst, m_OgrComponentHolder->m_Dst.SetFromUserInput(resFormatStr.c_str()), resFormatStr.c_str(), resUnit );
				m_OgrComponentHolder->CreateTransformer();
				m_Source_is_expected_to_be_col_first = ProjectionIsColFirst(m_OgrComponentHolder->m_Src);
				m_Projection_is_col_first = ProjectionIsColFirst(m_OgrComponentHolder->m_Dst);
				frame.ThrowUpWhateverCameUp();
				return;
			}
		}
		
		m_PreRescaler *= m_PostRescaler;
		m_PostRescaler = CrdTransformation(); // clear
	}

	TR ApplyDirect(const TA& p) const
	{
		assert(!m_OgrComponentHolder);
		assert(m_PreRescaler.IsIdentity());
		assert(m_PostRescaler.IsIdentity());

		return Convert<TR>(p);
	}

	TR ApplyProjection(const TA& p) const
	{
		assert(m_OgrComponentHolder && m_OgrComponentHolder->m_Transformer);
		assert(m_PreRescaler.IsIdentity());
		assert(m_PostRescaler.IsIdentity());

		if (!IsDefined(p))
			return UNDEFINED_OR_ZERO(TR);

		DPoint res = prj2dms_order(p.first, p.second, m_Source_is_expected_to_be_col_first);
		if (!m_OgrComponentHolder->m_Transformer->Transform(1, &res.first, &res.second, nullptr))
			return UNDEFINED_OR_ZERO(TR);
		res = prj2dms_order(res.first, res.second, m_Projection_is_col_first);
		return Convert<TR>(res);
	}
	TR ApplyScaled(const TA& p) const
	{
		assert(!m_OgrComponentHolder);
		assert(!m_PreRescaler.IsIdentity());
		assert(m_PostRescaler.IsIdentity());

		if (!IsDefined(p))
			return UNDEFINED_OR_ZERO(TR);

		return Convert<TR>( m_PreRescaler.Apply(DPoint(p)) );
	}
	TR ApplyScaledProjection(const TA& p) const
	{
		assert(m_OgrComponentHolder && m_OgrComponentHolder->m_Transformer);
		assert(!m_PreRescaler.IsIdentity() || !m_PostRescaler.IsIdentity());

		if (!IsDefined(p))
			return UNDEFINED_OR_ZERO(TR);

		DPoint res = m_PreRescaler.Apply(DPoint(p));
		res = prj2dms_order(res.first, res.second, m_Source_is_expected_to_be_col_first);
		if (!m_OgrComponentHolder->m_Transformer->Transform(1, &res.first, &res.second, nullptr))
			return UNDEFINED_OR_ZERO(TR);
		res = prj2dms_order(res.first, res.second, m_Projection_is_col_first);
		return Convert<TR>( m_PostRescaler.Apply( res ) );
	}

	using iterator = typename DataArrayBase<TR>::iterator;
	using const_iterator = typename DataArrayBase<TA>::const_iterator;

	void DispatchTransformation_impl(iterator ri, const_iterator ai, Float64* resX, Float64* resY, int* successFlags, UInt32 s)
	{
		assert(m_OgrComponentHolder);
		assert(s <= PROJ_BLOCK_SIZE);
		bool source_is_expected_to_be_col_first = this->m_Source_is_expected_to_be_col_first;
		bool projection_is_col_first = this->m_Projection_is_col_first;

		for (int i = 0; i != s; ++i)
		{
			DPoint rescaledA = m_PreRescaler.Apply(DPoint(ai[i]));
			rescaledA = prj2dms_order(rescaledA, source_is_expected_to_be_col_first);
			resX[i] = rescaledA.first;
			resY[i] = rescaledA.second;
		}
		if (!m_OgrComponentHolder->m_Transformer->Transform(s, resX, resY, nullptr /*Z*/, successFlags))
		{
			if (s > 1)
			{
				auto halfSize = s / 2;
				DispatchTransformation_impl(ri, ai, resX, resY, successFlags, halfSize);
				DispatchTransformation_impl(ri + halfSize, ai + halfSize, resX, resY, successFlags, s - halfSize);
			}
			else
				Assign(*ri, Undefined());
			return;
		}
		for (int i = 0; i != s; ++ri, ++i)
		{
			if (!successFlags[i])
			{
				DPoint rescaledA = m_PreRescaler.Apply(DPoint(ai[i]));
				rescaledA = prj2dms_order(rescaledA, source_is_expected_to_be_col_first);
				resX[i] = rescaledA.first;
				resY[i] = rescaledA.second;
				if (!m_OgrComponentHolder->m_Transformer->Transform(1, resX + i, resY + i, nullptr /*Z*/, successFlags + i))
				{
					Assign(*ri, Undefined());
					continue;
				}
			}
			auto reprojectedPoint = prj2dms_order(resX[i], resY[i], projection_is_col_first);
			auto rescaledPoint = m_PostRescaler.Apply(reprojectedPoint);
			Assign(*ri, Convert<TR>(rescaledPoint));
		}
	}

	void Dispatch(iterator ri, const_iterator ai, const_iterator ae)
	{
		if (m_OgrComponentHolder) 
		{
			Float64 resX[PROJ_BLOCK_SIZE];
			Float64 resY[PROJ_BLOCK_SIZE];
			int     successFlags[PROJ_BLOCK_SIZE];
	
			auto n = ae - ai;
			while (n)
			{
				auto s = n;
				MakeMin(s, PROJ_BLOCK_SIZE);
				DispatchTransformation_impl(ri, ai, resX, resY, successFlags, s);
				n -= s;
				ai += s;
				ri += s;
			}
		}
		else 
			if (m_PreRescaler.IsIdentity())
				for (; ai != ae; ++ai, ++ri)
					Assign(*ri, ApplyDirect(*ai));
			else
				for (; ai != ae; ++ai, ++ri)
					Assign(*ri, ApplyScaled(*ai) );
	}

	typedef typename sequence_traits<TR>::container_type TPR;
	typedef typename sequence_traits<TPR>::seq_t         pr_seq;
	typedef typename pr_seq::iterator                    seq_iterator;

	typedef typename sequence_traits<TA>::container_type TPA;
	typedef typename sequence_traits<TPA>::cseq_t  pa_cseq;
	typedef typename pa_cseq::const_iterator       cseq_iterator;

	void Dispatch(seq_iterator pri, cseq_iterator pai, cseq_iterator pae)
	{
		for (; pai!=pae; ++pri, ++pai)
		{
			pri->resize_uninitialized(pai->size());
			Dispatch(pri->begin(), pai->begin(), pai->end());
		}
	}

	SharedPtr<SpatialRefBlock>   m_OgrComponentHolder;
	CrdTransformation            m_PreRescaler;
	CrdTransformation            m_PostRescaler;
	bool                         m_Source_is_expected_to_be_col_first = dms_order_tag::col_first;
	bool                         m_Projection_is_col_first = dms_order_tag::col_first;
};

template<typename TR, typename TA>
void DispatchMapping(Type2DConversion<TR,TA>& functor, typename Type2DConversion<TR,TA>::iterator ri,  typename Unit<TA>::range_t tileRange, SizeT n)
{
	if (functor.m_OgrComponentHolder) 
	{
		Float64 resX[PROJ_BLOCK_SIZE];
		Float64 resY[PROJ_BLOCK_SIZE];
		int     successFlags[PROJ_BLOCK_SIZE];
		bool    source_is_expected_to_be_col_first = functor.m_Source_is_expected_to_be_col_first;
		bool    projection_is_col_first = functor.m_Projection_is_col_first;

		while (n)
		{
			auto s = n;
			MakeMin(s, PROJ_BLOCK_SIZE);
			for (SizeT i = 0; i != s; ++i)
			{
				DPoint rescaledA = functor.m_PreRescaler.Apply(DPoint(Range_GetValue_naked(tileRange, i)));
				rescaledA = prj2dms_order(rescaledA, source_is_expected_to_be_col_first);
				resX[i] = rescaledA.first;
				resY[i] = rescaledA.second;
			}
			if (!functor.m_OgrComponentHolder->m_Transformer->Transform(s, resX, resY, nullptr /*Z*/, successFlags))
				fast_fill(successFlags, successFlags + s, 0);
			for (SizeT i = 0; i != s; ++ri, ++i)
			{
				if (successFlags[i])
				{
					auto reprojectedPoint = prj2dms_order(resX[i], resY[i], projection_is_col_first);
					auto rescaledPoint = functor.m_PostRescaler.Apply(reprojectedPoint);
					Assign(*ri, Convert<TR>(rescaledPoint));
				}
				else
					Assign(*ri, Undefined());
			}
			n -= s;
		}
	}
	else 
		if (functor.m_PreRescaler.IsIdentity())
			for (SizeT i=0; i!=n; ++ri, ++i)
				Assign(*ri, functor.ApplyDirect(Range_GetValue_naked(tileRange, i)));
		else
			for (SizeT i=0; i!=n; ++ri, ++i)
				Assign(*ri, functor.ApplyScaled(Range_GetValue_naked(tileRange, i)) );
}

template<typename TR, typename TA, typename RI>
void DispatchMappingCount(Type2DConversion<TR, TA>& functor, RI ri, typename Unit<TA>::range_t srcTileRange, typename Unit<TR>::range_t dstRange, SizeT n)
{
	SizeT k = Cardinality(srcTileRange);
	if (functor.m_OgrComponentHolder)
		if (functor.m_PreRescaler.IsIdentity() && functor.m_PostRescaler.IsIdentity())
			for (SizeT i = 0; i != k; ++i)
			{
				auto j = Range_GetIndex_checked(dstRange, functor.ApplyProjection(Range_GetValue_naked(srcTileRange, i)));
				if (j<n)
					ri[j]++;
			}
		else
			for (SizeT i = 0; i != k; ++i)
			{
				auto j = Range_GetIndex_checked(dstRange, functor.ApplyScaledProjection(Range_GetValue_naked(srcTileRange, i)));
				if (j<n)
					ri[j]++;
			}
	else
		if (functor.m_PreRescaler.IsIdentity())
			for (SizeT i = 0; i != k; ++i)
			{
				auto j = Range_GetIndex_checked(dstRange, functor.ApplyDirect(Range_GetValue_naked(srcTileRange, i)));
				if (j<n)
					ri[j]++;
			}
		else
			for (SizeT i = 0; i != k; ++i)
			{
				auto j = Range_GetIndex_checked(dstRange, functor.ApplyScaled(Range_GetValue_naked(srcTileRange, i)));
				if (j<n)
					ri[j]++;
			}
}


// *****************************************************************************
//			Polymorphic Functor applicator
// *****************************************************************************

// assign converted elements
template <typename Functor>
void AssignFuncRes0(
	const Functor&                                                                         func, 
	typename sequence_traits<typename Functor::res_type >::container_type::reference       res,
	typename sequence_traits<typename Functor::arg1_type>::container_type::const_reference arg)
{
	Assign(res, func(arg) );
}

// assign converted sequences
template <typename Functor>
void AssignFuncRes0(
	  const Functor&                                 func, 
	  SA_Reference     <typename Functor::res_type > res,
	  SA_ConstReference<typename Functor::arg1_type> arg)
{
	res.resize_uninitialized(arg.size());
	dms_transform(arg.begin(), arg.end(), res.begin(), func);
}

// *****************************************************************************
//			Meta Function Class that returns a TypeConversion Functor
// *****************************************************************************

template <typename T> struct is_2d : std::integral_constant<bool, dimension_of<T>::value == 2> {};

struct TypeConversionF
{
	template <typename TR, typename TA>
	struct apply : 
		std::conditional<( is_numeric_v<TR> && is_numeric_v<TA>)
		,	Type1DConversion<TR, TA>
		,	std::conditional_t<(is_2d<TR>::value && is_2d<TA>::value)
			,	Type2DConversion<TR, TA>
			,	Type0DConversion<TR, TA>
			>
		>
	{};

};

template <typename MetaFunc, typename TR, typename TA>
struct ConversionGenerator 
	:	MetaFunc::template apply<
			std::conditional_t<(is_composite_v<TR> && is_composite_v<TA> ), field_of_t<TR>, TR >
		,	std::conditional_t<(is_composite_v<TR> && is_composite_v<TA> ), field_of_t<TA>, TA >
	>
{};

template <
	typename TR,
	typename TA,
	typename TCF,
	typename CIT,
	typename RIT
>
void do_transform(const AbstrUnit* dstUnit, const AbstrUnit* srcUnit, CIT srcBegin, CIT srcEnd, RIT dstBegin)
{
	using FunctorType = typename ConversionGenerator<TCF, TR, TA>::type;
	FunctorType functor(dstUnit, srcUnit);

	parallel_for_if_separable<SizeT, TR>(0, srcEnd - srcBegin, [dstBegin, srcBegin, &functor](SizeT i)
		{
			AssignFuncRes0(functor, dstBegin[i], srcBegin[i]);
		}
	);
}

template <
	typename TR,
	typename TA,
	typename TCF,
	typename CIT,
	typename RIT
>
void do_convert(const AbstrUnit* dstUnit, const AbstrUnit* srcUnit, CIT srcIter, CIT srcEnd, RIT dstIter)
{
	using FunctorType = typename ConversionGenerator<TCF, TR, TA>::type;

	FunctorType functor(dstUnit, srcUnit);
	functor.Dispatch(dstIter, srcIter, srcEnd);
}

template <
	typename TR,
	typename TA,
	typename TCF,
	typename RIT
>
void do_mapping(const Unit<TR>* dstUnit, const Unit<TA>* srcUnit, tile_id t, RIT dstIter, RIT dstEnd)
{
	using FunctorType = typename ConversionGenerator<TCF, TR, TA>::type;

	FunctorType functor(dstUnit, srcUnit);
	auto tileRange = srcUnit->GetTileRange(t);
	SizeT n = Cardinality(tileRange);
	MG_CHECK(dstIter + n == dstEnd);
	DispatchMapping(functor, dstIter, tileRange, n);
}

template <
	typename TR,
	typename TA,
	typename TCF,
	typename RIT
>
void do_mapping_count(const Unit<TR>* dstUnit, const Unit<TA>* srcUnit, tile_id t, RIT dstIter, RIT dstEnd)
{
	using FunctorType = typename ConversionGenerator<TCF, TR, TA>::type;

	FunctorType functor(dstUnit, srcUnit);
	auto srcTileRange = srcUnit->GetTileRange(t);
	auto dstRange = dstUnit->GetRange();
	SizeT n = Cardinality(dstRange);
	MG_CHECK(dstIter + n == dstEnd);
	DispatchMappingCount(functor, dstIter, srcTileRange, dstRange, n);
}

// *****************************************************************************
//			CONVERSIONS WITH UNIT CASTING
// *****************************************************************************

#include "CastedUnaryAttrOper.h"

template <typename TR, typename TA, typename TCF>
class TransformAttrOperator : public AbstrCastedUnaryAttrOperator
{
	typedef DataArray<TR>    ResultType;
	typedef field_of_t<TR>   field_type;
	typedef DataArray<TA>    Arg1Type;
	typedef Unit<field_type> Arg2Type;

public:
	TransformAttrOperator(AbstrOperGroup* gr, bool reverseArgs = false)
		:	AbstrCastedUnaryAttrOperator(gr
			,	ResultType::GetStaticClass() 
				,	Arg1Type::GetStaticClass()
				,	Arg2Type::GetStaticClass()
			,	composition_of_v<TR>
			,	reverseArgs
			)
		{}

	// Override Operator
	SharedPtr<const AbstrDataObject> CreateFutureTileCaster(SharedPtr<AbstrDataItem> resultAdi, bool lazy, const AbstrUnit* valuesUnitA, const AbstrDataItem* arg1A, const AbstrUnit* argUnitA MG_DEBUG_ALLOCATOR_SRC_ARG) const override
	{
		auto tileRangeData = AsUnit(arg1A->GetAbstrDomainUnit()->GetCurrRangeItem())->GetTiledRangeData();
		auto valuesUnit = debug_cast<const Unit<field_of_t<TR>>*>(valuesUnitA);

		auto arg1 = MakeShared(const_array_cast<TA>(arg1A));
		auto arg2 = MakeShared(debug_cast<const Arg2Type*>(argUnitA));
		assert(arg1);

		using prepare_data = SharedPtr<Arg1Type::future_tile>;
		auto futureTileFunctor = make_unique_FutureTileFunctor<TR, prepare_data, false>(resultAdi, lazy, tileRangeData, get_range_ptr_of_valuesunit(valuesUnit)
			, [arg1](tile_id t) { return arg1->GetFutureTile(t); }
			, [arg2, arg1A](sequence_traits<TR>::seq_t resData, prepare_data arg1FutureData)
			{
				auto argData = arg1FutureData->GetTile();
				do_transform<TR, TA, TCF>(arg2, arg1A->GetAbstrValuesUnit(), argData.begin(), argData.end(), resData.begin());
			}
			MG_DEBUG_ALLOCATOR_SRC_PARAM
		);

		return futureTileFunctor.release();
	}

	void Calculate(AbstrDataObject* borrowedDataHandle, const AbstrDataItem* argDataA, const AbstrUnit* argUnit, tile_id t) const override
	{
		auto argData = const_array_cast<TA>(argDataA)->GetDataRead(t);
		auto resultData =  mutable_array_cast<TR>(borrowedDataHandle)->GetDataWrite(t);

		do_transform<TR, TA, TCF>(argUnit, argDataA->GetAbstrValuesUnit(), argData.begin(), argData.end(), resultData.begin());
	}
};





template <typename TR, typename TA>
class ConvertAttrOperator : public AbstrCastedUnaryAttrOperator
{
	typedef DataArray<TR>    ResultType;
	typedef field_of_t<TR>   field_type;
	typedef DataArray<TA>    Arg1Type;
	typedef Unit<field_type> Arg2Type;

public:
	ConvertAttrOperator(AbstrOperGroup* gr, bool reverseArgs = false)
		:	AbstrCastedUnaryAttrOperator(gr
			,	ResultType::GetStaticClass() 
				,	Arg1Type::GetStaticClass()
				,	Arg2Type::GetStaticClass()
			,	composition_of_v<TR>
			,	reverseArgs
			)
		{}

	// Override Operator
	SharedPtr<const AbstrDataObject> CreateFutureTileCaster(SharedPtr<AbstrDataItem> resultAdi, bool lazy, const AbstrUnit* valuesUnitA, const AbstrDataItem* arg1A, const AbstrUnit* argUnitA MG_DEBUG_ALLOCATOR_SRC_ARG) const override
	{
		auto tileRangeData = AsUnit(arg1A->GetAbstrDomainUnit()->GetCurrRangeItem())->GetTiledRangeData();
		auto valuesUnit = debug_cast<const Unit<field_type>*>(valuesUnitA);

		auto arg1 = const_array_cast<TA>(arg1A);
		auto dstUnit = MakeShared(debug_cast<const Arg2Type*>(argUnitA));
		auto srcUnit = MakeShared(arg1A->GetAbstrValuesUnit());
		assert(arg1);

		using prepare_data = SharedPtr<Arg1Type::future_tile>;
		auto futureTileFunctor = make_unique_FutureTileFunctor<TR, prepare_data, false>(resultAdi, lazy, tileRangeData, get_range_ptr_of_valuesunit(valuesUnit)
			, [arg1](tile_id t) { return arg1->GetFutureTile(t); }
			, [srcUnit, dstUnit](sequence_traits<TR>::seq_t resData, prepare_data arg1FutureData)
			{
				auto argData = arg1FutureData->GetTile();
				do_convert<TR, TA, TypeConversionF>(dstUnit, srcUnit, argData.begin(), argData.end(), resData.begin());
			}
			MG_DEBUG_ALLOCATOR_SRC_PARAM
		);

		return futureTileFunctor.release();
	}
	void Calculate(AbstrDataObject* borrowedDataHandle, const AbstrDataItem* argDataA, const AbstrUnit* argUnit, tile_id t) const override
	{
		auto argData = const_array_cast<TA>(argDataA)->GetDataRead(t);
		auto resultData =  mutable_array_cast<TR>(borrowedDataHandle)->GetDataWrite(t);

		do_convert<TR, TA, TypeConversionF>(argUnit, argDataA->GetAbstrValuesUnit(), argData.begin(), argData.end(), resultData.begin());
	}
};

CommonOperGroup cog_mapping("mapping");

template <typename TR, typename TA>
class MappingOperator : public AbstrMappingOperator
{
	typedef DataArray<TR> ResultType;
	typedef Unit<TA> Arg1Type;
	typedef Unit<TR> Arg2Type;

public:
	MappingOperator()
		:	AbstrMappingOperator(&cog_mapping
			,	ResultType::GetStaticClass() 
				,	Arg1Type::GetStaticClass()
				,	Arg2Type::GetStaticClass()
			)
		{}

	// Override Operator
	void Calculate(AbstrDataObject* borrowedDataHandle, const AbstrUnit* argDomainUnit, const AbstrUnit* argValuesUnit, tile_id t) const override
	{
		auto resultData =  mutable_array_cast<TR>(borrowedDataHandle)->GetDataWrite(t);

		do_mapping<TR, TA, TypeConversionF>(
			debug_cast<const Unit<TR>*>(argValuesUnit), 
			debug_cast<const Unit<TA>*>(argDomainUnit), t, 
			resultData.begin(), resultData.end()
		);
	}
};

CommonOperGroup cog_mapping_count("mapping_count");

template <typename TR, typename TA, typename Cardinal = UInt32>
class MappingCountOperator : public AbstrMappingCountOperator
{
	typedef DataArray<Cardinal> ResultType;
	typedef Unit<TA> Arg1Type;
	typedef Unit<TR> Arg2Type;
	typedef Unit<Cardinal> Arg3Type;

public:
	MappingCountOperator()
		: AbstrMappingCountOperator(&cog_mapping_count
			, ResultType::GetStaticClass()
			, Arg1Type::GetStaticClass()
			, Arg2Type::GetStaticClass()
			, Arg3Type::GetStaticClass()
		)
	{}

	// Override Operator
	void Calculate(DataWriteHandle& borrowedDataHandle, const AbstrUnit* argDomainUnit, const AbstrUnit* argValuesUnit, tile_id t) const override
	{
		auto resultData = mutable_array_cast<Cardinal>(borrowedDataHandle)->GetDataWrite(no_tile, dms_rw_mode::read_write);

		do_mapping_count<TR, TA, TypeConversionF>(
			debug_cast<const Unit<TR>*>(argValuesUnit),
			debug_cast<const Unit<TA>*>(argDomainUnit), t,
			resultData.begin(), resultData.end()
		);
	}
};


#include "OperAttrUni.h"



template <typename TR>
ConstUnitRef cast_unit_creator_field(const AbstrOperGroup* gr, const ArgSeqType& args)
{
	return cast_unit_creator< field_of_t<TR>>(args);
}



template <typename TR, typename TA>
struct CastAttrOperator: UnaryAttrOperator<TR, TA>
{
	CastAttrOperator(AbstrOperGroup* gr)
		:	UnaryAttrOperator<TR, TA>(gr, ArgFlags(), cast_unit_creator_field<TR>, composition_of_v<TR>)
	{}

	void CalcTile(sequence_traits<TR>::seq_t resData, sequence_traits<TA>::cseq_t arg1Data, ArgFlags af MG_DEBUG_ALLOCATOR_SRC_ARG) const override
	{
		dms_assert(arg1Data.size() == resData.size());

		do_transform<TR, TA, boost::mpl::quote2<Type0DConversion> >(
			Unit<field_of_t<TR>>::GetStaticClass()->CreateDefault()
		,	Unit<field_of_t<TA>>::GetStaticClass()->CreateDefault()
		,	arg1Data.begin(), arg1Data.end()
		,	resData.begin()
		);
	}
};

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
		:	UnaryOperator(gr
			, ResultType::GetStaticClass()
			,	Arg1Type::GetStaticClass()
			) 
	{}

	// Override Operator
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

			Assign(result->GetDataWrite()[0], SharedStr(dataBegin, dataBegin+ vout.CurrPos()));

			resLock.Commit();
		}
		return true;
	}
};

// *****************************************************************************
//											UnitGroups
// *****************************************************************************

template <typename V> 
CommonOperGroup* GetUnitGroup()
{
	static CommonOperGroup cog(ValueWrap<V>::GetStaticClass()->GetID());
	return &cog;
}

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
//											INSTANTIATION
// *****************************************************************************

#include "RtcTypeLists.h"
#include "utl/TypeListOper.h"

#include "LispTreeType.h"

namespace 
{
	AbstrOperGroup* RD2LatLong_og     () { static CommonOperGroup cog("RD2LatLong"     ); return &cog; }
	AbstrOperGroup* RD2LatLongEd50_og () { static CommonOperGroup cog("RD2LatLongEd50"); return &cog; }
	AbstrOperGroup* RD2LatLongWgs84_og() { static CommonOperGroup cog("RD2LatLongWgs84"); return &cog; }
	AbstrOperGroup* RD2LatLongGE_og   () { static CommonOperGroup cog("RD2LatLongGE"); return &cog; }
	AbstrOperGroup* LatLongWgs842RD_og() { static CommonOperGroup cog("LatLongWgs842RD"); return &cog; }

	template <typename TR, typename TA>
	struct Rd2LlOpers
	{
		Rd2LlOpers()
			: cRD2LL(RD2LatLong_og())
			, cRD2LLEd50(RD2LatLongEd50_og())
			, cRD2LLWgs84(RD2LatLongWgs84_og())
			, cRD2LLGE(RD2LatLongGE_og())
			, cLLWgs842RD(LatLongWgs842RD_og())
		{}

		TransformAttrOperator < TR, TA, boost::mpl::quote2<RD2LatLong     > > cRD2LL;
		TransformAttrOperator < TR, TA, boost::mpl::quote2<RD2LatLongEd50 > > cRD2LLEd50;
		TransformAttrOperator < TR, TA, boost::mpl::quote2<RD2LatLongWgs84> > cRD2LLWgs84;
		TransformAttrOperator < TR, TA, boost::mpl::quote2<RD2LatLongGE   > > cRD2LLGE;
		TransformAttrOperator < TR, TA, boost::mpl::quote2<LatLongWgs842RD> > cLLWgs842RD;
	};

	template <typename TR, typename TA>
	struct Rd2LlOpersWithSeq : Rd2LlOpers<TR, TA>
	{
		Rd2LlOpers<
			typename sequence_traits<TR>::container_type
			, typename sequence_traits<TA>::container_type
		>	m_SequenceOpers;
	};

	template <typename Res, typename Arg>
	struct NamedCastAttrOper : CastAttrOperator<Res, Arg>
	{
		NamedCastAttrOper() : CastAttrOperator<Res, Arg>(GetUnitGroup<Res>()) {}
	};

	CommonOperGroup cog_Convert(token::convert);

	template <typename TA, typename TRL>
	struct convertAndCastOpers
	{
		convertAndCastOpers()
			: convertOpers(&cog_Convert, false)
			, lookupOpers(&cog_lookup, true) // support for v[u] notation, which becomes lookup(u, v)
		{}

		tl_oper::inst_tuple<TRL, ConvertAttrOperator<_, TA>, AbstrOperGroup*, bool> convertOpers;
		tl_oper::inst_tuple<TRL, ConvertAttrOperator<_, TA>, AbstrOperGroup*, bool> lookupOpers;
		tl_oper::inst_tuple<TRL, NamedCastAttrOper  <_, TA> > castOpers;
	};
	template <typename TA, typename TRL>
	struct mappingOpers
	{
		tl_oper::inst_tuple<TRL, MappingOperator<_, TA> > m_MappingOpers;
	};
	template <typename TA, typename TRL>
	struct mappingCountOpers
	{
		tl_oper::inst_tuple<TRL, MappingCountOperator<_, TA, UInt8 > > m_MappingCountOpers_08;
		tl_oper::inst_tuple<TRL, MappingCountOperator<_, TA, UInt16> > m_MappingCountOpers_16;
		tl_oper::inst_tuple<TRL, MappingCountOperator<_, TA, UInt32> > m_MappingCountOpers_32;
	};


	Rd2LlOpersWithSeq<DPoint, DPoint> rd2llDD;
	Rd2LlOpersWithSeq<DPoint, FPoint> rd2llDF;
	Rd2LlOpersWithSeq<FPoint, FPoint> rd2llFF;

	convertAndCastOpers< SharedStr, typelists::strings>   stringConvertAndCastOpers;

	tl_oper::inst_tuple<typelists::scalars, convertAndCastOpers<_, typelists::numerics> > numericConvertAndCastOpers;

	tl_oper::inst_tuple<typelists::points, convertAndCastOpers<_, typelists::points   > > pointConvertAndCastOpers;
	tl_oper::inst_tuple<typelists::numeric_sequences, convertAndCastOpers<_, typelists::numeric_sequences> > numericSequenceConvertAndCastOpers;
	tl_oper::inst_tuple<typelists::point_sequences, convertAndCastOpers<_, typelists::point_sequences> > pointSequenceConvertAndCastOpers;

	tl_oper::inst_tuple<typelists::points, convertAndCastOpers<_, typelists::strings> > points2stringConvertAndCastOpers;
	tl_oper::inst_tuple<typelists::sequences, convertAndCastOpers<_, typelists::strings> > seq2stringConvertAndCastOpers;

	tl_oper::inst_tuple<typelists::ints, mappingOpers<_, typelists::num_objects> > numericMappingOpers;
	tl_oper::inst_tuple<typelists::domain_points, mappingOpers<_, typelists::points   > > pointMappingOpers;
	tl_oper::inst_tuple<typelists::domain_points, mappingCountOpers<_, typelists::domain_points   > > pointMappingCountOpers;
	tl_oper::inst_tuple<typelists::sequences, NamedCastAttrOper  <_, SharedStr> > castSequenceOpers;
//		convertAndCastOpers< SharedStr, typelists::points>    points2stringConvertAndCastOpers;
//		convertAndCastOpers< SharedStr, typelists::sequences> seq2stringConvertAndCastOpers;
//TEST */

	//	ConstAttrOperator
//	#define INSTANTIATE(x) ConstAttrOperator<x> ca##x;
//	INSTANTIATE_FLD_ELEM

	tl_oper::inst_tuple<typelists::fields, ConstAttrOperator<_>> constAttrOpers;

	#define PI	3.14159265358979323846

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

	CommonOperGroup cog_asDataString("asDataString");
	AsDataStringOperator g_AsDataString(&cog_asDataString);
}
