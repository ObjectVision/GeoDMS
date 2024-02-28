// Copyright (C) 1998-2023 Object Vision b.v. 
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

struct StudyObject
{
    SharedDataItemInterestPtr study_object;
    SizeT                     index;
    SharedStr                 extra_info;

    Explain::context_handle   explain_context;
};

class StudyObjectHistory
{
public:
    StudyObjectHistory();
    ~StudyObjectHistory();

    auto currentContext() -> Explain::CalcExplImpl* const;
    auto currentStudyObject() -> SharedDataItemInterestPtr const;
    auto currentIndex() -> Int64 const;
    auto currentExtraInfo() -> SharedStr const;

    bool previous();
    bool next();
    void insert(SharedDataItemInterestPtr studyObject, SizeT index, SharedStr extra_info);
    auto nrPreviousStudyObjects() -> SizeT const;
    auto nrNextStudyObjects() -> SizeT const;
    void deleteAfterCurrentIndex();

    void ClearGarbage();
private:
    std::vector<StudyObject> study_objects;

    Int64 current_index = -1; // current index in history
    std::vector<Explain::context_handle> garbage;
};

class ValueInfoWindow : public QWidget
{
public:
    ValueInfoWindow(SharedDataItemInterestPtr studyObject, SizeT index, SharedStr extraInfo, QWidget* parent = nullptr);
    ValueInfoBrowser* m_browser = nullptr;
};

struct ValueInfoBrowser : QUpdatableTextBrowser
{
    ValueInfoBrowser(QWidget* parent, SharedDataItemInterestPtr studyObject, SizeT index, SharedStr extraInfo, QWidget* window);
    bool update() override;
    void updateNavigationButtons();
    void updateWindowTitle();

public slots:
    void nextStudyObject();
    void previousStudyObject();
    void addStudyObject(SharedDataItemInterestPtr studyObject, SizeT index, SharedStr extraInfo);
    void onAnchorClicked(const QUrl& link);

public:
    StudyObjectHistory m_history;
    std::unique_ptr<QPushButton> back_button;
    std::unique_ptr<QPushButton> forward_button;
    QWidget* value_info_window = nullptr;
};

#endif //!defined(DMS_QT_VALUE_INFO_H)
