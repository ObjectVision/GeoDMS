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

#ifndef _TIF_IMP_HPP_
#define _TIF_IMP_HPP_

#include "ImplMain.h"
#include "ViewPortInfo.h"

#include "dbg/Check.h"
#include "geo/color.h"
#include "geo/SequenceTraits.h"
#include "mem/TileData.h"

#include <vector>

//  ---------------------------------------------------------------------------

const UInt32 MAX_BITS       = 64;
const UInt32 MAX_BITS_PAL    = 8;
const UInt32 MAX_COLORS_PAL  = (1L << MAX_BITS_PAL);

//  --ENUMERATORS--------------------------------------------------------------

enum class TifFileMode { READ, WRITE, UNDEFINED }; //, TIF_EDITPALETTE };
struct SafeFileWriterArray;

//  ---------------------------------------------------------------------------

//  --TYPES  ------------------------------------------------------------------

/*
 * TIFF is defined as an incomplete type to hide the
 * library's internal data structures from clients.
 */
typedef	struct tiff TIFF;

//  --CLASSES------------------------------------------------------------------

struct TifErrorFrame
{
	STGIMPL_CALL TifErrorFrame();
	STGIMPL_CALL ~TifErrorFrame();

	void ThrowUpWhateverCameUp();
	void ReleaseError();

	TifErrorFrame* m_PrevFrame;
	CharPtr m_ErrType = nullptr;
	SharedStr m_Msg;
};

//  --CLASSES------------------------------------------------------------------

class TifImp
{
private:
	TIFF * m_TiffHandle = nullptr;
	TifFileMode m_Mode  = TifFileMode::UNDEFINED; // only defined when (m_TiffHandle != nullptr)

public:
	UInt32 GetTileWidth()     const; 
	STGIMPL_CALL UInt32 GetTileByteWidth() const;
	UInt32 GetTileHeight()    const; 
	STGIMPL_CALL UPoint GetTileSize()      const;
	STGIMPL_CALL SizeT  GetTileByteSize()  const; // the # bytes in a row - aligned tile.

	static STGIMPL_CALL void UnpackCheck(UInt32 nrDmsBitsPerPixel, UInt32 nrRasterBitsPerPixel, CharPtr functionName, CharPtr direction, CharPtr dataSourceName);
	static STGIMPL_CALL void UnpackStrip(void* stripBuf, Int32 currDataSize, UInt32 nrBitsPerPixel);
	static STGIMPL_CALL void UnpackStrip(UInt32* pixelData, void* stripBuff, UInt32 nrBitsPerPixel, Int32& currNrProcesedBytes, UInt32 nrBytesPerRow, UInt32 tw, UInt32 th);
	static void UnpackStrip(UInt16* pixelData, void* stripBuff, UInt32 nrBitsPerPixel, Int32& currNrProcesedBytes, UInt32 nrBytesPerRow, UInt32 tw, UInt32 th) {}
	static void UnpackStrip(Float64* pixelData, void* stripBuff, UInt32 nrBitsPerPixel, Int32& currNrProcesedBytes, UInt32 nrBytesPerRow, UInt32 tw, UInt32 th) {}
	static STGIMPL_CALL void UnpackStrip(UInt8* pixelData, void* stripBuff, UInt32 nrBitsPerPixel, Int32& currNrProcesedBytes, UInt32 nrBytesPerRow, UInt32 tw, UInt32 th);

	template <int N>
	static void UnpackStrip(bit_iterator<N, bit_block_t> pixelData, void* stripBuff, UInt32 nrBitsPerPixel, Int32& currNrProcesedBytes, UInt32 nrBytesPerRow, UInt32 tw, UInt32 th)
	{
		if (nrBitsPerPixel == 8)
		{
			char* byteBuff = reinterpret_cast<char*>(stripBuff);
			for (; th; --th, byteBuff += nrBytesPerRow)
				for (Int32 i = 0; i != tw; ++i)
					*pixelData++ = byteBuff[i];

			currNrProcesedBytes = (N*currNrProcesedBytes + 7) / 8;
		}
	}

	static void PackStrip(void* stripBuf, Int32 currDataSize, UInt32 nrBitsPerPixel) { return UnpackStrip(stripBuf, currDataSize, nrBitsPerPixel); } // all TIFF transformations are their own inverse

	bool OpenForRead (WeakStr name, SafeFileWriterArray* sfwa);
	bool OpenForWrite(WeakStr name, SafeFileWriterArray* sfwa, UInt32 bitsPerSample, UInt32 samplesPerPixel, bool hasPalette, SAMPLEFORMAT sampleFormat);

	bool IsTiledTiff() const;

public:

	STGIMPL_CALL TifImp();     // constructor.
	STGIMPL_CALL ~TifImp();    // destructor.

	STGIMPL_CALL bool Open(WeakStr, TifFileMode, SafeFileWriterArray* sfwa);
	STGIMPL_CALL bool IsOpen();
	STGIMPL_CALL bool Close();

//	========== Support for tiffDirect

	STGIMPL_CALL bool OpenForReadDirect (WeakStr name, SafeFileWriterArray* sfwa);
	STGIMPL_CALL bool OpenForWriteDirect(WeakStr name, SafeFileWriterArray* sfwa);
	STGIMPL_CALL bool ReadNextDir();
//	STGIMPL_CALL void ReportFieldInfo();
	STGIMPL_CALL void WriteStrip(UInt32 row, const void* data, UInt32 size);

	STGIMPL_CALL void SetBaseFlags(UInt32 bitsPerSample);
	STGIMPL_CALL void SetTiled();

	STGIMPL_CALL void SetDataMode(UInt32 bitsPerSample, UInt32 samplesPerPixel, bool hasPalette, SAMPLEFORMAT sampleFormat);
	void SetPalette  (UInt32 bitsPerPixel, SAMPLEFORMAT sampleFormat) { SetDataMode(bitsPerPixel, 1, true, sampleFormat); }
	void SetTrueColor(UInt32 bitsPerPixel, SAMPLEFORMAT sampleFormat) { MG_CHECK(bitsPerPixel == 24 || bitsPerPixel == 32); SetDataMode(8, bitsPerPixel / 8, false, sampleFormat); }

//	========== Support for supported File structure: IndexedColor
	STGIMPL_CALL UInt32 GetHeight() const;
	STGIMPL_CALL UInt32 GetWidth () const;
	STGIMPL_CALL UPoint GetSize  () const;

	STGIMPL_CALL void SetHeight(UInt32);
	STGIMPL_CALL void SetWidth (UInt32);

	STGIMPL_CALL UInt32 GetNrBitsPerPixel() const; 
	STGIMPL_CALL void  SetNrBitsPerPixel(UInt32 nrBitsPerPixel); 

	STGIMPL_CALL void SetColorTableSize(PALETTE_SIZE);      // actual nr in the color table; 0 if true color

	STGIMPL_CALL bool HasColorTable() const;

	STGIMPL_CALL PALETTE_SIZE GetClrImportant() const;
	STGIMPL_CALL void     GetColor(PALETTE_SIZE, UByte& r, UByte& g, UByte& b) const;
	STGIMPL_CALL DmsColor GetColor(PALETTE_SIZE index) const;

	STGIMPL_CALL bool SetColorMap(UInt16* red, UInt16* green, UInt16* blue);

	STGIMPL_CALL bool IsTrueColorImage();
	STGIMPL_CALL bool IsPalettedImage();

	STGIMPL_CALL SizeT ReadTile(void* stripBuff, UInt32 tile_x, UInt32 tile_y, UInt32 strip_y, SizeT tileByteSize) const;
	STGIMPL_CALL SizeT WriteTile(void* stripBuff, UInt32 tile_x, UInt32 tile_y);
};


#endif // _TIF_HPP_, Do not add stuff after this line, please !!!
