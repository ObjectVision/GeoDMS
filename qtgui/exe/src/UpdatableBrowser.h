// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(DMS_QT_UPDATABLE_BROWSER_H)
#define DMS_QT_UPDATABLE_BROWSER_H

//#include <QTextBrowser.h>
#include <QWebEngineView>

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
#include "waiter.h"

class FindTextWindow : public QWidget
{
public:
    FindTextWindow(QWidget* parent);
    void findInText(bool backwards = false);

public slots:
    void nextClicked(bool checked = false);
    void previousClicked(bool checked = false);

    void findInQTextBrowser(bool backwards = false);
    void findInQWebEnginePage(bool backwards = false);

    QLineEdit* find_text = nullptr;
    QCheckBox* match_whole_word = nullptr;
    QCheckBox* match_case = nullptr;
    QPushButton* previous = nullptr;
    QPushButton* next = nullptr;
    QLabel* result_info = nullptr;
};

struct DmsWebEnginePage : QWebEnginePage
{
    DmsWebEnginePage(QObject* parent = nullptr);
	bool acceptNavigationRequest(const QUrl &url, QWebEnginePage::NavigationType type, bool isMainFrame) override;
};

struct QUpdatableWebBrowser : QWebEngineView, MsgGenerator
{
    QUpdatableWebBrowser(QWidget* parent);
    void restart_updating();
    void GenerateDescription() override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    QShortcut* find_shortcut = nullptr;
    FindTextWindow* find_window = nullptr;

public slots:
    void openFindWindow();

protected:
    void focusInEvent(QFocusEvent* event) override {
        event->ignore();
    }

    void focusOutEvent(QFocusEvent* event) override {
        event->ignore();
    }

    void keyPressEvent(QKeyEvent* e) override {
        if (e->key() == Qt::Key_Tab)
            e->ignore();
    }

    Waiter m_Waiter;
    virtual bool update() = 0;

private:
    DmsWebEnginePage* current_page = nullptr;
    QMenu* context_menu = nullptr;
};

struct QUpdatableTextBrowser : QTextBrowser, MsgGenerator
{
    QUpdatableTextBrowser(QWidget* parent);
    void restart_updating();
    void GenerateDescription() override;

public slots:
    void openFindWindow();


protected:
    Waiter m_Waiter;
    virtual bool update() = 0;

private:
    QShortcut* find_shortcut = nullptr;
    FindTextWindow* find_window = nullptr;
};

#endif //!defined(DMS_QT_UPDATABLE_BROWSER_H)
