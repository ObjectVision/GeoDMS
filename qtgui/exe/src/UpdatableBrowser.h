// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(DMS_QT_UPDATABLE_BROWSER_H)
#define DMS_QT_UPDATABLE_BROWSER_H

#include <QTextBrowser.h>
#include <QTimer.h>
#include <QMdiSubWindow.h>

#include "dbg/DebugContext.h"

#include "waiter.h"

struct QUpdatableTextBrowser : QTextBrowser, MsgGenerator
{
    using QTextBrowser::QTextBrowser;

    void restart_updating()
    {
        m_Waiter.start(this);
        QPointer<QUpdatableTextBrowser> self = this;
        QTimer::singleShot(0, [self]()
            {
                if (self)
                {
                    if (!self->update())
                        self->restart_updating();
                    else
                        self->m_Waiter.end();
                }
            }
        );
    }
    void GenerateDescription() override
    {
        auto pw = dynamic_cast<QMdiSubWindow*>(parentWidget());
        if (!pw)
            return;
        SetText(SharedStr(pw->windowTitle().toStdString().c_str()));
    }

protected:
    Waiter m_Waiter;

    virtual bool update() = 0;
};

#endif //!defined(DMS_QT_UPDATABLE_BROWSER_H)
