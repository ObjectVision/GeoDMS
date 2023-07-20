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
// Non DMS-based class used to stream from 'E00-grids' (Esri)
//
// *****************************************************************************

#ifndef _E00Imp_H_
#define _E00Imp_H_

#include "ImplMain.h"

#include "stdio.h"	// FP

class E00Imp
{
public:

	STGIMPL_CALL E00Imp();
	STGIMPL_CALL ~E00Imp();

	// read functions
	STGIMPL_CALL bool OpenForRead(const char* name);
	STGIMPL_CALL bool ReadCells(char* data, long cnt);
	
	// headerinfo, layout
	STGIMPL_CALL long NrOfRows() { return nrows; };
	STGIMPL_CALL long NrOfCols() { return ncols; };
	STGIMPL_CALL void SetNrOfRows(long r) { nrows = r; };
	STGIMPL_CALL void SetNrOfCols(long c) { ncols = c; };

	// headerinfo, attributes
	long  Precision;
	float PixelX;
	float PixelY;
	float Ulx;
	float Lry;
	float Lrx;
	float Uly;

	// write functions
	STGIMPL_CALL void CopyHeaderInfoFrom(const E00Imp & src);
	STGIMPL_CALL bool OpenForWrite(const char * name);
	STGIMPL_CALL bool WriteCells(char* data, long cnt);

	STGIMPL_CALL void Close();

private:

	
	FILE *			FP;
	char			sFileName[255];
	unsigned long	nGridPos;
	unsigned long   nGridPosPhysical;	// see NrOfColsPhysical()

	// header attributes
	long ncols;
	long nrows;

	// helper functions
	bool ReadHeader();
	bool WriteHeader();
	void Clear();

	// Columns are completed to a mulpiple of 5 ?!
	long NrOfColsPhysical() { return ((ncols + 4) / 5) * 5; };	

};



#endif // _E00Imp_H_