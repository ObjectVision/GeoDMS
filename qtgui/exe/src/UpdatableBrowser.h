// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(DMS_QT_UPDATABLE_BROWSER_H)
#define DMS_QT_UPDATABLE_BROWSER_H

#include <QTextBrowser.h>
#include <QTimer.h>
#include <QMdiSubWindow.h>
#include <QShortCut>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QFocusEvent>
#include "qtextdocument.h"

#include "DmsMainWindow.h"

#include "dbg/DebugContext.h"
#include "act/waiter.h"

class FindTextWindow : public QWidget
{

public:
    FindTextWindow(QWidget* parent);
    void findInText(bool backwards = false);
    void setSearchMode(bool treeViewMode);

public slots:
    void nextClicked(bool checked = false);
    void previousClicked(bool checked = false);
    void onSourceGroupChanged(QAbstractButton* button, bool checked);

    void findInQTextBrowser(bool backwards = false);
    void findInTreeView();

    QLineEdit* find_text = nullptr;

    QCheckBox* match_whole_word = nullptr;
    QCheckBox* match_case = nullptr;
    QPushButton* previous = nullptr;
    QPushButton* next = nullptr;
    QLabel* result_info = nullptr;

private:
    bool m_treeViewMode = false;
};

struct QUpdatableBrowser : QTextBrowser, MsgGenerator
{
    QUpdatableBrowser(QWidget* parent, bool handleAnchors = false);
    void restart_updating();
    void GenerateDescription() override;
    QShortcut* find_shortcut = nullptr;
    FindTextWindow* find_window = nullptr;

public slots:
    void openFindWindow();
    void onAnchorClicked(const QUrl &url);

protected:
    void focusInEvent(QFocusEvent* event) override {
        if (m_handleAnchors)
            event->ignore();
        else
            QTextBrowser::focusInEvent(event);
    }

    void focusOutEvent(QFocusEvent* event) override {
        if (m_handleAnchors)
            event->ignore();
        else
            QTextBrowser::focusOutEvent(event);
    }

    void keyPressEvent(QKeyEvent* e) override {
        if (m_handleAnchors && e->key() == Qt::Key_Tab)
            e->ignore();
        else
            QTextBrowser::keyPressEvent(e);
    }

    Waiter m_Waiter;
    virtual bool update() = 0;

private:
    bool m_handleAnchors = false;
};

#endif //!defined(DMS_QT_UPDATABLE_BROWSER_H)
