/*/////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 1999-2003 Intel Corporation. All Rights Reserved.
//
//          Purpose: user defined list of functions for dispatcher
//
//          Note: only copy the function prototype of all the
//               functions you want from the release inlcude files
//
*/

#include "ippBase.h"

//IPPAPI( const IppLibraryVersion*, ippsGetLibVersion, (void) )

//IPPAPI( Ipp32f*,  ippsMalloc_32f, (int length) )
//IPPAPI( void, ippsFree, (void* ptr) )

// ipp core funccs

/*
IPPAPI( const IppLibraryVersion*, ippGetLibVersion, (void) )
IPPAPI( const char*, ippGetStatusString, ( IppStatus StsCode ) )
IPPAPI( const char*, ippCoreGetStatusString, ( IppStatus StsCode ) )
IPPAPI( IppCpuType, ippCoreGetCpuType, (void) )
*/

#if defined(DMS_USE_INTEL_IPPI)

// ipp image lib funcs

//IPPAPI( const IppLibraryVersion*, ippiGetLibVersion, (void) )

// ipp image lib alloc funcs

IPPAPI( Ipp16s*,  ippiMalloc_16s_C1,   ( int widthPixels, int heightPixels, int* pStepBytes ) )
IPPAPI( Ipp32f*,  ippiMalloc_32f_C1,   ( int widthPixels, int heightPixels, int* pStepBytes ) )
IPPAPI( void, ippiFree, (void* ptr) )

// ipp image lib copy funcs

IPPAPI ( IppStatus, ippiCopy_16s_C1R,
                    ( const Ipp16s* pSrc, int srcStep,
                      Ipp16s* pDst, int dstStep,IppiSize roiSize ))
IPPAPI ( IppStatus, ippiCopy_32f_C1R,
                    ( const Ipp32f* pSrc, int srcStep,
                      Ipp32f* pDst, int dstStep,IppiSize roiSize ))

IPPAPI(IppStatus, ippiMirror_32s_C1R, (const Ipp32s* pSrc, int srcStep, Ipp32s* pDst, int dstStep,
                                       IppiSize roiSize, IppiAxis flip))
IPPAPI(IppStatus, ippiMirror_16u_C1R, (const Ipp16u* pSrc, int srcStep, Ipp16u* pDst, int dstStep,
                                       IppiSize roiSize, IppiAxis flip))

// ipp image lib neigbourhood funcs

IPPAPI( IppStatus, ippiConvFull_32f_C1R,( const Ipp32f* pSrc1, int src1Step,
        IppiSize src1Size, const Ipp32f* pSrc2, int src2Step, IppiSize src2Size,
        Ipp32f* pDst, int dstStep ))
IPPAPI( IppStatus, ippiConvFull_16s_C1R,( const Ipp16s* pSrc1, int src1Step,
        IppiSize src1Size, const Ipp16s* pSrc2, int src2Step, IppiSize src2Size,
        Ipp16s* pDst, int dstStep, int divisor ))


#endif //defined(DMS_USE_INTEL_IPPI)

/* /////////////////////////////////////////////////////////////////////////////
//                  Convolution functions
///////////////////////////////////////////////////////////////////////////// */
/* /////////////////////////////////////////////////////////////////////////////
//  Name:       ippsConv
//  Purpose:    Linear Convolution of 1D signals
//  Parameters:
//      pSrc1                pointer to the first source vector
//      pSrc2                pointer to the second source vector
//      lenSrc1              length of the first source vector
//      lenSrc2              length of the second source vector
//      pDst                 pointer to the destination vector
//  Returns:    IppStatus
//      ippStsNullPtrErr        pointer(s) to the data is NULL
//      ippStsSizeErr           length of the vectors is less or equal zero
//      ippStsMemAllocErr       no memory for internal buffers
//      ippStsNoErr             otherwise
//  Notes:
//          Length of the destination data vector is lenSrc1+lenSrc2-1.
//          The input signal are exchangeable because of
//          commutative convolution property.
//          Some other values may be returned by FFT transform functions
*/

#if defined(DMS_USE_INTEL_IPPS)

IPPAPI(IppStatus, ippsConv_32f, ( const Ipp32f* pSrc1, int lenSrc1,
       const Ipp32f* pSrc2, int lenSrc2, Ipp32f* pDst))
IPPAPI(IppStatus, ippsConv_16s_Sfs, ( const Ipp16s* pSrc1, int lenSrc1,
       const Ipp16s* pSrc2, int lenSrc2, Ipp16s* pDst, int scaleFactor))
IPPAPI( IppStatus, ippsConv_64f,( const Ipp64f* pSrc1, int lenSrc1,
        const Ipp64f* pSrc2, int lenSrc2, Ipp64f* pDst))

#endif //defined(DMS_USE_INTEL_IPPS)

/* //////////////////////// End of file "funclist.h" //////////////////////// */
