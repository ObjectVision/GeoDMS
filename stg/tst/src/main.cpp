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

// general
#include "dbg/check.h"   // dms_assert
#include "dbg/debug.h"	 // DBG_TRACE
//#include "dbg/TraceToConsole.h" // MTA: DBG_TRACE ook meteen naar console
#include <direct.h>      // _chdir

// DMS interface
#include "stg/StorageInterface.h"	// DMS_TreeItem_SetStorageManager etc.
//#include "stg/AbstrStorageManager.h"	// ODBC test
#include "dllMain.h"						// DMS_STG_Load				
#include "TicBase.h"						// TreeItem funcs
#include "ClcInterface.h"				// DMS_Clc_Load


// test functions
void SrcToDst(const char * src, const char * dst);	
void ODBCTest();
void XdbTest();





// entry point
void main( int argc, char *argv[ ], char *envp[ ] )
{

	// start convertfunction
	if (argc != 3)
	{
		printf("usage     :    %s <src> <dst>", _strlwr(argv[0])); 
		printf("\nsupported :    *.asc, *.bmp, *.pal, *.xml (write-only), *.e00 (read-only)"); 

		printf("\n\ncontinuing with compiled test\n\n");
	}
	else
	{
		// initialise DMS stuff
		DMS_Stg_Load();
		DMS_Clc_Load();

		// call conversion
		SrcToDst(argv[1], argv[2]);

		// done
		return;
	}

	// data directory
	_chdir("/Projects/Temp"); 
	
	DBG_INIT("StgTst.log");
	DBG_START("", "main", true);

	// initialise DMS stuff
	DMS_Stg_Load();
	DMS_Clc_Load();


	// filenames
	char s_file[255];
	char s_asc[255];
	char s_bmp[255];
	char s_e00[255];
	char s_xml[255];
	char s_pal[255];
	sprintf(s_file, "natstroom1");
	sprintf(s_asc, "%s.asc", s_file);
	sprintf(s_bmp, "%s.bmp", s_file);
	sprintf(s_e00, "%s.e00", s_file);
	sprintf(s_xml, "%s.xml", s_file);
	sprintf(s_pal, "%s.pal", s_file);

	// conversion tests
	SrcToDst(s_asc, "copy.asc");
  
}	





// Takes a grid-file and converts it to the type implied by the extension of dst
void SrcToDst(const char * src, const char * dst)
{	
	DBG_START("", "SrcToDst", true);
	printf("SrcToDst: %s %s\n", src, dst);	


	// Pickup file-extensions
	char src_type[255]; src_type[0] = 0;
	char dst_type[255]; dst_type[0] = 0;
	bool xml = false;
	{
		long pos = 0;

		// Source
		pos = strlen(src) - 1;
		while ((pos >= 0) && (src[pos] != '.')) pos--;
		if (pos == 0) return;
		strcpy(src_type, src+pos+1);
		_strlwr(src_type);
		DBG_TRACE(("src_type: %s", src_type));

		// Destination
		pos = strlen(dst) - 1;
		while ((pos >= 0) && (dst[pos] != '.')) pos--;
		if (pos == 0) return;
		strcpy(dst_type, dst+pos+1);
		_strlwr(dst_type);
		DBG_TRACE(("dst_type: %s", dst_type));

		// Xml?
		if (strcmp(dst_type, "xml") == 0) xml = true;
	}


	// Convert by assigning the src dataitem to the destination item
	{
		// Root object of tree
		TreeItem * ti_root = DMS_CreateTree("Convert");

		// Source storage manager
		TreeItem * ti_src = DMS_CreateTreeItem(ti_root, "SrcItem");
		DMS_TreeItem_SetStorageManager(ti_src, src, src_type, true);

		// Destination storage manager
		TreeItem * ti_dst = NULL;
		if (xml == false)
		{
			ti_dst = DMS_CreateTreeItem(ti_root, "DstItem");
			DMS_TreeItem_SetStorageManager(ti_dst, dst, dst_type, false);
		}

		// Create sheets, copy width and height from source to destination
		{
			// Source sheet
			DMS_StorageManager_ReadTree(ti_src->GetStorageManager(), ti_src);		
			int rb=0; int re=0; int cb=0; int ce=0; 
			DMS_SPointUnit_GetRange
				((AbstrUnit *) DMS_TreeItem_GetItem(ti_src, "GridSize"), &rb, &cb, &re, &ce);
			printf("\tGridSize: (%d,%d), (%d,%d)\n", rb, cb, re, ce);

			// Destination sheet, not needed in when generating xml output
			if (xml == false)
			{
				DMS_StorageManager_ReadTree(ti_dst->GetStorageManager(), ti_dst);		
				DMS_SPointUnit_SetRange
					((AbstrUnit *) DMS_TreeItem_GetItem(ti_dst, "GridSize"), rb, cb, re, ce);					
			}
		}


		// Convert source to destination
		if (xml == false)
		{
			// Conversion directive (Grid)
			bool grid = false;
			if 
			(
				(DMS_TreeItem_GetItem(ti_src, "GridData") != NULL) &&
				(DMS_TreeItem_GetItem(ti_dst, "GridData") != NULL)
			)
			{
				grid = true;
				DMS_TreeItem_SetExpr(DMS_TreeItem_GetItem(ti_dst, "GridData"), "/SrcItem/GridData");
			}

			// Conversion directive (Palette)
			bool palette = false;
			if 
			(
				(DMS_TreeItem_GetItem(ti_src, "GridPalette") != NULL) &&
				(DMS_TreeItem_GetItem(ti_dst, "GridPalette") != NULL)
			)
			{
				palette = true;
				DMS_TreeItem_SetExpr(DMS_TreeItem_GetItem(ti_dst, "GridPalette"), "/SrcItem/GridPalette");
			}


			// Debug output
			DMS_TreeItem_Dump(ti_root, "dms_dump_gridtest.txt");

			// Trigger conversion
			if (grid == true)
			{
				DMS_TreeItem_Update(DMS_TreeItem_GetItem(ti_dst, "GridData"));			
			}
			if (palette == true)
			{
				DMS_TreeItem_Update(DMS_TreeItem_GetItem(ti_dst, "GridPalette"));
			}
		}

		// xml output
		else
		{
			DMS_TreeItem_Dump(ti_src, dst);
		}

		// Clean up
		DMS_TreeItem_SetAutoDelete(ti_root);
	}
}


// Param values should be directly visible in xml output
void PrintParamInt32(TreeItem * context, char * name)
{
	AbstrParam * param = (AbstrParam *) DMS_TreeItem_GetItem
	(
		context, 
		name
	);
	if (param == NULL) return;
	printf
	(
		"\t%s: %d\n", 
		name, 
		DMS_Int32Param_GetValue(param)
	);
};


// Param values should be directly visible in xml output
void PrintParamFloat32(TreeItem * context, char * name)
{
	AbstrParam * param = (AbstrParam *) DMS_TreeItem_GetItem
	(
		context, 
		name
	);
	if (param == NULL) return;
	printf
	(
		"\t%s: %f\n", 
		name, 
		DMS_Float32Param_GetValue(param)
	);
};




/*

void XdbTest()
{
	DBG_START("", "XdbTest", true);

	// Root object of tree
	TreeItem * ti_root = DMS_CreateTree("XdbTree");

	// Source storage manager
	TreeItem * ti_xdb = DMS_CreateTreeItem(ti_root, "Stone");
	DMS_TreeItem_SetStorageManager(ti_xdb, "stoneII.xdb", "xdb", true);
	DMS_StorageManager_ReadTree(ti_xdb->GetStorageManager());		

	// Destination storage manager
	TreeItem * ti_xdb2 = DMS_CreateTreeItem(ti_root, "Stone3");
	DMS_TreeItem_SetStorageManager(ti_xdb2, "stoneIII.xdb", "xdb", true);
	DMS_CreateDataItem
	(
		ti_xdb2, 
		"diepte", 
		DMS_DataItem_GetDomainUnit
		(
			(AbstrDataItem *)DMS_TreeItem_GetItem(ti_xdb, "diepte")
		), 
		DMS_DataItem_GetValuesUnit
		(
			(AbstrDataItem *)DMS_TreeItem_GetItem(ti_xdb, "diepte")			
		), 
		DCT_Dense, 
		0
	);
	DMS_StorageManager_ReadTree(ti_xdb2->GetStorageManager());		

	// copy data
	DMS_TreeItem_SetExpr(DMS_TreeItem_GetItem(ti_xdb2, "diepte"),  "/Stone/diepte");
	DMS_TreeItem_Update(DMS_TreeItem_GetItem(ti_xdb2, "diepte"));			


	// Debug
	DMS_TreeItem_Dump(ti_root, "dms_dump_xdb.txt");
  
}







*/





/*
void ODBCTest()
{
	DBG_START("", "ODBCTest", true);

	TreeItem					*p_storageholder	=	DMS_CreateTree("dummy");
 	AbstrStorageManager	*sm_odbc				=	DMS_StorageManager_Open(p_storageholder, "XXX.odbc", "odbc", true);
	InpStreamBuff			*inp_odbc			=	sm_odbc->OpenInpStream("cByte", true);
	unsigned	long			recordcount;

	inp_odbc->ReadBytes((char*) &recordcount, sizeof(recordcount));

	if (recordcount > 0)
	{
		char	*data	= new	char[recordcount];
		inp_odbc->ReadBytes(data, recordcount);
		delete [] data;
	}

	DMS_StorageManager_Release(sm_odbc);

	DMS_TreeItem_SetAutoDelete(p_storageholder);

	return;
}

*/



