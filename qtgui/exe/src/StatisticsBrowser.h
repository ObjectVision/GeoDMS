// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(DMS_QT_STATISTICS_BROWSER_H)
#define DMS_QT_STATISTICS_BROWSER_H

#include "UpdatableBrowser.h"

struct StatisticsBrowser : QUpdatableTextBrowser
{
    using QUpdatableTextBrowser::QUpdatableTextBrowser;

    bool update() override
    {
        vos_buffer_type textBuffer;
        SuspendTrigger::Resume();
        bool done = NumericDataItem_GetStatistics(m_Context, textBuffer);
        textBuffer.emplace_back(char(0));
        setText(begin_ptr(textBuffer));
        return done;
    }
    SharedTreeItemInterestPtr m_Context;
};

#endif //!defined(DMS_QT_STATISTICS_BROWSER_H)
