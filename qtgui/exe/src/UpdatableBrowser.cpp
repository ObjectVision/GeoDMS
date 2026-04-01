#include "UpdatableBrowser.h"
#include <QContextMenuEvent>
#include <QDialog>
#include <QLayout>
#include <qfontdatabase.h>

#include "TicInterface.h"

// Forward declaration of TreeItem_FindItem from TreeItem.cpp
TIC_CALL SharedTreeItem TreeItem_FindItem(const TreeItem* searchLoc, TokenID id);

FindTextWindow::FindTextWindow(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlag(Qt::Window, true);
    setFixedSize(300, 200);

    auto* layout = new QVBoxLayout(this);
    find_text = new QLineEdit(this);
    match_whole_word = new QCheckBox("Match whole word", this);
    match_case = new QCheckBox("Match case", this);
    previous = new QPushButton("Previous", this);
    next = new QPushButton("Next", this);

    QWidget* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
    spacer->setFocusPolicy(Qt::FocusPolicy::NoFocus);
    result_info = new QLabel("", this);

    // connections
    connect(next, &QPushButton::clicked, this, &FindTextWindow::nextClicked);
    connect(previous, &QPushButton::clicked, this, &FindTextWindow::previousClicked);

    // fill layout
    layout->addWidget(find_text);
    layout->addWidget(match_whole_word);
    layout->addWidget(match_case);
    layout->addWidget(previous);
    layout->addWidget(next);
    layout->addWidget(spacer);
    layout->addWidget(result_info);

    setLayout(layout);
}

void FindTextWindow::findInTreeView()
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
        }
        else
        {
            result_info->setText("Item not found");
        }
    }
    catch (...)
    {
        result_info->setText("Error during search");
    }
}

void FindTextWindow::setSearchMode(bool treeViewMode)
{
    m_treeViewMode = treeViewMode;
    if (treeViewMode)
    {
        match_whole_word->setChecked(false);
        match_case->setChecked(false);
        match_whole_word->setEnabled(false);
        match_case->setEnabled(false);
        previous->setEnabled(false);
    }
    else
    {
        match_whole_word->setEnabled(true);
        match_case->setEnabled(true);
        previous->setEnabled(true);
    }
}

void FindTextWindow::findInQTextBrowser(bool backwards)
{
    auto* updatable_text_browser = dynamic_cast<QTextBrowser*>(parent());
    MG_CHECK(updatable_text_browser);

    // set find flag
    int backwards_flag = backwards ? QTextDocument::FindBackward : 0;
    int match_case_flag = match_case->isChecked() ? QTextDocument::FindCaseSensitively : 0;
    int match_whole_word_flag = match_whole_word->isChecked() ? QTextDocument::FindWholeWords : 0;
    QTextDocument::FindFlags find_flags = static_cast<QTextDocument::FindFlags>(backwards_flag | match_case_flag | match_whole_word_flag);

    auto found = updatable_text_browser->find(find_text->text(), find_flags);
    result_info->setText(found ? "" : "No more matches found");
}

void FindTextWindow::findInText(bool backwards)
{
    if (find_text->text().isEmpty())
        return;

    if (m_treeViewMode)
    {
        findInTreeView();
    }
    else
    {
        auto* current_parent = parent();
        auto* updatable_text_browser = dynamic_cast<QTextBrowser*>(current_parent);
        MG_CHECK(updatable_text_browser);
        findInQTextBrowser(backwards);
    }
}

void FindTextWindow::nextClicked(bool checked)
{
    findInText(false);
}

void FindTextWindow::previousClicked(bool checked)
{
    findInText(true);
}

void QUpdatableWebBrowser::onAnchorClicked(const QUrl& url) {
    MainWindow::TheOne()->onInternalLinkClick(url, dynamic_cast<QWidget*>(parent()));
}

QUpdatableWebBrowser::QUpdatableWebBrowser(QWidget* parent)
    : QTextBrowser(parent)
{
    setOpenLinks(false);
    setOpenExternalLinks(false);
    setProperty("DmsHelperWindowType", DmsHelperWindowType::HW_UNKNOWN);

    find_shortcut = new QShortcut(QKeySequence(tr("Ctrl+F", "Find")), this);
    connect(find_shortcut, &QShortcut::activated, this, &QUpdatableWebBrowser::openFindWindow);
    connect(this, &QTextBrowser::anchorClicked, this, &QUpdatableWebBrowser::onAnchorClicked);
}

void QUpdatableWebBrowser::restart_updating()
{
    m_Waiter.start(this);
    QPointer<QUpdatableWebBrowser> self = this;
    QTimer::singleShot(0, MainWindow::TheOne(),
        [self]()
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

void QUpdatableWebBrowser::GenerateDescription()
{
    auto pw = dynamic_cast<QMdiSubWindow*>(parentWidget());
    if (!pw)
        return;
    SetText(SharedStr(pw->windowTitle().toStdString().c_str()));
}

void QUpdatableWebBrowser::contextMenuEvent(QContextMenuEvent* event)
{
    /*if (!context_menu)
    {
		context_menu = new QMenu(this);
        context_menu->addAction("View page source", [this, event]()
            {
                page()->toHtml([this](const QString& result)
                    {
                        auto* page_source_dialog = new QDialog(this);
                        auto* layout = new QVBoxLayout(page_source_dialog);
                        auto* page_source_widget = new QTextEdit(page_source_dialog);
                        layout->addWidget(page_source_widget);
                        page_source_widget->setPlainText(result);
                        page_source_widget->setWindowModality(Qt::ApplicationModal);
                        page_source_widget->setAttribute(Qt::WA_DeleteOnClose);
                        page_source_dialog->show();
                    });

            }
        );
	}

	context_menu->exec(event->globalPos());*/
}

void QUpdatableWebBrowser::openFindWindow()
{
    if (!find_window)
    {
        find_window = new FindTextWindow(this);
    }

    // update title
    DmsHelperWindowType helper_window_type = static_cast<DmsHelperWindowType>(property("DmsHelperWindowType").value<QVariant>().toInt());

    switch (helper_window_type)
    {
    case DmsHelperWindowType::HW_DETAILPAGES: find_window->setWindowTitle("Find in Detail pages"); break;
    case DmsHelperWindowType::HW_STATISTICS: find_window->setWindowTitle("Find in Statistics"); break;
    case DmsHelperWindowType::HW_VALUEINFO: find_window->setWindowTitle("Find in Value info"); break;
    default: find_window->setWindowTitle("Find in.."); break;
    }

    find_window->show();
}

QUpdatableTextBrowser::QUpdatableTextBrowser(QWidget* parent)
    : QTextBrowser(parent)
{
    setOpenLinks(false);
    setOpenExternalLinks(false);
    setProperty("DmsHelperWindowType", DmsHelperWindowType::HW_UNKNOWN);

    find_shortcut = new QShortcut(QKeySequence(tr("Ctrl+F", "Find")), this);
    connect(find_shortcut, &QShortcut::activated, this, &QUpdatableTextBrowser::openFindWindow);
}

void QUpdatableTextBrowser::restart_updating()
{
    m_Waiter.start(this);
    QPointer<QUpdatableTextBrowser> self = this;
    QTimer::singleShot(0, MainWindow::TheOne(),
        [self]()
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

void QUpdatableTextBrowser::GenerateDescription()
{
    auto pw = dynamic_cast<QMdiSubWindow*>(parentWidget());
    if (!pw)
        return;
    SetText(SharedStr(pw->windowTitle().toStdString().c_str()));
}

void QUpdatableTextBrowser::openFindWindow()
{
    if (!find_window)
    {
        find_window = new FindTextWindow(this);
    }

    // update title
    DmsHelperWindowType helper_window_type = static_cast<DmsHelperWindowType>(property("DmsHelperWindowType").value<QVariant>().toInt());

    switch (helper_window_type)
    {
    case DmsHelperWindowType::HW_DETAILPAGES: find_window->setWindowTitle("Find in Detail pages"); break;
    case DmsHelperWindowType::HW_STATISTICS: find_window->setWindowTitle("Find in Statistics"); break;
    case DmsHelperWindowType::HW_VALUEINFO: find_window->setWindowTitle("Find in Value info"); break;
    default: find_window->setWindowTitle("Find in.."); break;
    }

    find_window->show();
}