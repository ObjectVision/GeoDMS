// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "StoragePCH.h"
#include "ImplMain.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "mci/ValueClass.h"
#include "ptr/SharedStr.h"
#include "utl/Environment.h"
#include "utl/FileSystem.h"
#include "utl/PlatformError.h"
#include "utl/StrFormat.h"

#include <cctype>
#include <cerrno>
#include <cstdlib> // _doserrno

#if defined(_MSC_VER)
#include <share.h>
#endif

// ============================= SAMPLEFORMAT

const SAMPLEFORMAT::ENUM sfArray[] = {
	SAMPLEFORMAT::SF_UINT, // unsigned int scalar
	SAMPLEFORMAT::SF_VOID, // unsigned int point
	SAMPLEFORMAT::SF_VOID, // unsigned float scalar
	SAMPLEFORMAT::SF_VOID, // unsigned float point
	SAMPLEFORMAT::SF_INT,  // signed int scalar
	SAMPLEFORMAT::SF_COMPLEXINT, // signed int point
	SAMPLEFORMAT::SF_IEEEFP,        // signed float scalar
	SAMPLEFORMAT::SF_COMPLEXIEEEFP // signed float point
};

SAMPLEFORMAT::SAMPLEFORMAT(bool isSigned, bool isFloat, bool isComplex)
	: m_Value(
		sfArray[(int(isSigned) << 2) + (int(isFloat) << 1) + int(isComplex)]
	)
{}

SAMPLEFORMAT::SAMPLEFORMAT(const ValueClass* vc)
	: m_Value(SAMPLEFORMAT(vc->IsSigned(), !vc->IsIntegral(), vc->GetNrDims() == 2).m_Value)
{
}

// ============================= FilePtrHandle

#include "FilePtrHandle.h"

FilePtrHandle::FilePtrHandle()
{}

FilePtrHandle::~FilePtrHandle()
{
	if (m_FP) 
		fclose(m_FP);
}

FileResult FilePtrHandle::OpenFH(WeakStr name, FileCreationMode fcm, bool translate, UInt32 nrPagesInBuffer)
{
	bool createNew = MakeNew(fcm);
	if (createNew)
		GetWritePermission(name);
	assert(createNew || UseExisting(fcm));
	CloseFH();

	m_TranslateText = translate;
	m_CanRead = !createNew;
	m_CanWrite = (fcm != FCM_OpenReadOnly);

	CharPtr mode = createNew
		? translate ? "wt" : "wb"
		:
		(fcm == FCM_OpenReadOnly
			? translate ? "rt" : "rb"
			: translate ? "rt+" : "rb+"
			);
	auto fileName = ConvertDmsFileName(name);
#if defined(_MSC_VER)
	// fileName is UTF-8; the narrow _fsopen interprets it as the active
	// code page (CP1252 on Western installs), so non-ASCII paths fail to
	// open even though the file is there. Use the wide-char variant --
	// same root cause as #1101.
	auto wideName = Utf8_2_wchar(fileName.c_str());
	auto wideMode = Utf8_2_wchar(mode);
	m_FP = _wfsopen(wideName.get(), wideMode.get(), fcm != FCM_OpenReadOnly ? _SH_DENYRW : _SH_DENYWR);
#else
	m_FP = fopen(fileName.c_str(), mode);
#endif

	if (m_FP != nullptr)
	{
		// the buffer size the callers ask for (16 pages for a .dbf or .shp, 1 for a header): it was
		// ignored since the call was commented out, so every file ran through the 4 KB CRT default
		if (nrPagesInBuffer)
			setvbuf(m_FP, nullptr, _IOFBF, SizeT(nrPagesInBuffer) * 4096);
		m_FileName = fileName;
		return {}; // success
	}

	// emit a detailed error message, including the filename and the reason for failure
	CharPtr hint = "";
#if defined(_MSC_VER)
	if (fileName.ssize() > _MAX_PATH)
		hint = " Note that the filename is longer than _MAX_PATH, which is 260 characters";
	auto errMsg = mySSPrintF("_fsopen('{}', '{}')\nreturned error {}: {}.{}"
#else
	auto errMsg = mySSPrintF("fopen('{}', '{}')\nreturned error {}: {}.{}"
#endif
		, fileName.c_str(), mode
		, errno, strerror(errno)
		, hint
	);
	return std::unexpected(errMsg);
}

void FilePtrHandle::CloseFH()
{
	if (!m_FP)
		return;
	fclose(m_FP);
	m_FP = 0;
}

// The reason the C runtime gives for the I/O call that just failed: errno, and on Windows also the system
// error code that the CRT translated into errno (_doserrno). The translation loses information: a network
// share that drops and a write that the volume refuses can both become EINVAL or EACCES. Both are cleared
// before the call, so that a value left by an earlier call is not reported as the reason.
namespace {
	struct IoErrorCause
	{
		int           crtErr = 0;
		unsigned long osErr  = 0;
	};

	void ClearIoErrorCause()
	{
		errno = 0;
#if defined(_MSC_VER)
		_doserrno = 0;
#endif
	}

	IoErrorCause TakeIoErrorCause()
	{
		IoErrorCause result;
		result.crtErr = errno;
#if defined(_MSC_VER)
		result.osErr = _doserrno;
#endif
		return result;
	}

	SharedStr IoErrorCauseText(const IoErrorCause& cause)
	{
		SharedStr result = cause.crtErr
			? mySSPrintF("error {}: {}", cause.crtErr, strerror(cause.crtErr))
			: SharedStr();
#if defined(_MSC_VER)
		if (cause.osErr)
		{
			auto osText = platform::GetSystemErrorText(cause.osErr);
			CharPtr osBegin = osText.begin(), osEnd = osText.csend();
			while (osEnd != osBegin && std::isspace(static_cast<unsigned char>(osEnd[-1])))
				--osEnd; // FormatMessage ends its text with a line break
			auto osPart = mySSPrintF("Windows error {}: {}", cause.osErr, SharedStr(CharPtrRange(osBegin, osEnd)).c_str());
			result = result.empty() ? osPart : mySSPrintF("{}\n{}", result.c_str(), osPart.c_str());
		}
#endif
		if (result.empty())
			result = SharedStr("the C runtime gave no reason");
		return result;
	}
}

FileResult FilePtrHandle::ReadBlock(void* data, SizeT size)
{
	assert(IsOpen());
	if (!size)
		return {}; // fread answers 0 for 0 bytes, and data may be null then

	ClearIoErrorCause();
	SizeT nrRead = fread(data, 1, size, m_FP);
	if (nrRead == size)
		return {};
	auto cause = TakeIoErrorCause();

	if (feof(m_FP) && !ferror(m_FP))
		return std::unexpected(mySSPrintF("reading {} bytes from '{}' stopped after {} bytes, where the file ends"
			, size, m_FileName.c_str(), nrRead));
	return std::unexpected(mySSPrintF("reading {} bytes from '{}' stopped after {} bytes\n{}"
		, size, m_FileName.c_str(), nrRead, IoErrorCauseText(cause).c_str()));
}

FileResult FilePtrHandle::WriteBlock(const void* data, SizeT size)
{
	assert(IsOpen());
	if (!size)
		return {}; // fwrite answers 0 for 0 bytes, and data may be null then

	ClearIoErrorCause();
	SizeT nrWritten = fwrite(data, 1, size, m_FP);
	if (nrWritten == size)
		return {};
	auto cause = TakeIoErrorCause();

	return std::unexpected(mySSPrintF("writing {} bytes to '{}' stopped after {} bytes\n{}"
		, size, m_FileName.c_str(), nrWritten, IoErrorCauseText(cause).c_str()));
}

FileResult FilePtrHandle::CloseAfterWrite()
{
	if (!m_FP)
		return {};

	bool earlierWriteFailed = ferror(m_FP) != 0;
	ClearIoErrorCause();
	int closeResult = fclose(m_FP);
	auto cause = TakeIoErrorCause();
	m_FP = nullptr;

	if (closeResult != 0)
		return std::unexpected(mySSPrintF("closing '{}' failed, so the last part of what was written to it may be missing\n{}"
			, m_FileName.c_str(), IoErrorCauseText(cause).c_str()));
	if (earlierWriteFailed)
		return std::unexpected(mySSPrintF("a write to '{}' failed before the file was closed, so the file is incomplete"
			, m_FileName.c_str()));
	return {};
}

SizeT FilePtrHandle::GetFileSize() const
{
	dms_assert(IsOpen());
	dms_assert(!m_TranslateText);

	fpos_t currPos;
	fgetpos(m_FP, &currPos);

	fseek(m_FP, 0, SEEK_END);
#if defined(_MSC_VER)
	SizeT fileSize = _ftelli64(m_FP);
#else
	SizeT fileSize = ftello(m_FP);
#endif

	fsetpos(m_FP, &currPos);

	return fileSize;
}
