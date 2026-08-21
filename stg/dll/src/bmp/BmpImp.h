// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#ifndef __STGIMPL_BMP_IMPL_H
#define __STGIMPL_BMP_IMPL_H

#include "ImplMain.h"

#include "bmp/BmpCompat.h"
#include "vt/color.h"
#include "ptr/OwningPtr.h"

#include <vector>
#include "vt/BaseBounds.h"

//  ---------------------------------------------------------------------------

//  --ENUMERATORS--------------------------------------------------------------
enum BmpFileType { BMP_CORE, BMP_INFO, BMP_V4, BMP_V5 };
enum BmpFileMode { BMP_READ, BMP_WRITE, BMP_EDITPALETTE };
enum BmpFormat   { NO_COMPRESSION, RLE8, RLE4 };
//  ---------------------------------------------------------------------------

//  --CLASSES------------------------------------------------------------------

class BmpImp
{
public:
	typedef UInt32 row_t;
	typedef UInt32 col_t;
	typedef UInt32 filepos_t;
	typedef UInt16 rowsize_t;
	typedef UInt32 size_t;

private:

	std::unique_ptr<BITMAPFILEHEADER>   m_FileHeader;
	std::unique_ptr<BITMAPINFOHEADER>   m_InfoHeader;
	        std::vector<RGBQUAD>   m_RgbQuads;
	mutable std::vector<filepos_t> m_FileRowOffsets;
	mutable std::vector<UByte>     m_CodedBuf;

	HANDLE      m_FH;

	BmpFileType m_BmpFileType : 2;
	BmpFileMode m_BmpFileMode : 2;

	bool   m_IsFileOpen  : 1;
	bool   m_FileExisted : 1;
	mutable Boolean   m_RowSizeWasUsed : 1; // NrOfColors cannot be changed after RowsOffses is used in File Output
	mutable filepos_t m_RowsOffset;
	mutable rowsize_t m_RowSize;
	filepos_t m_ColorsOffset;

	mutable row_t     m_InternRowNumber = 0;
	mutable filepos_t m_InternRowFilePointer = 0;

	filepos_t GetRowsOffset() const;
	rowsize_t GetRowSize() const;

	void      _CalcRowsOffsetSize() const;
	void      _SetFileRowOffsets();


private:
	Boolean GetRle8Row(row_t rowNr, UByte* inpBuf) const;
	[[nodiscard]] Boolean GetCodedRle8Row(UByte*) const;
	[[nodiscard]] Boolean SkipCodedRle8Rows(row_t) const;
	[[nodiscard]] Boolean ReadBuf(UByte*, DWORD, DWORD& read, DWORD& pos) const;

	Boolean SetRle8Row (row_t rowNr, UByte* outBuf);
	Boolean SetCodedRow(row_t rowNr, UByte* outBuf);
	size_t  GetFileSize() const;

	[[noreturn]] void Error(CharPtr action, CharPtr problem);

public:
	STGIMPL_CALL BmpImp();     // constructor.
	STGIMPL_CALL ~BmpImp();    // destructor.

	STGIMPL_CALL Boolean Open(WeakStr, BmpFileMode);
	STGIMPL_CALL Boolean Close();

	STGIMPL_CALL row_t GetHeight() const;
	STGIMPL_CALL col_t GetWidth () const;

	STGIMPL_CALL Boolean GetCodedRow(row_t, UByte*) const;
	STGIMPL_CALL Boolean GetRow(row_t, UByte*) const;

	STGIMPL_CALL DWORD        GetInfoHeaderSize   () const;
	STGIMPL_CALL PALETTE_SIZE GetMaxSizeColorTable() const; // 0 if true color
	STGIMPL_CALL PALETTE_SIZE GetColorTableSize   () const; // actual nr in the color table; 0 if true color
	STGIMPL_CALL PALETTE_SIZE GetClrImportant     () const;

	STGIMPL_CALL void GetColorFromFile(PALETTE_SIZE, UByte&, UByte&, UByte&) const;
	STGIMPL_CALL void GetColor(PALETTE_SIZE, UByte&, UByte&, UByte&) const;
	STGIMPL_CALL DmsColor GetCombinedColor(PALETTE_SIZE index) const;

	STGIMPL_CALL Boolean GetSubFormat(BmpFormat &);

	STGIMPL_CALL Boolean GetInchResolution(Float &, Float &);

	bool IsOpen     () const { return m_IsFileOpen;  }
	bool FileExisted() const { return m_FileExisted; }

	STGIMPL_CALL bool IsTrueColorImage();

	STGIMPL_CALL Boolean SetHeight(row_t);
	STGIMPL_CALL Boolean SetWidth (col_t);

	STGIMPL_CALL Boolean SetRow(row_t, UByte*);

	STGIMPL_CALL Boolean SetTrueColorImage();
	STGIMPL_CALL Boolean SetFalseColorImage(PALETTE_SIZE nr = 256);
	STGIMPL_CALL Boolean SetClrImportant(PALETTE_SIZE nr);

	STGIMPL_CALL Boolean SetColor         (UShort index, UByte r, UByte g, UByte b);
	STGIMPL_CALL Boolean SetColorWithAlpha(UShort index, UByte r, UByte g, UByte b, UByte alpha);

	STGIMPL_CALL void SetCombinedColor(PALETTE_SIZE index, DmsColor combined);

	STGIMPL_CALL Boolean SetSubFormat(BmpFormat);

	STGIMPL_CALL Boolean SetInchResolution(Float, Float);
};


#endif // __STGIMPL_BMP_IMPL_H
