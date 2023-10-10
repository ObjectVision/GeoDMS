// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(DMS_QT_VALUE_INFO_H)
#define DMS_QT_VALUE_INFO_H

#include "TicBase.h"
#include "AbstrDataItem.h"
#include "Explain.h"

#include <QList>

#include "UpdatableBrowser.h"

class StudyObjectHistory
{
public:
    StudyObjectHistory();
    ~StudyObjectHistory();

    auto currentContext() -> Explain::CalcExplImpl*;
    auto currentStudyObject() -> SharedDataItemInterestPtr;
    auto currentIndex() -> Int64;

    bool previous();
    bool next();
    void insert(SharedDataItemInterestPtr studyObject, SizeT index);
    auto countPrevious() -> SizeT;
    auto countNext() -> SizeT;
    void deleteFromCurrentIndexUpToEnd();

private:
    std::vector<SharedDataItemInterestPtr> study_objects;
    std::vector<Explain::context_handle>   explain_contexts;
    std::vector<SizeT>                     indices;
    Int64                                  current_index = -1; // current index in history

    QList<Explain::context_handle>   deleted_contexts; // efficient continuation for inserted context handles with overlapping interest
};

struct ValueInfoBrowser : QUpdatableTextBrowser
{
    ValueInfoBrowser(QWidget* parent, SharedDataItemInterestPtr studyObject, SizeT index);
    bool update() override;

public slots:
    void nextStudyObject();
    void previousStudyObject();
    void addStudyObject(SharedDataItemInterestPtr studyObject, SizeT index);
    void onAnchorClicked(const QUrl& link);

    StudyObjectHistory m_history;
    //Explain::context_handle m_Context;

    // scenario: history -> back -> back -> click new branch. Here temporarily store history before deletion in light of overlap
    //SharedDataItemInterestPtr m_StudyObject;
    //SizeT m_Index;
};

#endif //!defined(DMS_QT_VALUE_INFO_H)
