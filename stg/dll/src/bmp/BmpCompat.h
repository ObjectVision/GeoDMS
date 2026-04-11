// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

// Portable BMP file format compatibility layer.
// On Windows: uses native Win32 types and file I/O from <windows.h>.
// On Linux:   defines BMP packed structs and wraps POSIX file I/O.

#if defined(_MSC_VER)

#include <windows.h>
#include <fileapi.h>
#include <io.h>

#else // Linux / POSIX

#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "cpc/Types.h"       // DWORD, HANDLE, LONG, etc.
#include "utl/Environment.h"  // ConvertDmsFileName

typedef uint16_t WORD;

// --- BMP binary structures (packed, little-endian, matches Windows layout) ---

#pragma pack(push, 2)
typedef struct tagBITMAPFILEHEADER {
	uint16_t bfType;
	uint32_t bfSize;
	uint16_t bfReserved1;
	uint16_t bfReserved2;
	uint32_t bfOffBits;
} BITMAPFILEHEADER;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct tagBITMAPINFOHEADER {
	uint32_t biSize;
	int32_t  biWidth;
	int32_t  biHeight;
	uint16_t biPlanes;
	uint16_t biBitCount;
	uint32_t biCompression;
	uint32_t biSizeImage;
	int32_t  biXPelsPerMeter;
	int32_t  biYPelsPerMeter;
	uint32_t biClrUsed;
	uint32_t biClrImportant;
} BITMAPINFOHEADER;

typedef struct tagBITMAPV4HEADER {
	uint32_t bV4Size;
	int32_t  bV4Width;
	int32_t  bV4Height;
	uint16_t bV4Planes;
	uint16_t bV4BitCount;
	uint32_t bV4V4Compression;
	uint32_t bV4SizeImage;
	int32_t  bV4XPelsPerMeter;
	int32_t  bV4YPelsPerMeter;
	uint32_t bV4ClrUsed;
	uint32_t bV4ClrImportant;
	uint32_t bV4RedMask;
	uint32_t bV4GreenMask;
	uint32_t bV4BlueMask;
	uint32_t bV4AlphaMask;
	uint32_t bV4CSType;
	uint8_t  bV4Endpoints[36]; // CIEXYZTRIPLE
	uint32_t bV4GammaRed;
	uint32_t bV4GammaGreen;
	uint32_t bV4GammaBlue;
} BITMAPV4HEADER;

typedef struct tagBITMAPCOREHEADER {
	uint32_t bcSize;
	uint16_t bcWidth;
	uint16_t bcHeight;
	uint16_t bcPlanes;
	uint16_t bcBitCount;
} BITMAPCOREHEADER;

typedef struct tagRGBQUAD {
	uint8_t rgbBlue;
	uint8_t rgbGreen;
	uint8_t rgbRed;
	uint8_t rgbReserved;
} RGBQUAD;

typedef struct tagRGBTRIPLE {
	uint8_t rgbtBlue;
	uint8_t rgbtGreen;
	uint8_t rgbtRed;
} RGBTRIPLE;
#pragma pack(pop)

// BMP compression constants
constexpr uint32_t BI_RGB  = 0;
constexpr uint32_t BI_RLE8 = 1;
constexpr uint32_t BI_RLE4 = 2;

// Windows BOOL macros
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#ifndef NULL
#define NULL nullptr
#endif

// --- Win32 file I/O emulation using POSIX ---

// HANDLE is void* (from cpc/Types.h); store fd as intptr_t cast to void*
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)

typedef const void* LPCVOID;
typedef void*       LPVOID;

// CreateFile flags (only the subset used by BmpImp)
constexpr DWORD GENERIC_READ        = 0x80000000;
constexpr DWORD GENERIC_WRITE       = 0x40000000;
constexpr DWORD FILE_SHARE_READ     = 0x00000001;
constexpr DWORD OPEN_ALWAYS         = 4;
constexpr DWORD CREATE_ALWAYS       = 2;
constexpr DWORD FILE_ATTRIBUTE_NORMAL = 0x80;

inline HANDLE CreateFileW(
	const wchar_t* /*lpFileName -- unused, use char* overload below*/,
	DWORD /*dwDesiredAccess*/, DWORD /*dwShareMode*/,
	void* /*lpSecurityAttributes*/,
	DWORD /*dwCreationDisposition*/,
	DWORD /*dwFlagsAndAttributes*/,
	void* /*hTemplateFile*/)
{
	// Not used on Linux; BmpImp::Open is overridden below
	return INVALID_HANDLE_VALUE;
}

// Portable CreateFile that takes a char* path (called from BmpImp::Open)
inline HANDLE BmpCompat_OpenFile(const char* path, DWORD desiredAccess, DWORD creationDisposition)
{
	int flags = 0;
	if ((desiredAccess & GENERIC_WRITE) && (desiredAccess & GENERIC_READ))
		flags = O_RDWR;
	else if (desiredAccess & GENERIC_WRITE)
		flags = O_WRONLY;
	else
		flags = O_RDONLY;

	if (creationDisposition == CREATE_ALWAYS)
		flags |= O_CREAT | O_TRUNC;
	else if (creationDisposition == OPEN_ALWAYS)
		flags |= O_CREAT;

	int fd = ::open(path, flags, 0666);
	if (fd < 0)
		return INVALID_HANDLE_VALUE;
	return reinterpret_cast<HANDLE>(static_cast<intptr_t>(fd));
}

inline int BmpCompat_HandleToFd(HANDLE h)
{
	return static_cast<int>(reinterpret_cast<intptr_t>(h));
}

inline bool ReadFile(HANDLE hFile, void* lpBuffer, DWORD nNumberOfBytesToRead, DWORD* lpNumberOfBytesRead, void* /*lpOverlapped*/)
{
	ssize_t n = ::read(BmpCompat_HandleToFd(hFile), lpBuffer, nNumberOfBytesToRead);
	if (n < 0) { if (lpNumberOfBytesRead) *lpNumberOfBytesRead = 0; return false; }
	if (lpNumberOfBytesRead) *lpNumberOfBytesRead = static_cast<DWORD>(n);
	return true;
}

inline bool WriteFile(HANDLE hFile, const void* lpBuffer, DWORD nNumberOfBytesToWrite, DWORD* lpNumberOfBytesWritten, void* /*lpOverlapped*/)
{
	ssize_t n = ::write(BmpCompat_HandleToFd(hFile), lpBuffer, nNumberOfBytesToWrite);
	if (n < 0) { if (lpNumberOfBytesWritten) *lpNumberOfBytesWritten = 0; return false; }
	if (lpNumberOfBytesWritten) *lpNumberOfBytesWritten = static_cast<DWORD>(n);
	return true;
}

inline DWORD SetFilePointer(HANDLE hFile, long lDistanceToMove, long* /*lpDistanceToMoveHigh*/, DWORD dwMoveMethod)
{
	int whence = SEEK_SET;
	if (dwMoveMethod == 1) whence = SEEK_CUR;
	else if (dwMoveMethod == 2) whence = SEEK_END;
	off_t pos = ::lseek(BmpCompat_HandleToFd(hFile), lDistanceToMove, whence);
	return static_cast<DWORD>(pos);
}

inline bool CloseHandle(HANDLE hFile)
{
	return ::close(BmpCompat_HandleToFd(hFile)) == 0;
}

#endif // _MSC_VER
