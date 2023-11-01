// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "Potential.h"

#include "dbg/debug.h"
#include "geo/Conversions.h"
#include "geo/PointOrder.h"
#include "xct/DmsException.h"

#include "AbstrUnit.h"

#include "aggrFuncNum.h"
#include "attrUniStructNum.h"

#include "UseIpp.h"
#include "ippcore.h"
#include "ipps.h"


#if defined(DMS_USE_INTEL_IPPI)
#	include "IppiUtils.h"
#endif


// *****************************************************************************
//											impl
// *****************************************************************************

#if defined(DMS_USE_INTEL_IPPI)
bool IsAligned32(const void* data, UInt32 nrBytesPerRow)
{
	return 
		(reinterpret_cast<SizeT>(data) & 0x001F) == 0
	&&	(nrBytesPerRow                 & 0x001F) == 0;
}

template <typename T> struct ippiMallocator;
template <> struct ippiMallocator<Float32> {
	Float32* allocate(IPoint size, int* nrBytesPerRowPtr) { return  ippiMalloc_32f_C1(size.Col(), size.Row(), nrBytesPerRowPtr); }
	IppStatus copy  (const Float32* pSrc, int srcStep, Float32* pDst, int dstStep, IppiSize roiSize) { return ippiCopy_32f_C1R(pSrc,  srcStep, pDst, dstStep, roiSize); }
	IppStatus mirror(const Float32* pSrc, int srcStep, Float32* pDst, int dstStep, IppiSize roiSize) 
	{ 
		return ippiMirror_32s_C1R(
			reinterpret_cast<const Ipp32s*>(pSrc), srcStep,
			reinterpret_cast<      Ipp32s*>(pDst), dstStep,
			roiSize,
			ippAxsBoth
		);
	}
};

template <> struct ippiMallocator<Int16> {
	Int16* allocate(IPoint size, int* nrBytesPerRowPtr) { return  ippiMalloc_16s_C1(size.Col(), size.Row(), nrBytesPerRowPtr); }
	IppStatus copy  (const Int16* pSrc, int srcStep, Int16* pDst, int dstStep, IppiSize roiSize) { return ippiCopy_16s_C1R(pSrc,  srcStep, pDst, dstStep, roiSize); }
	IppStatus mirror(const Int16* pSrc, int srcStep, Int16* pDst, int dstStep, IppiSize roiSize)
	{
		return ippiMirror_16u_C1R(
			reinterpret_cast<const Ipp16u*>(pSrc), srcStep,
			reinterpret_cast<      Ipp16u*>(pDst), dstStep,
			roiSize,
			ippAxsBoth
		);
	}
};

template <typename T>
struct TCAlignedGrid : TGrid<const T>
{
	TCAlignedGrid(const TGrid& src)
		:	TGrid(src)
		,	m_NrBytesPerRow(0)
		,	m_DataBase(0)
	{
		UInt32 orgNrBytesPerRow = src.NrBytesPerRow();
			
		if (IsAligned32(GetDataPtr(), orgNrBytesPerRow))
			m_NrBytesPerRow = orgNrBytesPerRow;
		else
		{
			while(true) {
				m_DataBase = ippiMallocator<T>().allocate(src.GetSize(), &m_NrBytesPerRow);
				if (m_DataBase) break;
				MustCoalesceHeap(src.size() * sizeof(T));
			}; 
			dms_assert(NrBytesPerRow() >= orgNrBytesPerRow); 

			IppStatus result = ippiMallocator<T>().copy(
				GetDataPtr(), TGrid::NrBytesPerRow(), 
				m_DataBase,   m_NrBytesPerRow, 
				Convert<IppiSize>(GetSize())
			);
			DmsIppCheckResult(result, "ippiCopy", MG_POS);

			SetDataPtr(m_DataBase);
		}
		dms_assert(NrBytesPerRow() >= orgNrBytesPerRow); 
		dms_assert(IsAligned32(GetDataPtr(), m_NrBytesPerRow));
	}

	~TCAlignedGrid()
	{
		if (m_DataBase)
			ippiFree(m_DataBase);
	}

	UInt32   NrBytesPerRow() { return UInt32(m_NrBytesPerRow); }

private:
	int m_NrBytesPerRow;
	T*  m_DataBase;
};

template <typename T>
struct TAlignedGrid : TGrid<T>
{
	TAlignedGrid(const TGrid<T>& dest)
		: TGrid<T>(dest)
		, m_OrgData(0)
		, m_NrBytesPerRow(0)
	{
		UInt32 orgNrBytesPerRow = dest.NrBytesPerRow();

		if (IsAligned32(GetDataPtr(), orgNrBytesPerRow))
			m_NrBytesPerRow = orgNrBytesPerRow;
		else
		{
			m_OrgData = GetDataPtr();
			while(true) {
				SetDataPtr( ippiMallocator<T>().allocate(dest.GetSize(), &m_NrBytesPerRow) );
				if (GetDataPtr()) break;
				MustCoalesceHeap(sizeof(T)*Cardinality(GetSize()));
			}; 
		}
		dms_assert(NrBytesPerRow() >= orgNrBytesPerRow); 
		dms_assert(IsAligned32(GetDataPtr(), m_NrBytesPerRow));
	}

	~TAlignedGrid()
	{
		if (m_OrgData)
		{
			IppStatus result = ippiMallocator<T>().copy(
				GetDataPtr(), m_NrBytesPerRow, 
				m_OrgData, TGrid::NrBytesPerRow(),
				Convert<IppiSize>(GetSize())
			);
			DmsIppCheckResult(result, "ippiCopy", MG_POS);
			ippiFree(GetDataPtr()); 
		}
	}

	UInt32 NrBytesPerRow() { return m_NrBytesPerRow; }

private:
	int  m_NrBytesPerRow;
	T*   m_OrgData;
};

template <typename T>
struct TReversedKernel: TGrid<T>
{
	TReversedKernel(const TGrid<const T>& src)
		:	TGrid(src.GetSize(), 0) // prevent destruction of src.m_Data in case of allocation failure
	{
		while(true) {
			SetDataPtr( ippiMallocator<T>().allocate(src.GetSize(), &m_NrBytesPerRow) );
			if (GetDataPtr()) break;
			MustCoalesceHeap(sizeof(T)*Cardinality(GetSize()));
		}; 
		UInt32 nrBytesPerRow = src.GetSize().Col() * sizeof(T); 
		IppStatus result =  ippiMallocator<T>().mirror(
			src.GetDataPtr(), src.NrBytesPerRow(), 
			GetDataPtr(),     NrBytesPerRow(), 
			Convert<IppiSize>(GetSize())
		);
		DmsIppCheckResult(result, "ippiMirror_32f_C1R", MG_POS);
	}
	~TReversedKernel()
	{
		if (GetDataPtr())
			ippiFree(GetDataPtr()); 
	}
	UInt32 NrBytesPerRow() { return m_NrBytesPerRow; }

private:
	int  m_NrBytesPerRow;
};

bool PotentialIppi16s(
	const TCInt16Grid& dataOrg, 
	const TInt16Grid & outputOrg,
	const TCInt16Grid& weightOrg
)
{
	DBG_START("Potential", "Ippi", MG_DEBUG_POTENTIAL);

	TCAlignedGrid  <Int16> data  (dataOrg);
	TReversedKernel<Int16> weight(weightOrg);
	TAlignedGrid   <Int16> output(outputOrg);

	dms_assert(IsAligned32(data  .GetDataPtr(), data.  NrBytesPerRow()));
	dms_assert(IsAligned32(weight.GetDataPtr(), weight.NrBytesPerRow()));
	dms_assert(IsAligned32(output.GetDataPtr(), output.NrBytesPerRow()));

	dms_assert(data.GetSize() == output.GetSize());

	IppStatus result = ippiConvFull_16s_C1R(
		data  .GetDataPtr(), data.  NrBytesPerRow(), Convert<IppiSize>(data  .GetSize()),
		weight.GetDataPtr(), weight.NrBytesPerRow(), Convert<IppiSize>(weight.GetSize()),
		output.GetDataPtr(), output.NrBytesPerRow(), 0
	);
	DmsIppCheckResult(result, "ippiConvFull_16s_C1R", MG_POS);
	return result == ippStsNoErr;
}

bool PotentialIppi32f(potential_context<Float32>& context,
	const TCFloat32Grid& dataOrg,
	const TFloat32Grid & outputOrg,
	const TCFloat32Grid& weightOrg
)
{
	DBG_START("Potential", "Ippi", MG_DEBUG_POTENTIAL);

	TCAlignedGrid  <Float32> data  (dataOrg);
	TReversedKernel<Float32> weight(weightOrg);
	TAlignedGrid   <Float32> output(outputOrg);

	dms_assert(IsAligned32(data  .GetDataPtr(), data.  NrBytesPerRow()));
	dms_assert(IsAligned32(weight.GetDataPtr(), weight.NrBytesPerRow()));
	dms_assert(IsAligned32(output.GetDataPtr(), output.NrBytesPerRow()));

	dms_assert(data.GetSize() == output.GetSize());

	IppStatus result = ippiConvFull_32f_C1R(
		data  .GetDataPtr(), data.  NrBytesPerRow(), Convert<IppiSize>(data  .GetSize()),
		weight.GetDataPtr(), weight.NrBytesPerRow(), Convert<IppiSize>(weight.GetSize()),
		output.GetDataPtr(), output.NrBytesPerRow()
	);
	DmsIppCheckResult(result, "ippiConvFull_32f_C1R", MG_POS);
	return result == ippStsNoErr;
}

#endif //defined(DMS_USE_INTEL_IPPI)

//================================================== IppsArray

template <typename A>
void IppsArray<A>::clean()
{
	if (m_Data) //delete [] (m_Data);
		::operator delete(m_Data, std::align_val_t{ 64 });
}

template <typename A>
void IppsArray<A>::reserve(TileSize nrElem)
{
	if (nrElem > m_Capacity)
	{
		clean();

		IppSizeL byteSize = nrElem * sizeof(A);
		m_Data = reinterpret_cast<A*>(::operator new(byteSize, std::align_val_t{ 64 }));
		m_Capacity = nrElem;
	}
}

namespace potential::impl {

template <typename A, typename T>
TileSize IppsArray_Init(IppsArray<A>* self, UPoint& zeroInfo, const UGrid<const T>& dataOrg, SideSize kernelWidth)
{
	DBG_START("IppsArray", "Init", MG_DEBUG_POTENTIAL);

	dms_assert(kernelWidth);
	dms_assert(dataOrg.GetSize().Row());

	SideSize nrOrgRows = dataOrg.GetSize().Row();
	SideSize nrOrgCols = dataOrg.GetSize().Col();
	SideSize borderSize = kernelWidth - 1;
	SideSize bufferWidth = nrOrgCols + borderSize;
	TileSize bufferSize = TileSize(bufferWidth) * TileSize(nrOrgRows) - TileSize(borderSize);
	dms_assert(bufferSize <= self->capacity());

	if (zeroInfo.Col() != nrOrgCols)
	{
		zeroInfo.Col() = nrOrgCols;
		zeroInfo.Row() = 0;
	}

	const T* src = dataOrg.begin();
	A* bufferPtr = self->begin();
	SideSize nrRows = 0;
	while (true)
	{
		auto src1 = src + nrOrgCols;
		fast_copy(src, src1, bufferPtr);
		src = src1;
		if (++nrRows == nrOrgRows)
			break;
		if (zeroInfo.Row() < nrRows)
		{
			fast_zero(bufferPtr + nrOrgCols, bufferPtr + bufferWidth);
			zeroInfo.Row()++;
		}
#if defined(MG_DEBUG)
		else
		{
			for (auto ptr = bufferPtr + nrOrgCols; ptr != bufferPtr + bufferWidth; ++ptr)
				dms_assert(*ptr == 0); // must have been set in previous call
		}
#endif
		bufferPtr += bufferWidth; // start of next line in overlap area
	}
	dms_assert(self->begin() + bufferSize == bufferPtr + nrOrgCols);
	return bufferSize;
}

template <typename A, typename T>
void IppsArray_InitReversed(IppsArray<A>* self, const UGrid<const T>& dataOrg, SideSize tileDataWidth)
{
	DBG_START("IppsArray", "InitReversed", MG_DEBUG_POTENTIAL);

	dms_assert(self);

	SideSize nrOrgRows = dataOrg.GetSize().Row();
	SideSize nrOrgCols = dataOrg.GetSize().Col();
	SideSize paddingWidth = tileDataWidth -1;
	TileSize bufferSize = dataOrg.size() + SizeT(paddingWidth) * (nrOrgRows-1);
	if (self->WasInitialized())
	{
		dms_assert(self->capacity() == bufferSize);
		return;
	}
	self->reserve(bufferSize);

	const T* srckernelDataEnd = dataOrg.end();
	A* resultBufferPtr = self->begin();
	while (nrOrgRows)
	{
		const T* srcKernelDataRowBegin = srckernelDataEnd - nrOrgCols;
		resultBufferPtr = fast_copy(srcKernelDataRowBegin, srckernelDataEnd, resultBufferPtr);
		srckernelDataEnd = srcKernelDataRowBegin;
		if (--nrOrgRows)
		{
			A* bufferAfterPadding = resultBufferPtr + paddingWidth;
			fast_zero(resultBufferPtr, bufferAfterPadding);
			resultBufferPtr = bufferAfterPadding;
		}
	}
	dms_assert(self->begin() + self->capacity() == resultBufferPtr);
}

/*
inline IppStatus ippsConv( const Ipp16s* pSrc1, int lenSrc1, const Ipp16s* pSrc2, int lenSrc2, Ipp16s* pDst)
{
	return ippsConv_16s_Sfs(pSrc1, lenSrc1, pSrc2, lenSrc2, pDst, 1);
}
*/

inline IppStatus ippsConv( const Ipp32f* pSrc1, int lenSrc1, const Ipp32f* pSrc2, int lenSrc2, Ipp32f* pDst, IppsArray<UInt8>& buffer)
{
	int buffSize;
	IppStatus status = ippsConvolveGetBufferSize(lenSrc1, lenSrc2, ipp32f, (IppEnum)(ippAlgAuto), &buffSize);
	if (status != ippStsNoErr)
		return status;

	if (buffer.capacity() < buffSize)
		buffer = IppsArray<UInt8>(buffSize);

	return ippsConvolve_32f(pSrc1, lenSrc1, pSrc2, lenSrc2, pDst, (IppEnum)(ippAlgAuto), buffer.begin());
}

inline IppStatus ippsConv( const Ipp64f* pSrc1, int lenSrc1, const Ipp64f* pSrc2, int lenSrc2, Ipp64f* pDst, IppsArray<UInt8>& buffer)
{
	int buffSize;
	IppStatus status = ippsConvolveGetBufferSize(lenSrc1, lenSrc2, ipp64f, (IppEnum)(ippAlgAuto), &buffSize);
	if (status != ippStsNoErr)
		return status;

	if (buffer.capacity() < buffSize)
		buffer = IppsArray<UInt8>(buffSize);

	return ippsConvolve_64f(pSrc1, lenSrc1, pSrc2, lenSrc2, pDst, (IppEnum)(ippAlgAuto), buffer.begin());
}


template <typename A, typename T>
TileSize PotentialIppsRaw(potential_context<A>& context, UPoint& zeroInfo, const kernel_info& kernelInfo, const UGrid<const T>& dataOrg)
{
	DBG_START("Potential", "Ipps", MG_DEBUG_POTENTIAL);

	if (!context.WasInitialized())
	{
		TileSize bufferSize = Cardinality(kernelInfo.maxDataSize) + Cardinality(UPoint(kernelInfo.orgWeightSize.Col()-1, kernelInfo.maxDataSize.Row() - 1));
		context.paddedInput.reserve(bufferSize);
		context.overlappingOutput.reserve(Cardinality(kernelInfo.maxColvolvedSize));
	}
	auto& weightBuffer = *kernelInfo.weightBuffer<A>(dataOrg.GetSize().Col());

	SideSize nrCols = dataOrg.GetSize().Col() + kernelInfo.orgWeightSize.Col() - 1;
	SideSize nrRows = dataOrg.GetSize().Row() + kernelInfo.orgWeightSize.Row() - 1;

	auto dataBufferSize = IppsArray_Init(&context.paddedInput, zeroInfo, dataOrg, kernelInfo.orgWeightSize.Col()); // fill all in-between space with zeroes once

	TileSize outputSize = dataBufferSize + weightBuffer.capacity() - 1;
	dms_assert(outputSize == SizeT(nrCols) * nrRows ); // elementary math

	dms_assert(dataBufferSize <= context.paddedInput.capacity());
	dms_assert(outputSize <= context.overlappingOutput.capacity());


	IppStatus result = 
		ippsConv(
			context.paddedInput.begin(), dataBufferSize,
			weightBuffer.begin(), weightBuffer.capacity(),
			context.overlappingOutput.begin(),
			context.ippsBuffer
		);
	DmsIppCheckResult(result, "ippsConv", MG_POS);	
	if (result != ippStsNoErr)
		DmsException::throwMsgF("Convolution failed with code %d", result);

	return outputSize;
}

inline Float64 Sqr64(Float64 v) { return v*v; }

template <typename A, typename T>
TileSize PotentialIppsSmooth(potential_context<A>& context, UPoint& zeroInfo, const kernel_info& kernelInfo, const UGrid<const T>& dataOrg)
{
	auto outputSize = PotentialIppsRaw<A, T>(context, zeroInfo, kernelInfo, dataOrg);

	Float64 sumSqrData = 0; 
	auto firstOutput = context.overlappingOutput.begin();
	auto lastOutput = context.overlappingOutput.begin() + outputSize;
	for (auto ptr = firstOutput, end = lastOutput; ptr !=end ; ++ptr)
		sumSqrData += Sqr64(*ptr);

	auto errThreshold = sqrt(sumSqrData) / 1000000000.0;
	auto errThresholdNeg = - errThreshold;
	for (auto ptr = firstOutput, end = lastOutput; ptr != end; ++ptr)
		if (*ptr < errThreshold && errThresholdNeg < *ptr)
			*ptr = 0.0;

	return outputSize;
}

// *****************************************************************************
//	CalculateClassic
// *****************************************************************************

template <typename T>
bool CalculateClassic(AnalysisType at,
		const UGrid<const T>& dataGrid, const UGrid<const T>& kernelGrid,
		const kernel_info& kernelInfo, IppsArray<T>& output)
{
	DBG_START("Potential", "CalculateClassic", true);

	auto dataSize = dataGrid.GetSize();
	auto kernelSize = kernelGrid.GetSize();

	if (!output.WasInitialized())
	{
		output.reserve(Cardinality(kernelInfo.maxColvolvedSize));
	}

	UPoint outputSize = dataSize + kernelSize - UPoint(1, 1);
	dms_assert(Cardinality(outputSize) <= output.capacity());

	UGrid<T> outputGrid(outputSize, output.begin());
	for (UCoordType outputRow = 0; outputRow != outputSize.Row(); ++outputRow)
	{
		auto firstDataRow = (outputRow + 1 > kernelSize.Row()) ? outputRow + 1 - kernelSize.Row() : 0;
		Range<UCoordType> dataRowRange(firstDataRow, Min<UCoordType>(outputRow + 1, dataSize.Row()));

		Range<UCoordType> weightRowRange((dataRowRange.first + kernelSize.Row()) - (outputRow + 1), (dataRowRange.second + kernelSize.Row()) - (outputRow + 1));

		dms_assert(dataRowRange.first   < dataRowRange.second);
		dms_assert(weightRowRange.first < weightRowRange.second);

		for (UCoordType outputCol = 0; outputCol != outputSize.Col(); ++outputCol)
		{
			auto firstDataCol = (outputCol + 1 > kernelSize.Col()) ? outputCol + 1 - kernelSize.Col() : 0;
			Range<UCoordType> dataColRange(firstDataCol, Min<UCoordType>(outputCol + 1, dataSize.Col()));

			Range<UCoordType> weightColRange(dataColRange.first  - (outputCol + 1 - kernelSize.Col()), dataColRange.second - (outputCol + 1 - kernelSize.Col()));

			UCoordType weightColDiff = weightColRange.second - weightColRange.first;

			dms_assert(dataColRange  .first < dataColRange  .second);
			dms_assert(weightColRange.first < weightColRange.second);

			T* outputPtr = outputGrid.elemptr(shp2dms_order(outputCol, outputRow));	
			*outputPtr = 0;
			
			for (UCoordType weightRow = weightRowRange.first, dataRowLocal = dataRowRange.first; weightRow != weightRowRange.second; ++weightRow, ++dataRowLocal)
			{
				const T* dataPtr   = dataGrid  .elemptr(shp2dms_order(dataColRange  .first, dataRowLocal));
				const T* weightPtr = kernelGrid.elemptr(shp2dms_order(weightColRange.first, weightRow   ));
				const T* weightEnd = weightPtr + weightColDiff;

				switch (at)
				{
					case AnalysisType::PotentialSlow:
						while (weightPtr != weightEnd)
							*outputPtr += *dataPtr++ * *weightPtr++;
						break;
					case AnalysisType::Proximity:
						while (weightPtr != weightEnd)
							MakeMax(*outputPtr, *dataPtr++ * *weightPtr++); 
						break;
				}
			} // next weightRow, dataRowLocal
		} // next outputCol, dataCol
	} // next outputRow, dataRow

	return true;
} // CalculateClassic

} // namespace potential::impl

#if defined(DMS_POTENTIAL_I16)

//bool Potential(AnalysisType at, const TCInt16Grid& data, const TInt16Grid & output, const TCInt16Grid& weight)
bool Potential(AnalysisType at, potential_contexts& context, const kernel_info& weightBuffer, const TCInt16Grid& data)
{
	DBG_START("Potential", "", MG_DEBUG_POTENTIAL);

	switch (at) {
	case AnalysisType::PotentialSlow:
	case AnalysisType::Proximity:
		return impl::CalculateClassic<Int16>(at, data, output, weightBuffer.weightBufferI16);

#if defined(DMS_USE_INTEL_IPPI)
	case AnalysisType::PotentialIppi:
		return impl::PotentialIppi16s(data, output, weight);
#endif //defined(DMS_USE_INTEL_IPPI)

#if defined(DMS_USE_INTEL_IPPS)
		//		case AnalysisType::PotentialIppsPacked:
	case AnalysisType::PotentialRawIppsPacked:
		return impl::PotentialIppsRaw<Int16>(potential_context<A>&context, kernel_info & kernelInfo, const TGrid<const A>&dataOrg)
			//data, output, weight);

	case AnalysisType::PotentialIpps64:
		return impl::PotentialIppsSmooth<Float64>(data, output, weight);
	case AnalysisType::PotentialRawIpps64:
		return impl::PotentialIppsRaw<Float64>(data, output, weight);
#endif //defined(DMS_USE_INTEL_IPPS)

	default:
		throwIllegalAbstract(MG_POS, "Potential");
	}

	return true;
}
#endif //defined(DMS_POTENTIAL_I16)
/* REMOVE
template < typename T>
MDL_CALL kernel_info PrepareConvolutionKernels(AnalysisType at, const UGrid<const T>& weightOrg, UPoint maxDataTileSize)
{
	DBG_START("PrepareConvolutionKernel", "Ipps", MG_DEBUG_POTENTIAL);

	//	dms_assert(dataOrg.GetSize() == outputOrg.GetSize());
	kernel_info result;
	if (Cardinality(maxDataTileSize))
	{
		result.maxDataSize = maxDataTileSize;
		result.orgWeightSize = weightOrg.GetSize();
		result.maxColvolvedSize = maxDataTileSize + result.orgWeightSize - UPoint(1, 1);

		dms_assert(maxDataTileSize.Col());
		dms_assert(weightOrg.GetSize().Row());

		result.orgWeightGrid = weightOrg;
	}
	return result;
}
*/

template < typename T>
MDL_CALL void AddConvolutionKernel(kernel_info& self, AnalysisType at, SideSize nrDataCols)
{
	DBG_START("AddConvolutionKernel", "Ipps", MG_DEBUG_POTENTIAL);

	if (!nrDataCols)
		return;

	//	dms_assert(dataOrg.GetSize() == outputOrg.GetSize());
	const UGrid<const T>& weightOrg = *std::any_cast<UGrid<const T>>(&self.orgWeightGrid);
	SizeT weightBufferSize = weightOrg.size() + Cardinality(UPoint(nrDataCols - 1, weightOrg.GetSize().Row() - 1));
	if (at == AnalysisType::PotentialIpps64 || at == AnalysisType::PotentialRawIpps64)
		potential::impl::IppsArray_InitReversed(self.weightBuffer<Float64>(nrDataCols), weightOrg, nrDataCols);
	else if (at == AnalysisType::PotentialIppsPacked || at == AnalysisType::PotentialRawIppsPacked)
		potential::impl::IppsArray_InitReversed(self.weightBuffer<T>(nrDataCols), weightOrg, nrDataCols);
}

bool Potential(AnalysisType at, potential_contexts& context, const kernel_info& kernelInfo, const UCFloat32Grid& dataOrg)
{
	DBG_START("Potential", "Float32", MG_DEBUG_POTENTIAL);

	using namespace potential;

	dms_assert(!dataOrg.empty());

	switch (at) {
		case AnalysisType::PotentialSlow:
		case AnalysisType::Proximity:
			return impl::CalculateClassic<Float32>(at, 
				dataOrg, std::any_cast<UGrid<const Float32>>(kernelInfo.orgWeightGrid),
				kernelInfo, context.F32.overlappingOutput
			);

#if defined(DMS_USE_INTEL_IPPI)
		case AnalysisType::PotentialIppi:
			return impl::PotentialIppi32f(data, output, weight);
#endif //defined(DMS_USE_INTEL_IPPI)

#if defined(DMS_USE_INTEL_IPPS)
		case AnalysisType::PotentialRawIppsPacked:
			return impl::PotentialIppsRaw   <Float32>(context.F32, context.zeroInfo, kernelInfo, dataOrg);

		case AnalysisType::PotentialIppsPacked:
			return impl::PotentialIppsSmooth<Float32>(context.F32, context.zeroInfo, kernelInfo, dataOrg);

		case AnalysisType::PotentialRawIpps64:
			return impl::PotentialIppsRaw   <Float64>(context.F64, context.zeroInfo, kernelInfo, dataOrg);

		case AnalysisType::PotentialIpps64:
			return impl::PotentialIppsSmooth<Float64>(context.F64, context.zeroInfo, kernelInfo, dataOrg);
#endif //defined(DMS_USE_INTEL_IPPS)

		default:
			throwIllegalAbstract(MG_POS, "Potential");
	}
		
	return true;
}

bool Potential(AnalysisType at, potential_contexts& context, const kernel_info& kernelInfo, const UCFloat64Grid& dataOrg)
{
	DBG_START("Potential", "Float64", MG_DEBUG_POTENTIAL);

	using namespace potential;

	if (at == AnalysisType::PotentialIppsPacked   ) at = AnalysisType::PotentialIpps64;
	if (at == AnalysisType::PotentialRawIppsPacked) at = AnalysisType::PotentialRawIpps64;

	switch (at) {
		case AnalysisType::PotentialSlow:
		case AnalysisType::Proximity:
			return impl::CalculateClassic<Float64>(at,
				dataOrg, std::any_cast<UGrid<const Float64>>(kernelInfo.orgWeightGrid),
				kernelInfo, context.F64.overlappingOutput
			);

#if defined(DMS_USE_INTEL_IPPS)
		case AnalysisType::PotentialRawIpps64:
			return impl::PotentialIppsRaw   <Float64>(context.F64, context.zeroInfo, kernelInfo, dataOrg);

		case AnalysisType::PotentialIpps64:
			return impl::PotentialIppsSmooth<Float64>(context.F64, context.zeroInfo, kernelInfo, dataOrg);
#endif //defined(DMS_USE_INTEL_IPPS)
		default:
			throwIllegalAbstract(MG_POS, "Potential");
	}
		
	return true;
}

template IppsArray<Float32>;
template IppsArray<Float64>;

//REMOVE template kernel_info PrepareConvolutionKernel<Float32>(AnalysisType at, const UGrid<const Float32>& weightOrg, UPoint maxDataTileSize);
//REMOVE template kernel_info PrepareConvolutionKernel<Float64>(AnalysisType at, const UGrid<const Float64>& weightOrg, UPoint maxDataTileSize);

template void AddConvolutionKernel<Float32>(kernel_info& self, AnalysisType at, SideSize nrDataCols);
template void AddConvolutionKernel<Float64>(kernel_info& self, AnalysisType at, SideSize nrDataCols);

// *****************************************************************************

#if defined(MDL_EXPORTS)

bool MDL_Potential32(
	AnalysisType at, 
	const TCFloat32Grid& data, 
	const TFloat32Grid&  output,
	const TCFloat32Grid& weight)
{
	return Potential(at, data, output, weight);
}

bool MDL_Potential64(
	AnalysisType at, 
	const TCFloat64Grid& data, 
	const TFloat64Grid&  output,
	const TCFloat64Grid& weight)
{
	return Potential(at, data, output, weight);
}

#endif


