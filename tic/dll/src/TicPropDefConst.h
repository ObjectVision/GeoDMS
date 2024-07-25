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
#pragma once

#ifndef __TICPROPDEFCONST_H
#define __TICPROPDEFCONST_H

#define	NAME_NAME               "name"
#define	FULLNAME_NAME           "FullName"      

#define STORAGENAME_NAME        "StorageName"
#define STORAGETYPE_NAME        "StorageType"
#define STORAGEREADONLY_NAME    "StorageReadOnly"
#define STORAGEDRIVER_NAME      "StorageDriver"
#define STORAGEOPTIONS_NAME     "StorageOptions"
#define DISABLESTORAGE_NAME     "DisableStorage"
#define KEEPDATA_NAME           "KeepData"
#define LAZYCALC_NAME           "LazyCalculated"
#define FREEDATA_NAME           "FreeData"
#define ISHIDDEN_NAME           "IsHidden"
#define INHIDDEN_NAME           "InHidden"
#define STOREDATA_NAME          "StoreData"
#define SYNCMODE_NAME           "SyncMode"
#define SOURCE_NAME             "Source"
#define	FULL_SOURCE_NAME        "FullSource"
#define	READ_ONLY_SM_NAME       "ReadOnlyStorageManagers"
#define	WRITE_ONLY_SM_NAME      "NonReadOnlyStorageManagers"
#define	ALL_SM_NAME             "AllStorageManagers"
#define EXPR_NAME               "Expr"
#define CALCRULE_NAME           "CalcRule"
#define DESCR_NAME              "Descr"
#define ICHECK_NAME             "IntegrityCheck"
#define SIZE_ESTIMATOR_NAME     "SizeEstimator"
#define LABEL_NAME              "Label"


#define	NRSUBITEMS_NAME         "NrSubItems"
#define	ISCALCULABLE_NAME       "HasCalculator"
#define	ISLOADABLE_NAME         "IsLoadable"
#define	ISSTORABLE_NAME         "IsStorable"
#define	DIALOGTYPE_NAME         "DialogType"
#define	DIALOGDATA_NAME         "DialogData"
#define	PARAMTYPE_NAME          "ParamType"
#define	PARAMDATA_NAME          "ParamData"
#define	CDF_NAME                "cdf"
#define	URL_NAME                "url"
#define	VIEW_ACTION_NAME        "ViewAction"
#define	VIEW_DATA_NAME          "ViewData"
#define	USERNAME_NAME           "UserName"
#define	PASSWORD_NAME           "Password"
#define	SQLSTRING_NAME          "SqlString"
#define	TABLETYPE_NAME          "TableType"
#define	USING_NAME              "Using"
#define	EXPLICITSUPPLIERS_NAME  "ExplicitSuppliers"
#define	ISTEMPLATE_NAME	        "IsTemplate"
#define	INTEMPLATE_NAME	        "InTemplate"
#define	ISENDOGENOUS_NAME	    "IsEndogeous"
#define	ISPASSOR_NAME	        "IsPassor"
#define	INENDOGENOUS_NAME	    "InEndogeous"


#define	HASMISSINGVALUES_NAME   "HasMissingValues"

#define	CONFIGSTORE_NAME        "ConfigStore"
#define	CONFIGFILELINENR_NAME   "ConfigFileLineNr"
#define	CONFIGFILECOLNR_NAME    "ConfigFileColNr"
#define	CONFIGFILENAME_NAME     "ConfigFileName"
#define	CASEDIR_NAME            "CaseDir"

// PropDefs in AbstrUnit.cpp
#define	FORMAT_NAME              "Format"
#define	SR_NAME                  "SpatialReference"
#define METRIC_NAME              "Metric"
#define PROJECTION_NAME          "Projection"
#define VALUETYPE_NAME           "ValueType"

#endif // __TICPROPDEFCONST_H
