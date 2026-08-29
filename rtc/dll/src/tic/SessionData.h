// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__TIC_SESSIONDATA_H)
#define __TIC_SESSIONDATA_H

#include "ptr/OwningPtr.h"
#include "ptr/WeakPtr.h"

#include "DataController.h"
struct TreeItem;

enum class supplier_level
{
	none = 0,
	meta = 1,
	calc = 2,

	usage_flags = calc+meta,

	uses_source_flag = 4, // flagged if the dedicated source is a supplier
	calc_source = calc + uses_source_flag,
	meta_source = meta + uses_source_flag,
};

TIC_CALL supplier_level TreeItem_GetSupplierLevel(const TreeItem* ti);
TIC_CALL void TreeItem_SetAnalysisTarget(const TreeItem* ti, bool mustClean);
TIC_CALL void TreeItem_SetAnalysisSource(const TreeItem* ti);

//----------------------------------------------------------------------
// struct SessionData
//----------------------------------------------------------------------

extern leveled_counted_section s_SessionUsageCounter;

struct SessionData : std::enable_shared_from_this<SessionData>
{
	static TIC_CALL std::shared_ptr<SessionData> Create(CharPtr configLoadDir, CharPtr configSubDir); // call this before reading a config in order to set cfgColFirst right

	TIC_CALL void Open  (const TreeItem* configRoot);       // call this after reading a config to take ownership of the configRoot, stamp the load TimeStamp and create the hidden ConfigSettings child

	void ActivateThis();

	static void activateIt(const TreeItem* configRoot); // for now, assume session to be a singleton
	TIC_CALL static std::shared_ptr<SessionData> GetIt(const TreeItem* configRoot);
	static std::shared_ptr<SessionData> getIt(const TreeItem* configRoot);
	static void ReleaseIt(const TreeItem* configRoot); // WARNING: this might point to a destroyed configRoot
	static std::weak_ptr<SessionData> GetItWeak(const TreeItem* configRoot) { return GetIt(configRoot); }

	bool IsCancelling() const { return m_IsCancelling;  }
	// Mark the session as cancelling WITHOUT deactivating it (keeps Curr() valid so in-flight workers
	// can still observe IsCancelling() and cancel gracefully). Used at config teardown to drain workers.
	// Also raises the lock-free IsSessionTearingDown() flag; see there.
	void SetCancelling();

	// Null-safe form of the above, for callers that run with or without a current session.
	static bool IsCurrCancelling() { auto curr = Curr(); return curr && curr->IsCancelling(); }

	TIC_CALL static std::shared_ptr<SessionData> Curr();
	TIC_CALL static void ReleaseCurr();

	void release();

	SharedTreeItem  GetConfigRoot   () const { return m_ConfigRoot; } 
	WeakStr         GetConfigLoadDir() const { return m_ConfigLoadDir; }
	SharedStr       GetConfigDir    () const;
	bool   IsConfigDirty   () const;

	SharedStr       GetConfigIniFile() const;
	static SharedStr GetConfigIniFile(CharPtr configDir);

	SharedTreeItem GetConfigSettings() const;
	SharedTreeItem GetConfigSettings(CharPtr section, CharPtr key) const;

	         SharedStr ReadConfigString(CharPtr section, CharPtr key, CharPtr defaultValue) const;
	Int32     ReadConfigValue (CharPtr section, CharPtr key, Int32   defaultValue) const;

	const TreeItem* GetContainer(const TreeItem* context, CharPtr name) const;
	const TreeItem* GetClassificationContainer(const TreeItem* context) const;

public:

	SessionData(CharPtr configLoadDir, CharPtr configSubDir);
	~SessionData();
private:
	SessionData(const SessionData&) = delete;

	void deactivateThis();

	SharedTreeItem                m_ConfigRoot, m_ConfigSettings; // std::shared_ptr owners (SessionData owns the config root)
	SharedStr                     m_ConfigLoadDir;
	SharedStr                     m_ConfigSubDir;
	SharedStr                     m_ConfigDir;
	TimeStamp                     m_ConfigLoadTS;
	bool                          m_IsCancelling = false;
};


//----------------------------------------------------------------------
// session cancellation
//----------------------------------------------------------------------
// These used to live in namespace DSM, the last remnant of the DataStoreManager that owned the
// retired CalcCache. Nothing about them concerns a data store: they are the session's cancellation
// surface, so they belong beside SessionData.

// True from the moment a configuration starts closing down until the next one is activated, and
// readable WITHOUT taking a lock. Any code that can run while the config tree is being destroyed must
// ask this rather than SessionData::IsCurrCancelling(), for two independent reasons:
//   - Curr() takes sd_SessionDataCriticalSection, and the tree is destroyed from INSIDE
//     SessionData::ReleaseIt while that non-recursive mutex is held, so a Curr() call made from the
//     destructor cascade would self-deadlock;
//   - by then Curr() is already null (deactivateThis clears s_CurrSD before the cascade runs), so
//     IsCurrCancelling() answers false during exactly the window this is meant to detect.
// It starts out false, so a process that never opens a session (the stg/tst drivers, unit tests)
// behaves as it always did.
bool IsSessionTearingDown();

// Cancel the current chore if it lost its interest, or if the session or the enclosing
// CancelableFrame is cancelling. Does nothing on the meta thread outside a CancelableFrame.
TIC_CALL void CancelIfOutOfInterest(const TreeItem* item = nullptr);

// Throw task_canceled when a CancelableFrame is active; otherwise report a failed precondition
// against item, since being asked to cancel outside a cancelable context is a programming error.
[[noreturn]] void CancelOrThrow(const TreeItem* item);

//----------------------------------------------------------------------
// helper func
//----------------------------------------------------------------------

const TreeItem* GetCacheRoot(const TreeItem* subItem);


#endif // __TIC_SESSIONDATA_H
