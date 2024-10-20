#include "UpdatableBrowser.h"
#include <QContextMenuEvent>
#include <QDialog>
#include <QLayout>
#include <QWebEngineSettings>
#include <qfontdatabase.h>

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

void FindTextWindow::findInQTextBrowser(bool backwards)
{
    auto* updatable_text_browser = dynamic_cast<QUpdatableTextBrowser*>(parent());
    assert(updatable_text_browser);

    // set find flag
    int backwards_flag = backwards ? QTextDocument::FindBackward : 0;
    int match_case_flag = match_case->isChecked() ? QTextDocument::FindCaseSensitively : 0;
    int match_whole_word_flag = match_whole_word->isChecked() ? QTextDocument::FindWholeWords : 0;
    QTextDocument::FindFlags find_flags = static_cast<QTextDocument::FindFlags>(backwards_flag | match_case_flag | match_whole_word_flag);

    auto found = updatable_text_browser->find(find_text->text(), find_flags);
    result_info->setText(found ? "" : "No more matches found");
}

void FindTextWindow::findInQWebEnginePage(bool backwards)
{
    auto* updatable_web_browser = dynamic_cast<QUpdatableWebBrowser*>(parent());
    assert(updatable_web_browser);
    int backwards_flag = backwards ? QWebEnginePage::FindBackward : 0;
    int match_case_flag = match_case->isChecked() ? QWebEnginePage::FindCaseSensitively : 0;
    QWebEnginePage::FindFlags find_flags = static_cast<QWebEnginePage::FindFlags>(backwards_flag | match_case_flag);

    updatable_web_browser->findText(find_text->text(), find_flags);
    // TODO: implement callback in findText to feed result_info
    //result_info->setText(found ? "" : "No more matches found");
}

void FindTextWindow::findInText(bool backwards)
{
    if (find_text->text().isEmpty())
        return;
 
    auto* current_parent = parent();
    auto* updatable_text_browser = dynamic_cast<QUpdatableTextBrowser*>(current_parent);
    if (updatable_text_browser)
    {
        findInQTextBrowser(backwards);
        return;
    }

    auto* updatable_web_browser = dynamic_cast<QUpdatableWebBrowser*>(current_parent);
    if (updatable_web_browser)
        findInQWebEnginePage(backwards);

    return;
}

void FindTextWindow::nextClicked(bool checked)
{
    findInText();
}

void FindTextWindow::previousClicked(bool checked)
{
    findInText(true);
}


DmsWebEnginePage::DmsWebEnginePage(QObject* parent)
	: QWebEnginePage(parent)
{
}

bool DmsWebEnginePage::acceptNavigationRequest(const QUrl& url, QWebEnginePage::NavigationType type, bool isMainFrame)
{
    if (type == QWebEnginePage::NavigationType::NavigationTypeLinkClicked)
    {
        MainWindow::TheOne()->onInternalLinkClick(url, dynamic_cast<QWidget*>(parent()));
        return false;
    }
    return true;
}

QUpdatableWebBrowser::QUpdatableWebBrowser(QWidget* parent)
    : QWebEngineView(parent)
{
    current_page = new DmsWebEnginePage(this);
    setPage(current_page);

    // settings
    auto settings = page()->settings();
    QString family = QFontDatabase::applicationFontFamilies(1).at(0);
    settings->setAttribute(QWebEngineSettings::JavascriptEnabled, false);
    settings->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, false);
    settings->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard, false);
    settings->setAttribute(QWebEngineSettings::LinksIncludedInFocusChain, false);
    settings->setAttribute(QWebEngineSettings::JavascriptCanPaste, false);
    settings->setFontFamily(QWebEngineSettings::StandardFont, family);
    settings->setFontSize(QWebEngineSettings::DefaultFontSize, 13);

    connect(pageAction(QWebEnginePage::ViewSource), SIGNAL(triggered(bool)), this, SLOT(slt_openImage_triggered()));

    setProperty("DmsHelperWindowType", DmsHelperWindowType::HW_UNKNOWN);

    find_shortcut = new QShortcut(QKeySequence(tr("Ctrl+F", "Find")), this);
    connect(find_shortcut, &QShortcut::activated, this, &QUpdatableWebBrowser::openFindWindow);

    setFocusPolicy(Qt::FocusPolicy::ClickFocus);
}

void QUpdatableWebBrowser::restart_updating()
{
    m_Waiter.start(this);
    QPointer<QUpdatableWebBrowser> self = this;
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

void QUpdatableWebBrowser::GenerateDescription()
{
    auto pw = dynamic_cast<QMdiSubWindow*>(parentWidget());
    if (!pw)
        return;
    SetText(SharedStr(pw->windowTitle().toStdString().c_str()));
}

//const std::function<void(const QString&)>& resultCallback

void test_callback(const QString& string)
{

}

//resultCallback = 

void QUpdatableWebBrowser::contextMenuEvent(QContextMenuEvent* event)
{
    if (!context_menu)
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

	context_menu->exec(event->globalPos());
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