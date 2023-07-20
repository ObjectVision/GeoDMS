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
#pragma hdrstop

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
