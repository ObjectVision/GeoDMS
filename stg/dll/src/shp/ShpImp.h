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

#ifndef _ShpImp_H_
#define _ShpImp_H_

#include "ImplMain.h"
#include "FilePtrHandle.h"

#include "dbg/Check.h"
#include "geo/iterrange.h"
#include "geo/PointOrder.h"
#include "geo/Range.h"
#include "geo/BaseBounds.h"
#include "geo/SequenceArray.h"
#include "geo/GeoSequence.h"

#include "stdio.h"
#include <vector>

// ----------------------------------------------------------------
//
// ESRI structs ('ESRI Shapefile Technical Description', july 1998)
//
// ----------------------------------------------------------------



// To handle ESRI shapefile (*.shp)
// For now only ShapeType = 5 is supported (Polygons)
enum class ShapeTypes {
	ST_None       = 0,
	ST_Point      = 1,
	ST_Polyline   = 3,
	ST_Polygon    = 5,
	ST_MultiPoint = 8
};

inline bool IsNone           (ShapeTypes st) { return st== ShapeTypes::ST_None; }
inline bool IsComposite      (ShapeTypes st) { return (st== ShapeTypes::ST_Polygon) || (st== ShapeTypes::ST_Polyline) || (st== ShapeTypes::ST_MultiPoint); }
inline bool IsCompositeOrNone(ShapeTypes st) { return IsComposite(st) || IsNone(st); }
inline bool IsPoint          (ShapeTypes st) { return st== ShapeTypes::ST_Point; }
inline bool IsPointOrNone    (ShapeTypes st) { return IsPoint(st) || IsNone(st); }
inline bool IsKnown          (ShapeTypes st) { return IsPoint(st) || IsComposite(st); }
inline bool IsKnownOrNone    (ShapeTypes st) { return IsKnown(st) || IsNone(st); }

// ESRI type struct for fileheader
#pragma pack(push, 4)

typedef Point<Float64> ShpPoint;
	std::size_t Read (      ShpPoint& self, FILE * fp);
	std::size_t Write(const ShpPoint& self, FILE * fp);


// Used by ShpImp::GetPoints() and ShpImp::AddPoints()
typedef Range<ShpPoint>  ShpBox;

struct ShpHeader
{
	ShpHeader(ShapeTypes shapeType)
		:	m_FileCode(9994), m_Unused1(0), m_Unused2(0), m_Unused3(0), m_Unused4(0), m_Unused5(0)
		,	m_FileLength(0), m_Version(1000), m_ShapeType(shapeType)
		,	m_Box(ShpPoint(0, 0), ShpPoint(0, 0))
		,	m_ZRange(0, 0)
		,	m_MRange(0, 0) 
	{};
	STGIMPL_CALL std::size_t Read (FILE * fp);
	STGIMPL_CALL std::size_t Write(FILE * fp) const;

	long m_FileCode;
	long m_Unused1;
	long m_Unused2;
	long m_Unused3;
	long m_Unused4;
	long m_Unused5;
	long m_FileLength;
	long m_Version;
	ShapeTypes	   m_ShapeType; // long
	ShpBox         m_Box;
	Range<Float64> m_ZRange;
	Range<Float64> m_MRange;
};


// ESRI type struct for recordheader
struct ShpRecordHeader
{
	ShpRecordHeader() { RecordNumber = 0; ContentLength = 0; };
	STGIMPL_CALL std::size_t Read (FILE * fp);
	STGIMPL_CALL std::size_t Write(FILE * fp);

	long RecordNumber;
	long ContentLength;
};


// ESRI type struct of fixed part of polygon 
struct ShpPolygonHeader
{
	ShpPolygonHeader(ShapeTypes shapeType)
		:	m_ShapeType(long(shapeType))
		,	m_Box(ShpPoint(0, 0), ShpPoint(0, 0))
		,	m_NumParts(0)
		,	m_NumPoints(0)
	{
		dms_assert(IsCompositeOrNone(shapeType));
	};

	bool HasParts() const { return m_ShapeType != long(ShapeTypes::ST_MultiPoint); }

	STGIMPL_CALL std::size_t Read (FILE * fp);
	STGIMPL_CALL std::size_t Write(FILE * fp) const;

	DRect GetBoundingBox() const { return shp2dms_order(m_Box); }

	long   m_ShapeType;
	ShpBox m_Box;
	long   m_NumParts;
	long   m_NumPoints;
};

typedef Int32          ShpPointIndex;
typedef Int32          ShpPartIndex;

// ESRI type struct for polygon part content; has been altered to use internal storage
struct ShpParts : sequence_array_index<ShpPointIndex>
{
	ShpParts() : sequence_array_index<ShpPointIndex>(0, 0) {};

	void Assign(const ShpParts& src) { **this = *src; }
	STGIMPL_CALL std::size_t Read (FILE * fp, ShpPartIndex num_parts);
	STGIMPL_CALL std::size_t Write(FILE * fp) const;
};


// ESRI type struct for polygon points content; has been altered to use internal storage
struct ShpPoints : sequence_array_index<ShpPoint>
{
	ShpPoints() : sequence_array_index<ShpPoint>(0, 0) {};

	void Assign(const ShpPoints& src) { **this = *src; }
	STGIMPL_CALL std::size_t Read (FILE * fp, ShpPointIndex num_points);
	STGIMPL_CALL std::size_t Write(FILE * fp) const;
};


// Polygon record
struct ShpPolygon
{
	ShpPolygonHeader m_Header;
	ShpParts         m_Parts;
	ShpPoints        m_Points;

	ShpPolygon(ShapeTypes shapeType)
		:	m_Header(shapeType)
	{}

	STGIMPL_CALL void Assign(const ShpPolygon& src);
	STGIMPL_CALL std::size_t Read (FILE * fp);
	STGIMPL_CALL std::size_t Write(FILE * fp) const;

	bool CalcBox();

	long CalcNrWordsInRecord() const;

	UInt32 NrParts()  const { return (*m_Parts ).size(); }
	UInt32 NrPoints() const { return (*m_Points).size(); }
	UInt32 NrPoints(UInt32 partNr) const
	{
		UInt32 partStart = GetPartStart(partNr);
		return ((++partNr < NrParts()) 
			?	(*m_Parts)[partNr]
			:	NrPoints()) 
		-	partStart;
	}

	UInt32 GetPartStart(UInt32 partNr) const
	{
		dms_assert(partNr < NrParts());
		return (*m_Parts)[partNr];
	}

	const ShpPoint& GetPoint(UInt32 pointNr) const // Gets a point from any ring 
	{
		dms_assert(pointNr < NrPoints());
		return (*m_Points)[pointNr];
	}

	const ShpPoint& GetPoint(UInt32 partNr, UInt32 pointNr) const // Gets a point from a ring
	{
		dms_assert(pointNr < NrPoints(partNr));
		return (*m_Points)[SizeT(GetPartStart(partNr))+pointNr];
	}

	template <typename InIter> 
	void AddPoints(InIter first, InIter last);

private:
	bool Check() const;
};

typedef ShpPoints::reference::const_iterator ConstPointIter;
typedef ShpPoints::reference::iterator       PointIter;
typedef IterRange<ConstPointIter>            ConstPointIterRange;

#pragma pack(pop)

// *****************************************************************************
//
// Non DMS-based class used to manipulate ESRI Shape files
//
// *****************************************************************************


class ShpImp
{
public:
	
	STGIMPL_CALL ShpImp();

	// File IO
	STGIMPL_CALL std::size_t OpenAndReadHeader(WeakStr name, SafeFileWriterArray* sfwa);

	STGIMPL_CALL bool Read (WeakStr name, SafeFileWriterArray* sfwa);  // Reads complete shapefile into memory
	STGIMPL_CALL bool Write(WeakStr name, SafeFileWriterArray* sfwa, SharedStr pszESRIWkt);  // Writes memory to shapefile(.shp), indexfile(.shx) and projectionfile(.prj)
	STGIMPL_CALL void Close();              // Closes open files. Memory is unaffected			

	// Memory to client: all types
	STGIMPL_CALL UInt32 NrRecs() const  { return m_NrRecs; }; // The total nr of shapes in the file (as set in ReadHeader)
	const ShpBox&  GetBoundingBox() const { return m_BoundingBox; }
	ShapeTypes     GetShapeType()   const { return m_ShapeType; }

	// Memory to client: ST_Point
	std::vector<ShpPoint>::const_iterator PointSet_GetPointsBegin() const { return m_Points.begin(); }
	std::vector<ShpPoint>::const_iterator PointSet_GetPointsEnd()   const { return m_Points.end(); }

	// Memory to client: ST_Polyline, ST_Polygon 
	STGIMPL_CALL UInt32 ShapeSet_NrPoints() const;                           // Total number of ShpPoints in shapefile
	STGIMPL_CALL UInt32 ShapeSet_NrParts() const;                            // Total number of rings in shapefile
	STGIMPL_CALL UInt32 ShapeSet_NrPoints(UInt32 recnr) const;               // Number of ShpPoints in record
	STGIMPL_CALL UInt32 ShapeSet_NrParts(UInt32 recnr) const;                // Number of rings in record
	STGIMPL_CALL UInt32 ShapeSet_NrPoints(UInt32 recnr, UInt32 partnr) const;// Number of ShpPoints in ring
	STGIMPL_CALL ConstPointIterRange ShapeSet_GetPoints(UInt32 recnr) const; // Gets all rings
	STGIMPL_CALL ConstPointIterRange ShapeSet_GetPoints(UInt32 recnr, UInt32 partnr) const;	// Gets a specific ring
	STGIMPL_CALL const ShpPoint& ShapeSet_GetPoint(UInt32 recnr, UInt32 partnr, UInt32 pointnr); // Gets a point from a ring

	// Client to memory
	STGIMPL_CALL void SetShapeType(ShapeTypes shapeType);

	template <typename InIter> void PointSet_AddPoints(InIter first, InIter last);  // Adds a set of points
	STGIMPL_CALL ShpPolygon& ShapeSet_PushBackPolygon(ShapeTypes shapeType);
	STGIMPL_CALL ShpPolygon& ShapeSet_PushBackPolygon();
	STGIMPL_CALL void ShapeSet_PrepareDataStore(UInt32 nrRecs, UInt32 nrSeqsToKeep);
	
	// Debug
	void Render(long width, long height);

	UInt32     GetFileLength() const { return m_FileLength;    }
	FILE*      GetFP        () const { return m_FH;            }
	const ShpPolygon& GetPolygon(UInt32 recNr) const
	{
		dms_assert(recNr < m_Polygons.size());
		return m_Polygons[recNr];
	}
	ShpPolygon& GetPolygon(UInt32 recNr)
	{
		dms_assert(recNr < m_Polygons.size());
		return m_Polygons[recNr];
	}

private:
	// Helper functions
	bool Open(WeakStr name, SafeFileWriterArray* sfwa, bool alsoWrite, bool writePrj);
	void Clear();                                   // reset

	UInt32 CalcNrWordsInShx() const;                // nr of (16 bit) words in index file
	UInt32 CalcNrWordsInFile();						// nr of (16 bit) words in shape file

	bool CalcBox();                                 // update shapefile bounding box; returs false if nodata
	void CheckShapeType() const;                    // did someone set a correct shapeType?

#if defined(MG_DEBUG)
	STGIMPL_CALL void CheckShapeType(ShapeTypes st) const;// is shapeType compatible?
#endif

	// File IO
	FilePtrHandle m_FH;                              // *.shp
	FilePtrHandle m_FHX;                             // *.shx
	FilePtrHandle m_PRJ;							 // *.prj

	ShapeTypes                    m_ShapeType;       // read or set shapeType

public:
	std::vector<ShpPolygon>        m_Polygons;        // Data      for ShapeType no 3 && 5
	sequence_array<ShpPointIndex>  m_SeqParts;        // PartData  for ShapeType no 3 && 5
	sequence_array<ShpPoint>       m_SeqPoints;       // PointData for ShapeType no 3 && 5
	std::vector<ShpPoint>          m_Points;          // Data for ShapeType no 1

	ShpBox                        m_BoundingBox;     // read from header: ((Xmin, Ymin), Xmax, Ymax))
	UInt32 m_NrRecs;
	UInt32 m_FileLength;
};

template <typename InIter> 
void ShpPolygon::AddPoints(InIter first, InIter last)
{
	// Add part 
	(*m_Parts).push_back((*m_Points).size());

	(*m_Points).append(first, last);
}

// Throw some points into a fresh shape file
template <typename InIter> 
void ShpImp::PointSet_AddPoints(InIter first, InIter last)
{
	MG_DEBUGCODE( CheckShapeType(ShapeTypes::ST_Point) );

	UInt32 nrOrgPoints = m_Points.size();

	m_Points.resize(nrOrgPoints + (last - first) );

	fast_copy(first, last, m_Points.begin() + nrOrgPoints);
}


#endif // _ShpImp_H_