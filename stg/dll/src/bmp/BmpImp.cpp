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

#include "StoragePch.h"
#include "ImplMain.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "BmpImp.h"

#include "utl/Environment.h"
#include "ptr/SharedStr.h"
#include "ptr/OwningPtrSizedArray.h"
#include "set/VectorFunc.h"

#include <windows.h>
#include <io.h>
//  ---------------------------------------------------------------------------

//  --DEFINES------------------------------------------------------------------
#define BMP_ID          0x4d42          // the chars 'BM'

// bitmaps rows are DWORD aligned (=32 bis = 4 byte units)
#define WIDTHBYTES(nrBits)   (((nrBits + 31) / 32) * 4)

#define FILE_BEGIN        0

#define MAX_COLORS      256

//
#define RLE_ESCAPE  0
#define RLE_EOL     0
#define RLE_EOF     1
#define RLE_JMP     2

inline bool WriteFile(
  HANDLE hFile,                    // handle to file
  LPCVOID lpBuffer,                // data buffer
  DWORD nNumberOfBytesToWrite      // number of bytes to write
)                                  // RETURNS: number of bytes written
{
	DWORD nrOfBytesWritten;
	return WriteFile(hFile, lpBuffer, nNumberOfBytesToWrite, &nrOfBytesWritten, NULL);
}

inline bool ReadFile(
  HANDLE hFile,                // handle to file
  LPVOID lpBuffer,             // data buffer
  DWORD nNumberOfBytesToRead   // number of bytes to read
)
{
	DWORD nrOfBytesRead;
	return ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, &nrOfBytesRead, NULL);
}

inline Boolean BmpImp::IsTrueColorImage()
{
	dms_assert(m_InfoHeader);
	return m_InfoHeader->biClrUsed == 0;
}


inline void BmpImp::_CalcRowsOffsetSize() const
{
	dms_assert(m_IsFileOpen);
	if (m_RowSizeWasUsed) return;

	m_RowSizeWasUsed = TRUE;
	m_RowsOffset = m_ColorsOffset +
                GetColorTableSize() * (m_BmpFileType == BMP_CORE ? sizeof(RGBTRIPLE) : sizeof(RGBQUAD));
	m_RowSize = WIDTHBYTES(m_InfoHeader->biWidth * m_InfoHeader->biBitCount);
}

inline BmpImp::filepos_t BmpImp::GetRowsOffset() const
{
	_CalcRowsOffsetSize();
	return m_RowsOffset;
}

inline BmpImp::rowsize_t BmpImp::GetRowSize() const
{
	_CalcRowsOffsetSize();
	return m_RowSize;
}

//  ---------------------------------------------------------------------------
//  Does    : Constructor
//  ---------------------------------------------------------------------------

BmpImp::BmpImp()
	:	m_FileHeader( new BITMAPFILEHEADER)
	,	m_InfoHeader( new BITMAPINFOHEADER)
{
	m_FH             = INVALID_HANDLE_VALUE;
	m_BmpFileType    = BMP_INFO;
	m_RowSize        = 0;
	m_IsFileOpen     = FALSE;
	m_ColorsOffset   = 0;
	m_RowsOffset     = 0;
	m_RowSizeWasUsed = FALSE;

	// Set defaults...
	m_FileHeader->bfType = BMP_ID;
	m_FileHeader->bfSize = 0;
	m_FileHeader->bfReserved1 = 0;
	m_FileHeader->bfReserved2 = 0;
	m_FileHeader->bfOffBits = 0;

	m_InfoHeader->biSize = sizeof(BITMAPINFOHEADER);
	m_InfoHeader->biWidth = 0;
	m_InfoHeader->biHeight = 0;
	m_InfoHeader->biPlanes = 1;
	m_InfoHeader->biBitCount = 0;
	m_InfoHeader->biCompression = BI_RGB;
	m_InfoHeader->biSizeImage = 0;
	m_InfoHeader->biXPelsPerMeter = 0;
	m_InfoHeader->biYPelsPerMeter = 0;
	m_InfoHeader->biClrUsed = 0;
	m_InfoHeader->biClrImportant = 0;
}

//  ---------------------------------------------------------------------------
//  Does    : Destructor
//  ---------------------------------------------------------------------------

BmpImp::~BmpImp()
{
	Close();
}

//  ---------------------------------------------------------------------------
//
//  Name    : BmpImp::Open
//
//  Does    : Opens a bmp file to read or save.
//
//  Args    : Char          *filename   : (I), File to open or save
//            BmpFileType   mode        : (I), Read or save flag
//
//  Returns : Boolean (TRUE on success, FALSE on failure).
//
bool BmpImp::Open(WeakStr filename, BmpFileMode mode)
{
	DWORD size;

	// close bmp if open
	Close();

	m_FileExisted = IsFileOrDirAccessible(filename);
	if ((mode == BMP_READ) && !m_FileExisted)
		return FALSE;

	if (mode != BMP_READ)
		GetWritePermission(filename);

	m_FH = CreateFile(ConvertDmsFileName(filename).c_str(), 
				(mode == BMP_READ) ? GENERIC_READ : GENERIC_WRITE|GENERIC_READ, 
				(mode == BMP_READ) ? FILE_SHARE_READ : 0, // share mode
				NULL,            // SD
				(m_FileExisted ? OPEN_ALWAYS : CREATE_ALWAYS), // how to create
				FILE_ATTRIBUTE_NORMAL,
				NULL);
	if (m_FH == INVALID_HANDLE_VALUE)
		return FALSE;

	m_BmpFileMode = mode;

	if (m_FileExisted)
	{
		// Read file header
		if	(		!	ReadFile(m_FH, m_FileHeader, sizeof(BITMAPFILEHEADER))
			// Check for BMP-format
				||	m_FileHeader->bfType != BMP_ID
			// Read info header size
				||	!	ReadFile(m_FH, &size, sizeof(DWORD))
			)
		{
			Error("Open", "Cannot read header");
		}

		// Set pointer back before size
		SetFilePointer(m_FH, sizeof(BITMAPFILEHEADER), NULL, FILE_BEGIN);

		switch (size)
		{
			case sizeof(BITMAPINFOHEADER):
			case sizeof(BITMAPV4HEADER):
//			case sizeof(BITMAPV5HEADER):

				m_BmpFileType = BMP_INFO;

				// Read info header
				if (! ReadFile(m_FH, m_InfoHeader, sizeof(BITMAPINFOHEADER)))
					Error("Open", "Cannot read BITMAPINFOHEADER");

				// Number of colors NOT yet given
				if (0 == m_InfoHeader->biClrUsed)
				{
					m_InfoHeader->biClrImportant = m_InfoHeader->biClrUsed = GetMaxSizeColorTable();
				}

				break;

			case sizeof(BITMAPCOREHEADER):

				BITMAPCOREHEADER   core_header;

				m_BmpFileType = BMP_CORE;

				// Read core header
				if (! ReadFile(m_FH, &core_header, sizeof(BITMAPCOREHEADER)))
				{
					Error("Open", "Cannot read BITMAPCOREHEADER");
				}

				// Convert core header to info header
				m_InfoHeader->biSize       = sizeof(BITMAPINFOHEADER);
				m_InfoHeader->biWidth      = core_header.bcWidth;
				m_InfoHeader->biHeight     = core_header.bcHeight;
				m_InfoHeader->biPlanes     = core_header.bcPlanes;
				m_InfoHeader->biBitCount   = core_header.bcBitCount;

				m_InfoHeader->biCompression = BI_RGB;
				m_InfoHeader->biSizeImage =
					WIDTHBYTES( DWORD(core_header.bcWidth) * core_header.bcBitCount )
					*	DWORD(core_header.bcHeight);

				m_InfoHeader->biXPelsPerMeter  = 0;
				m_InfoHeader->biYPelsPerMeter  = 0;
				m_InfoHeader->biClrImportant = m_InfoHeader->biClrUsed = GetMaxSizeColorTable();

				break;
			default:
				Error("Open", "Unknown header size");
		}

		// Check compression type
		if (m_InfoHeader->biCompression != BI_RGB &&
		    m_InfoHeader->biCompression != BI_RLE8)
		{
			Error("Open", "Unsupported compression type");
		}
	}
	else // !fileExists
	{
		// We will save only in the BMP_INFO format
		m_BmpFileType = BMP_INFO;

		// set m_FileHeader to defaults
		m_FileHeader->bfType = BMP_ID;
		m_FileHeader->bfSize = 0;
		m_FileHeader->bfReserved1 = 0;
		m_FileHeader->bfReserved2 = 0;
		m_FileHeader->bfOffBits = 0;
	}

	// Create filerrowpointers list
	_SetFileRowOffsets();

	m_ColorsOffset = sizeof(BITMAPFILEHEADER) + GetInfoHeaderSize();

	dms_assert(m_BmpFileType != BMP_CORE); // READING RGBTRIPLES IN CACHE NOT IMPLEMENTED;
	m_RgbQuads.resize(m_InfoHeader->biClrUsed);

	m_InternRowNumber = 0;
	m_InternRowFilePointer = 0;

	m_IsFileOpen = TRUE;

	// Read colors in buffer
	if (m_FileExisted)
	{
		if (m_BmpFileType == BMP_INFO)
		{
			UInt32 colorTableByteSize = m_InfoHeader->biClrUsed * sizeof(RGBQUAD);

			SetFilePointer(m_FH, m_ColorsOffset, NULL, FILE_BEGIN);
			ReadFile(m_FH, &*m_RgbQuads.begin(), colorTableByteSize); // just read as much as available
		}
		else
		{
			for (
				std::vector<RGBQUAD>::iterator 
					p = m_RgbQuads.begin(),
					e = m_RgbQuads.end(); 
				p != e; 
				++p
			)
				GetColorFromFile(
					p - m_RgbQuads.begin(), 
					p->rgbRed, 
					p->rgbGreen, 
					p->rgbBlue
				);
		}
	}

	return TRUE;
}

//  ---------------------------------------------------------------------------
//
//  Name    : BmpImp::Close
//
//  Does    : Close file.
//
//  Returns : Boolean (TRUE on success, FALSE on failure).
//
Boolean BmpImp::Close()
{
	if (!m_IsFileOpen) return TRUE;

	if (m_BmpFileMode == BMP_WRITE)
	{
		UInt32 dataSize = GetFileSize();
		m_FileHeader->bfSize = dataSize;
		m_FileHeader->bfOffBits = GetRowsOffset();
		m_InfoHeader->biSizeImage = dataSize - m_FileHeader->bfOffBits;

		SetFilePointer(m_FH, 0, NULL, FILE_BEGIN);
		if (! WriteFile(m_FH, m_FileHeader, sizeof(BITMAPFILEHEADER)))
			Error("Close", "Cannot write BitmapFileHeader");
		if (! WriteFile(m_FH, m_InfoHeader, sizeof(BITMAPINFOHEADER)))
			Error("Close", "Cannot write BitmapInfoHeader");
	}

	if (m_BmpFileMode != BMP_READ)
	{
		// change palettecolors
		PALETTE_SIZE nrColors = GetColorTableSize();

		dms_assert(nrColors == m_RgbQuads.size());

		SetFilePointer(m_FH, m_ColorsOffset, NULL, FILE_BEGIN);
		if (! WriteFile(m_FH, reinterpret_cast<CharPtr>(&*m_RgbQuads.begin()), sizeof(RGBQUAD)*nrColors))
			Error("Close", "Cannot write RgbQuads");
	}

	vector_clear(m_FileRowOffsets);
	vector_clear(m_CodedBuf);

	m_IsFileOpen = FALSE;
	m_RowSizeWasUsed = FALSE;

	bool res = CloseHandle(m_FH);
	m_FH = INVALID_HANDLE_VALUE;
	return res;
}

UInt32 BmpImp::GetFileSize() const
{
	_CalcRowsOffsetSize();
	
	if (m_InfoHeader->biCompression == BI_RGB)
		return GetRowsOffset() + GetRowSize() * GetHeight();
	UInt32 res = m_FileRowOffsets[GetHeight()];
	dms_assert(res);
	return res;
}

void BmpImp::_SetFileRowOffsets()
{
	// Create filerrowpointers list
	if (m_InfoHeader->biCompression == BI_RGB)
		vector_clear(m_FileRowOffsets);
	else
		vector_zero_n(m_FileRowOffsets, SizeT(m_InfoHeader->biHeight) + 1);
}

Boolean BmpImp::GetSubFormat(BmpFormat & f)
{
    switch(m_InfoHeader->biCompression)
    {
        case BI_RGB:
            f = NO_COMPRESSION;
            break;
        case BI_RLE8:
            f = RLE8;
            break;
        case BI_RLE4:
            f = RLE4;
            break;
    }

    return TRUE;
}

Boolean BmpImp::SetSubFormat(BmpFormat f)
{
    if (m_IsFileOpen) return FALSE;

    switch(f)
    {
        case NO_COMPRESSION:
            m_InfoHeader->biCompression = BI_RGB;
            break;
        case RLE8:
            m_InfoHeader->biCompression = BI_RLE8;
            break;
        case RLE4:
            m_InfoHeader->biCompression = BI_RLE8;
            break;
    }

    return TRUE;
}

BmpImp::col_t 
BmpImp::GetWidth() const
{
    return m_InfoHeader->biWidth;
}

BmpImp::row_t 
BmpImp::GetHeight() const
{
    return m_InfoHeader->biHeight;
}

//  ---------------------------------------------------------------------------
//
//  Name    : BMP::GetCodedRow
//
//  Does    : Reads a row coded.
//
//  Args    : Long rowNumber : (I), number of row to be read.
//            UByte *buf     : (O), pointer to buffer that will be filled.
//
//  Returns : Boolean (TRUE on success, FALSE on failure).
//
Boolean BmpImp::GetCodedRow(row_t rowNumber, UByte *buf) const
{
	if ( rowNumber >= row_t(m_InfoHeader->biHeight) )
		return FALSE;

	if (m_InfoHeader->biCompression == BI_RLE8)
        return GetRle8Row(rowNumber, buf);

    // set filepointer to proper position
	LONG rowsOffset = GetRowsOffset();
	LONG rowSize    = GetRowSize();
	Long filepointer = rowsOffset + rowSize * rowNumber;
	SetFilePointer(m_FH, filepointer, NULL, FILE_BEGIN);

	return ReadFile(m_FH, buf, rowSize);
}

//  ---------------------------------------------------------------------------
//
//  Name    : BmpImp::GetRle8Row
//
//  Does    : Reads a row coded in rle8.
//
//  Args    : Long rowNumber : (I), number of row to be read.
//            UByte *buf     : (O), pointer to buffer that will be filled.
//
//  Returns : Boolean (TRUE on success, FALSE on failure).
//
Boolean BmpImp::GetRle8Row(row_t rowNumber, UByte *buf) const
{
	dms_assert(m_BmpFileMode == BMP_READ); // pFileRowOffsets was initialized before SetFalseColor was called
	dms_assert(rowNumber < m_FileRowOffsets.size());
    // See if this row have been read already...
	m_InternRowNumber = rowNumber;
	if (m_InternRowNumber == 0)
		m_FileRowOffsets[m_InternRowNumber] = GetRowsOffset();

	while (!m_FileRowOffsets[m_InternRowNumber])
		m_InternRowNumber--;

	m_InternRowFilePointer = m_FileRowOffsets[m_InternRowNumber];
	
	if (rowNumber > m_InternRowNumber)
	{
		if (!SkipCodedRle8Rows(rowNumber - m_InternRowNumber))
		return FALSE;
	}

	//while (rowNumber >= m_InternRowNumber)
	//   if (!GetCodedRle8Row(buf))
	//        return FALSE;

	if (!GetCodedRle8Row(buf))
		return FALSE;

	return TRUE;
}

//  ---------------------------------------------------------------------------
//
//  Name    : BmpImp::SkipCodedRle8Row
//
//  Does    : Reads a row coded in rle8.
//
//  Args    : Long rowNumber : (I), number of row to be read.
//            UByte *buf     : (O), pointer to buffer that will be filled.
//
//  Returns : Boolean (TRUE on success, FALSE on failure).
//
[[nodiscard]] Boolean BmpImp::SkipCodedRle8Rows(row_t rows) const
{
	if (! rows)
		return TRUE;

    // set filepointer to proper position
	if (m_InternRowFilePointer==0)
		m_InternRowFilePointer = GetRowsOffset();

	SetFilePointer(m_FH, m_InternRowFilePointer, NULL, FILE_BEGIN);

	DWORD index = 0, bytesRead = 0,
	    cur_read, cur_pos, cnt;

	size_t buf_width = m_InfoHeader->biWidth * rows;
	MakeMin(buf_width, size_t(5000));
	MakeMax(buf_width, size_t(25));

	OwningPtrSizedArray<UByte> c( buf_width, dont_initialize MG_DEBUG_ALLOCATOR_SRC_EMPTY);

	cur_read = 0;
	cur_pos = -1;
	if (!ReadBuf(c.begin(), buf_width, cur_read, cur_pos))
		return false;
        
	for(;;)
	{
		cur_pos++;
		bytesRead++;

		// Read next buffer
		if 
			(
				(cur_read == buf_width) &&
				((cur_read - cur_pos) < 5)
			)
		{
			if (!ReadBuf(c.begin(), buf_width, cur_read, cur_pos))
				return false;
		}

		if (c[cur_pos] == RLE_ESCAPE)
		{
			cur_pos++;
			bytesRead++;

			switch (c[cur_pos])
			{
				case RLE_EOL:
				case RLE_EOF:
					m_InternRowFilePointer += bytesRead;
					m_InternRowNumber++;
					m_FileRowOffsets[m_InternRowNumber] = m_InternRowFilePointer;
					if (!--rows)
						return TRUE;
					index = 0;
					bytesRead = 0;
					break;

				case RLE_JMP: // skip this...
					cur_pos += 2;
					bytesRead += 2;
					break;

				default:
					cnt = c[cur_pos];
					// Read next buffer
					if 	(
							(cur_read == buf_width) &&
							((cur_read - cur_pos) <= cnt+1)
						)
					{
						cur_pos++;
						if (!ReadBuf(c.begin(), buf_width, cur_read, cur_pos))
							return false;
						cur_pos--;
					}

					dms_assert(m_InfoHeader->biWidth >= 0);
					if (index + cnt > UInt32(m_InfoHeader->biWidth) )
						return FALSE;

					cur_pos   += cnt;
					bytesRead += cnt;
					index     += cnt;

					if (cnt & 1)
					{
						cur_pos++;
						bytesRead++;
					}
					break;
			}
		}
		else
		{
			cnt = c[cur_pos];

			cur_pos++;
			bytesRead++;
			dms_assert(m_InfoHeader->biWidth >= 0);
			if (cnt+index > UInt32(m_InfoHeader->biWidth) )
				return FALSE;

			index += cnt;
		}
	}
}

//  ---------------------------------------------------------------------------
//
//  Name    : BmpImp::GetCodedRle8Row
//
//  Does    : Reads a row coded in rle8.
//
//  Args    : Long rowNumber : (I), number of row to be read.
//            UByte *buf     : (O), pointer to buffer that will be filled.
//
//  Returns : Boolean (TRUE on success, FALSE on failure).
//
[[nodiscard]] Boolean BmpImp::GetCodedRle8Row(UByte *buf) const
{
    // set filepointer to proper position
	dms_assert(m_InternRowFilePointer != 0); // must be set before.

	SetFilePointer(m_FH, m_InternRowFilePointer, NULL, FILE_BEGIN);

	DWORD index = 0, bytesRead = 0, cur_read, cur_pos, copy_pos, cnt, cnt2;

	Short buf_width = m_InfoHeader->biWidth;
	if (buf_width < 25)
	{
		buf_width = 25;
	}

	OwningPtrSizedArray<UByte> c( buf_width, dont_initialize MG_DEBUG_ALLOCATOR_SRC("BmpImp"));

	cur_read = 0;
	cur_pos = -1;
	if (!ReadBuf(c.begin(), buf_width, cur_read, cur_pos))
		return false;
        
	for(;;)
	{
		cur_pos++;
		bytesRead++;

		// Read next buffer
		if 
		(
			(cur_read == buf_width) &&
			((cur_read - cur_pos) < 5)
		)
		{
			if (!ReadBuf(c.begin(), buf_width, cur_read, cur_pos))
				return false;
		}

		if (c[cur_pos] == RLE_ESCAPE)
		{
			cur_pos++;
            bytesRead++;

            switch (c[cur_pos])
            {
                case RLE_EOL:
                case RLE_EOF:
                    m_InternRowFilePointer += bytesRead;
                    m_InternRowNumber++;
                    m_FileRowOffsets[m_InternRowNumber] = m_InternRowFilePointer;
					return TRUE;

                case RLE_JMP: // skip this...
										cur_pos += 2;
										bytesRead += 2;
                    break;

                default:

                    cnt = c[cur_pos];

					// Read next buffer
					if 
					(
						(cur_read == buf_width) &&
						((cur_read - cur_pos) <= cnt+1)
					)
					{
						cur_pos++;
						if (!ReadBuf(c.begin(), buf_width, cur_read, cur_pos))
							return false;
						cur_pos--;
					}

					dms_assert(m_InfoHeader->biWidth >= 0);
                    if (index + cnt > UInt32(m_InfoHeader->biWidth))
						return FALSE;

					copy_pos = cur_pos + 1;
					for (cnt2 = 0; cnt2 < cnt; ++copy_pos, ++cnt2)
						buf[index+cnt2] = c[copy_pos];

					cur_pos += cnt;
                    bytesRead += cnt;
                    index += cnt;

                    if (cnt & 1)
                    {
						cur_pos++;
						bytesRead++;
                    }
					break;
            }
		}
		else
		{
            cnt = c[cur_pos];

			cur_pos++;
            bytesRead++;

			dms_assert(m_InfoHeader->biWidth >= 0);
            if (cnt+index > UInt32(m_InfoHeader->biWidth))
				return FALSE;

			for (cnt2 = 0; cnt2 < cnt; cnt2++)
				buf[index+cnt2] = c[cur_pos];

            index += cnt;
		}
	}
}

//  ---------------------------------------------------------------------------
//
//  Name    : BmpImp::ReadBuf
//
//  Does    : Reads a buffer.
//
//  Returns : Boolean (TRUE on success, FALSE on failure).
//
[[nodiscard]] Boolean BmpImp::ReadBuf(UByte *buf, DWORD width, DWORD &read, DWORD &pos) const
{
	if (0 == read)
	{
		if (!ReadFile(m_FH, buf, width, &read, NULL))
			return false;
	}
	else
	{
		// Copy buffer part still valid
		if (read > pos)
		{
			DWORD total = read - pos;

			for (DWORD cnt = 0; cnt < total; cnt++)
			{
				buf[cnt] = buf[cnt + pos];
			}

			if (!ReadFile(m_FH, buf + total, width - total, &read, NULL))
				return false;
			read += total;
		}
		else
		{
			if (!ReadFile(m_FH, buf, width, &read, NULL))
				return false;
		}
	}

	pos = 0;

	return TRUE;
}

//  ---------------------------------------------------------------------------
//
//  Name    : BmpImp::GetRow
//
//  Does    : Reads a row color decoded.
//
//  Args    : Long rowNumber : (I), number of row to be read.
//            UByte *in_buf  : (O), pointer to buffer that will be filled.
//
//  Returns : Boolean (TRUE on success, FALSE on failure).
//
Boolean BmpImp::GetRow(row_t rowNumber, UByte *in_buf) const
{
	dms_assert(rowNumber < GetHeight());

	// Convert the row
	switch(m_InfoHeader->biBitCount)
	{
		case 1:
		{
			vector_resize(m_CodedBuf, GetRowSize() );
			std::vector<UByte>::iterator buf = m_CodedBuf.begin();
			if (! GetCodedRow(rowNumber, &*buf))
				return FALSE;

			for (UInt32 cnt = 0, width = m_InfoHeader->biWidth; cnt < width; cnt++)
			{
				UInt32 bit_cnt = cnt >> 3;
				UInt32 shift_cnt = 7 - (cnt && 7);
				in_buf[cnt] = (buf[bit_cnt] >> shift_cnt) & 0x1;
			}
			break;
		}
		case 4:
		{
			vector_resize(m_CodedBuf, GetRowSize() );
			std::vector<UByte>::iterator buf = m_CodedBuf.begin();
			if (! GetCodedRow(rowNumber, &*buf))
				return FALSE;

			for (UInt32 cnt = 0, width = m_InfoHeader->biWidth; cnt < width; cnt++)
			{
				UInt32 bit_cnt = cnt >> 1;
				if (0 == (cnt & 0x01))
					in_buf[cnt] = (buf[bit_cnt] >> 4) & 0x0F;
				else
					in_buf[cnt] = buf[bit_cnt] & 0x0F;
			}
			break;
		}
		case 8:
			if (! GetCodedRow(rowNumber, in_buf)) return FALSE;
			break;

		case 24:
			if (! GetCodedRow(rowNumber, in_buf)) return FALSE;

			// A true color row, make it 3 bytes per pixel R, G & B instead
            // B, G and R
			UInt32 cnt = m_InfoHeader->biWidth;
			while (cnt--)
			{
				std::swap(in_buf[0], in_buf[2]);
				 in_buf += 3;
			}
			break;

	}
	return TRUE;
}

//  ---------------------------------------------------------------------------
//
//  Name    : BmpImp::GetColorTableSize
//
//  Does    : Retrieves the nr of used colors in the bmp file.

DWORD        BmpImp::GetInfoHeaderSize   () const
{
	return m_InfoHeader->biSize; 
}

PALETTE_SIZE BmpImp::GetMaxSizeColorTable() const
{
	WORD bitCount = m_InfoHeader->biBitCount;
	return (bitCount == 0 || bitCount > 16)
		? 0
		: (1 << bitCount);
}

PALETTE_SIZE BmpImp::GetColorTableSize() const    // actual nr in the color table; 0 if true color
{
	return m_InfoHeader->biClrUsed;
}


//  ---------------------------------------------------------------------------
//
//  Name    : BMP::GetColorFromFile
//
//  Does    : Read a color.
//
//  Returns : Boolean (TRUE on success, FALSE on failure).
//

void BmpImp::GetColorFromFile(PALETTE_SIZE index, UByte &r, UByte &g, UByte &b) const
{
	dms_assert(m_IsFileOpen);
	dms_assert(index < GetColorTableSize());

	Long filepointer = m_ColorsOffset;

	switch(m_BmpFileType)
	{
		case BMP_INFO:
         // set filepointer to proper position
         filepointer += (index * sizeof(RGBQUAD));
         SetFilePointer(m_FH, filepointer, NULL, FILE_BEGIN);

         RGBQUAD      rgb_quad;
         if (!ReadFile(m_FH, &rgb_quad, sizeof(RGBQUAD)))
             break;

         r = rgb_quad.rgbRed;
         g = rgb_quad.rgbGreen;
         b = rgb_quad.rgbBlue;

			return;
		case BMP_CORE:
         // set filepointer to proper position
         filepointer += (index * sizeof(RGBTRIPLE));
         SetFilePointer(m_FH, filepointer, NULL, FILE_BEGIN);

         RGBTRIPLE    rgb_triple;
         if (!ReadFile(m_FH, &rgb_triple, sizeof(RGBTRIPLE)))
             break;

         r = rgb_triple.rgbtRed;
         g = rgb_triple.rgbtGreen;
         b = rgb_triple.rgbtBlue;

			return;
	}
	r = g = b = 0;
}


//  ---------------------------------------------------------------------------
//
//  Name    : BMP::GetColor
//
//  Does    : Read a color from cache
//
//  Args    : UShort index              (I), Index of color
//            UByte &r                  (O), Red value
//            UByte &g                  (O), Green value
//            UByte &b                  (O), Blue value
//
//  Returns : Boolean (TRUE on success, FALSE on failure).
//
//  Note: BBQ, added 24-10-2000
//
void BmpImp::GetColor(PALETTE_SIZE index, UByte &r, UByte &g, UByte &b) const
{
    Long filepointer = m_ColorsOffset;

	// RGW, Check if index inside the biClrUsed range, 
	// otherwise just return BLACK
	// But if biClrUsed == 0, we ignore it
	if 
	(
		(m_InfoHeader->biClrUsed > 0) && (index >= m_InfoHeader->biClrUsed)
		||
		(index >= MAX_COLORS) 
	)
	{
		r = 0;
		g = 0;
		b = 0;
	}

	r = m_RgbQuads[index].rgbRed;
	g = m_RgbQuads[index].rgbGreen;
	b = m_RgbQuads[index].rgbBlue;
}




//  ---------------------------------------------------------------------------
//
//  Name    : BmpImp::SetHeight
//
//  Does    : Set the height of the bmp.
//
//  Args    : UShort height  (I), the height to be set.
//
//  Returns : Boolean (TRUE on success, FALSE on failure).
//
Boolean BmpImp :: SetHeight(row_t height)
{
	dms_assert(!m_IsFileOpen || m_BmpFileMode == BMP_WRITE);
	m_InfoHeader->biHeight = height;

	return TRUE;
}

//  ---------------------------------------------------------------------------
//
//  Name    : BmpImp::SetWidth
//
//  Does    : Set the width of the bmp.
//
//  Args    : UShort width  (I), the width to be set.
//
//  Returns : Boolean (TRUE on success, FALSE on failure).
//
Boolean BmpImp :: SetWidth(col_t width)
{
	dms_assert(!m_IsFileOpen 
		|| (m_InfoHeader->biWidth == width) 
		|| (m_BmpFileMode == BMP_WRITE && !m_RowSizeWasUsed));
	m_InfoHeader->biWidth = width;

	return TRUE;
}

//  ---------------------------------------------------------------------------
//
//  Name    : BmpImp::SetRle8Row
//
//  Does    : writes a row coded in rle8.
//
//  Args    : Long rowNumber : (I), number of row to be read.
//            UByte *buf     : (O), pointer to buffer that will be written.
//
//  Returns : Boolean (TRUE on success, FALSE on failure).
//
Boolean BmpImp::SetRle8Row(row_t rowNumber, UByte *buf)
{
    if (m_InternRowNumber != rowNumber)
        return FALSE;

	if (m_InternRowNumber == 0)
		m_FileRowOffsets[m_InternRowNumber] = GetRowsOffset();

    m_InternRowFilePointer = m_FileRowOffsets[m_InternRowNumber];

    // set filepointer to proper position
    SetFilePointer(m_FH, m_InternRowFilePointer, NULL, FILE_BEGIN);

    UByte absoluteMode[256];
    UByte absoluteCount = 0;
    UByte doubleCount = 0;
    UByte esc[2];
    Long i;
    UByte currentByte;
    Long bytesWritten = 0;

    // Walk through the whole row...
    for (i = 0; i < m_InfoHeader->biWidth; i++)
    {
        currentByte = buf[i];
        doubleCount = 1;

        while (i + 1 < m_InfoHeader->biWidth && buf[i + 1] == currentByte &&
               doubleCount < 255)
        {
            i++;
            doubleCount++;
        }

        // Flush current absolute mode array...
        if (absoluteCount == 255 || (doubleCount != 1 && absoluteCount != 0))
        {
            // There should be more than 2 absoluteCounts
            if (absoluteCount <= RLE_JMP)
            {
                for (UByte j = 0; j < absoluteCount; j++)
                {
                    // Write as doubles...
                    esc[0] = 1;
                    esc[1] = absoluteMode[j];

                    if (! WriteFile(m_FH, (CharPtr)esc, 2))
                        return FALSE;

                    bytesWritten += 2;
                }
            }
            else
            {
                esc[0] = RLE_ESCAPE;
                esc[1] = absoluteCount;

                if (! WriteFile(m_FH, (CharPtr)esc, 2))
                    return FALSE;

                if (! WriteFile(m_FH, (CharPtr)absoluteMode, absoluteCount))
                    return FALSE;

                bytesWritten += 2;
                bytesWritten += absoluteCount;

                if (absoluteCount % 2)
                {
                    if (! WriteFile(m_FH, (CharPtr)esc, 1))
                        return FALSE;

                    bytesWritten++;
                }
            }

            absoluteCount = 0;
        }

        // No doubles found...
        if (doubleCount == 1)
        {
            // Store in absolute mode array...
            absoluteMode[absoluteCount++] = currentByte;
        }
        else
        {
            // Write doubles...
            esc[0] = doubleCount;

            if (! WriteFile(m_FH, (CharPtr)esc, 1))
                return FALSE;

            if (! WriteFile(m_FH, (CharPtr)&currentByte, 1))
                return FALSE;

            bytesWritten += 2;
        }
    }

    // Flush current absolute mode array...
    if (absoluteCount != 0)
    {
        // There should be more than 2 absoluteCounts
        if (absoluteCount <= RLE_JMP)
        {
            for (UByte j = 0; j < absoluteCount; j++)
            {
                // Write as doubles...
                esc[0] = 1;
                esc[1] = absoluteMode[j];

                if (! WriteFile(m_FH, (CharPtr)esc, 2))
                    return FALSE;

                bytesWritten += 2;
            }
        }
        else
        {
            esc[0] = RLE_ESCAPE;
            esc[1] = absoluteCount;

            if (! WriteFile(m_FH, (CharPtr)esc, 2))
                return FALSE;

            if (! WriteFile(m_FH, (CharPtr)absoluteMode, absoluteCount))
                return FALSE;

            bytesWritten += 2;
            bytesWritten += absoluteCount;

            if (absoluteCount % 2)
            {
                if (! WriteFile(m_FH, (CharPtr)esc, 1))
                    return FALSE;

                bytesWritten++;
            }
        }
    }

    // Write end of line or end of file...
    esc[1] = (m_InternRowNumber == m_InfoHeader->biHeight - 1) ?
             RLE_EOF : RLE_EOL;

    esc[0] = RLE_ESCAPE;

    if (! WriteFile(m_FH, (CharPtr)esc, 2))
        return FALSE;

    bytesWritten += 2;

    //
    // Set new filepointer...
    //
    m_InternRowFilePointer += bytesWritten;
    m_InternRowNumber++;
    m_FileRowOffsets[m_InternRowNumber] = m_InternRowFilePointer;

    return TRUE;
}

//  ---------------------------------------------------------------------------
//
//  Name    : SetCodedRow
//
//  Does    : Set a coded row of the bmp file
//
//  Args    : Long      rowNumber   : (I), number of strip
//            UByte*    buf         : (I), contains the values to be set
//
//  Returns : Boolean (TRUE on success, FALSE on failure).
//
Boolean BmpImp::SetCodedRow(row_t rowNumber, UByte* buf)
{
	MGD_PRECONDITION(m_IsFileOpen);
	dms_assert(m_BmpFileMode == BMP_WRITE);

	if (rowNumber >= UInt32(m_InfoHeader->biHeight))
		return FALSE;

	if (m_InfoHeader->biCompression == BI_RLE8)
		return SetRle8Row(rowNumber, (UByte*)buf);

    // set filepointer to proper position
	rowsize_t rowSize     = GetRowSize();
	filepos_t filepointer = GetRowsOffset() + rowSize * rowNumber;
	SetFilePointer(m_FH, filepointer, NULL, FILE_BEGIN);

	return WriteFile(m_FH, buf, rowSize);
}

//  ---------------------------------------------------------------------------
//
//  Name    : BmpImp::SetRow
//
//  Does    : Set a row of the bmp file
//
//  Args    : Long      rowNumber   : (I), number of strip
//            UByte*    buf         : (I), contains the values to be set
//
//  Returns : Boolean (TRUE on success, FALSE on failure).
//
Boolean BmpImp::SetRow(row_t rowNumber, UByte* buf)
{
	dms_assert(rowNumber < GetHeight());

	// Convert the row
	dms_assert(m_InfoHeader->biWidth >= 0);
	switch(m_InfoHeader->biBitCount)
	{
		case 1:
		{
			vector_zero_n(m_CodedBuf, GetRowSize());
			std::vector<UByte>::iterator out_buf = m_CodedBuf.begin();
			for (SizeT cnt = 0; cnt != m_InfoHeader->biWidth; ++cnt)
			{
				auto bit_cnt = cnt / 8;
				UInt32 shift_cnt = 7 - (cnt & 0x07);
				if (buf[cnt] & 0x01)
					out_buf[bit_cnt] |= (1 << shift_cnt);
			}
			return SetCodedRow(rowNumber, &*out_buf);
		}
		case 4:
		{
			vector_zero_n(m_CodedBuf, GetRowSize());
			std::vector<UByte>::iterator out_buf = m_CodedBuf.begin();
			for (SizeT cnt = 0; cnt != m_InfoHeader->biWidth; ++cnt)
			{
				auto bit_cnt = cnt / 2;
//				UInt32 shift_cnt = (4 - 4 * (cnt & 0x01));
				if (cnt & 0x01)
					out_buf[bit_cnt] |= (buf[cnt] & 0x0F);
				else
					out_buf[bit_cnt] |= ((buf[cnt] & 0x0F) << 4);
			}
			return SetCodedRow(rowNumber, &*out_buf);
		}
		case 8:
			return SetCodedRow(rowNumber, buf);

		case 24:
			vector_resize(m_CodedBuf, GetRowSize(), 0);
			std::vector<UByte>::iterator out_buf = m_CodedBuf.begin();
			// A true color row, make it 3 bytes per pixel B, G & R
			// instead of R, G & B
			for (SizeT cnt = 0; cnt != m_InfoHeader->biWidth; ++cnt)
			{
				auto bit_cnt = cnt * 3;
				out_buf[bit_cnt]     = buf[bit_cnt + 2];
				out_buf[bit_cnt + 1] = buf[bit_cnt + 1];
				out_buf[bit_cnt + 2] = buf[bit_cnt];
			}
			return SetCodedRow(rowNumber, &*out_buf);
	}
	return false;
}

//  ---------------------------------------------------------------------------
//
//  Name    : BmpImp::SetTrueColorImage
//
//  Does    : Set the fileformat to be saved as a truecolor
//
//  Returns : Boolean (TRUE on success, FALSE on failure).
//
Boolean BmpImp :: SetTrueColorImage()
{
	if (m_InfoHeader->biBitCount == 24 && m_InfoHeader->biClrUsed == 0)
		return TRUE; // skip the next if when OK

	if (m_IsFileOpen && (m_BmpFileMode != BMP_WRITE || m_RowSizeWasUsed))
		Error ("SetTrueColorImage", "m_BmpFileMode != BMP_WRITE OR Row Data was already written");

	m_InfoHeader->biBitCount = 24;
	m_InfoHeader->biClrUsed = 0;
	m_InfoHeader->biClrImportant = 0;
	m_RgbQuads.resize(0);

	return TRUE;
}

//  ---------------------------------------------------------------------------
//
//  Name    : BmpImp::SetFalseColorImage
//
//  Does    : Set the fileformat to be saved as a falsecolor
//
//  Args    : UShort colors : (I), number of colors.
//
//  Returns : Boolean (TRUE on success, FALSE on failure).
//
Boolean BmpImp :: SetFalseColorImage(UShort colors)
{
	dms_assert(colors >= 2);
	if (colors == GetColorTableSize())
		return TRUE; // skip the next if when OK

	if (m_IsFileOpen && (m_BmpFileMode != BMP_WRITE || m_RowSizeWasUsed))
		Error ("SetFalseColorImage", "m_BmpFileMode != BMP_WRITE OR Row Data was already written");

	m_InfoHeader->biClrUsed = colors;
	if (m_InfoHeader->biClrImportant > colors) 
		m_InfoHeader->biClrImportant = colors;

	if (colors == 2)
		m_InfoHeader->biBitCount = 1;
	else
		if (colors <= 16)
			m_InfoHeader->biBitCount = 4;
		else
			m_InfoHeader->biBitCount = 8;

	m_RgbQuads.resize(colors);

	return TRUE;
}

Boolean BmpImp :: SetClrImportant(PALETTE_SIZE colors)
{
	dms_assert(!m_IsFileOpen || m_BmpFileMode != BMP_READ || colors == m_InfoHeader->biClrImportant);
	dms_assert(colors <= m_InfoHeader->biClrUsed);
	m_InfoHeader->biClrImportant = colors;
	return TRUE;
}

PALETTE_SIZE BmpImp :: GetClrImportant() const
{
	return m_InfoHeader->biClrImportant;
}

//  ---------------------------------------------------------------------------
//
//  Name    : BmpImp::SetColor
//
//  Does    : Set a color in the palette on given index, in BMP_INFO formatcolor
//
//  Args    : UShort index : (I), index of color
//            UByte  r     : (I), the red component of the color
//            UByte  g     : (I), the green component of the color
//            UByte  b     : (I), the blue component of the color
//
//  Returns : Boolean (TRUE on success, FALSE on failure).
//
Boolean BmpImp :: SetColor (UShort index, UByte r, UByte g, UByte b)
{
	dms_assert(!m_IsFileOpen || m_BmpFileMode != BMP_READ);

	dms_assert(index < GetColorTableSize());

	m_RgbQuads[index].rgbRed      = r;
	m_RgbQuads[index].rgbGreen    = g;
	m_RgbQuads[index].rgbBlue     = b;
	m_RgbQuads[index].rgbReserved = 0;

	return TRUE;
}

Boolean BmpImp :: SetColorWithAlpha(UShort index, UByte r, UByte g, UByte b, UByte alpha)
{
	dms_assert(!m_IsFileOpen || m_BmpFileMode != BMP_READ);

	dms_assert(index < GetColorTableSize());

	m_RgbQuads[index].rgbRed      = r;
	m_RgbQuads[index].rgbGreen    = g;
	m_RgbQuads[index].rgbBlue     = b;
	m_RgbQuads[index].rgbReserved = alpha;

	return TRUE;
}

void BmpImp::SetCombinedColor(PALETTE_SIZE index, DmsColor combined)
{
	SetColor(index, 
		GetRed  (combined),
		GetGreen(combined), 
		GetBlue (combined)
	);
}

// Gets colorvalues from disk as a long
DmsColor BmpImp::GetCombinedColor(PALETTE_SIZE index) const
{
	unsigned char r, g, b;

	GetColor(index, r, g, b);
	return CombineRGB(r, g, b);
}


//  ---------------------------------------------------------------------------
//
//  Name    : BmpImp::GetInchResolution
//
//  Does    : Get an image resolution in inches
//
//  Args    : Float & xres : (I), the x component.
//            Float & yres : (I), the y component.
//
//  Returns : Boolean (TRUE on success, FALSE on failure).
//
Boolean BmpImp :: GetInchResolution (Float & xres, Float & yres)
{
    // Recalc from DPM (dot per meter) to DPI (dot per inch)...
    xres = ((Float)m_InfoHeader->biXPelsPerMeter / 100.0) * 2.54;
    yres = ((Float)m_InfoHeader->biYPelsPerMeter / 100.0) * 2.54;

    return TRUE;
}

//  ---------------------------------------------------------------------------
//
//  Name    : BmpImp::SetInchResolution
//
//  Does    : Set an image resolution in inches
//
//  Args    : Float xres : (I), the x component.
//            Float yres : (I), the y component.
//
//  Returns : Boolean (TRUE on success, FALSE on failure).
//
Boolean BmpImp::SetInchResolution (Float xres, Float yres)
{
    // Recalc from DPI (dot per inch) to DPM (dot per meter)...
    m_InfoHeader->biXPelsPerMeter = (Long)((xres / 2.54) * 100.0);
    m_InfoHeader->biYPelsPerMeter = (Long)((yres / 2.54) * 100.0);

    return TRUE;
}

[[noreturn]] void BmpImp::Error(CharPtr action, CharPtr problem)
{
	if (m_FH != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_FH); 
		m_FH = INVALID_HANDLE_VALUE;
	}
	throwErrorF("Bitmap", "%s: %s", action, problem);
}
