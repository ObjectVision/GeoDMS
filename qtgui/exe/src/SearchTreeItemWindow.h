// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(DMS_QT_SEARCH_TREEITEM_WINDOW_H)
#define DMS_QT_SEARCH_TREEITEM_WINDOW_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QKeyEvent>

class SearchTreeItemWindow : public QWidget
{
    Q_OBJECT

public:
    SearchTreeItemWindow(QWidget* parent);

protected:
    void keyPressEvent(QKeyEvent* event) override;

public slots:
    void nextClicked(bool checked = false);
    void findInTreeView(bool closeOnSuccess = false);
    void onFindTextChanged(const QString& text);

public:
    QLineEdit* find_text = nullptr;
    QPushButton* next = nullptr;
    QLabel* result_info = nullptr;
};

#endif //!defined(DMS_QT_SEARCH_TREEITEM_WINDOW_H)
