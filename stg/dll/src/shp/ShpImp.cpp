// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "StoragePCH.h"
#include "ImplMain.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

// *****************************************************************************
//
// Implementation of non-DMS based class ShpImp. This class is used
// by ShpStorageManager to read and write ESRI shape files
//
// *****************************************************************************

#include "ShpImp.h"

#include "cpc/EndianConversions.h"

#include "dbg/debug.h"
#include "vt/Conversions.h"
#include "vt/StringBounds.h"
#include "utl/splitPath.h"
#include "utl/Environment.h"
#include "set/VectorFunc.h"
#include "mem/SeqLock.h"

#include <string.h>
#if defined(_MSC_VER)
#include <share.h>
#endif


// -----------------------------------------------------
//
// Template functions to Read\Write big or little endian
//
// -----------------------------------------------------

// Read without changing byte order
template<class T>  std::size_t ReadLittleEndian(FILE * fp, T& t)
{
	std::size_t i = fread(&t, 1, sizeof(T), fp);
	ConvertLittleEndian(t);
	return i;
};


// Write without changing byte order
template<class T> std::size_t WriteLittleEndian(FILE * fp, T t)
{
	ConvertLittleEndian(t);
	return fwrite(&t, 1, sizeof(T), fp);
};


// Read and flip byte order
template<class T> std::size_t ReadBigEndian(FILE * fp, T& t)
{
	std::size_t i= fread(&t, 1, sizeof(T), fp);
	ConvertBigEndian(t);
	return i;
};


// Flip byte order and write
template<class T> std::size_t WriteBigEndian(FILE * fp, T t)
{
	ConvertBigEndian(t);
	return fwrite(&t, 1, sizeof(T), fp);
};

// Dat file name differs only in extension from the filename
SharedStr CalcShxName(CharPtr name)
{
	return getFileNameBase(name) + ".shx";
}

SharedStr CalcPrjName(CharPtr name)
{
	return getFileNameBase(name) + ".prj";
}


// ---------------------------------------------------
//
// Implementations for class ShpImp and helper classes
//
// ---------------------------------------------------

// Initialise privates
ShpImp::ShpImp()
{
	DBG_START("ShpImp", "ShpImp", false);

	Clear();
}


// Reset members
void ShpImp::Clear()
{
	DBG_START("ShpImp", "Clear", m_Polygons.size() > 0);
	m_NrRecs = 0;
	m_ShapeType = ShapeTypes::ST_None;
	vector_clear(m_Polygons);
	vector_clear(m_Points);
}

// Get rid of file connection
void ShpImp::Close()
{
	DBG_START("ShpImp", "Close", false);

	m_FH. CloseFH();
	m_FHX.CloseFH();
	m_PRJ.CloseFH();
}


FileResult ShpImp::Open(WeakStr name, bool alsoWrite, bool writePrj)
{
	assert(!m_FH.IsOpen());
	Close();

	FileCreationMode fcm = alsoWrite ? FCM_CreateAlways : FCM_OpenReadOnly;
	auto r = m_FH.OpenFH(name, fcm, false, NR_PAGES_DATFILE);
	if (!r)
		return r;

	r = m_FHX.OpenFH(CalcShxName(name.c_str()), fcm, false, NR_PAGES_HDRFILE);
	if (!r && alsoWrite)
	{
		Close();      // Don't accept not creating FHX if we need to create and write
		return r;
	}

	if (writePrj) // Create .prj file for shapefiles only when writing
	{
		r = m_PRJ.OpenFH(CalcPrjName(name.c_str()), fcm, true, NR_PAGES_HDRFILE);
		if (!r && alsoWrite)
		{
			Close();
			return r;
		}
		assert(m_PRJ.IsOpen());
	}

	assert(m_FH.IsOpen());
	// no assert on m_FHX: for reading, a missing .shx is tolerated (see above) and Read() then
	// takes the record count from the records themselves

	return {}; // success, even if .shx and .prj files could not be created as we will read from whatever was found
}

// Read the complete file to memory (only polygons for now)
std::size_t ShpImp::OpenAndReadHeader(WeakStr name)
{
	DBG_START("ShpImp", "ReadHeader", false);

	assert(!m_FH.IsOpen());

	// Try to open
	Close();
	Clear();

	if (!Open(name, false, false))
		return 0;

	DBG_TRACE(("Opened: {}", name.c_str()));


	// Read header
	ShpHeader head(ShapeTypes::ST_None);
	std::size_t postFileHeaderPos = head.Read(m_FH);

	if (!IsKnown(head.m_ShapeType))
		throwErrorF("Shp", "ShapeType {} in ShapeFile '{}' is not supported",
			int(head.m_ShapeType), name.c_str());
	SetShapeType(head.m_ShapeType);

//	m_BoundingBox.first.first   = head.m_Xmin;
//	m_BoundingBox.first.second  = head.m_Ymin;
//	m_BoundingBox.second.first  = head.m_Max;
//	m_BoundingBox.second.second = head.m_Ymax;
	// The header fields come from the file. These checks used to be asserts, i.e. nothing in
	// Release, and a corrupt or truncated file then reached the record loop with a wrapped
	// length or a record count near 4e9 (doc/code-fixes.md STG-01..STG-05).
	constexpr Int32 headerWords = Int32(sizeof(ShpHeader) / 2); // lengths are counted in 16-bit words
	if (postFileHeaderPos != sizeof(ShpHeader))
		throwErrorF("Shp", "ShapeFile '{}' is shorter than its {}-byte header", name.c_str(), sizeof(ShpHeader));
	if (head.m_FileLength < headerWords)
		throwErrorF("Shp", "ShapeFile '{}' declares a length of {} words, less than its header", name.c_str(), head.m_FileLength);
	m_FileLength = UInt32(head.m_FileLength) * 2u;
	auto actualFileLength = m_FH.GetFileSize();
	if (m_FileLength > actualFileLength)
		throwErrorF("Shp", "ShapeFile '{}' declares {} bytes but holds only {}", name.c_str(), m_FileLength, actualFileLength);

	// Read SHX header to derive nrRecs
	if (m_FHX)
	{
		if (head.Read(m_FHX) != sizeof(ShpHeader))
			throwErrorF("Shp", "the index file of ShapeFile '{}' is shorter than its header", name.c_str());
		if (head.m_FileLength < headerWords || (head.m_FileLength - headerWords) % 4 != 0)
			throwErrorF("Shp", "the index file of ShapeFile '{}' declares an invalid length of {} words", name.c_str(), head.m_FileLength);
		m_NrRecs = (head.m_FileLength - headerWords) / 4;
		// every record takes at least a record header and a shape type in the .shp
		if (SizeT(m_NrRecs) * (sizeof(ShpRecordHeader) + sizeof(Int32)) > SizeT(m_FileLength) - sizeof(ShpHeader))
			throwErrorF("Shp", "the index file of ShapeFile '{}' declares {} records, more than the {} bytes of the .shp can hold", name.c_str(), m_NrRecs, m_FileLength - sizeof(ShpHeader));
	}
	else
		m_NrRecs = -1; // no .shx: Read() takes the count from the records themselves
	return postFileHeaderPos;
}

// Read the complete file to memory (only polygons for now)
bool ShpImp::Read(WeakStr name)
{
	DBG_START("ShpImp", "Read", false);

	std::size_t pos = OpenAndReadHeader(name);
	if (!pos)
		return false;

	ShpRecordHeader rhead;
	if (IsPoint(m_ShapeType))
	{
		// Read points
		ShapeTypes shapeType;
		m_Points.resize(0); 
		if (m_NrRecs != UInt32(-1)) m_Points.reserve(m_NrRecs);
		while (pos < m_FileLength)
		{
			MG_CHECK(m_FileLength - pos >=  (8+4+2*8));

			pos += rhead.Read(m_FH);
			MG_CHECK( rhead.ContentLength *2 == 4+2*8);

			m_Points.push_back(ShpPoint());

			MG_CHECK( SizeT(rhead.RecordNumber) == m_Points.size() );
			MG_CHECK( (m_FileLength - (pos - 8)) / 2 >=  UInt32(rhead.ContentLength));

			Int32 shapeTypeAsInt = 0;
			pos += ReadLittleEndian(m_FH, shapeTypeAsInt);
			shapeType = decltype(shapeType)(shapeTypeAsInt);

			MG_CHECK( IsPoint(shapeType) );


			if (IsNone(shapeType))
				MakeUndefined(m_Points.back());
			else
			{
				if (!IsPoint(shapeType))
					throwErrorF("Shape", "Unsupported type {} at record {} in shapefile {}\n"
						"Expected shapetype: ST_Point", 
						int(shapeType), rhead.RecordNumber, name.c_str());
				pos += ::Read(m_Points.back(), m_FH);
			}
		}
		if (m_NrRecs == UInt32(-1))
			m_NrRecs = m_Points.size(); // no .shx
		else if (m_Points.size() != m_NrRecs)
			throwErrorF("Shp", "ShapeFile '{}' holds {} point records while its index file declares {}", name.c_str(), m_Points.size(), m_NrRecs);
	}
	else
	{
		// Read lines, polygons or multipoints
		if (m_NrRecs != UInt32(-1))
			ShapeSet_PrepareDataStore(m_NrRecs, 0);
		else
			ShapeSet_PrepareDataStore(0, 0);
		SeqLock<sequence_array<ShpPointIndex>> lockParts (m_SeqParts , dms_rw_mode::write_only_all);
		SeqLock<sequence_array<ShpPoint>     > lockPoints(m_SeqPoints, dms_rw_mode::write_only_all);

		while (pos < m_FileLength)
		{
			MG_CHECK(m_FileLength - pos >=  8);

			pos += rhead.Read(m_FH);

			// Add polygon and fill it
			ShapeSet_PushBackPolygon(ShapeTypes::ST_None);

			MG_CHECK( SizeT(rhead.RecordNumber) == m_Polygons.size() );
			MG_CHECK( (m_FileLength - (pos - 8)) / 2 >=  UInt32(rhead.ContentLength));

			pos += m_Polygons.back().Read(m_FH, 2 * SizeT(rhead.ContentLength));
		}
		if (!m_NrRecs) // no .shx: PrepareDataStore(0, 0) left the count to the records themselves
			m_NrRecs = m_Polygons.size();
		else if (m_Polygons.size() != m_NrRecs) // a stale .shx made ReadSequences index past m_Polygons
			throwErrorF("Shp", "ShapeFile '{}' holds {} records while its index file declares {}", name.c_str(), m_Polygons.size(), m_NrRecs);
		assert(m_SeqPoints.size() == m_NrRecs);
		assert(m_SeqParts .size() == m_NrRecs);
	}
	// Done
	DBG_TRACE(("pos = {} (expected {})", pos, m_FileLength));
	Close();
	return true;
}

void ShpImp::ShapeSet_PrepareDataStore(UInt32 nrRecs, UInt32 nrSeqsToKeep)
{
	m_SeqPoints.Resize(0,      nrRecs, nrSeqsToKeep MG_DEBUG_ALLOCATOR_SRC("Shp: SeqPoints"));
	m_SeqParts .Resize(nrRecs, nrRecs, nrSeqsToKeep MG_DEBUG_ALLOCATOR_SRC("Shp: SeqParts"));
	m_NrRecs = nrRecs;
}

// Write memory content to disk (only polygons and arcs for now)
bool ShpImp::Write(WeakStr name, SharedStr wktPrjStr)
{
	DBG_START("ShpImp", "Write", false);

	// Try to open
	Close();
	if (!Open(name, true, !wktPrjStr.empty()))
		return false;

	if (!wktPrjStr.empty())
		fwrite(wktPrjStr.c_str(), sizeof(char), strlen(wktPrjStr.c_str()), m_PRJ);

	// Write headers
	CheckShapeType();

	ShpHeader head(m_ShapeType);

	CalcBox();

	head.m_Box = m_BoundingBox;

	head.m_FileLength = CalcNrWordsInShx();
	head.Write(m_FHX);

	head.m_FileLength = CalcNrWordsInFile();

	std::size_t pos = head.Write(m_FH);
	UInt32 recNr = 0;

	ShpRecordHeader rhead;
	if (IsPoint(m_ShapeType))
	{
		rhead.ContentLength = (sizeof(ShpPoint) + sizeof(UInt32) ) / 2; // length is the same for each point
		assert(rhead.ContentLength == 10);

		for (auto point = m_Points.begin(), pointEnd = m_Points.end(); point != pointEnd; ++point)
		{
			// Write record to index file
			rhead.RecordNumber = ThrowingConvert<Int32>( pos / 2 );
			rhead.Write(m_FHX);

			// Write record to shpfile
			rhead.RecordNumber = ++recNr;
			pos += rhead.Write(m_FH);

			pos += WriteLittleEndian(m_FH, Int32(ShapeTypes::ST_Point));
			pos += ::Write(*point, m_FH);
		}
	}
	else
	{
		// Write polygons
		auto
			pol    = m_Polygons.begin(),
			polEnd = m_Polygons.end();
		for (; pol != polEnd; ++pol) 
		{
			// Record header
			rhead.ContentLength = pol->CalcNrWordsInRecord();

			// Write record to index file
			rhead.RecordNumber = ThrowingConvert<Int32>(pos / 2);
			rhead.Write(m_FHX);

			// Write record to shpfile (base 1)
			rhead.RecordNumber = ++recNr;
			pos += rhead.Write(m_FH);
			pos += pol->Write(m_FH);
		}
	}

	// Done
	DBG_TRACE(("pos = {} (expected {})", pos, head.m_FileLength * 2));
	return true;
}

// Shx file size in 16bit words (as in ESRI fileheader)
UInt32 ShpImp::CalcNrWordsInShx() const
{
    DBG_START("ShpHeader", "CalcNrWordsInShx", false);

	UInt32 nrRecs = IsPoint(m_ShapeType)
		?	m_Points.size()
		:	m_Polygons.size(); 

	DBG_TRACE(("size: {} recs", nrRecs));
	return 
		4*nrRecs  // 8 bytes per rec in shx
	+	50;       // 100 bytes header
}


// Shp file size in 16bit words (as in ESRI fileheader)
UInt32 ShpImp::CalcNrWordsInFile()
{
    DBG_START("ShpHeader", "CalcNrWordsInFile", false);

	if (IsPoint(m_ShapeType))
		return
			(	sizeof(ShpHeader) 
			+	m_Points.size()
				*	(	sizeof(ShpPoint)
					+	sizeof(ShpRecordHeader)
					+	sizeof(Int32) // ShapeType
					)
			) / 2;

	UInt32 result = sizeof(ShpHeader) / 2;	 // fileheader
	auto
		i = m_Polygons.begin(),
		e = m_Polygons.end();
	for (; i!=e; ++i)
	{
		result += sizeof(ShpRecordHeader) /2 // recordheader
			+ (*i).CalcNrWordsInRecord();    // record
	}

	DBG_TRACE(("size: {}", result));
	return result;
}


// Bounding box for the complete file
bool ShpImp::CalcBox()
{
    DBG_START("ShpImp", "CalcBox", false);
	
	if (IsPoint(m_ShapeType))
	{
		auto
			point    = m_Points.begin(),
			pointEnd = m_Points.end();
		if (point == pointEnd)
		{
			m_BoundingBox = ShpBox(); // empty box if no defined data 
			DBG_TRACE(("empty pointset"));
			return false;
		}

		// Loop through the points
		m_BoundingBox = RangeFromSequence(point, pointEnd); 
		return true;
	}

	// Loop through the records
	auto
		i = m_Polygons.begin(),
		e = m_Polygons.end();

	for (; i != e; ++i)
	{
		if (i->CalcBox()) // find first non-empty polygon
		{
			m_BoundingBox = i->m_Header.m_Box;
			break;
		}
	}
	if (i == e)
	{
		m_BoundingBox = ShpBox(); // empty box if no defined data 
		DBG_TRACE(("no polygons"));
		return false;
	}

	while (++i < e)
	{
		if (i->CalcBox())
			m_BoundingBox |= i->m_Header.m_Box;
	}	
	return true;
}


// did someone set a correct shapeType?
void ShpImp::CheckShapeType() const
{
	if (!IsKnown(m_ShapeType))
		throwErrorF("ShpImp", "unsupported m_ShapeType {}", int(m_ShapeType));
}

#if defined(MG_DEBUG)
void ShpImp::CheckShapeType(ShapeTypes st) const
{
	CheckShapeType();
	assert(!IsNone(st));
	assert(IsPoint(st)==IsPoint(m_ShapeType));
}
#endif

// Read file header
std::size_t ShpHeader::Read(FILE * fp)
{
	DBG_START("ShpHeader", "Read", false);
	
	std::size_t pos = fread(this, 1, sizeof(ShpHeader), fp);

	// Big endian
	ConvertBigEndian(m_FileCode);
	ConvertBigEndian(m_Unused1 );
	ConvertBigEndian(m_Unused2 );
	ConvertBigEndian(m_Unused3 );
	ConvertBigEndian(m_Unused4 );
	ConvertBigEndian(m_Unused5 );
	ConvertBigEndian(m_FileLength);

	// Litte endian
	ConvertLittleEndian(m_Version);
	ConvertLittleEndian(m_ShapeType);
	ConvertLittleEndian(m_Box);
	ConvertLittleEndian(m_ZRange);
	ConvertLittleEndian(m_MRange);

	DBG_TRACE(("FileCode   = {}", m_FileCode));
	DBG_TRACE(("Unused1    = {}", m_Unused1));
	DBG_TRACE(("Unused2    = {}", m_Unused2));
	DBG_TRACE(("Unused3    = {}", m_Unused3));
	DBG_TRACE(("Unused4    = {}", m_Unused4));
	DBG_TRACE(("Unused5    = {}", m_Unused5));
	DBG_TRACE(("FileLength = {}", m_FileLength));
	DBG_TRACE(("Version    = {}", m_Version));
	DBG_TRACE(("ShapeType  = {}", int(m_ShapeType)));
	DBG_TRACE(("Xmin       = {:E}", m_Box.first.first ));
	DBG_TRACE(("Ymin       = {:E}", m_Box.first.second));
	DBG_TRACE(("Xmax       = {:E}", m_Box.second.first));
	DBG_TRACE(("Ymax       = {:E}", m_Box.second.second));
	DBG_TRACE(("Zmin       = {:E}", m_ZRange.first));
	DBG_TRACE(("Zmax       = {:E}", m_ZRange.second));
	DBG_TRACE(("Mmin       = {:E}", m_MRange.first));
	DBG_TRACE(("Mmax       = {:E}", m_MRange.second));

	// Number of bytes read
	return pos;
}

// Write file header
std::size_t ShpHeader::Write(FILE * fp) const
{
	DBG_START("ShpHeader", "Write", false);
	
	// Big endian
	std::size_t pos = WriteBigEndian(fp, m_FileCode);
	pos += WriteBigEndian(fp, m_Unused1 );
	pos += WriteBigEndian(fp, m_Unused2 );
	pos += WriteBigEndian(fp, m_Unused3 );
	pos += WriteBigEndian(fp, m_Unused4 );
	pos += WriteBigEndian(fp, m_Unused5 );
	pos += WriteBigEndian(fp, m_FileLength);

	// Litte endian
	pos += WriteLittleEndian(fp, m_Version);
	pos += WriteLittleEndian(fp, m_ShapeType);
	pos += WriteLittleEndian(fp, m_Box);
	pos += WriteLittleEndian(fp, m_ZRange);
	pos += WriteLittleEndian(fp, m_MRange);

	DBG_TRACE(("FileCode   = {}", m_FileCode));
	DBG_TRACE(("Unused1    = {}", m_Unused1));
	DBG_TRACE(("Unused2    = {}", m_Unused2));
	DBG_TRACE(("Unused3    = {}", m_Unused3));
	DBG_TRACE(("Unused4    = {}", m_Unused4));
	DBG_TRACE(("Unused5    = {}", m_Unused5));
	DBG_TRACE(("FileLength = {}", m_FileLength));
	DBG_TRACE(("Version    = {}", m_Version));
	DBG_TRACE(("ShapeType  = {}", int(m_ShapeType)));
	DBG_TRACE(("Xmin       = {:E}", m_Box.first.first ));
	DBG_TRACE(("Ymin       = {:E}", m_Box.first.second));
	DBG_TRACE(("Xmax       = {:E}", m_Box.second.first));
	DBG_TRACE(("Ymax       = {:E}", m_Box.second.second));
	DBG_TRACE(("Zmin       = {:E}", m_ZRange.first));
	DBG_TRACE(("Zmax       = {:E}", m_ZRange.second));
	DBG_TRACE(("Mmin       = {:E}", m_MRange.first));
	DBG_TRACE(("Mmax       = {:E}", m_MRange.second));

	// Number of bytes read
	return pos;
}

// Read recordheader (= recordnr + size of record (in shorts, excluding header))
std::size_t ShpRecordHeader::Read(FILE * fp)
{
	DBG_START("ShpRecordHeader", "Read", false);

	// Big endian
	std::size_t pos = fread(this, 1, sizeof(ShpRecordHeader), fp);

	ConvertBigEndian(RecordNumber);
	ConvertBigEndian(ContentLength);

	DBG_TRACE(("RecordNumber   = {}", RecordNumber));
	DBG_TRACE(("ContentLength  = {}", ContentLength));

	// Number of bytes read
	return pos;
}


// Write recordheader
std::size_t ShpRecordHeader::Write(FILE * fp)
{
    DBG_START("ShpRecordHeader", "Write", false);

	// Big endian
	std::size_t pos = WriteBigEndian(fp, RecordNumber);
	pos += WriteBigEndian(fp, ContentLength);

	DBG_TRACE(("RecordNumber   = {}", RecordNumber));
	DBG_TRACE(("ContentLength  = {}", ContentLength));

	// Number of bytes written
	return pos;
}


// Read fixed part of polygon content. 
// Gives a bounding box + number of points and parts to come
std::size_t ShpPolygonHeader::Read(FILE * fp)
{
	DBG_START("ShpPolygonHeader", "Read", false);

	// Little endian
	std::size_t pos =
		   fread(&m_ShapeType, 1, sizeof(m_ShapeType), fp); ConvertLittleEndian(m_ShapeType);
	if (m_ShapeType == Int32(ShapeTypes::ST_None))
	{
		// a null shape record has no box, parts or points; it used to be read as if it had them,
		// taking the following record's bytes for its box and counts
		m_NumParts  = 0;
		m_NumPoints = 0;
		return pos;
	}
	pos += fread(&m_Box,       1, sizeof(m_Box      ), fp); ConvertLittleEndian(m_Box );
	if (HasParts())
	{
		pos   += fread(&m_NumParts, 1, sizeof(m_NumParts  ), fp); ConvertLittleEndian(m_NumParts);
	}
	else
		m_NumParts = 1;
	pos      += fread(&m_NumPoints, 1, sizeof(m_NumPoints), fp);  ConvertLittleEndian(m_NumPoints);

	DBG_TRACE(("ShapeType      = {}", m_ShapeType));
	DBG_TRACE(("Xmin           = {:E}", m_Box.first.first));
	DBG_TRACE(("Ymin           = {:E}", m_Box.first.second));
	DBG_TRACE(("Xmax           = {:E}", m_Box.second.first));
	DBG_TRACE(("Ymax           = {:E}", m_Box.second.second));
	DBG_TRACE(("NumParts       = {}", m_NumParts));
	DBG_TRACE(("NumPoints      = {}", m_NumPoints));

	// Number of bytes read
	return pos;
}


// Write fixed part of polygon content. 
std::size_t ShpPolygonHeader::Write(FILE * fp) const
{
    DBG_START("ShpPolygonHeader", "Write", false);

	bool hasParts = HasParts();
	// Little endian
#if defined(CC_BYTEORDER_INTEL)
	 // byteorder OK
	if (hasParts)
		return fwrite(this, 1, sizeof(ShpPolygonHeader), fp);
#endif
	// conversion from BIG to little endian required
	std::size_t pos =
		   WriteLittleEndian(fp, m_ShapeType);
	pos += WriteLittleEndian(fp, m_Box);
	if (hasParts)
		pos += WriteLittleEndian(fp, m_NumParts);
	pos += WriteLittleEndian(fp, m_NumPoints);

	// Number of bytes written
	return pos;
}

// Read the parts array. 
// (This array contains a segmentations of the m_Points array)
std::size_t ShpParts::Read(FILE * fp, ShpPartIndex num_parts)
{
    DBG_START("ShpParts", "Read", false);
	
	// Make room
	get_ptr()->resize_uninitialized(num_parts MG_DEBUG_ALLOCATOR_SRC("ShpParts::Read"));
	if (!num_parts)
		return 0;

	// Read
	std::size_t pos = fread(&*(get_ptr()->begin()), sizeof(ShpPointIndex), num_parts, fp);

	ConvertLittleEndian(get_ptr()->begin(), get_ptr()->begin()+pos);

	// Number of bytes read
	return pos * sizeof(ShpPointIndex);
}


// Write the parts array. 
// (This array contains a segmentations of the m_Points array)
std::size_t ShpParts::Write(FILE * fp) const
{
	DBG_START("ShpParts", "Write", false);
	
	if (get_ptr()->empty())
		return 0;
#if defined(CC_BYTEORDER_INTEL)
	 // byteorder OK
	 return fwrite(&*(get_ptr()->begin()), sizeof(ShpPointIndex), get_ptr()->size(), fp) * sizeof(ShpPointIndex);
#else
	// conversion from BIG to little endian required
	std::size_t pos = 0;
	for (int i=0; i<size(); i++)
	{
		pos += WriteLittleEndian(fp, (*this)[i]);
		DBG_TRACE(("[{}] = {}", i, (*this)[i]));
	}

	// Number of bytes written
	return pos;
#endif
}

inline std::size_t Read(ShpPoint& self, FILE * fp)
{
	std::size_t pos = ReadLittleEndian(fp, self.first);
	return pos + ReadLittleEndian(fp, self.second);
}

inline std::size_t Write(const ShpPoint& self, FILE * fp)
{
	std::size_t pos = WriteLittleEndian(fp, self.first);
	return pos + WriteLittleEndian(fp, self.second);
}

// Read the coordinates of the vertices
std::size_t Read(FILE * fp, ShpPoint* ptr, long nrPoints)
{
    DBG_START("ShpPoints", "Read", false);
	
	// Read points
	std::size_t pos = fread(ptr, sizeof(ShpPoint), nrPoints, fp);
	
	Float64* i = &(ptr->first);
	Float64* e = i + 2*pos;

	ConvertLittleEndian(i, e);

	return pos * sizeof(ShpPoint);
}


// Write the coordinates of the vertices
std::size_t Write(FILE * fp, const ShpPoint* ptr, UInt32 nrPoints)
{
    DBG_START("ShpPoints", "Write", false);
	
	// Write
#if defined(CC_BYTEORDER_INTEL)
	// byteorder OK
	return fwrite(ptr, sizeof(ShpPoint), nrPoints, fp) * sizeof(ShpPoint);
#else
	// conversion from BIG to little endian required
	std::size_t pos = 0;
	const ShpPoint* e = first + nrPoints
	while (ptr != e);
		pos += (*ptr++).Write(fp);
	return pos;
#endif
}


// Read the parts array. 
// (This array contains a segmentations of the m_Points array)
std::size_t ShpPoints::Read(FILE * fp, ShpPointIndex num_points)
{
	// Make room
	get_ptr()->resize_uninitialized(num_points MG_DEBUG_ALLOCATOR_SRC("ShpPoints::Read"));
	if (!num_points)
		return 0;
	return ::Read(fp, &*(get_ptr()->begin()), num_points);
}

// Write the coordinates of the vertices
std::size_t ShpPoints::Write(FILE * fp) const
{
	if (get_ptr()->empty())
		return 0;
	return ::Write(fp, &*(get_ptr()->begin()), get_ptr()->size());
}

void ShpPolygon::Assign(const ShpPolygon& src)
{
	m_Header = src.m_Header;
	m_Parts.Assign(src.m_Parts);
	m_Points.Assign(src.m_Points);
	CheckInvariants();
}


// Read a complete polygon record
std::size_t ShpPolygon::Read(FILE* fp, std::size_t contentBytes)
{
	DBG_START("ShpPolygon", "Read", false);

	std::size_t pos = m_Header.Read(fp);

	// The counts come from the file and size the part and point arrays, so they are checked
	// against the record's declared content length before anything is allocated or read: a
	// count larger than the record left the tail of those arrays uninitialised, and the garbage
	// part offsets were then used as indices.
	if (m_Header.m_NumParts < 0 || m_Header.m_NumPoints < 0)
		throwErrorF("Shp", "a shapefile record declares a negative number of parts ({}) or points ({})", m_Header.m_NumParts, m_Header.m_NumPoints);
	std::size_t expectedBytes = pos
		+ (m_Header.HasParts() ? std::size_t(m_Header.m_NumParts) * sizeof(ShpPointIndex) : 0)
		+ std::size_t(m_Header.m_NumPoints) * sizeof(ShpPoint);
	if (expectedBytes != contentBytes)
		throwErrorF("Shp", "a shapefile record with {} parts and {} points needs {} bytes of content, but its header declares {}", m_Header.m_NumParts, m_Header.m_NumPoints, expectedBytes, contentBytes);

	if (m_Header.HasParts())
		pos += m_Parts .Read(fp, m_Header.m_NumParts);
	else
	{
		m_Parts.get_ptr()->resize_uninitialized(1 MG_DEBUG_ALLOCATOR_SRC("ShpPolygon::Read"));
		m_Parts.get_ptr()->front() = 0;
	}
	pos += m_Points.Read(fp, m_Header.m_NumPoints);
	if (pos != contentBytes)
		throwErrorF("Shp", "a shapefile record is truncated: {} of its {} content bytes could be read", pos, contentBytes);

	CheckInvariants();

	return pos;
}


// Write a complete polygon record
std::size_t ShpPolygon::Write(FILE * fp) const
{
	DBG_START("ShpPolygon", "Write", false);
	
	std::size_t pos = m_Header.Write(fp);

	CheckInvariants();

	if (m_Header.HasParts())
		pos += m_Parts.Write(fp);
	pos += m_Points.Write(fp);

	return pos;
}

// The invariants of a record as the ESRI specification states them, checked in Release too: the
// parts and points come from a file, and every accessor indexes by them. This used to be a
// bool Check() whose body was all asserts, i.e. unconditionally true in Release.
void ShpPolygon::CheckInvariants() const
{
	SizeT nrParts  = (*m_Parts ).size();
	SizeT nrPoints = (*m_Points).size();
	MG_USERCHECK2((nrPoints > 0) == (nrParts > 0), "shapefile record: parts without points, or points without parts");
	MG_USERCHECK2(SizeT(m_Header.m_NumParts) == nrParts && SizeT(m_Header.m_NumPoints) == nrPoints, "shapefile record: the header counts disagree with the part and point arrays");
	assert(m_Parts.m_Index == m_Points.m_Index);
	if (!nrParts)
		return;
	MG_USERCHECK2((*m_Parts)[0] == 0, "shapefile record: the first part does not start at point 0");
	for (SizeT i = 1; i < nrParts; ++i)
		MG_USERCHECK2((*m_Parts)[i] > (*m_Parts)[i-1], "shapefile record: the parts are not in ascending order");
	MG_USERCHECK2((*m_Parts).back() < ShpPointIndex(nrPoints), "shapefile record: a part starts beyond the last point");
}
// Record size in 16bit words (as in ESRI recordheader)
Int32 ShpPolygon::CalcNrWordsInRecord() const
{
	UInt32 sz = sizeof(ShpPoint) * (*m_Points).size();
	if (m_Header.HasParts())
		sz +=( sizeof(ShpPolygonHeader) + sizeof(Int32) * (*m_Parts ).size());
	else
		sz +=( sizeof(ShpPolygonHeader) - sizeof(Int32));
	return sz / 2;
}

// Bounding box for one record		
bool ShpPolygon::CalcBox()
{
    DBG_START("ShpImp", "CalcBox(recnr)", false);

	m_Header.m_NumPoints = (*m_Points).size();
	m_Header.m_NumParts  = (*m_Parts ).size();
	if (!m_Header.m_NumParts)
	{
		assert(!m_Header.m_NumPoints);
		DBG_TRACE(("empty polygon"));
		return false;
	}
	assert(m_Header.m_NumPoints);

	ConstPointIter
		point    = (*m_Points).begin(),
		pointEnd = (*m_Points).end();
	assert(point != pointEnd); 
	
	m_Header.m_Box = RangeFromSequence(point, pointEnd); // Loop through the points
	return true;
}

// The total nr of points in the shapefile
UInt32 ShpImp::ShapeSet_NrPoints() const
{
	DBG_START("ShpImp", "NrPoints()", false);

	// How many points in polygon?
	UInt32 result = 0;
	for (auto i = m_Polygons.begin(), e = m_Polygons.end(); i!=e; ++i) 
		result += i->NrPoints();

	return result;
}


// The total nr of parts in the shape file (= the number of rings)
UInt32 ShpImp::ShapeSet_NrParts() const
{
    DBG_START("ShpImp", "NrParts()", false);

	UInt32 result = 0;
	for (auto i = m_Polygons.begin(), e = m_Polygons.end(); i!=e; ++i)
		result += i->NrParts();

	return result;
}

// The total nr of points in a polygonrecord
UInt32 ShpImp::ShapeSet_NrPoints(UInt32 recNr) const
{
	assert(recNr < NrRecs() );

	// How many points in polygon?
	return m_Polygons[recNr].NrPoints();
}


// The nr of rings in a polygon
UInt32 ShpImp::ShapeSet_NrParts(UInt32 recNr) const
{
	assert(recNr < NrRecs() );
	assert(recNr < m_Polygons.size()); // is this a polygon shapefile?

	// How many parts in polygon?
	return (*m_Polygons[recNr].m_Parts).size();
}


// The nr of points in a ring
UInt32 ShpImp::ShapeSet_NrPoints(UInt32 recNr, UInt32 partNr) const
{
	DBG_START("ShpImp", "NrPoints(recnr, partnr)", false);

	assert(recNr < NrRecs() );
	assert(recNr < m_Polygons.size()); // is this a polygon shapefile?

	return m_Polygons[recNr].NrPoints(partNr);
}


ConstPointIterRange ShpImp::ShapeSet_GetPoints(UInt32 recNr) const // Gets all rings
{
	DBG_START("ShpImp", "GetPoints", false);

	assert(recNr < m_Polygons.size());
	const ShpPolygon& polygon = m_Polygons[recNr];

	sequence_array<ShpPoint>::const_reference r = *polygon.m_Points;

	return ConstPointIterRange(r.begin(), r.end());
}

ConstPointIterRange ShpImp::ShapeSet_GetPoints(UInt32 recNr, UInt32 partNr) const // Gets a specific ring
{
	DBG_START("ShpImp", "GetPoints", false);

	assert(recNr < m_Polygons.size());
	const ShpPolygon& polygon = m_Polygons[recNr];

	// How many parts in polygon?
	assert(partNr < polygon.m_Parts.get_ptr()->size());
	UInt32 cnt = polygon.NrPoints(partNr);

	UInt32 partStart = (*polygon.m_Parts)[partNr];

	ConstPointIter 
		begin = (*polygon.m_Points).begin() + partStart;
	return ConstPointIterRange(begin, begin + cnt);
}

const ShpPoint& ShpImp::ShapeSet_GetPoint(UInt32 recNr, UInt32 partNr, UInt32 pointNr) // Gets a point from a ring
{
	return GetPolygon(recNr).GetPoint(partNr, pointNr);
}

void ShpImp::SetShapeType(ShapeTypes shapeType)
{
	assert((m_ShapeType == ShapeTypes::ST_None && !m_Polygons.size()) || m_ShapeType == shapeType);

	m_ShapeType = shapeType;

	CheckShapeType();
}

ShpPolygon& ShpImp::ShapeSet_PushBackPolygon(ShapeTypes shapeType)
{
	m_Polygons.push_back(ShpPolygon(shapeType));
	
	ShpPolygon& polygon = m_Polygons.back();
	polygon.m_Points.m_Container = &m_SeqPoints;
	polygon.m_Parts .m_Container = &m_SeqParts;
	polygon.m_Points.m_Index = m_SeqPoints.size(); m_SeqPoints.push_back(Undefined() MG_DEBUG_ALLOCATOR_SRC("ShpImp::ShapeSet_PushBackPolygon.m_SeqPoints"));
	polygon.m_Parts .m_Index = m_SeqParts .size(); m_SeqParts .push_back(Undefined() MG_DEBUG_ALLOCATOR_SRC("ShpImp::ShapeSet_PushBackPolygon.m_SeqParts" ));
	return polygon;
}

ShpPolygon& ShpImp::ShapeSet_PushBackPolygon()
{
	MG_DEBUGCODE( CheckShapeType(ShapeTypes::ST_Polygon) );
	return ShapeSet_PushBackPolygon(m_ShapeType);
}

