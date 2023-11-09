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
#include "ser/SafeFileWriter.h"
#include "utl/Environment.h"

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
	:	m_FP(0) 
{}

FilePtrHandle::~FilePtrHandle()
{
	if (m_FP) 
		fclose(m_FP);
}

bool FilePtrHandle::OpenFH(WeakStr name, SafeFileWriterArray* sfwa, FileCreationMode fcm, bool translate, UInt32 nrPagesInBuffer)
{
	bool createNew = MakeNew(fcm);
	SharedStr workingName = GetWorkingFileName(sfwa, name, fcm);
	if (createNew)
		GetWritePermission(workingName);
//		return CreateFH(name, sfwa, fcm, translate, nrPagesInBuffer);
	dms_assert(createNew || UseExisting(fcm));
	CloseFH();

	m_FP = _fsopen( 
		ConvertDmsFileName(workingName).c_str(), 
		createNew
		?	translate ? "wt" : "wb"
		:
		(	fcm == FCM_OpenReadOnly 
			?	translate ?	"rt"  : "rb"
			:	translate ?	"rt+" : "rb+"
		),
		fcm != FCM_OpenReadOnly ? _SH_DENYRW : _SH_DENYWR
	);
	m_TranslateText = translate;
	m_CanRead       = !createNew;
	m_CanWrite      = (fcm != FCM_OpenReadOnly);
	SetBufferSize(nrPagesInBuffer);
	return m_FP;
}

void FilePtrHandle::SetBufferSize(UInt32 nrPagesInBuffer)
{
//	if (m_FP)
//		setvbuf(m_FP, NULL, nrPagesInBuffer ? _IOFBF : _IONBF, nrPagesInBuffer * 4096);
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
