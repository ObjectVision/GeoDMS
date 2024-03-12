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


// override bool QWebEnginePage::acceptNavigationRequest(const QUrl &url, QWebEnginePage::NavigationType type, bool isMainFrame)
// implement custompage
// custom page
// setPage(QWebEnginePage *page)
// custom page should be child of QWebEngineView
struct QUpdatableTextBrowser : QWebEngineView, MsgGenerator // QTextBrowser // TODO: rename
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
    DmsWebEnginePage* page = nullptr;
    QShortcut* find_shortcut = nullptr;
    FindTextWindow* find_window = nullptr;
};

#endif //!defined(DMS_QT_UPDATABLE_BROWSER_H)
