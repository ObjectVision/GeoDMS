// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "FindTreeItemWindow.h"

#include <QVBoxLayout>

#include "dbg/DmsCatch.h"
#include "TicInterface.h"
#include "DmsMainWindow.h"

// Forward declaration of TreeItem_FindItem from TreeItem.cpp
TIC_CALL SharedTreeItem TreeItem_FindItem(const TreeItem* searchLoc, TokenID id);

FindTreeItemWindow::FindTreeItemWindow(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlag(Qt::Window, true);
    setMinimumSize(300, 120);
    resize(350, 150);
    setWindowTitle("Find TreeItem");

    auto* layout = new QVBoxLayout(this);
    find_text = new QLineEdit(this);
    next = new QPushButton("Find", this);

    QWidget* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
    spacer->setFocusPolicy(Qt::FocusPolicy::NoFocus);
    result_info = new QLabel("", this);

    // connections
    connect(next, &QPushButton::clicked, this, &FindTreeItemWindow::nextClicked);
    connect(find_text, &QLineEdit::returnPressed, this, &FindTreeItemWindow::findInTreeView);
    connect(find_text, &QLineEdit::textChanged, this, &FindTreeItemWindow::onFindTextChanged);

    // fill layout
    layout->addWidget(find_text);
    layout->addWidget(next);
    layout->addWidget(spacer);
    layout->addWidget(result_info);

    setLayout(layout);
}

void FindTreeItemWindow::findInTreeView()
{
    auto search_text = find_text->text();
    if (search_text.isEmpty())
    {
        result_info->setText("Please enter search text");
        return;
    }

    auto main_window = MainWindow::TheOne();
    if (!main_window)
    {
        result_info->setText("Main window not available");
        return;
    }

    auto current_item = main_window->getCurrentTreeItem();
    if (!current_item)
    {
        result_info->setText("No current item");
        return;
    }

    try {
        TokenID search_token = GetTokenID_mt(search_text.toUtf8().constData());
        auto found_item = TreeItem_FindItem(current_item, search_token);

        if (found_item)
        {
            main_window->setCurrentTreeItem(const_cast<TreeItem*>(found_item.get()));
            result_info->setText(QString("Found: %1").arg(found_item->GetFullName().c_str()));
            onFindTextChanged(search_text);
        }
        else
        {
            result_info->setText("Item not found");
        }
    }
    catch (...)
    {
        auto x = catchException(false);
        result_info->setText(x->GetAsText().c_str());
    }
}

void FindTreeItemWindow::nextClicked(bool checked)
{
    findInTreeView();
}

void FindTreeItemWindow::onFindTextChanged(const QString& text)
{
    auto main_window = MainWindow::TheOne();
    if (!main_window)
        return;

    auto current_item = main_window->getCurrentTreeItem();
    if (!current_item)
        return;

    auto current_id = current_item->GetID();
    if (text.compare(current_id.GetStr().c_str(), Qt::CaseInsensitive) == 0)
        next->setText("Find next");
    else
        next->setText("Find");
}

void FindTreeItemWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape)
        close();
    else
        QWidget::keyPressEvent(event);
}
