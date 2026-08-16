// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/***************************************************************************
 *
 * A Transformation is used in a draw_context to
 * transform from world to device coords.
 * Multiple Transformation can be combined.
 ****************************************************************************/

#ifndef __RTC_GEO_TRANSFORM_H
#define __RTC_GEO_TRANSFORM_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "RtcBase.h"

#include "geom/Range.h"
#include "geom/Point.h"
#include "ser/FormattedStream.h"

#include <array>
#include <algorithm>
#include <cmath>

[[noreturn]] RTC_CALL void IllegalSingularity();

//----------------------------------------------------------------------
// TransformComplexity: how much a Transformation can represent. Ascending; a consumer that needs a
// simpler guarantee (e.g. axis-separability for the fast raster path) must refuse a more complex one.
// See Transformation_complexity_plan.md. Level c (AnisoScale) is the historical/default level.
//----------------------------------------------------------------------

enum class TransformComplexity : UInt8
{
	Translation = 0, // a: factor == (1,1)
	IsoScale    = 1, // b: |fx|==|fy|, axis-aligned (+orientation)
	AnisoScale  = 2, // c: per-axis factor (+orientation)   <-- default / historical
	Affine2D    = 3, // d: 2x2 matrix (rotation/shear) + translation
	Affine3D    = 4, // e: reserved for true 3D (x,y,z); not stored yet (Phase 9)
	Projective  = 5, // f: planar homography with perspective divide
};

//----------------------------------------------------------------------
// Minimal 3x3 (row-major homogeneous) and 2x2 helpers. No matrix library exists in rtc and the need
// is small and Transform-local, so these are free inline helpers rather than a general matrix class.
//----------------------------------------------------------------------

namespace transform_detail
{
	// C = A * B (both row-major 3x3)
	template <class T>
	std::array<T, 9> Mul3x3(const std::array<T, 9>& a, const std::array<T, 9>& b)
	{
		std::array<T, 9> c;
		for (int i = 0; i != 3; ++i)
			for (int j = 0; j != 3; ++j)
				c[3 * i + j] = a[3 * i + 0] * b[0 + j] + a[3 * i + 1] * b[3 + j] + a[3 * i + 2] * b[6 + j];
		return c;
	}

	template <class T>
	T Det3x3(const std::array<T, 9>& m)
	{
		return m[0] * (m[4] * m[8] - m[5] * m[7])
			-  m[1] * (m[3] * m[8] - m[5] * m[6])
			+  m[2] * (m[3] * m[7] - m[4] * m[6]);
	}

	template <class T>
	std::array<T, 9> Inverse3x3(const std::array<T, 9>& m)
	{
		T det = Det3x3(m);
		if (det == 0)
			IllegalSingularity();
		T inv = T(1) / det;
		std::array<T, 9> r;
		r[0] = (m[4] * m[8] - m[5] * m[7]) * inv;
		r[1] = (m[2] * m[7] - m[1] * m[8]) * inv;
		r[2] = (m[1] * m[5] - m[2] * m[4]) * inv;
		r[3] = (m[5] * m[6] - m[3] * m[8]) * inv;
		r[4] = (m[0] * m[8] - m[2] * m[6]) * inv;
		r[5] = (m[2] * m[3] - m[0] * m[5]) * inv;
		r[6] = (m[3] * m[7] - m[4] * m[6]) * inv;
		r[7] = (m[1] * m[6] - m[0] * m[7]) * inv;
		r[8] = (m[0] * m[4] - m[1] * m[3]) * inv;
		return r;
	}
}

// 2x2 Jacobian (local linear part of a Transformation at a point): [[xx xy][yx yy]].
template <class T>
struct JacobianMatrix
{
	T xx, xy, yx, yy;
	T Det() const { return xx * yy - xy * yx; }
};

// 2x2 helpers used to carry a device circle through transforms as a geo-space ellipse (quadratic form).
template <class T> JacobianMatrix<T> GramMatrix(const JacobianMatrix<T>& m) // mᵀ·m (symmetric)
{
	return { m.xx*m.xx + m.yx*m.yx, m.xx*m.xy + m.yx*m.yy,
	         m.xx*m.xy + m.yx*m.yy, m.xy*m.xy + m.yy*m.yy };
}
template <class T> JacobianMatrix<T> Congruence(const JacobianMatrix<T>& g, const JacobianMatrix<T>& a) // gᵀ·a·g
{
	T t00 = g.xx*a.xx + g.yx*a.yx, t01 = g.xx*a.xy + g.yx*a.yy;
	T t10 = g.xy*a.xx + g.yy*a.yx, t11 = g.xy*a.xy + g.yy*a.yy;
	return { t00*g.xx + t01*g.yx, t00*g.xy + t01*g.yy,
	         t10*g.xx + t11*g.yx, t10*g.xy + t11*g.yy };
}
template <class T> JacobianMatrix<T> Scaled(const JacobianMatrix<T>& m, T s) { return { m.xx*s, m.xy*s, m.yx*s, m.yy*s }; }
template <class T> T QuadForm(const JacobianMatrix<T>& a, const Point<T>& d) // dᵀ·a·d
{
	return a.xx*d.first*d.first + (a.xy + a.yx)*d.first*d.second + a.yy*d.second*d.second;
}
// Half-extents of the AABB of the ellipse { d : dᵀ·a·d <= 1 } (a SPD), via the diagonal of a⁻¹.
template <class T> Point<T> EllipseAABBHalf(const JacobianMatrix<T>& a)
{
	T det = a.Det();
	return Point<T>(std::sqrt(a.yy / det), std::sqrt(a.xx / det));
}

enum class OrientationType : UInt8
{
	Default    = 0,
	LeftRight  = 0,
	RightLeft  = 1,
	TopBottom  = 0,
	BottomTop  = 2,
	NegateX    = 1,
	NegateY    = 2,
	NegateXY   = NegateX + NegateY
};

#pragma warning (push)
#pragma warning (disable: 26827)

// Provide a bitwise OR operator for OrientationType:
constexpr inline OrientationType operator|(OrientationType lhs, OrientationType rhs) noexcept
{
	using T = std::underlying_type_t<OrientationType>;
	static_assert(std::is_integral<T>::value, "Underlying type must be an integral type");
	return static_cast<OrientationType>(static_cast<T>(lhs) | static_cast<T>(rhs));
}

#pragma warning (pop)

inline bool IsRightLeft(OrientationType orientation) { return int(orientation) & 1; }
inline bool IsBottomTop(OrientationType orientation) { return int(orientation) & 2; }
inline bool MustNegateX(OrientationType orientation) { return IsRightLeft(orientation); }
inline bool MustNegateY(OrientationType orientation) { return IsBottomTop(orientation); }

// How a roi->viewPort fitting Transformation distributes scale over the axes.
// Isotropic letterboxes (maps: aspect ratio preserved); Stretch scales each axis
// independently so the roi fills the viewPort (charts: value vs index axes).
enum class FitMode : UInt8
{
	Isotropic = 0,
	Stretch   = 1,
};

template <typename T>
Point<T> GetTopLeft(const Range<Point<T>>& extents, OrientationType orientation)
{
	return
		shp2dms_order(
			IsRightLeft(orientation) ? extents.second.X() : extents.first.X()
		,	IsBottomTop(orientation) ? extents.second.Y() : extents.first.Y()
		);
}

//----------------------------------------------------------------------
// class  : Transformation : an Affine Group over the field Point<T> which is T^2
//----------------------------------------------------------------------

template <class T>
class Transformation  // p' = fp + c
{
public:
	typedef T                              scalar_type;
	typedef Point<scalar_type>             point_type;
	typedef Range<point_type>              rect_type;
	typedef typename product_type<T>::type area_type;
// 3 ctors
  	Transformation() {}
  	Transformation(const point_type& pageBase, const point_type factor)
    	:	m_Factor(factor), m_Offset(pageBase)	
	{
		CheckSingularity();
	}
  	Transformation(const point_type& pageBase, const point_type factor, const point_type& worldBase)
    	:	m_Factor(factor), m_Offset(pageBase - worldBase*factor )
	{
		CheckSingularity();
	}

	Transformation(const rect_type& roi, const rect_type& viewPort, OrientationType orientation, FitMode fitMode = FitMode::Isotropic)
	{
		point_type sizeROI = Size(roi);
		point_type sizeVP  = Size(viewPort);
		if (sizeROI == point_type(0, 0))
			sizeROI = sizeVP;
		dms_assert( !(sizeROI == point_type(0, 0)) );
		if (fitMode == FitMode::Stretch && sizeROI.first != 0 && sizeROI.second != 0)
			m_Factor = point_type(sizeVP.first / sizeROI.first, sizeVP.second / sizeROI.second);
		else
		{
			T f = (sizeVP.first * sizeROI.second < sizeVP.second* sizeROI.first || (sizeROI.second == 0))
					?	(sizeVP.first / sizeROI.first)
					:	(sizeVP.second/ sizeROI.second);
			m_Factor = point_type(f, f);
		}
		if (MustNegateY(orientation)) m_Factor.Row()= -m_Factor.Row();
		if (MustNegateX(orientation)) m_Factor.Col()= -m_Factor.Col();

		m_Offset  = Center(viewPort) - Center(roi)*m_Factor;
		CheckSingularity();
	}

	Transformation Inverse() const
	{
		return Transformation() / (*this);
	}

//	Scale operator to transform from world size to page size (axis-separable: per-axis only)
	template <typename F> Point<F> PageScale (const Point<F>& p) const { assert(IsAxisSeparable()); return p * m_Factor; }
	template <typename F> Point<F> WorldScale(const Point<F>& p) const { assert(IsAxisSeparable()); dms_assert(!IsSingular()); return p / m_Factor; }
	template <typename P> Range<P> PageScale (const Range<P>& r) const { return Range<P>(PageScale (r.first), PageScale (r.second));}
	template <typename P> Range<P> WorldScale(const Range<P>& r) const { return Range<P>(WorldScale(r.first), WorldScale(r.second));}

//	Apply operator to transform from world space to page space (point form works at any complexity)
	template <typename F> Point<F> Apply(const Point<F>& p) const
	{
		if (m_Complexity <= TransformComplexity::AnisoScale)
			return DPoint(p) * m_Factor + m_Offset;
		return ApplyHomogeneous<F>(DPoint(p), m_H);
	}
	template <typename F> Point<F> Reverse(const Point<F>& p) const
	{
		if (m_Complexity <= TransformComplexity::AnisoScale)
		{
			dms_assert(!IsSingular());
			return (DPoint(p) - m_Offset) / m_Factor;
		}
		return ApplyHomogeneous<F>(DPoint(p), transform_detail::Inverse3x3(m_H));
	}
	// Perspective denominator w of the homogeneous map at p (always 1 for <= Affine2D, so yaw/affine and
	// the level-c fast path are unaffected). For a projective (tilt) transform the horizon is the locus
	// w == 0; callers use the sign of w to cap source-extent enumeration at the horizon so a tilted view
	// never reverse-maps points behind the camera into garbage grid coords. Use on the inverse transform
	// (grid2dev.Inverse()) to test the in-front side of a device point.
	template <typename F> scalar_type ApplyDenom(const Point<F>& p) const
	{
		if (m_Complexity <= TransformComplexity::Affine2D)
			return scalar_type(1);
		return m_H[6]*scalar_type(p.first) + m_H[7]*scalar_type(p.second) + m_H[8];
	}
	// Rect overloads: the image of a rect is only a rect when axis-separable; for >c it is a rotated/
	// projective quad, so return its axis-aligned bounding box. The axis-separable branch is the exact
	// historical two-corner result (intentionally NON-normalized, preserving inversion under axis flips
	// that downstream relies on). Callers needing the actual corners use ApplyQuad / ReverseQuad.
	template <typename P> Range<P> Apply  (const Range<P>& r) const { return IsAxisSeparable() ? Range<P>(Apply  (r.first), Apply  (r.second)) : ApplyBounds  (r); }
	template <typename P> Range<P> Reverse(const Range<P>& r) const { return IsAxisSeparable() ? Range<P>(Reverse(r.first), Reverse(r.second)) : ReverseBounds(r); }

//	The four transformed corners of a rect (use when the actual quad shape matters under rotation/tilt)
	template <typename P> std::array<P, 4> ApplyQuad(const Range<P>& r) const
	{
		return { Apply(r.first)
			,	Apply(P(r.second.first, r.first.second))
			,	Apply(P(r.first.first,  r.second.second))
			,	Apply(r.second) };
	}
//	Axis-aligned bounding box of the transformed rect (use for clip extents under any complexity)
	template <typename P> Range<P> ApplyBounds(const Range<P>& r) const
	{
		auto q = ApplyQuad(r);
		P lo = q[0], hi = q[0];
		for (int i = 1; i != 4; ++i)
		{
			lo.first  = std::min(lo.first,  q[i].first);  lo.second = std::min(lo.second, q[i].second);
			hi.first  = std::max(hi.first,  q[i].first);  hi.second = std::max(hi.second, q[i].second);
		}
		return Range<P>(lo, hi);
	}
//	The four reverse-transformed corners, and their axis-aligned bounding box (Reverse counterparts)
	template <typename P> std::array<P, 4> ReverseQuad(const Range<P>& r) const
	{
		return { Reverse(r.first)
			,	Reverse(P(r.second.first, r.first.second))
			,	Reverse(P(r.first.first,  r.second.second))
			,	Reverse(r.second) };
	}
	template <typename P> Range<P> ReverseBounds(const Range<P>& r) const
	{
		auto q = ReverseQuad(r);
		P lo = q[0], hi = q[0];
		for (int i = 1; i != 4; ++i)
		{
			lo.first  = std::min(lo.first,  q[i].first);  lo.second = std::min(lo.second, q[i].second);
			hi.first  = std::max(hi.first,  q[i].first);  hi.second = std::max(hi.second, q[i].second);
		}
		return Range<P>(lo, hi);
	}

//	Inplace Apply operators (that modify their arguments)
	template <typename F> void  InplApply  (Point<F>& p) const { if (m_Complexity <= TransformComplexity::AnisoScale) { p *= m_Factor; p += m_Offset; } else p = Apply(p); }
	template <typename F> void  InplReverse(Point<F>& p) const { if (m_Complexity <= TransformComplexity::AnisoScale) { dms_assert(!IsSingular()); p -= m_Offset; p /= m_Factor; } else p = Reverse(p); }
	template <typename P> void  InplApply  (Range<P>& r) const { if (IsAxisSeparable()) { InplApply  (r.first); InplApply  (r.second); MakeBounds(r.first, r.second); } else r = ApplyBounds  (r); }
	template <typename P >void  InplReverse(Range<P>& r) const { if (IsAxisSeparable()) { InplReverse(r.first); InplReverse(r.second); MakeBounds(r.first, r.second); } else r = ReverseBounds(r); }

	//	Combine two transformations to a new Transformation: [f*g](x) := g(f(x)), where f = *this
	Transformation operator *(const Transformation& g) const
	{
		if (IsAxisSeparable() && g.IsAxisSeparable())
			return Transformation(g.Apply(m_Offset), m_Factor*g.m_Factor);
		// g(f(x)) applies f's matrix then g's: composite homogeneous matrix = Hg * Hf
		return FromHomography(transform_detail::Mul3x3(g.AsHomography(), AsHomography()), CombineComplexity(m_Complexity, g.m_Complexity));
	}

	// Combine this with post applyting then invertion of g: [f/g](x) := g^-1(f(x))
	Transformation operator /(const Transformation& g) const
	{
		if (IsAxisSeparable() && g.IsAxisSeparable())
			return Transformation(g.Reverse(m_Offset), m_Factor / g.m_Factor);
		// g^-1(f(x)): composite homogeneous matrix = inv(Hg) * Hf
		return FromHomography(transform_detail::Mul3x3(transform_detail::Inverse3x3(g.AsHomography()), AsHomography()), CombineComplexity(m_Complexity, g.m_Complexity));
	}

	// Post-translate the output by viewOffset: x -> f(x) + d  (composes a translation onto the matrix for >c)
	Transformation operator +(const point_type& viewOffset) const
	{
		if (IsAxisSeparable())
			return Transformation(m_Offset+viewOffset, m_Factor);
		std::array<T, 9> tr = { T(1), T(0), viewOffset.first,  T(0), T(1), viewOffset.second,  T(0), T(0), T(1) };
		return FromHomography(transform_detail::Mul3x3(tr, m_H), m_Complexity);
	}
	Transformation operator -(const point_type& viewOffset) const { return *this + (-viewOffset); }

	// Change this transformation x->f(x) to x->g(f(x)), thus f := (g o f), thus include applying g after this
	void operator *=(const Transformation& g) { *this = *this * g; }
	// Change this transformation x->f(x) to x->f(sx)  (axis-separable input pre-scale)
	void operator *=(const point_type& scalePair) { assert(IsAxisSeparable()); m_Factor *= scalePair; }
	void operator *=(scalar_type scaleFactor)     { assert(IsAxisSeparable()); m_Factor *= scaleFactor; }

	void operator /=(const Transformation& g) { *this = *this / g; }
	// Change this transformation x->f(x) to x->f(x/s)
	void operator /=(const point_type& scalePair) { assert(IsAxisSeparable()); m_Factor /= scalePair; }
	void operator /=(scalar_type scaleFactor)     { assert(IsAxisSeparable()); m_Factor /= scaleFactor; }

	// Change this transformation x->f(x) to x->f(x)+d
	void operator +=(const point_type& viewOffset) { *this = *this + viewOffset; }
	void operator -=(const point_type& viewOffset) { *this = *this - viewOffset; }

	// ---- complexity classification ----
	TransformComplexity Complexity()     const { return m_Complexity; }
	bool                IsAxisSeparable() const { return m_Complexity <= TransformComplexity::AnisoScale; } // levels a..c
	bool                IsAffine()        const { return m_Complexity <= TransformComplexity::Affine3D;  }  // levels a..e
	bool                IsProjective()    const { return m_Complexity == TransformComplexity::Projective; } // level f
	bool                CanReverse()      const { return IsAxisSeparable() ? (m_Factor.first != 0 && m_Factor.second != 0) : (transform_detail::Det3x3(m_H) != 0); }
	// Row-major 3x3 homogeneous matrix, valid for every complexity (synthesized for <= AnisoScale).
	// Used by serialization and, in later phases, to build a Qt QTransform / GDI parallelogram blit.
	std::array<T, 9>    Matrix()          const { return AsHomography(); }

	// ---- builders for the higher-complexity levels ----
	static Transformation FromHomography(const std::array<T, 9>& h, TransformComplexity c = TransformComplexity::Projective)
	{
		Transformation t; t.m_H = h; t.m_Complexity = c; return t;
	}
	static Transformation Affine2x3(const std::array<T, 6>& m) // [a b c; d e f]: u = a*x+b*y+c, v = d*x+e*y+f
	{
		return FromHomography({ m[0], m[1], m[2],  m[3], m[4], m[5],  T(0), T(0), T(1) }, TransformComplexity::Affine2D);
	}
	static Transformation Rotation(scalar_type angleRad, const point_type& center) // rotate about center
	{
		scalar_type cs = std::cos(angleRad), sn = std::sin(angleRad), cx = center.first, cy = center.second;
		return Affine2x3({ cs, -sn, cx - cs * cx + sn * cy,
		                   sn,  cs, cy - sn * cx - cs * cy });
	}
	// Planar perspective "tilt" (pitch): rotates the ground plane about the HORIZONTAL axis through
	// `center`, foreshortening the SECOND (Y / vertical screen) axis, so the scale shrinks toward the top
	// (far field) and the horizon is a HORIZONTAL line. NOTE this build is colrow (point.first = X/col,
	// point.second = Y/row), so the vertical screen axis that must foreshorten is `second`, NOT `first`
	// (using `first` tilts about a VERTICAL axis -> vertical horizon + scale depending on viewport X, the
	// pre-2026-06-19 bug). eyeDist D>0 controls the strength (smaller D = stronger perspective; the horizon
	// sits where w -> 0, i.e. second-offset == D/sin(pitch)). Level f (projective). Built from the 3x3
	// homography (row-major over (first,second,1)) core { first'=first/w, second'=cos*second/w,
	// w=1-(sin/D)*second }, conjugated by translate(+/-center). Keep |pitch| well below the angle that
	// brings the visible rect to the horizon (callers clamp; eyeDist=client height keeps it off-screen).
	static Transformation Tilt(scalar_type pitchRad, const point_type& center, scalar_type eyeDist)
	{
		scalar_type sn = std::sin(pitchRad), cs = std::cos(pitchRad);
		scalar_type cx = center.first, cy = center.second;
		if (eyeDist == 0) eyeDist = 1;
		std::array<T, 9> core = { T(1), T(0), T(0),  T(0), cs, T(0),  T(0), -sn / eyeDist, T(1) };
		std::array<T, 9> tNeg = { T(1), T(0), -cx,  T(0), T(1), -cy,  T(0), T(0), T(1) };
		std::array<T, 9> tPos = { T(1), T(0),  cx,  T(0), T(1),  cy,  T(0), T(0), T(1) };
		return FromHomography(transform_detail::Mul3x3(tPos, transform_detail::Mul3x3(core, tNeg)), TransformComplexity::Projective);
	}

	// ---- local linearization (rotation/tilt-safe; replaces scalar ZoomLevel for LOD in later phases) ----
	JacobianMatrix<T> JacobianAt(const point_type& p) const
	{
		if (IsAxisSeparable())
			return JacobianMatrix<T>{ m_Factor.first, T(0), T(0), m_Factor.second };
		scalar_type x = p.first, y = p.second;
		scalar_type nx = m_H[0]*x + m_H[1]*y + m_H[2];
		scalar_type ny = m_H[3]*x + m_H[4]*y + m_H[5];
		scalar_type w  = m_H[6]*x + m_H[7]*y + m_H[8];
		scalar_type w2 = w * w;
		return JacobianMatrix<T>{ (m_H[0]*w - nx*m_H[6]) / w2, (m_H[1]*w - nx*m_H[7]) / w2,
		                          (m_H[3]*w - ny*m_H[6]) / w2, (m_H[4]*w - ny*m_H[7]) / w2 };
	}
	scalar_type LocalScaleAt(const point_type& p) const { return std::sqrt(std::abs(JacobianAt(p).Det())); }

	bool operator == (const Transformation& rhs) const
	{
		bool aSep = IsAxisSeparable(), bSep = rhs.IsAxisSeparable();
		if (aSep && bSep)
			return m_Factor == rhs.m_Factor && m_Offset == rhs.m_Offset; // historical comparison (cache-key stable)
		if (aSep != bSep)
			return false;
		return m_Complexity == rhs.m_Complexity && m_H == rhs.m_H;
	}

	// Axis-separable-only accessors (assert the contract; never trip at level c). Higher levels must
	// use Apply/Reverse/JacobianAt instead of reading per-axis factor/offset.
	point_type      Factor()      const { assert(IsAxisSeparable()); return m_Factor; }
	area_type       FactorProd()  const { assert(IsAxisSeparable()); return m_Factor.first * m_Factor.second; }
	bool            IsSingular()  const { assert(IsAxisSeparable()); return m_Factor.first == 0 || m_Factor.second == 0; }
	Float64         ZoomLevel()   const { return IsAxisSeparable() ? Abs(m_Factor.first) : LocalScaleAt(point_type(0, 0)); }
	point_type      V2WFactor()   const { assert(IsAxisSeparable()); assert(!IsSingular()); return point_type(1.0 / m_Factor.first, 1.0 / m_Factor.second); }
	point_type      Offset()      const { assert(IsAxisSeparable()); return m_Offset; }
	OrientationType X_dir() const { assert(IsAxisSeparable()); return m_Factor.X() >= 0 ? OrientationType::Default : OrientationType::NegateX; }
	OrientationType Y_dir() const { assert(IsAxisSeparable()); return m_Factor.Y() >= 0 ? OrientationType::Default : OrientationType::NegateY; }

	OrientationType Orientation() const { return X_dir() | Y_dir(); }

	bool IsNonScaling() const { return IsAxisSeparable() && m_Factor.X() == 1 && m_Factor.Y() == 1; }
	bool IsLinear    () const { return IsAxisSeparable() && m_Offset.X() == 0 && m_Offset.Y() == 0; }
	bool IsIdentity  () const { return IsNonScaling() && IsLinear(); }
	bool operator < (const Transformation& rhs) const
	{
		bool aSep = IsAxisSeparable(), bSep = rhs.IsAxisSeparable();
		if (aSep && bSep)
			return m_Offset < rhs.m_Offset || (!(rhs.m_Offset < m_Offset) && m_Factor < rhs.m_Factor); // historical ordering
		if (aSep != bSep)
			return aSep; // axis-separable keys sort before matrix keys
		if (m_Complexity != rhs.m_Complexity)
			return m_Complexity < rhs.m_Complexity;
		return m_H < rhs.m_H;
	}

private:
	void CheckSingularity()
	{
		if (IsSingular())
			IllegalSingularity();
	}

	// The 3x3 homogeneous matrix for any complexity (synthesized from factor/offset when <= AnisoScale).
	std::array<T, 9> AsHomography() const
	{
		if (m_Complexity <= TransformComplexity::AnisoScale)
			return { m_Factor.first, T(0), m_Offset.first,  T(0), m_Factor.second, m_Offset.second,  T(0), T(0), T(1) };
		return m_H;
	}
	template <typename F>
	static Point<F> ApplyHomogeneous(const DPoint& d, const std::array<T, 9>& h)
	{
		T nx = h[0]*d.first + h[1]*d.second + h[2];
		T ny = h[3]*d.first + h[4]*d.second + h[5];
		T w  = h[6]*d.first + h[7]*d.second + h[8];
		return Point<F>(F(nx / w), F(ny / w));
	}
	static TransformComplexity CombineComplexity(TransformComplexity a, TransformComplexity b) { return (a > b) ? a : b; }

	point_type m_Factor = point_type(1.0, 1.0);
	point_type m_Offset = point_type(0.0, 0.0);

	// Homogeneous payload, authoritative only when m_Complexity >= Affine2D; default identity so it is
	// always well-defined (the <= AnisoScale paths never read it). Row-major 3x3 over (first,second,1).
	std::array<T, 9>    m_H = { T(1), T(0), T(0),  T(0), T(1), T(0),  T(0), T(0), T(1) };
	TransformComplexity m_Complexity = TransformComplexity::AnisoScale;
};


using CrdType =  Float64;
using CrdPoint = Point<CrdType>;
using CrdRect =  Range<CrdPoint>;
using CrdTransformation = Transformation<CrdType>;

RTC_CALL FormattedOutStream& operator << (FormattedOutStream& os, const CrdTransformation& t);

inline CrdRect GetDefaultCrdRect() { return CrdRect(0.001*MinValue<CrdPoint>(), 0.001*MaxValue<CrdPoint>()); }

#endif // __RTC_GEO_TRANSFORM_H


