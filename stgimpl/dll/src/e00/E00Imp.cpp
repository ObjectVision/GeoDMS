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
// Implementation of non-DMS based class E00Imp. This class is used
// by AsciiStorageManager to read and write 'ascii-grids'.
//
// *****************************************************************************

#include "E00Imp.h"

#include "dbg/Check.h"
#include "dbg/debug.h"

#include <string.h>
#include <share.h>



// Initialise privates
E00Imp::E00Imp()
{
	DBG_START("E00Imp", "E00Imp", true);

	Clear();

}

// Reset 
// Function is not valid when a file is open (FP != NULL)
void E00Imp::Clear()
{
	DBG_START("E00Imp", "Clear", true);

	// admin
	FP = NULL;
	sFileName[0] = 0;
	nGridPos = 0;
	nGridPosPhysical = 0;

	// layout
	ncols = 0;
	nrows = 0;

	// header attributes
    Precision = 2;
	PixelX = 0;
	PixelY = 0;
	Ulx = 0;
	Lry = 0;
	Lrx = 0;
	Uly = 0;

}

// Get rid of open files
E00Imp::~E00Imp()
{
	DBG_START("E00Imp", "~E00Imp", true);
	Close();
	Clear();
}


// Opens the indicated 'ascii-grid' for reading. 
// The header is read.
bool E00Imp::OpenForRead(const char * name)
{
	DBG_START("E00Imp", "OpenForRead", true);

	// try to open
	Close();
    if((FP = _fsopen( name, "r", _SH_DENYWR )) == NULL) return false;
	else
	{
		strcpy(sFileName, name);
		DBG_TRACE(("Opened: %s", sFileName));
	}
 
	// read header info
	if (ReadHeader() == false) return false;
	
	// done
	return true;
}


// Opens the indicated 'ascii-grid' for writing
// The header is written
bool E00Imp::OpenForWrite(const char * name)
{
	DBG_START("E00Imp", "OpenForWrite", true);

	// try to open
	Close();
    if((FP = _fsopen( name, "w", _SH_DENYWR )) == NULL) return false;
	else
	{
		strcpy(sFileName, name);
		DBG_TRACE(("Opened: %s", sFileName));
	}
 
	// write header info
	if (WriteHeader() == false) return false;
	
	// done
	return true;
}

// Get grid bytes into an external buffer
bool E00Imp::ReadCells(char * buf, long cnt)
{

    DBG_START("E00Imp", "ReadCells", true);
    DBG_TRACE(("cnt		  : %ld", cnt));
    DBG_TRACE(("nGridPos  : %ld", nGridPos));

	if (FP == NULL) return false;

	// Clip
	if (cnt < 0) cnt = 0;
	long stripped = nGridPos + cnt - (NrOfCols() * NrOfRows());
	if (stripped < 0) stripped = 0;
	if (stripped > 0) cnt = cnt - stripped;

    DBG_TRACE(("stripped  : %ld", stripped));

	// Read
	long i=0;
	short content;
	for (i=0; i<cnt; i++)
	{
		fscanf( FP, "%d", &content);
		if (long(nGridPosPhysical % NrOfColsPhysical()) < NrOfCols())
		{
			dms_assert(content >= 0 && content <= 255);
			buf[i] = char(content);
			nGridPos++;
		}
		else i--;		// again
		nGridPosPhysical++;
	}
	for (i=cnt; i<cnt+stripped; i++)
	{
		// buf[i] = NoDataVal();
		buf[i] = 0;
	}

	// Done
	return true;

}

// Write the provided shorts to the grid
bool E00Imp::WriteCells(char * buf, long cnt)
{

    DBG_START("E00Imp", "WriteCells", true);

	// not yet; bunch of bullshit
	throwIllegalAbstract(MG_POS, "E00::WriteCells");

	// Done
	return false;

}


// Fixed header format. Will be generalised when needed.
bool E00Imp::ReadHeader()
{
    DBG_START("E00Imp", "ReadHeader", true);

	if (FP == NULL) return false;

	char dummy[255];

	// check if is an E00 file
	fscanf( FP, "%s", dummy);
    DBG_TRACE(("%s", dummy));
	if (strcmp(dummy, "EXP") != 0) return false;

	// scan for the GRD section 
	// should be somewhere in the beginning of file as a seperate string
	long i_max = 100;
	long i = 0;
	const char * s_grd = "GRD";
	fscanf( FP, "%s", dummy);
	while ((i++ < i_max) && (strcmp(dummy, s_grd) != 0)) 
	{
		fscanf( FP, "%s", dummy);
		DBG_TRACE(("dummy: %s", dummy));
	}
	if (strcmp(dummy, s_grd) != 0) return false;
 


	// scanf("%[ -~,\t]", char *);

	// Grid header layout:
	//
	//	.GRD
	//	precision 2
	//	width 3911 (realiter 3915)
	//	height 3852
	//  1
	//  -0.21474836470000E10  (?)   
	//	.
	//  pixel x-size
	//  pixel y-size
	//  .
	//  upper left  center x (-0.279..)
	//  lower right center y (-0.187..)   ?! ; probably wrong 
	//  .
	//  lower right center x ( 0.151..)
	//  upper left  center y ( 0.236..)
	//  .
//	long precision = 0;
	long width = 0;
	long height = 0;
    char sep = 0;
	float unknown = 0;
//	float pixel_x = 0;
//	float pixel_y = 0;
//	float ulx = 0;
//	float lry = 0;
//	float lrx = 0;
//	float uly = 0;

	// Read header lines.
	fscanf(FP, "%ld", &Precision);
	fscanf(FP, "%ld%ld%1s%f", &width, &height, &sep, &unknown);
	fscanf(FP, "%f%f", &PixelX, &PixelY);
	fscanf(FP, "%f%f", &Ulx, &Lry);
	fscanf(FP, "%f%f", &Lrx, &Uly);

	// Retain logical width and height
	nrows = height;
	ncols = width;	// logical width, physical width is a multiple of 5 


	// Done
	return true;
}


// Fixed header format. Will be generalised when needed.
bool E00Imp::WriteHeader()
{
    DBG_START("E00Imp", "WriteHeader", true);

	// not yet; bunch of bullshit
	throwIllegalAbstract(MG_POS, "E00::WriteHeader");

	return false;
}


// Black box copy
void E00Imp::CopyHeaderInfoFrom(const E00Imp & src)
{
    DBG_START("E00Imp", "CopyHeaderInfoFrom", true);
	
	ncols = src.ncols;     
	nrows = src.nrows;    

}

// Get rid of fill connection
void E00Imp::Close()
{
    DBG_START("E00Imp", "Close", true);

	if (FP != NULL) fclose(FP);
	FP = NULL;

	nGridPos = 0;
	nGridPosPhysical = 0;

}






