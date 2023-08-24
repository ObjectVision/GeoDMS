// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

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
        QTimer::singleShot(0, [this]()
            {
                if (!this->update())
                    this->restart_updating();
                else
                    this->m_Waiter.end();
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

