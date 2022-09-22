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
#include "StoragePCH.h"
#pragma hdrstop

// CompoundStorageManager.cpp: implementation of the CompoundStorageManager class.
//
//////////////////////////////////////////////////////////////////////

#include "dbg/debug.h"
#include "dbg/SeverityType.h"
#include "geo/Conversions.h"
#include "geo/StringBounds.h"
#include "mci/PropDef.h"
#include "ptr/SharedBase.h"
#include "ptr/SharedPtr.h"
#include "ser/SafeFileWriter.h" 
#include "ser/StringStream.h"
#include "set/vectorFunc.h"
#include "stg/StorageClass.h"
#include "utl/mySPrintF.h"
#include "utl/Environment.h"
#include "utl/SplitPath.h"

#include "DataStoreManagerCaller.h"

#include "cfs/CompoundStorageManager.h"

#include <windows.h>
//#define ATLASSERT(x) dms_assert(x)
#include <ComDef.h>
//#include <atlconv.h>

// {62570EE0-CFC3-11d1-B431-00C0F0111BC5}
static const GUID CLSID_CompoundStorageFile = 
{ 0x62570ee0, 0xcfc3, 0x11d1, { 0xb4, 0x31, 0x0, 0xc0, 0xf0, 0x11, 0x1b, 0xc5 } };

SUPPORT_OLEPTR(IStorage);
SUPPORT_OLEPTR(IStream);
SUPPORT_OLEPTR(IEnumSTATSTG);

//////////////////////////////////////////////////////////////////////
// inline function to convert a HRESULT to an ASMRESULT
//////////////////////////////////////////////////////////////////////

inline ASM_STATE ASMRESULT(HRESULT r) 
{
	switch(r) 
	{ 
		case S_OK:                     return ASM_OK;
		case S_FALSE:                  return ASM_E_DATAREAD; 
		case STG_E_ACCESSDENIED:       return ASM_E_ACCESSDENIED; 
		case STG_E_SHAREVIOLATION:     return ASM_E_SHAREVIOLATION;
		case STG_E_LOCKVIOLATION:      return ASM_E_LOCKVIOLATION; 
		case STG_E_FILENOTFOUND:       return ASM_E_FILENOTFOUND; 
		case STG_E_PATHNOTFOUND:       return ASM_E_PATHNOTFOUND;
		case STG_E_FILEALREADYEXISTS:  return ASM_E_FILEALREADYEXISTS; 
		case STG_E_MEDIUMFULL:         return ASM_E_MEDIUMFULL; 
		case STG_E_WRITEFAULT:         return ASM_E_WRITEFAULT; 
		case STG_E_INSUFFICIENTMEMORY: return ASM_E_INSUFFICIENTMEMORY; 
		case STG_E_REVERTED:           return ASM_ERR;
	} 
	return ASM_ERR; 
}

template <class T>
struct cfsptr : SharedBase {
	cfsptr(cfsptr<IStorage>* parent, T* resource)
	:	m_Parent(parent)
	,	m_Resource(resource) 
	{}

	~cfsptr() 
	{
		m_Resource->Commit(STGC_DEFAULT);
	}

	void Release() const { if (!DecRef()) delete this;	}

	T* operator ->() { return m_Resource; }

	SharedPtr<cfsptr<IStorage> > m_Parent;
	OlePtr<T>                    m_Resource;
};

inline void CompoundStorageManager::CheckResult(UInt32 result, CharPtr funcName, CharPtr path) const
{
	if (FAILED(result))
	{
		SharedStr errText = mySSPrintF("%s(%s)", funcName, GetNameStr().c_str() );
		if (path) errText = errText + ": "+ path;
		throwStorageError(ASMRESULT(result), errText.c_str());
	}
}

/*
 *	CompoundStorageOutStreamBuff
 *
 *	Private class for the CompoundStorageManager class.
 */

class CompoundStorageOutStreamBuff : public OutStreamBuff
{
public:
	/*
	 * construction can only be done with a valid pointer to a
	 * CompoundStorageManager and with a path.
	 */
	CompoundStorageOutStreamBuff(cfsptr<IStream>* ostr, CompoundStorageManager* csm, CharPtr name)
		: m_IStream(ostr), m_CurrPos(0), m_CSM(csm), m_Name(name), m_NameStrPtr(m_Name.c_str())
	{
	}

	~CompoundStorageOutStreamBuff()
	{
		(*m_IStream)->Commit(STGC_DEFAULT);
	}

	void WriteBytes(const Byte* data, streamsize_t size) override
	{
		if (!size) return;

		// write the data in chunks of 1GB:
		while (size)
		{
			ULONG chunkSize = size > 0x40000000 ? 0x40000000 : size;
			HRESULT result = (*m_IStream)->Write(data, size, nullptr);
			dms_assert(chunkSize <= size);
			size -= chunkSize;
			m_CurrPos += chunkSize;
			m_CSM->CheckResult(result, "WriteBytes", m_NameStrPtr);
		}
	}

	streamsize_t CurrPos() const override { return m_CurrPos; }
	bool AtEnd() const override { return false; }

private:
	streamsize_t                m_CurrPos;
	SharedPtr<cfsptr<IStream> > m_IStream;
	CompoundStorageManager*     m_CSM;
	SharedStr                   m_Name;
	CharPtr                     m_NameStrPtr;
};

/*
 *	CompoundStorageInpStreamBuff
 *
 *	Private class for the CompoundStorageManager class. 
 */

class CompoundStorageInpStreamBuff : public InpStreamBuff  
{
public:
	/*
	 * construction can only be done with a valid pointer to a 
	 * CompoundStorageManager and with a path.
	 */
	CompoundStorageInpStreamBuff(cfsptr<IStream>* istr, const CompoundStorageManager* csm, CharPtr name) 
		:	m_IStream(istr)
		,	m_CurrPos(0)
		,	m_CSM(csm)
		,	m_Name(name)
		,	m_NameStrPtr(m_Name.c_str())
		,	m_Size(0)
	{
		STATSTG stat;
		(*m_IStream)->Stat(&stat, STATFLAG_NONAME);
#if defined(DMS_64)
		m_Size = stat.cbSize.QuadPart;
#else
		MG_CHECK(stat.cbSize.HighPart == 0);
		m_Size = stat.cbSize.LowPart;
#endif
	}

	void ReadBytes(Byte* data, streamsize_t size) const override
	{
		if (!size) return;
		// read the data
		HRESULT result = (*m_IStream)->Read(data, ThrowingConvert<ULONG>(size), NULL);
		// convert result to ASM defined result
		m_CSM->CheckResult(result, "ReadBytes", m_NameStrPtr);
		m_CurrPos += size;
	}

	streamsize_t CurrPos() const override { return m_CurrPos; }
	bool         AtEnd  () const override { return m_CurrPos == m_Size; }

private:
	mutable streamsize_t          m_CurrPos;
	streamsize_t                  m_Size;
	SharedPtr<cfsptr<IStream> >   m_IStream;
	const CompoundStorageManager* m_CSM;
	SharedStr                     m_Name;
	CharPtr                       m_NameStrPtr;
};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

std::unique_ptr<wchar_t[]> Utf8_2_wchar(CharPtr utf8str)
{
	SizeT textLen = strlen(utf8str);
	std::unique_ptr<wchar_t[]> uft16Buff( new wchar_t[textLen + 1] );
	MultiByteToWideChar(utf8CP, 0, utf8str, textLen, uft16Buff.get(), textLen);
	uft16Buff[textLen] = 0;
	return uft16Buff;
}



/*
 *	Destructor
 */
CompoundStorageManager::~CompoundStorageManager()
{
	CloseStorage();
	dms_assert(!IsOpen()); // only called by AbstrStorageManager::Release
}


/*
 *	OpenStorage:
 *	Will open a file if it exists if else it will create one
 */

void CompoundStorageManager::DoOpenStorage(const StorageMetaInfo& smi, dms_rw_mode rwMode) const
{
	bool intentionToWrite = rwMode > dms_rw_mode::read_only;
	dms_assert(!IsOpen());
	dms_assert( ! ( intentionToWrite && m_IsReadOnly ) );

	DBG_START("CompoundStorageManager", "OpenStorage", false);
	DBG_TRACE(("filename= %s", GetNameStr().c_str()));

	HRESULT result;
	
	dms_assert( !(m_IsReadOnly && intentionToWrite) );

	DWORD dwMode = STGM_DIRECT | STGM_SHARE_EXCLUSIVE | (intentionToWrite ? STGM_READWRITE : STGM_READ);
			
	SafeFileWriter* sfw = DSM::GetSafeFileWriterArray()->OpenOrCreate(GetNameStr().c_str(), intentionToWrite ?FCM_OpenRwGrowable : FCM_OpenReadOnly);
	dms_assert(sfw);

	// try to open the file

	IStorage* tmp = 0;
	result = StgOpenStorage(Utf8_2_wchar(sfw->GetWorkingFileName().c_str()).get(), NULL, dwMode, NULL, 0, &tmp);
	m_Root = tmp;

	// check if this is a cs file
	if ((SUCCEEDED(result)) && (!IsCompoundStorageFile()))
		result = STG_E_DOCFILECORRUPT;

	// if file not found create one
	if (result == STG_E_FILENOTFOUND)
	{
		const_cast<CompoundStorageManager*>(this)->CreateNewFile(sfw->GetWorkingFileName());
		result = 0;
	}
	CheckResult(result, "OpenStorage", 0);
}

void CompoundStorageManager::DoCloseStorage(bool mustCommit) const
{
	dms_assert(IsOpen());

	DBG_START("CompoundStorageManager", "DoCloseStorage", false);
	DBG_TRACE(("filename=  %s", GetNameStr().c_str()));

	if (mustCommit) 
		m_Root->Commit(STGC_DEFAULT);
	m_Root.Reset();
}

std::unique_ptr<OutStreamBuff> CompoundStorageManager::DoOpenOutStream(const StorageMetaInfo& smi, CharPtr path, tile_id t)
{
	dms_assert(!m_IsReadOnly);
	dms_assert(DoesExist(smi.StorageHolder()));

	reportF(SeverityTypeID::ST_MajorTrace, "Write cfs(%s,%s)", GetNameStr().c_str(), path);

	// create new Compound storage file
	SharedPtr<cfsptr<IStream> > stream(GetStream(path, true));

	dms_assert(stream);

	return std::make_unique<CompoundStorageOutStreamBuff>(stream, this, path);
}

std::unique_ptr<InpStreamBuff> CompoundStorageManager::DoOpenInpStream(const StorageMetaInfo&, CharPtr path) const
{
	reportF(SeverityTypeID::ST_MajorTrace, "Read  cfs(%s,%s)", GetNameStr().c_str(), path);

	dms_assert(IsOpen());

	// create new Compound storage file
	SharedPtr<cfsptr<IStream> > stream( const_cast<CompoundStorageManager*>(this)->GetStream(path, false) );
	if (stream.is_null()) 
		return {};
	return std::make_unique<CompoundStorageInpStreamBuff>(stream, this, path);
}

/*
 *	IsCompoundStorageFile:
 *	Check method which checks if this file is a compound file and has the right 
 *	GUID.
 *
 *	Returns:
 *	bool: true if this is a compound file with the right GUID
 */
bool CompoundStorageManager::IsCompoundStorageFile() const
{
	CLSID cid;
	bool valid_file = true;

	// read the class id
	ReadClassStg(m_Root, &cid);
	// check the class ids
	if (cid != CLSID_CompoundStorageFile)
	{
		valid_file = false;
		m_Root.Reset();
	}
	return valid_file;
}

/*
 *	CreateNewFile: (some idiot #defined CreateFile as CreateFileA)
 *	Creting a new compound file and assigning it the right GUID.
 */
void CompoundStorageManager::CreateNewFile(WeakStr workingFileName)
{
	HRESULT result;
	DWORD dwMode = STGM_DIRECT | STGM_READWRITE | STGM_SHARE_EXCLUSIVE | STGM_CREATE;
	
	// create file
	IStorage* tmp = NULL;
	result = StgCreateDocfile(Utf8_2_wchar(workingFileName.c_str()).get(), dwMode, 0, &tmp);
	m_Root.Reset(tmp);
	// convert result to ASM defined result
	CheckResult(result, "CreateNewFile", 0);

	// write cls id to file
	result = WriteClassStg(m_Root, CLSID_CompoundStorageFile);
	// convert result to ASM defined result
	CheckResult(result, "CreateNewFile", 0);
	m_Root->Commit(STGC_DEFAULT);
}

/*
 *	GetData:
 *	Recursively searching the compound file by parsing the given path and 
 *	opening and reading a data stream as a stop criterium for this recursive
 *	method.
 *
 *	Parameters:
 *	p_sub(I): substorage to search from
 *	path(I): sub path to parse
 *
 *	Returns:
 *	CFile* : CFile object to write to.
 */

cfsptr<IStream>* CompoundStorageManager::GetStream(CharPtr path, bool mayCreate)
{
	dms_assert(path);

	SharedPtr< cfsptr<IStorage> > subStorage = new cfsptr<IStorage>(0, m_Root);
	m_Root->AddRef();
	// stop criterium of the recursion
	while (*path)
	{
		// parsing the path recursively by calling this method on path parts
		CharPtr new_path;			                // new path after parse
		SharedStr name = splitPathBase(path, &new_path);	// name of storage after parse

		IStorage* sub = OpenSubStorage(subStorage->m_Resource, name.c_str(), mayCreate);
		// POST: sub->refCount == 1 || sub==0
		if (!sub)
			return nullptr;
		subStorage = new cfsptr<IStorage>(subStorage, sub); // sub->refCount == 1
		path = new_path;
	}
	IStream* stream = OpenDataStream(subStorage->m_Resource, mayCreate); 
	// POST: stream->refCount == 1 || stream==0
	if (!stream) return nullptr;
	return new cfsptr<IStream>(subStorage, stream); // stream->refCount == 1
}

/*
 *	OpenDataStream:
 *	opening a stream with the name "DATA" on the given storage.
 *
 *	Parameters:
 *	p_parent(I): pointer to parent storage to open the data stream from.
 *
 *	Returns:
 *	IStream* : the opened data stream
 */

IStream* CompoundStorageManager::OpenDataStream(IStorage* p_parent, bool mayCreate)
{
	HRESULT result;
	DWORD dwMode = STGM_DIRECT | STGM_SHARE_EXCLUSIVE | (mayCreate ? STGM_READWRITE : STGM_READ);
	IStream* p_stream = NULL;

	// open or create the stream and return in p_stream
	static auto streamName = Utf8_2_wchar("DATA");
	result = p_parent->OpenStream(streamName.get(), NULL, dwMode, 0, &p_stream);
	if (result == STG_E_FILENOTFOUND && mayCreate)
		result = p_parent->CreateStream(streamName.get(), dwMode | STGM_CREATE, 0, 0, &p_stream);

	if (p_stream) 
	{
	
		CheckResult(result, "OpenDataStream", 0);
		
		LARGE_INTEGER li = {0};
		result = p_stream->Seek(li, STREAM_SEEK_SET, NULL);

		CheckResult(result, "OpenDataStream", 0);
	}
	return p_stream; // refCount == 1 || result==0
}

/*
 *	OpenSubStorage:
 *	opening a substorage with the given name from a given parent storage.
 *
 *	Parameters:
 *	p_parent(I): pointer to parent storage to open the substorage from.
 *	name(I): name of the substorage to open
 *
 *	Returns:
 *	IStorage* : opened substorage pointer
 */
IStorage* CompoundStorageManager::OpenSubStorage(IStorage* p_parent, CharPtr name, bool mayCreate)
{
	HRESULT result;
	DWORD dwMode = STGM_DIRECT | STGM_SHARE_EXCLUSIVE | (mayCreate ? STGM_READWRITE : STGM_READ);
	IStorage* p_child = NULL;

	if (StrLen(name) >= 32)
		throwErrorF("CompoundStorageManager", "Compound file %s: name of storage '%s' is longer than the allowed 31 chars", GetNameStr().c_str(), name);

	// open or create child storage and store in p_child
	auto streamName = Utf8_2_wchar(name);
	result = p_parent->OpenStorage(streamName.get(), NULL, dwMode, NULL, 0, &p_child);
	if (result == STG_E_FILENOTFOUND && mayCreate)
		result = p_parent->CreateStorage(streamName.get(), dwMode | STGM_CREATE, 0, 0, &p_child);
	// POST: p_child->RefCount() ==1 || p_child == NULL
	if (p_child)
	{
		CheckResult(result, "OpenSubStorage", 0);
	}

	return p_child;
}


//----------------------------------------------------------------------
// instantiation and registration
//----------------------------------------------------------------------

IMPL_DYNC_STORAGECLASS(CompoundStorageManager, "cfs")
