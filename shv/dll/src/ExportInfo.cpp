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

#include "ShvDllPch.h"

#include "ExportInfo.h"

#include "geo/Conversions.h"
#include "utl/SplitPath.h"

#include "stg/AbstrStorageManager.h"

#include "ShvUtils.h"
#include "ViewPort.h"

//===================================== support funcs

const TreeItem* GetExportSettingsContext(const TreeItem* context)
{
	const TreeItem* exportContext = context->FindItem(CharPtrRange("ExportSettings"));
	if (exportContext)
		return exportContext;
	return context;
}

bool Float64Eq(Float64 a, Float64 b)
{
	return abs(a/b-1) < 0.0001;
}

bool DPointEq(DPoint a, DPoint b)
{
	return Float64Eq(a.first, b.first) && Float64Eq(a.second, b.second);
}

//===================================== dedicated support funcs

UInt32 FindBest(UInt32 maxNrDotsPerTile, UInt32  nrDots)
{
	if (nrDots <= maxNrDotsPerTile)
		return nrDots;

	UInt32 slack = MAX_VALUE(UInt32);
	UInt32 bestNrDotsPerTile = maxNrDotsPerTile;
	for (UInt32 minNrDotsPerTile = maxNrDotsPerTile/2; maxNrDotsPerTile >= minNrDotsPerTile; --maxNrDotsPerTile)
	{
		UInt32 remainder = nrDots % maxNrDotsPerTile;
		if (!remainder)
			return maxNrDotsPerTile;
		remainder = maxNrDotsPerTile - remainder;
		if (remainder < slack)
		{
			slack = remainder;
			bestNrDotsPerTile = maxNrDotsPerTile;
		}
	}
	return bestNrDotsPerTile;
}

SharedStr GetFullFileNameBase(const TreeItem* context)
{
	/*const TreeItem* exportSettingsContext = GetExportSettingsContext(context);

	SharedStr dirName = AbstrStorageManager::GetFullStorageName(
		exportSettingsContext
	,	LoadValue<SharedStr>(context, GetTokenID_mt("DirName"), SharedStr("%localDataProjDir%"))
	);

	SharedStr fileNameBase = LoadValue<SharedStr>(exportSettingsContext, GetTokenID_mt("FileNameBase"), MakeFileName(context->GetFullName().c_str()) );

	return AbstrStorageManager::GetFullStorageName(
		exportSettingsContext
	,	LoadValue<SharedStr>(exportSettingsContext, GetTokenID_mt("FullFileNameBase"), DelimitedConcat(dirName.c_str(), fileNameBase.c_str()))
	);*/

	auto local_datadir_folder = AbstrStorageManager::GetFullStorageName("", "%localDataProjDir%");
	auto filename = MakeFileName(context->GetFullName().c_str());
	return local_datadir_folder + "/" + filename;
}

//===================================== struct ExportInfo impl

ExportInfo::ExportInfo(ViewPort* vp)
	:	m_ROI(vp->GetROI())
	,	m_FullFileNameBase(GetFullFileNameBase(vp->GetContext()))
{
	const TreeItem* context = GetExportSettingsContext( vp->GetContext() );

	// Relevant Objects whose ratios and world and paper sizes are to be collected:
	//	SubDot
	//	Dot
	//	Pixel
	//	Line
	//	Tile
	//	Paper
	// Double-Integer Ratios: NrSubDotsPerDot, NrDotsPerTile, NrTilesPerPaper

	//	Defaulted Ratios: NrSubDotsPerDot, DotSize (overridable by DPI), PixelSize, LinesPerPixel
	// User can specify PaperSize and Scale/DotWorldSize, default is undefined
	//  WorldSize is default derived from roi or derived from PaperSize and Scale/DotWorldSize if both are given
	//	PaperSize is default set according to the curent view if neither are given or derived from WorldSize and Scale/DotWorldSize if the latter is given
	//  PaperSize is then enlarged to accomodate an integer number of tiles and dots per tile
	//	WorldSize is then enlarged to accomodate a preferred Scale or DotWorldSize is 
	// WorldCenterOfInterest is derived from roi

	//	SubDotsPerDot
	UInt32 nrSubDotsPerDot = LoadValue<UInt32>(context, GetTokenID_mt("NrSubDotsPerDot"  ), 2);                        // alternative spec for nrSubDotsPerDot
	m_SubDotsPerDot  = LoadValue<IPoint>(context, GetTokenID_mt("NrSubDotsPerDot"   ), IPoint(nrSubDotsPerDot, nrSubDotsPerDot)); // used to specify sub-dot rendering accuracy (now: user is responsible for subdot aggregation)

		CrdType dpi         = LoadValue<Float64>(context, GetTokenID_mt("DPI"), 600);                                      // alternative spec for dotSize in dots per inch
	DPoint dotSize          = LoadValue<DPoint >(context, GetTokenID_mt("DotSize"), DPoint( 0.0254 / dpi, 0.0254 / dpi) ); // size of dot on paper in m (default 600 dpi)
	Float64 pixelSize       = LoadValue<Float64>(context, GetTokenID_mt("PixelSize"), 0.2 / 1000 );                        // size of pixel on screen in m (default 0.2 mm)
	Float64 nrLinesPerPixel = LoadValue<Float64>(context, GetTokenID_mt("ViewFactor"), 1.0 );                              // paper view distance relative to screen view distance
	
	if (!Float64Eq(m_SubDotsPerDot.first * dotSize.second, m_SubDotsPerDot.second * dotSize.first))
		context->throwItemError("NrSubDotsPerDot and DotSize imply non-square SubDots which aren't supported");

	// SubPixelFactor
	m_SubPixelFactor = (m_SubDotsPerDot.first / dotSize.first) * (nrLinesPerPixel * pixelSize);

	// User can specify PaperSize and Scale/DotWorldSize, defaults are undefined
	DPoint  paperSize    = LoadValue<DPoint >(context, GetTokenID_mt("PaperSize")   ); // [m]
	Float64 scale        = LoadValue<Float64>(context, GetTokenID_mt("Scale")       ); // paperSize / worldSize in m/geoCrd, for example: 1m/10000m
	DPoint  dotWorldSize = LoadValue<DPoint >(context, GetTokenID_mt("DotWorldSize")); // size of dot in represented world in geocrd; dotWordSize * scale = dotSize
	if (IsDefined(scale) && IsDefined(dotWorldSize))
	{
		if (DPointEq(dotWorldSize*scale, dotSize))
			dotWorldSize = UNDEFINED_VALUE(DPoint);
		else
			context->throwItemError("Scale and DotWorldSize are incompatible with the assumed or specified dotSize; remove one of the two");
	}

	//  WorldSize is default derived from roi or derived from PaperSize and Scale/DotWorldSize if both are given
	CrdPoint worldSize = Size(m_ROI); // [geoCrd]
	if (IsDefined(paperSize))
	{
		if (IsDefined(scale))
			worldSize = paperSize / scale;
		if (IsDefined(dotWorldSize))
			worldSize = paperSize * (dotWorldSize / dotSize);
	}
	else
	{
		//	PaperSize is default set according to the curent view if neither are given or derived from WorldSize and Scale/DotWorldSize if the latter is given
		if (IsDefined(scale))
			paperSize = worldSize * scale;
		else if (IsDefined(dotWorldSize))
			paperSize = worldSize * (dotSize / dotWorldSize);
		else
			paperSize = Convert<DPoint>(vp->CalcClientSize()) * pixelSize; 
	}

	//  PaperSize is then enlarged to accomodate an integer number of tiles and dots per tile
	IPoint maxNrSubDotsPerTile = LoadValue<IPoint>(context, GetTokenID_mt("MaxNrSubDotsPerTile"   ), IPoint(2048, 2048));
	IPoint nrDots = RoundUp<4>(paperSize / dotSize);
	IPoint nrSubDots = nrDots * m_SubDotsPerDot;
	IPoint maxNrDotsPerTile = maxNrSubDotsPerTile / m_SubDotsPerDot;

	//	DotsPerTile, NrTiles
	m_DotsPerTile = IPoint(FindBest(maxNrDotsPerTile.first, nrDots.first), FindBest(maxNrDotsPerTile.second, nrDots.second) );
	m_NrTiles = (nrDots-1) / m_DotsPerTile+1;
	nrDots = m_NrTiles * m_DotsPerTile;
	paperSize = dotSize * DPoint(nrDots);

	//	WorldSize is then enlarged to accomodate a preferred Scale or DotWorldSize is 
	if (!IsDefined(dotWorldSize))
	{
		if (!IsDefined(scale))
			scale = Min<Float64>(paperSize.first / worldSize.first, paperSize.second / worldSize.second);
		dotWorldSize = (dotSize / scale);
	}
	worldSize = DPoint(nrDots) * dotWorldSize;

	// some other props
	m_ROI = Inflate(Center(m_ROI), worldSize * 0.5);
}

