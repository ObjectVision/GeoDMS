// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(DMS_QT_STATISTICS_BROWSER_H)
#define DMS_QT_STATISTICS_BROWSER_H

#include "UpdatableBrowser.h"
#include "DmsMainWindow.h"

struct StatisticsBrowser : QUpdatableTextBrowser
{
    //using QUpdatableTextBrowser::QUpdatableTextBrowser;

    StatisticsBrowser(QWidget* parent = nullptr)
        : QUpdatableTextBrowser(parent)
    {
        setProperty("DmsHelperWindowType", DmsHelperWindowType::HW_STATISTICS);
        //TODO: implement onAnchorClicked for qwebengineview
        //connect(this, &StatisticsBrowser::anchorClicked, this, &StatisticsBrowser::onAnchorClicked);
    }

    bool update() override
    {
        vos_buffer_type textBuffer;
        SuspendTrigger::Resume();
        bool done = NumericDataItem_GetStatistics(m_Context, textBuffer);
        textBuffer.emplace_back(char(0));
        setHtml(begin_ptr(textBuffer));
        return done;
    }

    public slots:
    void onAnchorClicked(const QUrl& link)
    {
        MainWindow::TheOne()->onInternalLinkClick(link);
    }

    SharedTreeItemInterestPtr m_Context;
};

#endif //!defined(DMS_QT_STATISTICS_BROWSER_H)
