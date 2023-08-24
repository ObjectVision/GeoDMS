// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "TicBase.h"
#include "AbstrDataItem.h"
#include "Explain.h"

#include "UpdatableBrowser.h"

struct ValueInfoBrowser : QUpdatableTextBrowser
{
    ValueInfoBrowser(QWidget* parent, SharedDataItemInterestPtr studyObject, SizeT index);

    bool update() override;

    Explain::context_handle m_Context;
    SharedDataItemInterestPtr m_StudyObject;
    SizeT m_Index;
};

