// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__MG_TABLECONTROL_H)
#define __MG_TABLECONTROL_H

#include "ptr/WeakPtr.h"

#include "ChangeLock.h"
#include "ExportInfo.h"
#include "MovableContainer.h"
#include "TextEditController.h"

struct ThemeReadLocks;
struct FocusElemProvider;
enum SortOrder { SO_Ascending, SO_Descending };

//----------------------------------------------------------------------
// class  : SelChangeInvalidator
//----------------------------------------------------------------------

struct SelChangeInvalidatorBase
{
	SelChangeInvalidatorBase(TableControl* tableControl);
	~SelChangeInvalidatorBase();
	void ProcessChange(bool mustSetFocusElemIndex = true);

	CrdRect GetSelRect() const;

private:
	TableControl* m_TableControl;
	CrdRect       m_OldSelRect;
	SelRange      m_OldRowRange;
	SelRange      m_OldColRange;
};

struct SelChangeInvalidator : SelChangeInvalidatorBase
{
	SelChangeInvalidator(TableControl* tableControl);

	void ProcessChange(bool mustSetFocusElemIndex);

private:
	RecursiveChangeLock m_ChangeLock;
};

//----------------------------------------------------------------------
// class  : TableControl
//----------------------------------------------------------------------

class TableControl : public AutoSizeContainer
{
	using base_type = AutoSizeContainer;
public:
	TableControl(MovableObject* owner);
	~TableControl();
	GraphicClassFlags GetGraphicClassFlags() const override { return GraphicClassFlags::ChildCovered; };

	DataItemColumn* GetColumn(gr_elem_index i);
	const DataItemColumn* GetConstColumn(gr_elem_index i) const;
	gr_elem_index FindColumn(const AbstrDataItem* adi);

	void InsertColumn(DataItemColumn* dic);
	void InsertColumnAt(DataItemColumn* dic, SizeT pos);

	//	override event handlers
	bool OnKeyDown(UInt32 virtKey) override;
	void ProcessCollectionChange() override;
	bool OnCommand(ToolButtonID id) override;
	auto OnCommandEnable(ToolButtonID id) const -> CommandStatus override;
	void Sync(TreeItem* viewContext, ShvSyncMode sm) override;
	void DoUpdateView() override;

	void RemoveEntry(MovableObject* g) override;

	SizeT GetActiveRow() const;
	gr_elem_index GetActiveCol() const;

	DataItemColumn* GetActiveDIC();
	const DataItemColumn* GetActiveDIC() const;

	void ShowActiveCell();

	SizeT FirstActiveCol() const;

	void GoTo(SizeT row, gr_elem_index col);

	void GoLeft (bool shift);
	void GoRight(bool shift);
	void GoUp   (bool shift, UInt32 c);
	void GoDn   (bool shift, UInt32 c);
	void GoHome (bool shift, bool firstActiveCol);
	void GoEnd  (bool shift);
	void GoRow(SizeT row, bool mustSetFocusElemIndex);

	void SetRowHeight(UInt16 height);

	void HideSortOptions(bool value = true) { m_State.Set(TCF_HideSortOptions, value);   }
	bool HasSortOptions() const             { return ! m_State.Get(TCF_HideSortOptions); }

	bool ShowSelectedOnlyEnabled() const override;

	SizeT NrRows() const;
	SizeT GetRecNo(SizeT i) const;
	SizeT GetRowNr(SizeT i) const;
	ExportInfo GetExportInfo();

	SizeT nrRows() const;
	SizeT getRecNo(SizeT i) const;
	SizeT getRowNr(SizeT i) const;

	bool  InSelRange(SizeT row, gr_elem_index col) const { return m_Cols.IsInRange(col) && m_Rows.IsInRange(row); }
	void  Export();
	void  TableCopy() const;
	void  SelectAllRows(bool v);
	void  SelectRows();
	void  GoToFirstSelected();
	void  OnFocusElemChanged(SizeT newFE, SizeT oldFE);

	bool  CanContain(const TreeItem* viewCandidate) const;
	void  AddLayer  (const TreeItem* viewCandidate, bool isDropped);
	void  AddIdColumn(const AbstrUnit* domain, const AbstrDataItem* exampleAttr);
	auto  CreateIndex(const AbstrDataItem* attr)->FutureData;
	void  CreateTableIndex(DataItemColumn* dic, SortOrder so);
	void  CreateTableGroupBy(bool activate);

	SharedStr GetCaption() const;
	const AbstrUnit*     GetEntity()    const { return m_Entity;    }
	const AbstrDataItem* GetIndexAttr() const { return m_IndexAttr; }
	const AbstrUnit*     GetRowEntity() const;

//	insert / delete rows
	bool CheckCardinalityChangeRights(bool doThrow);

//	override Actor mfuncs
	ActorVisitState VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const override;
	bool MouseEvent(MouseEventDispatcher& med) override;

protected: 
	friend class  DataItemColumn; 
	friend struct SelChangeInvalidatorBase;
	friend class  TableHeaderControl;
	friend class  TableViewControl;
	friend class  EditPaletteControl;

	void SaveTo(OutStreamBuff* buffPtr) const;
	void NotifyCaptionChange();
	void NotifyRowColChange();
	void ProcessDIC(DataItemColumn* dic);
	void UpdateActiveCell(bool mustSetFocusElemIndex);
	void SetEntity(const AbstrUnit* newDomain);
	void SyncIndex      (ShvSyncMode sm);
	void SyncGroupBy    (ShvSyncMode sm);
	void UpdateShowSelOnly() override;

	SharedPtr<AbstrDataItem> CreateIdAttr(const AbstrUnit* domain, const AbstrDataItem* exampleAttr);

private:
	void UpdateTableIndex();

	SharedUnitInterestPtr        m_Entity, m_SelEntity, m_GroupByEntity;
	FutureData                   m_fd_Index;
	SharedDataItemInterestPtr    m_IndexAttr, m_SelIndexAttr, m_GroupByRel, m_LabelAttr;
	SharedPtr<FocusElemProvider> m_FocusElemProvider;
	WeakPtr<TableViewControl>    m_TableView;
	std::weak_ptr<const DataItemColumn> m_IndexColumn;

	SelRange m_Rows;
	SelRange m_Cols;

	CmdSignal  m_cmdOnCaptionChange;

	DECL_RTTI(SHV_CALL, Class);
};


#endif // __MG_TABLECONTROL_H
