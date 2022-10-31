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
#include "ImplMain.h"
#pragma hdrstop

#include "TifImp.h"

#include "RtcInterface.h"
#include "RtcVersion.h"

#include "cpc/EndianConversions.h"
#include "dbg/debug.h"
#include "dbg/SeverityType.h"
#include "geo/BaseBounds.h"
#include "geo/Conversions.h"
#include "geo/PointOrder.h"
#include "geo/Range.h"
#include "geo/Round.h"
#include "mci/ValueClass.h"
#include "mem/RectCopy.h"
#include "ptr/OwningPtrArray.h"
#include "ptr/SharedStr.h"
#include "ser/SafeFileWriter.h"
#include "set/BitVector.h"
#include "utl/Environment.h"
#include "utl/mySPrintF.h"
#include "ptr/SharedArrayPtr.h"

#include <tiff.h> // See http://www.libtiff.org/man/TIFFGetField.3t.html for TIFFTAG specification
#include <tiffio.h> 
#include "VersionComponent.h"

VersionComponent s_TiffVC(TIFFGetVersion() );

void TiffError2DmsException(CharPtr errType, CharPtr errFormat, va_list lst)
{
	throwErrorD(errType, myVSSPrintF(errFormat, lst).c_str());
}

void TiffWarning2Report(CharPtr errType, CharPtr errFormat, va_list lst)
{
	if (strncmp(errFormat, "unknown field", 13))
		reportD(SeverityTypeID::ST_Warning, errType, myVSSPrintF(errFormat, lst).c_str());
}

TifImp::TifImp()
{
	TIFFSetErrorHandler  (TiffError2DmsException);
	TIFFSetWarningHandler(TiffWarning2Report);
};

TifImp::~TifImp()
{
	Close();
};

bool TifImp::Open(WeakStr name, TifFileMode mode, SafeFileWriterArray* sfwa)
{
	if (IsOpen() && m_Mode != mode )
		Close();
	if (!IsOpen())
	{
		if ((mode == TifFileMode::READ)
				?	!OpenForRead       (name, sfwa)
				:	!OpenForWriteDirect(name, sfwa)
			)
			return false;
		m_Mode = mode;
	}
	return true;
};

bool TifImp::IsOpen()
{
	return m_TiffHandle;
}

bool TifImp::Close()
{
	if (m_TiffHandle != nullptr)
		TIFFClose(m_TiffHandle);
	m_TiffHandle = nullptr;
	return true;
};

//  ---------------------------------------------------------------------------

bool TifImp::IsTrueColorImage()
{
	uint16 result;
	return (TIFFGetField(m_TiffHandle, TIFFTAG_PHOTOMETRIC, &result) ==  1)
		&& (result == PHOTOMETRIC_RGB);
}

bool TifImp::IsPalettedImage()
{
	uint16 result;
	return (TIFFGetField(m_TiffHandle, TIFFTAG_PHOTOMETRIC, &result) == 1)
		&& (result == PHOTOMETRIC_PALETTE);
}

/* REMOVE
void TifImp::ReportFieldInfo()
{
	int i;
	TIFF* tif = m_TiffHandle;
	dms_assert(m_TiffHandle);

	reportD(ST_MinorTrace, TIFFFileName(tif));

	for (i = 0; i < tif->tif_nfields; i++)
	{
		const TIFFFieldInfo* fip = tif->tif_fieldinfo[i];
		if (!TIFFFieldSet(tif, fip->field_bit))
			continue;
		reportF(ST_MinorTrace, "field[%2d] %5lu, %2d, %2d, %d, %2d, %5s, %5s, %tx"
			, i
			, (unsigned long) fip->field_tag
			, fip->field_readcount, fip->field_writecount
			, fip->field_type
			, fip->field_bit
			, fip->field_oktochange ? "TRUE" : "FALSE"
			, fip->field_passcount ? "TRUE" : "FALSE"
			, fip->field_name
		);
		switch (fip->field_type)
		{
			case TIFF_NOTYPE:    // placeholder
			   break;
			case TIFF_SBYTE:     // !8-bit signed integer
			case TIFF_BYTE:      // 8-bit unsigned integer
			{
				UInt8 result = 0;
				TIFFGetField(m_TiffHandle, fip->field_tag, &result);
				if (result)
					reportF(ST_MinorTrace, "value = %d", UInt32(result));
				break;
			}
			case TIFF_ASCII:     // 8-bit bytes w/ last byte null
			{
				char* result = 0;
				TIFFGetField(m_TiffHandle, fip->field_tag, &result);
				if (result)
					reportF(ST_MinorTrace, "value = %tx", result);
				break;
			}
			case TIFF_SHORT:     // 16-bit unsigned integer
			case TIFF_SSHORT:    // !16-bit signed integer
			{
				UInt16 result = 0;
				TIFFGetField(m_TiffHandle, fip->field_tag, &result);
				reportF(ST_MinorTrace, "value = %d", UInt32(result));
				break;
			}
			case TIFF_LONG:      // 32-bit unsigned integer
			case TIFF_SLONG:     // !32-bit signed integer
			{
				UInt32 result = 0;
				TIFFGetField(m_TiffHandle, fip->field_tag, &result);
				reportF(ST_MinorTrace, "value = %d", result);
				break;
			}
			case TIFF_RATIONAL:  // 64-bit unsigned fraction
			case TIFF_UNDEFINED: // !8-bit untyped data
			case TIFF_SRATIONAL: // !64-bit signed fraction
			case TIFF_FLOAT:     // !32-bit IEEE floating point
			case TIFF_DOUBLE:    // !64-bit IEEE floating point
				break;
		default:
			reportF(ST_MajorTrace, "Field with unknown type %d", fip->field_type);
		}
	}
}
*/

UInt32 TifImp::GetWidth() const
{
	dms_assert(m_TiffHandle);

	UInt32 result = 0;
	TIFFGetField(m_TiffHandle, TIFFTAG_IMAGEWIDTH, &result);
	return result;
};


UInt32 TifImp::GetHeight() const
{
	dms_assert(m_TiffHandle);

	UInt32 result = 0;
	TIFFGetField(m_TiffHandle, TIFFTAG_IMAGELENGTH, &result);
	return result;
};

UPoint TifImp::GetSize() const
{ 
	return shp2dms_order(GetWidth(), GetHeight());
}
 

bool TifImp::HasColorTable() const
{
	dms_assert(m_TiffHandle);
	uint16* rcmap, * gcmap, * bcmap;
	return
		(GetNrBitsPerPixel() <= MAX_BITS_PAL)
		&& (TIFFGetField(m_TiffHandle, TIFFTAG_COLORMAP, &rcmap, &gcmap, &bcmap));
}

void TifImp::GetColor(PALETTE_SIZE index, UByte &r, UByte &g, UByte &b) const
{
	dms_assert(HasColorTable());
	dms_assert(index < GetClrImportant());

	//TIFFDirectory *td = &m_TiffHandle->tif_dir;
	dms_assert(m_TiffHandle);

//		td->td_samplesperpixel - td->td_extrasamples == 3
//		uint16 * td->td_transferfunction[3] ?? 
	uint16* rcmap, * gcmap, * bcmap;
	TIFFGetField(m_TiffHandle, TIFFTAG_COLORMAP, &rcmap, &gcmap, &bcmap);
	r = rcmap[index] / 256;
	g = gcmap[index] / 256;
	b = bcmap[index] / 256;
	//r = td->td_colormap[0][index] / 256;
	//g = td->td_colormap[1][index] / 256;
	//b = td->td_colormap[2][index] / 256;
	return;
}

DmsColor TifImp::GetColor(PALETTE_SIZE index) const
{
	dms_assert(HasColorTable());
	dms_assert(index < GetClrImportant());

	//TIFFDirectory *td = &m_TiffHandle->tif_dir;
	dms_assert(m_TiffHandle);
	uint16* rcmap, * gcmap, * bcmap;
	TIFFGetField(m_TiffHandle, TIFFTAG_COLORMAP, &rcmap, &gcmap, &bcmap);
//		td->td_samplesperpixel - td->td_extrasamples == 3
//		uint16 * td->td_transferfunction[3] ?? 

	return CombineRGB(
		rcmap[index] / 256,//td->td_colormap[0][index] / 256,
		gcmap[index] / 256,//td->td_colormap[1][index] / 256,
		bcmap[index] / 256 //td->td_colormap[2][index] / 256
	);
}

bool TifImp::SetColorMap(UInt16* red, UInt16* green, UInt16* blue)
{
	return TIFFSetField(m_TiffHandle, TIFFTAG_COLORMAP, red, green, blue);
}

PALETTE_SIZE TifImp::GetClrImportant() const
{
	dms_assert(HasColorTable()); // if case of TrueColor, we should not call this function, PRECONDITION

	return 1 << GetNrBitsPerPixel();
}

bool TifImp::OpenForReadDirect(WeakStr name, SafeFileWriterArray* sfwa)
{
	SharedStr workingName = GetWorkingFileName(sfwa, name, FCM_OpenReadOnly);

	m_TiffHandle = TIFFOpen(ConvertDmsFileName(name).c_str(), "r");
	return m_TiffHandle != nullptr;
}

bool TifImp::OpenForRead(WeakStr name, SafeFileWriterArray* sfwa)
{
	if (!OpenForReadDirect(name, sfwa))
		return false;
	
	// 1 byte false color? Tiled image? 
	//TIFFDirectory *td = &m_TiffHandle->tif_dir;
	if ( (m_TiffHandle == NULL) || (GetNrBitsPerPixel() > MAX_BITS) )
	{
		Close();
		return false;
	}
	m_Mode = TifFileMode::READ;
	return true;
}

bool TifImp::OpenForWriteDirect(WeakStr name, SafeFileWriterArray* sfwa)
{
	SharedStr workingName = GetWorkingFileName(sfwa, name, FCM_CreateAlways);
	GetWritePermission(workingName);

	m_TiffHandle = TIFFOpen(ConvertDmsFileName(workingName).c_str(), "a");
	if (m_TiffHandle)
		TIFFSetField(m_TiffHandle, TIFFTAG_SOFTWARE, "GeoDMS " BOOST_STRINGIZE( DMS_VERSION_MAJOR ) );
	return m_TiffHandle;
}

bool TifImp::OpenForWrite(WeakStr name, SafeFileWriterArray* sfwa, UInt32 bitsPerSample, UInt32 samplesPerPixel, bool hasPalette, SAMPLEFORMAT sampleFormat)
{
	if (!OpenForWriteDirect(name, sfwa))
		return false;
	SetDataMode(bitsPerSample, samplesPerPixel, hasPalette, sampleFormat);
	return true;
}

void TifImp::SetBaseFlags(UInt32 bitsPerSample)
{
	//TIFFSetFieldBit(m_TiffHandle, FIELD_TILEDIMENSIONS);
	TIFFSetField   (m_TiffHandle, TIFFTAG_TILEWIDTH,  UInt32(64) );
	TIFFSetField   (m_TiffHandle, TIFFTAG_TILELENGTH, UInt32(64) );

	TIFFSetField(m_TiffHandle, TIFFTAG_COMPRESSION,  COMPRESSION_LZW);
	TIFFSetField(m_TiffHandle, TIFFTAG_ORIENTATION,  ORIENTATION_TOPLEFT);

	TIFFSetField(m_TiffHandle, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG );
	TIFFSetField(m_TiffHandle, TIFFTAG_SUBFILETYPE, 0);

	m_Mode = TifFileMode::WRITE;
}

void TifImp::SetTiled()
{
	//TIFFSetFieldBit(m_TiffHandle, FIELD_TILEDIMENSIONS);
	TIFFSetField   (m_TiffHandle, TIFFTAG_TILEWIDTH,  UInt32(256) );
	TIFFSetField   (m_TiffHandle, TIFFTAG_TILELENGTH, UInt32(256) );
}

void TifImp::SetDataMode(UInt32 bitsPerSample, UInt32 samplesPerPixel, bool hasPalette, SAMPLEFORMAT sampleFormat)
{
	SetBaseFlags(bitsPerSample);

	TIFFSetField(m_TiffHandle, TIFFTAG_PHOTOMETRIC,  hasPalette ? PHOTOMETRIC_PALETTE : PHOTOMETRIC_RGB);
	TIFFSetField(m_TiffHandle, TIFFTAG_BITSPERSAMPLE, UInt16(bitsPerSample) );
	TIFFSetField(m_TiffHandle, TIFFTAG_SAMPLESPERPIXEL, samplesPerPixel);
	TIFFSetField(m_TiffHandle, TIFFTAG_SAMPLEFORMAT,    sampleFormat.m_Value );

	if (samplesPerPixel > 3)
	{
		unsigned short s = EXTRASAMPLE_ASSOCALPHA;
		TIFFSetField(m_TiffHandle, TIFFTAG_EXTRASAMPLES, 1, &s); // RGB and Alpha
	}
}

void TifImp::WriteStrip(UInt32 row, const void* data, UInt32 size)
{
	TIFFWriteEncodedStrip(m_TiffHandle, row, const_cast<void*>(data), size);
}

bool TifImp::ReadNextDir()
{
	return TIFFReadDirectory(m_TiffHandle);
}

bool TifImp::IsTiledTiff() const 
{
	dms_assert(m_TiffHandle);
	return TIFFIsTiled(m_TiffHandle);
} 

void TifImp::SetHeight(UInt32 h)
{
	TIFFSetField(m_TiffHandle, TIFFTAG_IMAGELENGTH, h);
}

void TifImp::SetWidth (UInt32 w)
{
	TIFFSetField(m_TiffHandle, TIFFTAG_IMAGEWIDTH, w);
}

UInt32 TifImp::GetTileWidth() const 
{ 
	dms_assert(m_TiffHandle);
	UInt32 tile_image_width_info;
	if (IsTiledTiff())
		TIFFGetField(m_TiffHandle, TIFFTAG_TILEWIDTH, &tile_image_width_info);
	else
		TIFFGetField(m_TiffHandle, TIFFTAG_IMAGEWIDTH, &tile_image_width_info);
	return tile_image_width_info;
};

UInt32 TifImp::GetTileByteWidth() const 
{ 
	dms_assert(m_TiffHandle);
	return IsTiledTiff()
		?	TIFFTileRowSize (m_TiffHandle)
		:	TIFFScanlineSize(m_TiffHandle);
}

UInt32 TifImp::GetTileHeight() const
{ 
	dms_assert(m_TiffHandle);
	UInt32 result, rps, il;
	if (IsTiledTiff())
		TIFFGetField(m_TiffHandle, TIFFTAG_TILELENGTH, &result); // m_TiffHandle->tif_dir.td_tilelength
	else
	{
		TIFFGetField(m_TiffHandle, TIFFTAG_ROWSPERSTRIP, &rps); // m_TiffHandle->tif_dir.td_rowsperstrip,
		TIFFGetField(m_TiffHandle, TIFFTAG_IMAGELENGTH, &il);  //m_TiffHandle->tif_dir.td_imagelength
		result = Min<UInt32>(rps, il);
	}
	return result;
};

UPoint TifImp::GetTileSize() const
{ 
	return shp2dms_order(
		GetTileWidth(), 
		GetTileHeight()
	);
}
 
SizeT TifImp::GetTileByteSize() const // the # bytes in a row - aligned tile.
{
	return IsTiledTiff() ? TIFFTileSize(m_TiffHandle) : TIFFStripSize(m_TiffHandle);
}


UInt32 TifImp::GetNrBitsPerPixel()  const
{
	dms_assert(m_TiffHandle);
	
	UInt16 bps, spp, config;
	TIFFGetField(m_TiffHandle, TIFFTAG_BITSPERSAMPLE, &bps); // m_TiffHandle->tif_dir.td_bitspersample;
	TIFFGetField(m_TiffHandle, TIFFTAG_PLANARCONFIG, &config);
	if (config == PLANARCONFIG_CONTIG)			   // m_TiffHandle->tif_dir.td_planarconfig == PLANARCONFIG_CONTIG)
	{
		TIFFGetField(m_TiffHandle, TIFFTAG_SAMPLESPERPIXEL, &spp);
		bps *= spp;  // m_TiffHandle->tif_dir.td_samplesperpixel;
	}
		
	return bps;
}

void TifImp::UnpackCheck(UInt32 nrDmsBitsPerPixel, UInt32 nrRasterBitsPerPixel, CharPtr functionName)
{
	if (nrRasterBitsPerPixel == nrDmsBitsPerPixel)
		return;
	if (nrRasterBitsPerPixel == 8 && nrDmsBitsPerPixel == 1)  // UnpackStrip(BoolPtr ...) can process this
		return;
	if (nrRasterBitsPerPixel == 4 && nrDmsBitsPerPixel == 8)  // UnpackStrip(UInt8* ...) can process this
		return;
	if (nrRasterBitsPerPixel == 24 && nrDmsBitsPerPixel == 32) // UnpackStrip(UInt32* ...) can process this
		return;
	throwErrorF(functionName, "Cannot convert %d bits DMS data from/to %d bits Raster file data", nrDmsBitsPerPixel, nrRasterBitsPerPixel);
}

void TifImp::UnpackStrip(void* stripBuff, Int32 currDataSize, UInt32 nrBitsPerPixel)
{
	if (nrBitsPerPixel >= 8)
		return;

	bit_block_t* stripBitsBegin = reinterpret_cast<bit_block_t*>(stripBuff);
	bit_block_t* stripBitsEnd   = stripBitsBegin  + ((currDataSize + sizeof(bit_block_t) - 1) / sizeof(bit_block_t));

	switch (nrBitsPerPixel) {
	case 1: BitSwap(stripBitsBegin, stripBitsEnd); break;
	case 2: TwipSwap(stripBitsBegin, stripBitsEnd); break;
	case 4: NibbleSwap(stripBitsBegin, stripBitsEnd); break;
	}
}
//  --FUNCS ------------------------------------------------------------------
void TifImp::UnpackStrip(UInt32* pixelData, void* stripBuff, UInt32 nrBitsPerPixel, Int32& currNrProcesedBytes, UInt32 nrBytesPerRow, UInt32 tw, UInt32 th)
{
	if (nrBitsPerPixel == 24) // expand to 32 bit values
	{
		dms_assert(stripBuff == (void*)pixelData); // expand to 4 byte values
		UInt32 tw3 = tw * 3, tw4 = tw * 4;
		dms_assert(nrBytesPerRow >= tw3);
		dms_assert(nrBytesPerRow <= tw4);

		currNrProcesedBytes = 4 * ((currNrProcesedBytes + 2) / 3);

		UInt8*    pixelDataBegin = reinterpret_cast<UInt8   *>(pixelData) + SizeT(th)*nrBytesPerRow;
		DmsColor* colorDataBegin = reinterpret_cast<DmsColor*>(pixelData) + SizeT(th)*tw;
		for (; th; --th)
		{
			pixelDataBegin -= nrBytesPerRow;
			UInt8*    pixelDataEnd = pixelDataBegin + tw3;
			DmsColor* colorDataEnd = colorDataBegin; colorDataBegin -= tw;

			dms_assert(reinterpret_cast<UInt8*>(colorDataBegin) >= pixelDataBegin);

			while (colorDataEnd != colorDataBegin)
			{
				pixelDataEnd -= 3;
				*--colorDataEnd = CombineRGB(pixelDataEnd[0], pixelDataEnd[1], pixelDataEnd[2]);
			}
			dms_assert(pixelDataEnd == pixelDataBegin);
		}
		dms_assert(reinterpret_cast<UInt8*>(colorDataBegin) == pixelDataBegin);
		return;
	}
}

void TifImp::UnpackStrip(UInt8* pixelData, void* stripBuff, UInt32 nrBitsPerPixel, Int32& currNrProcesedBytes, UInt32 nrBytesPerRow, UInt32 tw, UInt32 th)
{
	if (nrBitsPerPixel == 4)
	{
		currNrProcesedBytes *= 2;

		dms_assert(nrBytesPerRow * 2 >= tw);

		while (th)
		{
			--th;
			UInt8* pixelDataBegin = pixelData + th*nrBytesPerRow;
			UInt8* pixelDataEnd = pixelDataBegin + nrBytesPerRow;
			UInt8* byteDataBegin = pixelData + th*tw;
			UInt8* byteDataEnd = byteDataBegin + tw;
			if (tw % 2)
				*--byteDataEnd = ((*--pixelDataEnd) & 0xF0) >> 4;
			dms_assert(byteDataBegin >= pixelDataBegin);
			dms_assert((byteDataEnd - byteDataBegin) % 2 == 0);

			UInt16* bytePairBegin = reinterpret_cast<UInt16*>(byteDataBegin);
			UInt16* bytePairEnd = reinterpret_cast<UInt16*>(byteDataEnd);

			while (bytePairEnd != bytePairBegin)
			{
				--bytePairEnd, --pixelDataEnd;
				*bytePairEnd
					= (((*pixelDataEnd) & 0xF0) >> 4)
					| (((*pixelDataEnd) & 0x0F) << 8);
			}
			dms_assert(pixelDataEnd == pixelDataBegin);
		}
		return;
	}
}


SizeT TifImp::ReadTile(void* stripBuff, UInt32 tile_x, UInt32 tile_y, UInt32 strip_y, SizeT tileByteSize) const
{
	if (IsTiledTiff())
		return TIFFReadTile(m_TiffHandle, stripBuff, tile_x, tile_y, 0, 0);
	else
		return TIFFReadEncodedStrip(m_TiffHandle, strip_y, stripBuff, tileByteSize);
}

SizeT TifImp::WriteTile(void* stripBuff, UInt32 tile_x, UInt32 tile_y)
{
	return TIFFWriteTile(m_TiffHandle, stripBuff, tile_x, tile_y, 0, 0);
}

