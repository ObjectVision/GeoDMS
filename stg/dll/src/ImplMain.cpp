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
#include "utl/mySPrintF.h"

#include <share.h>

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
	//	dms_assert(!vc->IsSequence());
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
	m_FP = _fsopen(fileName.c_str(), mode, fcm != FCM_OpenReadOnly ? _SH_DENYRW : _SH_DENYWR);

	if (m_FP != nullptr)
		return {}; // success

	// emit a detailed error message, including the filename and the reason for failure
	CharPtr hint = "";
	if (fileName.ssize() > _MAX_PATH)
		hint = " Note that the filename is longer than _MAX_PATH, which is 260 characters";
	auto errMsg = mySSPrintF("_fsopen('%s', '%s')\nreturned error %d: %s.%s"
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

SizeT FilePtrHandle::GetFileSize() const
{
	dms_assert(IsOpen());
	dms_assert(!m_TranslateText);

	fpos_t currPos;
	fgetpos(m_FP, &currPos);

	fseek(m_FP, 0, SEEK_END);
	SizeT fileSize = _ftelli64(m_FP);

	fsetpos(m_FP, &currPos);

	return fileSize;
}
