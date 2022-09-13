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
// *****************************************************************************
//
// Implementation of non-DMS based class ShpImp. This class is used
// by ShpStorageManager to read and write ESRI shape files
//
// *****************************************************************************


#include "ImplMain.h"
#pragma hdrstop

#include "ShpImp.h"

#include "cpc/EndianConversions.h"

#include "dbg/debug.h"
#include "geo/Conversions.h"
#include "geo/StringBounds.h"
#include "utl/splitPath.h"
#include "utl/Environment.h"
#include "set/VectorFunc.h"
#include "mem/SeqLock.h"

#include <string.h>
#include <share.h>


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
	m_ShapeType = ST_None;
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


bool ShpImp::Open(WeakStr name,	SafeFileWriterArray* sfwa, bool alsoWrite, bool writePrj)
{
	dms_assert(!m_FH.IsOpen());
	Close();

	FileCreationMode fcm = alsoWrite ? FCM_CreateAlways : FCM_OpenReadOnly;
	if (!m_FH.OpenFH(name, sfwa, fcm, false, NR_PAGES_DATFILE))
		return false;

	if (!m_FHX.OpenFH(CalcShxName(name.c_str()), sfwa, fcm, false, NR_PAGES_HDRFILE) && alsoWrite)
	{
		Close();      // Don't accept not creating FHX if we need to create and write
		return false;
	}

	if (writePrj) // Create .prj file for shapefiles only when writing
	{
		if (!m_PRJ.OpenFH(CalcPrjName(name.c_str()), sfwa, fcm, true, NR_PAGES_HDRFILE) && alsoWrite)
		{
			Close();
			return false;
		}
		dms_assert(m_PRJ.IsOpen());
	}

	dms_assert(m_FH.IsOpen());
	dms_assert(m_FHX.IsOpen());

	return true;
}

// Read the complete file to memory (only polygons for now)
std::size_t ShpImp::OpenAndReadHeader(WeakStr name, SafeFileWriterArray* sfwa)
{
	DBG_START("ShpImp", "ReadHeader", false);

	dms_assert(!m_FH.IsOpen());

	// Try to open
	Close();
	Clear();

	if (!Open(name, sfwa, false, false))
		return 0;

	DBG_TRACE(("Opened: %s", name.c_str()));


	// Read header
	ShpHeader head(ST_None);
	std::size_t postFileHeaderPos = head.Read(m_FH);

	if (!IsKnown(head.m_ShapeType))
		throwErrorF("Shp", "ShapeType %d in ShapeFile '%s' is not supported",
			head.m_ShapeType, name.c_str());
	SetShapeType(head.m_ShapeType);

//	m_BoundingBox.first.first   = head.m_Xmin;
//	m_BoundingBox.first.second  = head.m_Ymin;
//	m_BoundingBox.second.first  = head.m_Max;
//	m_BoundingBox.second.second = head.m_Ymax;
	dms_assert(head.m_FileLength >= 0);
	m_FileLength = head.m_FileLength * 2;

	// Read SHX header to derive nrRecs
	if (m_FHX)
	{
		head.Read(m_FHX);
		dms_assert(head.m_FileLength >= 50);
		m_NrRecs = (head.m_FileLength - 50) / 4;
	}
	else
		m_NrRecs = -1;
	return postFileHeaderPos;
}

// Read the complete file to memory (only polygons for now)
bool ShpImp::Read(WeakStr name, SafeFileWriterArray* sfwa)
{
	DBG_START("ShpImp", "Read", false);

	std::size_t pos = OpenAndReadHeader(name, sfwa);
	if (!pos)
		return false;

	ShpRecordHeader rhead;
	if (IsPoint(m_ShapeType))
	{
		// Read points
		ShapeTypes shapeType;
		m_Points.resize(0); 
		if (m_NrRecs != -1) m_Points.reserve(m_NrRecs);
		while (pos < m_FileLength)
		{
			MG_CHECK(m_FileLength - pos >=  (8+4+2*8));

			pos += rhead.Read(m_FH);
			MG_CHECK( rhead.ContentLength *2 == 4+2*8);

			m_Points.push_back(ShpPoint());

			MG_CHECK( rhead.RecordNumber == m_Points.size() );
			MG_CHECK( (m_FileLength - (pos - 8)) >=  (UInt32(rhead.ContentLength) * 2));

			pos += ReadLittleEndian(m_FH, (long&)shapeType);

			MG_CHECK( IsPoint(shapeType) );


			if (IsNone(shapeType))
				MakeUndefined(m_Points.back());
			else
			{
				if (!IsPoint(shapeType))
					throwErrorF("Shape", "Unsupported type %d at record %d in shapefile %s\n"
						"Expected shapetype: ST_Point", 
						shapeType, rhead.RecordNumber, name.c_str());
				pos += ::Read(m_Points.back(), m_FH);
			}
		}
	}
	else
	{
		// Read lines, polygons or multipoints
		if (m_NrRecs != -1) 
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
			ShapeSet_PushBackPolygon(ST_None);

			MG_CHECK( rhead.RecordNumber == m_Polygons.size() );
			MG_CHECK( (m_FileLength - (pos - 8)) >=  (UInt32(rhead.ContentLength)*2));

			pos += m_Polygons.back().Read(m_FH);
		}
		if (!m_NrRecs)
			m_NrRecs = m_Polygons.size();
		dms_assert(m_Polygons .size() == m_NrRecs);
		dms_assert(m_SeqPoints.size() == m_NrRecs);
		dms_assert(m_SeqParts .size() == m_NrRecs);
	}
	// Done
	DBG_TRACE(("pos = %d (expected %d)", pos, m_FileLength));
	Close();
	return true;
}

void ShpImp::ShapeSet_PrepareDataStore(UInt32 nrRecs, UInt32 nrSeqsToKeep)
{
	m_SeqPoints.Resize(0,      nrRecs, nrSeqsToKeep MG_DEBUG_ALLOCATOR_SRC_STR("Shp: SeqPoints"));
	m_SeqParts .Resize(nrRecs, nrRecs, nrSeqsToKeep MG_DEBUG_ALLOCATOR_SRC_STR("Shp: SeqParts"));
	m_NrRecs = nrRecs;
}

// Write memory content to disk (only polygons and arcs for now)
bool ShpImp::Write(WeakStr name, SafeFileWriterArray* sfwa, SharedStr wktPrjStr)
{
	DBG_START("ShpImp", "Write", false);

	// Try to open
	Close();
	if (!Open(name, sfwa, true, !wktPrjStr.empty()))
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
		dms_assert(rhead.ContentLength == 10);

		for (auto point = m_Points.begin(), pointEnd = m_Points.end(); point != pointEnd; ++point)
		{
			// Write record to index file
			rhead.RecordNumber = ThrowingConvert<long>( pos / 2 );
			rhead.Write(m_FHX);

			// Write record to shpfile
			rhead.RecordNumber = ++recNr;
			pos += rhead.Write(m_FH);

			pos += WriteLittleEndian(m_FH, (long)ST_Point);
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
			rhead.RecordNumber = ThrowingConvert<long>(pos / 2);
			rhead.Write(m_FHX);

			// Write record to shpfile (base 1)
			rhead.RecordNumber = ++recNr;
			pos += rhead.Write(m_FH);
			pos += pol->Write(m_FH);
		}
	}

	// Done
	DBG_TRACE(("pos = %d (expected %d)", pos, head.m_FileLength * 2));
	return true;
}

// Shx file size in 16bit words (as in ESRI fileheader)
UInt32 ShpImp::CalcNrWordsInShx() const
{
    DBG_START("ShpHeader", "CalcNrWordsInShx", false);

	UInt32 nrRecs = IsPoint(m_ShapeType)
		?	m_Points.size()
		:	m_Polygons.size(); 

	DBG_TRACE(("size: %d recs", nrRecs));
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
					+	sizeof(long) // ShapeType
					) 
			) / 2;

	long result = sizeof(ShpHeader) / 2;	 // fileheader
	auto
		i = m_Polygons.begin(),
		e = m_Polygons.end();
	for (; i!=e; ++i)
	{
		result += sizeof(ShpRecordHeader) /2 // recordheader
			+ (*i).CalcNrWordsInRecord();    // record
	}

	DBG_TRACE(("size: %d", result));
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
		throwErrorF("ShpImp", "unsupported m_ShapeType %d", m_ShapeType);
}

#if defined(MG_DEBUG)
void ShpImp::CheckShapeType(ShapeTypes st) const
{
	CheckShapeType();
	dms_assert(!IsNone(st));
	dms_assert(IsPoint(st)==IsPoint(m_ShapeType));
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

	DBG_TRACE(("FileCode   = %d", m_FileCode));
	DBG_TRACE(("Unused1    = %d", m_Unused1));
	DBG_TRACE(("Unused2    = %d", m_Unused2));
	DBG_TRACE(("Unused3    = %d", m_Unused3));
	DBG_TRACE(("Unused4    = %d", m_Unused4));
	DBG_TRACE(("Unused5    = %d", m_Unused5));
	DBG_TRACE(("FileLength = %d", m_FileLength));
	DBG_TRACE(("Version    = %d", m_Version));
	DBG_TRACE(("ShapeType  = %d", m_ShapeType));
	DBG_TRACE(("Xmin       = %E", m_Box.first.first ));
	DBG_TRACE(("Ymin       = %E", m_Box.first.second));
	DBG_TRACE(("Xmax       = %E", m_Box.second.first));
	DBG_TRACE(("Ymax       = %E", m_Box.second.second));
	DBG_TRACE(("Zmin       = %E", m_ZRange.first));
	DBG_TRACE(("Zmax       = %E", m_ZRange.second));
	DBG_TRACE(("Mmin       = %E", m_MRange.first));
	DBG_TRACE(("Mmax       = %E", m_MRange.second));

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

	DBG_TRACE(("FileCode   = %d", m_FileCode));
	DBG_TRACE(("Unused1    = %d", m_Unused1));
	DBG_TRACE(("Unused2    = %d", m_Unused2));
	DBG_TRACE(("Unused3    = %d", m_Unused3));
	DBG_TRACE(("Unused4    = %d", m_Unused4));
	DBG_TRACE(("Unused5    = %d", m_Unused5));
	DBG_TRACE(("FileLength = %d", m_FileLength));
	DBG_TRACE(("Version    = %d", m_Version));
	DBG_TRACE(("ShapeType  = %d", m_ShapeType));
	DBG_TRACE(("Xmin       = %E", m_Box.first.first ));
	DBG_TRACE(("Ymin       = %E", m_Box.first.second));
	DBG_TRACE(("Xmax       = %E", m_Box.second.first));
	DBG_TRACE(("Ymax       = %E", m_Box.second.second));
	DBG_TRACE(("Zmin       = %E", m_ZRange.first));
	DBG_TRACE(("Zmax       = %E", m_ZRange.second));
	DBG_TRACE(("Mmin       = %E", m_MRange.first));
	DBG_TRACE(("Mmax       = %E", m_MRange.second));

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

	DBG_TRACE(("RecordNumber   = %d", RecordNumber));
	DBG_TRACE(("ContentLength  = %d", ContentLength));

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

	DBG_TRACE(("RecordNumber   = %d", RecordNumber));
	DBG_TRACE(("ContentLength  = %d", ContentLength));

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
	pos += fread(&m_Box,       1, sizeof(m_Box      ), fp); ConvertLittleEndian(m_Box );
	if (HasParts())
	{
		pos   += fread(&m_NumParts, 1, sizeof(m_NumParts  ), fp); ConvertLittleEndian(m_NumParts);
	}
	else
		m_NumParts = 1;
	pos      += fread(&m_NumPoints, 1, sizeof(m_NumPoints), fp);  ConvertLittleEndian(m_NumPoints);

	DBG_TRACE(("ShapeType      = %d", m_ShapeType));
	DBG_TRACE(("Xmin           = %E", m_Box.first.first));
	DBG_TRACE(("Ymin           = %E", m_Box.first.second));
	DBG_TRACE(("Xmax           = %E", m_Box.second.first));
	DBG_TRACE(("Ymax           = %E", m_Box.second.second));
	DBG_TRACE(("NumParts       = %d", m_NumParts));
	DBG_TRACE(("NumPoints      = %d", m_NumPoints));

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
	get_ptr()->resize_uninitialized(num_parts);
	if (!num_parts)
		return 0;

	// Read
	std::size_t pos = fread(&*(get_ptr()->begin()), sizeof(long), num_parts, fp);

	ConvertLittleEndian(get_ptr()->begin(), get_ptr()->begin()+pos);

	// Number of bytes read
	return pos * sizeof(long);
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
	 return fwrite(&*(get_ptr()->begin()), sizeof(long), get_ptr()->size(), fp) * sizeof(long);
#else
	// conversion from BIG to little endian required
	std::size_t pos = 0;
	for (long i=0; i<size(); i++)
	{
		pos += WriteLittleEndian(fp, (*this)[i]);
		DBG_TRACE(("[%d] = %d", i, (*this)[i]));
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
	get_ptr()->resize_uninitialized(num_points);
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
	dms_assert(Check());
}


// Read a complete polygon record
std::size_t ShpPolygon::Read(FILE* fp)
{
	DBG_START("ShpPolygon", "Read", false);
	
	std::size_t pos = m_Header.Read(fp);
	if (m_Header.HasParts())
		pos += m_Parts .Read(fp, m_Header.m_NumParts);
	else
	{
		m_Parts.get_ptr()->resize_uninitialized(1);
		m_Parts.get_ptr()->front() = 0;
	}
	pos += m_Points.Read(fp, m_Header.m_NumPoints);

	dms_assert(Check());

	return pos;
}


// Write a complete polygon record
std::size_t ShpPolygon::Write(FILE * fp) const
{
	DBG_START("ShpPolygon", "Write", false);
	
	std::size_t pos = m_Header.Write(fp);

	dms_assert(Check());

	if (m_Header.HasParts())
		pos += m_Parts.Write(fp);
	pos += m_Points.Write(fp);

	return pos;
}

bool ShpPolygon::Check() const
{
	dms_assert(((*m_Points).size() > 0) == ((*m_Parts ).size() > 0));
	dms_assert(((*m_Parts ).size() == 0) || (*m_Parts)[0] == 0);
	dms_assert(m_Header.m_NumParts  == (*m_Parts ).size());
	dms_assert(m_Header.m_NumPoints == (*m_Points).size());
	dms_assert(m_Parts.m_Index == m_Points.m_Index);
	for (UInt32 i = 1, n = (*m_Parts).size(); i < n; ++i)
	{
		dms_assert((*m_Parts)[i] > (*m_Parts)[i-1]);
	}
	dms_assert(((*m_Parts ).size() == 0) || (*m_Parts).back() < ShpPointIndex((*m_Points).size()));
	return true;
}
// Record size in 16bit words (as in ESRI recordheader)
long ShpPolygon::CalcNrWordsInRecord() const
{
	UInt32 sz = sizeof(ShpPoint) * (*m_Points).size();
	if (m_Header.HasParts())
		sz +=( sizeof(ShpPolygonHeader) + sizeof(long    ) * (*m_Parts ).size());
	else
		sz +=( sizeof(ShpPolygonHeader) - sizeof(long    ));
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
		dms_assert(!m_Header.m_NumPoints);
		DBG_TRACE(("empty polygon"));
		return false;
	}
	dms_assert(m_Header.m_NumPoints);

	ConstPointIter
		point    = (*m_Points).begin(),
		pointEnd = (*m_Points).end();
	dms_assert(point != pointEnd); 
	
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
inline UInt32 ShpImp::ShapeSet_NrPoints(UInt32 recNr) const
{
	dms_assert(recNr < NrRecs() );

	// How many points in polygon?
	return m_Polygons[recNr].NrPoints();
}


// The nr of rings in a polygon
inline UInt32 ShpImp::ShapeSet_NrParts(UInt32 recNr) const
{
	dms_assert(recNr < NrRecs() );
	dms_assert(recNr < m_Polygons.size()); // is this a polygon shapefile?

	// How many parts in polygon?
	return (*m_Polygons[recNr].m_Parts).size();
}


// The nr of points in a ring
UInt32 ShpImp::ShapeSet_NrPoints(UInt32 recNr, UInt32 partNr) const
{
	DBG_START("ShpImp", "NrPoints(recnr, partnr)", false);

	dms_assert(recNr < NrRecs() );
	dms_assert(recNr < m_Polygons.size()); // is this a polygon shapefile?

	return m_Polygons[recNr].NrPoints(partNr);
}


ConstPointIterRange ShpImp::ShapeSet_GetPoints(UInt32 recNr) const // Gets all rings
{
	DBG_START("ShpImp", "GetPoints", false);

	dms_assert(recNr < m_Polygons.size());
	const ShpPolygon& polygon = m_Polygons[recNr];

	sequence_array<ShpPoint>::const_reference r = *polygon.m_Points;

	return ConstPointIterRange(r.begin(), r.end());
}

ConstPointIterRange ShpImp::ShapeSet_GetPoints(UInt32 recNr, UInt32 partNr) const // Gets a specific ring
{
	DBG_START("ShpImp", "GetPoints", false);

	dms_assert(recNr < m_Polygons.size());
	const ShpPolygon& polygon = m_Polygons[recNr];

	// How many parts in polygon?
	dms_assert(partNr < polygon.m_Parts.get_ptr()->size());
	UInt32 cnt = polygon.NrPoints(partNr);

	UInt32 partStart = (*polygon.m_Parts)[partNr];

	ConstPointIter 
		begin = (*polygon.m_Points).begin() + partStart;
	return ConstPointIterRange(begin, begin + cnt);
//	return Range<const ShpPoint*>();
}

const ShpPoint& ShpImp::ShapeSet_GetPoint(UInt32 recNr, UInt32 partNr, UInt32 pointNr) // Gets a point from a ring
{
	return GetPolygon(recNr).GetPoint(partNr, pointNr);
}

void ShpImp::SetShapeType(ShapeTypes shapeType)
{
	dms_assert((m_ShapeType == 0 && !m_Polygons.size()) || m_ShapeType == shapeType);

	m_ShapeType = shapeType;

	CheckShapeType();
}

ShpPolygon& ShpImp::ShapeSet_PushBackPolygon(ShapeTypes shapeType)
{
	m_Polygons.push_back(ShpPolygon(shapeType));
	
//	dms_assert(m_SeqPoints.size() < m_SeqPoints.capacity()); // => m_SeqPoints.end() != NULL
//	dms_assert(m_SeqParts.size()  < m_SeqParts .capacity()); // => m_SeqParts.end()  != NULL

//	static_cast<sequence_array<ShpPoint>::iterator&>(m_Polygons.back().m_Points) = m_SeqPoints.end(); m_SeqPoints.push_back(Undefined());
//	static_cast<sequence_array<long>    ::iterator&>(m_Polygons.back().m_Parts ) = m_SeqParts.end();  m_SeqParts .push_back(Undefined());
	ShpPolygon& polygon = m_Polygons.back();
	polygon.m_Points.m_Container = &m_SeqPoints;
	polygon.m_Parts .m_Container = &m_SeqParts;
	polygon.m_Points.m_Index = m_SeqPoints.size(); m_SeqPoints.push_back(Undefined());
	polygon.m_Parts .m_Index = m_SeqParts .size(); m_SeqParts .push_back(Undefined());
	return polygon;
}

ShpPolygon& ShpImp::ShapeSet_PushBackPolygon()
{
	MG_DEBUGCODE( CheckShapeType(ST_Polygon) );
	return ShapeSet_PushBackPolygon(m_ShapeType);
}


