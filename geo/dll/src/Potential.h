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

#if !defined(__GEO_POTENTIAL_H)
#define  __GEO_POTENTIAL_H

#include "GeoBase.h"

#include "mem/Grid.h"

#include "IppBase.h"

//#define DMS_POTENTIAL_I16
const bool MG_DEBUG_POTENTIAL = false;

// *****************************************************************************
//											INTERFACE
// *****************************************************************************

using SideSize = UInt32;
using TileSize = SizeT;

enum class AnalysisType {
	PotentialIpps64 = 0, 
	PotentialRawIpps64 = 1, 
	PotentialIppsPacked = 2, 
	PotentialRawIppsPacked = 3, 
	Proximity = 4, 
#if defined(DMS_USE_INTEL_IPPI)
	PotentialIppi = 5,
#endif //defined(DMS_USE_INTEL_IPPI)
	PotentialSlow = 6,

#if defined(DMS_USE_INTEL_IPPS)
	PotentialDefault = PotentialIpps64
#else
	AT_PotentialDefault = PotentialSlow
#endif

};

// *****************************************************************************
//	INTERFACE FUNCTIONS: Potential
// *****************************************************************************

#if defined(MDL_EXPORTS)

extern "C" {

MDL_CALL bool DMS_CONV MDL_Potential32(AnalysisType at,
	const TCFloat32Grid& data, 
	const TFloat32Grid & output,
	const TCFloat32Grid& weight
);

MDL_CALL bool DMS_CONV MDL_Potential64(AnalysisType at,
	const TCFloat64Grid& data, 
	const TFloat64Grid & output,
	const TCFloat64Grid& weight
);

} // extern "C"

#endif

//================================================== IppsArray

template <typename A>
struct IppsArray
{
	IppsArray() = default;

	IppsArray(TileSize nrElem)
	{
		reserve(nrElem);
	}
	~IppsArray()
	{
		clean();
	}
	IppsArray(IppsArray& rhs) noexcept
		:	m_Data(rhs.m_Data)
		,	m_Capacity(rhs.m_Capacity)
	{
		rhs.m_Data = nullptr;
		rhs.m_Capacity = 0;
	}
	void reserve(TileSize nrElem);

	void operator = (IppsArray&& rhs) noexcept
	{
		std::swap(m_Data, rhs.m_Data);
		std::swap(m_Capacity, rhs.m_Capacity);
	}
	A* begin() { return m_Data; }
	const A* begin() const { return m_Data; }

	bool WasInitialized() const { return m_Capacity != 0;  }
	TileSize capacity() const { return m_Capacity;  }

private:
	void clean();

	A* m_Data = nullptr; // align_t{64}
	TileSize m_Capacity = 0;
};

//====================================================
#if defined(DMS_POTENTIAL_I16)

bool Potential(AnalysisType at, 
	const TCInt16Grid& data, 
	const TInt16Grid & output,
	const TCInt16Grid& weight
);
#endif //defined(DMS_POTENTIAL_I16)


struct kernel_info
{
	UPoint orgWeightSize, maxDataSize, maxColvolvedSize;
	std::map<SideSize, IppsArray<Float32>> weightBuffers32; // used by PotentialRawIppsPacked, PotentialIppsPacked for Float32 data; size: (nx+kx-1)*(ky-1)+kx = kx*ky + (nx-1)*(ky-1);
	std::map<SideSize, IppsArray<Float64>> weightBuffers64; // used by PotentialRawIpps64, PotentialIpps64; size: (nx+kx-1)*(ky-1)+kx = kx*ky + (nx-1)*(ky-1);
	std::any orgWeightGrid;            // used by PotentialSlow and Proximity; size: kx*ky
	TileCRef weightShadowTile;

	template <typename T> auto weightBuffer(SideSize paddingSize) const;
	template <> auto weightBuffer<Float32>(SideSize paddingSize) const { return &weightBuffers32.at(paddingSize); }
	template <> auto weightBuffer<Float64>(SideSize paddingSize) const { return &weightBuffers64.at(paddingSize); }
	template <typename T> auto weightBuffer(SideSize paddingSize);
	template <> auto weightBuffer<Float32>(SideSize paddingSize) { return &weightBuffers32[paddingSize]; }
	template <> auto weightBuffer<Float64>(SideSize paddingSize) { return &weightBuffers64[paddingSize]; }
#if defined(DMS_POTENTIAL_I16)
	IppsArray<Int16> weightBufferI16;
	template <> auto weightBuffer<Int16>() { return &weightBufferI16; }
	template <> auto weightBuffer<Int16>() const { return &weightBufferI16; }
#endif //defined(DMS_POTENTIAL_I16)
};

template < typename T>
MDL_CALL kernel_info PrepareConvolutionKernel(AnalysisType at, TileCRef weightShadowTile, const UGrid<const T>& weightOrg, UPoint maxDataTileSize)
{
	DBG_START("PrepareConvolutionKernel", "Ipps", MG_DEBUG_POTENTIAL);

	//	dms_assert(dataOrg.GetSize() == outputOrg.GetSize());
	kernel_info result;
	result.maxDataSize = maxDataTileSize;
	result.orgWeightSize = weightOrg.GetSize();
	if (Cardinality(maxDataTileSize))
	{
		result.maxColvolvedSize = maxDataTileSize + result.orgWeightSize - UPoint(1, 1);

		dms_assert(maxDataTileSize.Col());
		dms_assert(weightOrg.GetSize().Row());
	}
	result.weightShadowTile = weightShadowTile;
	result.orgWeightGrid = weightOrg;
	return result;
}

template < typename T>
MDL_CALL void AddConvolutionKernel(kernel_info& self, AnalysisType at, SideSize nrDataCols);


template <typename A>
struct potential_context
{
	potential_context() = default;
	potential_context(potential_context&&) = default;

	IppsArray<A> paddedInput;       // size: (nx+kx-1)*(ny-1)+nx = nx*ny + (kx-1)*(ny-1); not used for PotentialSlow and Proximity
	IppsArray<A> overlappingOutput; // size: (nx+kx-1)*(ny+ky-1)
	IppsArray<UInt8> ippsBuffer;

	bool WasInitialized() const { return overlappingOutput.WasInitialized(); }
};

struct potential_contexts
{
	potential_contexts() = default;
	potential_contexts(potential_contexts&&) = default;

	potential_contexts(potential_contexts const& rhs)
	{
		if (rhs.WasInitialized())
			throwIllegalAbstract(MG_POS, "potential_contexts::Copy Constructor with non-empty right hand side");
	}

	bool WasInitialized() const { return F32.WasInitialized() || F64.WasInitialized(); }

	potential_context<Float32> F32;
	potential_context<Float64> F64;
	template <typename> auto F() const;
	template <> auto F<Float32>() const { return &F32; };
	template <> auto F<Float64>() const { return &F64; };

	UPoint zeroInfo;
};

bool Potential(AnalysisType at, potential_contexts& context, const kernel_info& weightBuffer, const UCFloat32Grid& data);
bool Potential(AnalysisType at, potential_contexts& context, const kernel_info& weightBuffer, const UCFloat64Grid& data);

// *****************************************************************************

#endif //!defined(__GEO_POTENTIAL_H)
