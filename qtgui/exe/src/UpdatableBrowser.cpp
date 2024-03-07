#include "UpdatableBrowser.h"

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

void FindTextWindow::findInText(bool backwards)
{
    if (find_text->text().isEmpty())
        return;
    auto* updatable_text_browser = dynamic_cast<QUpdatableTextBrowser*>(parent());
    if (!updatable_text_browser)
        return;

    // set find flag
    int backwards_flag = backwards ? QTextDocument::FindBackward : 0;
    int match_case_flag = match_case->isChecked() ? QTextDocument::FindCaseSensitively : 0;
    int match_whole_word_flag = match_whole_word->isChecked() ? QTextDocument::FindWholeWords : 0;
    QTextDocument::FindFlags find_flags = static_cast<QTextDocument::FindFlags>(backwards_flag | match_case_flag | match_whole_word_flag);

    // TODO: implement find
    //auto found = updatable_text_browser->find(find_text->text(), find_flags);
    //result_info->setText(found ? "" : "No more matches found");
}

void FindTextWindow::nextClicked(bool checked)
{
    findInText();
}

void FindTextWindow::previousClicked(bool checked)
{
    findInText(true);
}

QUpdatableTextBrowser::QUpdatableTextBrowser(QWidget* parent)
    : QWebEngineView(parent)
{

    connect(pageAction(QWebEnginePage::ViewSource), SIGNAL(triggered(bool)), this, SLOT(slt_openImage_triggered()));

    /*setOpenLinks(false);
    setOpenExternalLinks(false);*/
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