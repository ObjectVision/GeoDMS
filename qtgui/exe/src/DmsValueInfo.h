// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(DMS_QT_VALUE_INFO_H)
#define DMS_QT_VALUE_INFO_H

#include "TicBase.h"
#include "AbstrDataItem.h"
#include "Explain.h"

#include "UpdatableBrowser.h"

class ValueInfoPanel : public QMdiSubWindow
{
public:
    ValueInfoPanel(QWidget* parent);
    ~ValueInfoPanel();
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
};

struct ValueInfoBrowser : QUpdatableTextBrowser
{
    ValueInfoBrowser(QWidget* parent, SharedDataItemInterestPtr studyObject, SizeT index);

    bool update() override;

    Explain::context_handle m_Context;
    SharedDataItemInterestPtr m_StudyObject;
    SizeT m_Index;
};

#endif //!defined(DMS_QT_VALUE_INFO_H)
