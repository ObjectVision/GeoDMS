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
#if !defined(__TIC_SESSIONDATA_H)
#define __TIC_SESSIONDATA_H

#include "ptr/OwningPtr.h"
#include "ptr/WeakPtr.h"
#include "ser/SafeFileWriter.h"

#include "DataController.h"
struct TreeItem;

//----------------------------------------------------------------------
// struct SessionData
//----------------------------------------------------------------------

extern std::recursive_mutex sd_SessionDataCriticalSection;
extern leveled_counted_section s_SessionUsageCounter;

struct SessionData
{
	static TIC_CALL void Create(CharPtr configLoadDir, CharPtr configSubDir); // call this before reading a config in order to set cfgColFirst right
	static TIC_CALL void SetConfigPointColFirst(bool cfgColFirst); // call this after create because GetConfigPointColFirst requires the configDir of the Curr SessionData

	TIC_CALL void Open  (const TreeItem* configRoot);       // call this after  reading a config to init the configRoot and open the DataStoreManager

	static void ActivateIt(const TreeItem* configRoot) // for now, assume session to be a singleton
	{
		dms_assert(s_CurrSD && (s_CurrSD->GetConfigRoot() == configRoot || s_CurrSD->m_ConfigRoot.is_null()) ); 
	}

	void ActivateThis();

	static WeakPtr<SessionData> GetIt(const TreeItem* configRoot)
	{
		if (Curr()) // Assumes SessionData is a singleton
			ActivateIt(configRoot); 
		return Curr(); 
	}

	static void CancelDataStoreManager(const TreeItem* configRoot);
	bool IsCancelling() const { return m_IsCancelling;  }

	static void ReleaseIt(const TreeItem* configRoot); // WARNING: this might point to a destroyed configRoot

	static WeakPtr<SessionData> Curr()
	{
		return s_CurrSD;
	}

	TIC_CALL void Release();

	const TreeItem* GetConfigRoot   () const { return m_ConfigRoot; } 
	WeakStr         GetConfigLoadDir() const { return m_ConfigLoadDir; }
	SharedStr       GetConfigDir    () const;
	TIC_CALL bool   IsConfigDirty   () const;

	SharedStr       GetConfigIniFile() const;
	static SharedStr GetConfigIniFile(CharPtr configDir);

	const TreeItem* GetConfigSettings() const;
	const TreeItem* GetConfigSettings(CharPtr section, CharPtr key) const;

	         SharedStr ReadConfigString(CharPtr section, CharPtr key, CharPtr defaultValue) const;
	TIC_CALL Int32     ReadConfigValue (CharPtr section, CharPtr key, Int32   defaultValue) const;

	const TreeItem* GetContainer(const TreeItem* context, CharPtr name) const;
	const TreeItem* GetClassificationContainer(const TreeItem* context) const;

	void SetActiveDesktop(const TreeItem* tiActive);
	const TreeItem* GetActiveDesktop() const;

	DataControllerMap& GetDcMap() { return m_DcMap; }
	SafeFileWriterArray* GetSafeFileWriterArray() { return &m_SFWA;  }


public: // ==== code analysis support: DMS_TreeItem_SetAnalysisSource
	std::map<const Actor*, UInt32> m_SupplierLevels;
	SharedPtr<const TreeItem>      m_SourceItem;

private:
	SessionData(CharPtr configLoadDir, CharPtr configSubDir);
	~SessionData();
	SessionData(const SessionData&); // undefined

	void DeactivateThis();
	SafeFileWriterArray           m_SFWA;

	WeakPtr<const TreeItem>       m_ConfigRoot, m_ConfigSettings, m_ActiveDesktop;
	SharedStr                     m_ConfigLoadDir;
	SharedStr                     m_ConfigSubDir;
	SharedStr                     m_ConfigDir;
	TimeStamp                     m_ConfigLoadTS;
	DataControllerMap             m_DcMap;
	bool                          m_cfgColFirst;
	bool                          m_IsCancelling = false;
	TIC_CALL static WeakPtr<SessionData>   s_CurrSD;
};


//----------------------------------------------------------------------
// helper func
//----------------------------------------------------------------------

const TreeItem* GetCacheRoot(const TreeItem* subItem);


#endif // __TIC_SESSIONDATA_H
