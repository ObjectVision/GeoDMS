

// The small single-widget windows of the Qt GUI, merged (2026-08):
// DmsAddressBar, DmsErrorWindow, DmsGuiParameters, DmsSplashScreen,
// DmsFileChangedWindow, SearchTreeItemWindow, UpdatableBrowser.

// ==== DmsAddressBar ====
#include "DmsAddressBar.h"
#include "DmsMainWindow.h"
#include "DmsTreeView.h"
#include "TicInterface.h"

#include <QApplication>
#include <QValidator>
#include <QRegularExpression>


DmsAddressBar::DmsAddressBar(QWidget* parent)
    : QLineEdit(parent) {
    setFont(QApplication::font());
    QRegularExpression rx("^[^0-9=+\\-|&!?><,.{}();\\]\\[][^=+\\-|&!?><,.{}();\\]\\[]+$");
    auto rx_validator = new QRegularExpressionValidator(rx, this);
    setValidator(rx_validator);
    setDmsCompleter();
}

void DmsAddressBar::setDmsCompleter() {
    auto dms_model = MainWindow::TheOne()->m_dms_model.get();
    TreeModelCompleter* completer = new TreeModelCompleter(this);
    completer->setModel(dms_model);
    completer->setSeparator("/");
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    setCompleter(completer);
}

void DmsAddressBar::setPath(CharPtr itemPath) {
    setText(itemPath);
    onEditingFinished();
}

void DmsAddressBar::findItem(const TreeItem* context, QString path, bool updateHistory) {
    if (!context)
        return;

    auto best_item_ref = TreeItem_GetBestItemAndUnfoundPart(context, path.toUtf8());
    auto found_treeitem = best_item_ref.first.get();
    if (!found_treeitem)
        return;
    MainWindow::TheOne()->setCurrentTreeItem(const_cast<TreeItem*>(found_treeitem), updateHistory);
}

void DmsAddressBar::setPathDirectly(QString path) {
    setText(path);
    findItem(MainWindow::TheOne()->getRootTreeItem(), path, false);
}

void DmsAddressBar::onEditingFinished() {
    findItem(MainWindow::TheOne()->getCurrentTreeItemOrRoot(), text().toUtf8(), true);
}

// ==== DmsErrorWindow ====
#include "DmsErrorWindow.h"
#include "DmsDetailPages.h"
#include "DmsMainWindow.h"
#include "dbg/DmsCatch.h"

void DmsErrorWindow::ignore() {
    done(QDialog::Rejected);
}

void DmsErrorWindow::terminate() {
    done(QDialog::Rejected);
    std::terminate();
}

void DmsErrorWindow::reopen() {
    done(QDialog::Accepted);
    // don't call now: MainWindow::TheOne()->reOpen();
    // but let the execution caller call it, after modal message pumping ended
}

void DmsErrorWindow::onAnchorClicked(const QUrl& link) {
    try {
        MainWindow::TheOne()->onInternalLinkClick(link);
    }
    catch (...) {
        catchAndReportException();
    }
}

DmsErrorWindow::DmsErrorWindow(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(QString("Error"));
    setMinimumSize(dms_params::error_window_size);

    auto grid_layout = new QGridLayout(this);
    m_message = new QTextBrowser(this);
    m_message->setOpenLinks(false);
    m_message->setOpenExternalLinks(false);
    connect(m_message, &QTextBrowser::anchorClicked, this, &DmsErrorWindow::onAnchorClicked);
    grid_layout->addWidget(m_message, 0, 0, 1, 3);

    // ok/apply/cancel buttons
    auto box_layout = new QHBoxLayout(this);
    m_ignore = new QPushButton(tr("&Ignore"), this);
    dms_params::SetDialogButtonSize(m_ignore);
    m_terminate = new QPushButton(tr("&Terminate"), this);
    dms_params::SetDialogButtonSize(m_terminate);

    m_reopen = new QPushButton(tr("&Reopen"), this);
    m_reopen->setAutoDefault(true);
    m_reopen->setDefault(true);

    connect(m_ignore, &QPushButton::released, this, &DmsErrorWindow::ignore);
    connect(m_terminate, &QPushButton::released, this, &DmsErrorWindow::terminate);
    connect(m_reopen, &QPushButton::released, this, &DmsErrorWindow::reopen);
    dms_params::SetDialogButtonSize(m_reopen);
    box_layout->addWidget(m_reopen);
    box_layout->addWidget(m_terminate);
    box_layout->addWidget(m_ignore);
    grid_layout->addLayout(box_layout, 14, 0, 1, 3);
    setWindowModality(Qt::ApplicationModal);
}

// ==== DmsGuiParameters ====
#include <QSize>
#include <QPushButton>
#include "DmsGuiParameters.h"

// windows
QSize const dms_params::file_changed_window_size = QSize(600, 200);
QSize const dms_params::error_window_size = QSize(800, 400);

// buttons
QSize const dms_params::default_push_button_minimum_size = QSize(75, 30);
QSize const dms_params::treeitem_visit_history_fixed_size = QSize(18, 18);

void dms_params::SetDialogButtonSize(QPushButton* button)
{
    assert(button);
    // sizeHint() already accounts for the label, the mnemonic and the application font, which
    // main_qt installs from :/res/fonts/dmstext.ttf before any dialog is constructed.
    button->setFixedSize(button->sizeHint().expandedTo(default_push_button_minimum_size));
}

// toolbar
QSize const dms_params::toolbar_button_spacing = QSize(30, 0);

// icons
const char* dms_params::default_view_icon = ":/res/images/TV_default_view.bmp";
const char* dms_params::table_view_icon = ":/res/images/TV_table.bmp";
const char* dms_params::map_view_icon = ":/res/images/TV_globe.bmp";
const char* dms_params::statistics_view_icon = ":/res/images/DP_statistics.bmp";
const int dms_params::treeitem_icon_size = 16;

// coordinates bar
int dms_params::coordinates_bar_width = 300;

// font
const char* dms_params::dms_font_resource = ":/res/fonts/dmstext.ttf";
const char* dms_params::remix_icon_font_resource = ":/res/fonts/remixicon.ttf";
const int dms_params::default_font_size = 10;

// registry keys
const char* dms_params::reg_key_LastConfigFile = "LastConfigFile";
const char* dms_params::reg_key_RecentFiles = "RecentFiles";
const char* dms_params::reg_key_PinnedFiles = "PinnedFiles";
const char* dms_params::reg_key_ReopenLastConfigAtStartup = "ReopenLastConfigAtStartup";

// stylesheets

// The TreeView, DetailPages, EventLog and ValueInfo docks are all children of
// the same QMainWindow, so every resize "slider" between them is a QMainWindow
// separator. Qt derives the separator's geometry, its drawn strip and its mouse
// hit-region from one value (QStyle::PM_DockWidgetSeparatorExtent, exposed to
// the stylesheet as the ::separator width/height), so the clickable area and the
// visible gap are the same rectangle: widening the grab necessarily widens the
// gap by the same amount (see issue #1151).
//
// We widen it from 5px to 8px so the split cursor snaps a little earlier and the
// handle is easier to grab, keep the strip transparent so it adds no extra ink
// of its own, and light it up on hover so the (now wider) grab band is
// discoverable. To retune, change both 8px values (width for the vertical
// TreeView/DetailPages separators, height for the horizontal EventLog one).
const char* dms_params::stylesheet_main_window =
    "QMainWindow::separator { background: transparent; width: 8px; height: 8px; }\n"
    "QMainWindow::separator:hover { background: rgba(128, 128, 128, 96); }\n";
const char* dms_params::stylesheet_treeitem_visit_history = "QComboBox QAbstractItemView {\n"
"min-width:400px;"
"}\n"
"QComboBox::drop-down:button{\n"
"background-color: transparant;\n"
"}\n"
"QComboBox::down-arrow {\n"
"image: url(:/res/images/arrow_down.png);\n"
"}\n";

const char* dms_params::stylesheet_toolbar =	"QToolBar { background: rgb(117, 117, 138);\n padding : 0px; }\n"
												"QToolButton {padding: 0px;}\n"
												"QToolButton:checked {background-color: rgba(255, 255, 255, 150);}\n"
												"QToolButton:checked {selection-color: rgba(255, 255, 255, 150);}\n"
												"QToolButton:checked {selection-background-color: rgba(255, 255, 255, 150);}\n";

// ==== DmsSplashScreen ====
#include "DmsSplashScreen.h"

#include <QApplication>
#include <QPainter>
#include <QScreen>
#include <QFontDatabase>

void DmsSplashScreen::drawContents(QPainter* painter) {
    QPixmap textPix = QSplashScreen::pixmap();
    painter->setPen(this->m_color);
    painter->drawText(this->m_rect, this->m_alignment, this->m_message);
}

void DmsSplashScreen::setMessageRect(QRect rect, int alignment) 
{
    this->m_rect = rect;
    this->m_alignment = alignment;
}

auto showSplashScreen() -> std::unique_ptr<DmsSplashScreen>
{
    int id = QFontDatabase::addApplicationFont(":/res/fonts/dmstext.ttf");
    QString family = QFontDatabase::applicationFontFamilies(id).at(0);
    QFont dms_text_font(family, 25);

    QPixmap pixmap(":/res/images/WorldHeatMap.jpg");

    auto app = dynamic_cast<QApplication*>(QCoreApplication::instance());
    assert(app);
    auto screen_at_mouse_pos = app->screenAt(QCursor::pos());
    QScreen* screen = screen_at_mouse_pos ? screen_at_mouse_pos : app->primaryScreen();
    const QRect screenGeom = screen->availableGeometry(); // use availableGeometry to avoid taskbar
    const double scaleFactor = 0.75;

    // target size = 75% of current screen, keep pixmap aspect ratio
    const QSize targetSize(qRound(screenGeom.width() * scaleFactor),
        qRound(screenGeom.height() * scaleFactor));
    QPixmap scaledPixmap = pixmap.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    std::unique_ptr<DmsSplashScreen> splash = std::make_unique<DmsSplashScreen>(scaledPixmap);

    // place message rect relative to splash size (bottom band)
    const QRect srect = splash->rect();
    const QSize msgSize(srect.width(), qRound(srect.height() * 0.15)); // 15% of splash height
    splash->setMessageRect(QRect(QPoint(srect.left(), srect.bottom() - msgSize.height()), msgSize), Qt::AlignCenter);

    dms_text_font.setBold(true);
    splash->setFont(dms_text_font);
    splash->m_color = QColor(255, 255, 255);

    // position splash centered on the screen containing the mouse, but make sure it stays on-screen
    const QPoint screenCenter = screenGeom.center();
    auto projectedTopLeft = screenCenter - splash->rect().center();
    if (projectedTopLeft.y() < screenGeom.top())
        projectedTopLeft.setY(screenGeom.top());
    if (projectedTopLeft.x() < screenGeom.left())
        projectedTopLeft.setX(screenGeom.left());
    // ensure splash fully visible horizontally and vertically
    if (projectedTopLeft.x() + splash->rect().width() > screenGeom.right() + 1)
        projectedTopLeft.setX(screenGeom.right() + 1 - splash->rect().width());
    if (projectedTopLeft.y() + splash->rect().height() > screenGeom.bottom() + 1)
        projectedTopLeft.setY(screenGeom.bottom() + 1 - splash->rect().height());

    splash->move(projectedTopLeft);
    splash->show();
    return splash;
}


// ==== DmsFileChangedWindow ====
#include "DmsFileChangedWindow.h"
#include "DmsMainWindow.h"
#include "dbg/DmsCatch.h"

#include <QLayout>

DmsFileChangedWindow::DmsFileChangedWindow(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(QString("Source changed.."));
    setMinimumSize(dms_params::file_changed_window_size);

    auto grid_layout = new QGridLayout(this);
    m_message = new QTextBrowser(this);
    m_message->setOpenLinks(false);
    m_message->setOpenExternalLinks(false);
    connect(m_message, &QTextBrowser::anchorClicked, this, &DmsFileChangedWindow::onAnchorClicked);
    grid_layout->addWidget(m_message, 0, 0, 1, 3);

    // ok/apply/cancel buttons
    auto box_layout = new QHBoxLayout(this);
    m_ignore = new QPushButton(tr("&Ignore"), this);
    dms_params::SetDialogButtonSize(m_ignore);

    m_reopen = new QPushButton(tr("&Reopen"), this);
    connect(m_ignore, &QPushButton::released, this, &DmsFileChangedWindow::ignore);
    connect(m_reopen, &QPushButton::released, this, &DmsFileChangedWindow::reopen);
    m_reopen->setAutoDefault(true);
    m_reopen->setDefault(true);
    dms_params::SetDialogButtonSize(m_reopen);
    box_layout->addWidget(m_reopen);
    box_layout->addWidget(m_ignore);
    grid_layout->addLayout(box_layout, 14, 0, 1, 3);

    setWindowModality(Qt::ApplicationModal);
}

void DmsFileChangedWindow::setFileChangedMessage(std::string_view changed_files) {
    std::string file_changed_message_markdown = "The following files have been changed:\n\n";
    size_t curr_pos = 0;
    while (curr_pos < changed_files.size()) {
        auto curr_line_end = changed_files.find('\n', curr_pos);
        auto link = std::string(changed_files.substr(curr_pos, curr_line_end - curr_pos));
        file_changed_message_markdown += "[" + link + "](" + link + ")\n\n";
        curr_pos = curr_line_end + 1;
    }
    file_changed_message_markdown += "\n\nDo you want to reopen the configuration?";
    m_message->setMarkdown(file_changed_message_markdown.c_str());
}

void DmsFileChangedWindow::ignore() {
    done(QDialog::Rejected);
}

void DmsFileChangedWindow::reopen() {
    done(QDialog::Accepted);
    try {
        MainWindow::TheOne()->reopen();
    }
    catch (...) {
        catchAndReportException();
    }
}

void DmsFileChangedWindow::onAnchorClicked(const QUrl& link) {
    try {
        auto clicked_file_link = link.toString().toStdString();
        MainWindow::TheOne()->openConfigSourceDirectly(clicked_file_link, "0");
    }
    catch (...) {
        catchAndReportException();
    }
}

// ==== SearchTreeItemWindow ====
// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "SearchTreeItemWindow.h"

#include <QVBoxLayout>

#include "dbg/DmsCatch.h"
#include "TicInterface.h"
#include "DmsMainWindow.h"

// Forward declaration of TreeItem_SearchItem from TreeItem.cpp
TIC_CALL SharedTreeItem TreeItem_SearchItem(const TreeItem* searchLoc, TokenID id);

SearchTreeItemWindow::SearchTreeItemWindow(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlag(Qt::Window, true);
    setMinimumSize(300, 120);
    resize(350, 150);
    setWindowTitle("Search TreeItem");

    auto* layout = new QVBoxLayout(this);
    find_text = new QLineEdit(this);
    next = new QPushButton("Find", this);

    QWidget* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
    spacer->setFocusPolicy(Qt::FocusPolicy::NoFocus);
    result_info = new QLabel("", this);

    // connections
    connect(next, &QPushButton::clicked, this, &SearchTreeItemWindow::nextClicked);
    connect(find_text, &QLineEdit::returnPressed, this, [this]() { findInTreeView(true); });
    connect(find_text, &QLineEdit::textChanged, this, &SearchTreeItemWindow::onFindTextChanged);

    // fill layout
    layout->addWidget(find_text);
    layout->addWidget(next);
    layout->addWidget(spacer);
    layout->addWidget(result_info);

    setLayout(layout);
}

void SearchTreeItemWindow::findInTreeView(bool closeOnSuccess)
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
        auto found_item = TreeItem_SearchItem(current_item, search_token);

        if (found_item)
        {
            main_window->setCurrentTreeItem(const_cast<TreeItem*>(found_item.get()));
            result_info->setText(QString("Found: %1").arg(found_item->GetFullName().c_str()));
            onFindTextChanged(search_text);
            if (closeOnSuccess)
                close();
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

void SearchTreeItemWindow::nextClicked(bool checked)
{
    findInTreeView();
}

void SearchTreeItemWindow::onFindTextChanged(const QString& text)
{
    auto main_window = MainWindow::TheOne();
    if (!main_window)
        return;

    auto current_item = main_window->getCurrentTreeItem();
    if (!current_item)
        return;

    // text-changed slot fires per keystroke; swallow without reporting to avoid log spam.
    try {
        auto current_id = current_item->GetID();
        if (text.compare(current_id.GetStr().c_str(), Qt::CaseInsensitive) == 0)
            next->setText("Find next");
        else
            next->setText("Find");
    }
    catch (...) {
        catchException(false);
    }
}

void SearchTreeItemWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape)
        close();
    else
        QWidget::keyPressEvent(event);
}


// ==== UpdatableBrowser ====
#include "UpdatableBrowser.h"

#include "dbg/DmsCatch.h"

#include <QContextMenuEvent>
#include <QDialog>
#include <QLayout>
#include <QFontDatabase>

FindTextWindow::FindTextWindow(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlag(Qt::Window, true);
    setMinimumSize(250, 180);
    resize(300, 200);

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

    findInQTextBrowser(backwards);
}

void FindTextWindow::nextClicked(bool checked)
{
    findInText(false);
}

void FindTextWindow::previousClicked(bool checked)
{
    findInText(true);
}

void FindTextWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape)
        close();
    else
        QWidget::keyPressEvent(event);
}

void QUpdatableBrowser::onAnchorClicked(const QUrl& url) {
    MainWindow::TheOne()->onInternalLinkClick(url, dynamic_cast<QWidget*>(parent()));
}

QUpdatableBrowser::QUpdatableBrowser(QWidget* parent, bool handleAnchors)
    : QTextBrowser(parent)
    , m_handleAnchors(handleAnchors)
{
    setOpenLinks(false);
    setOpenExternalLinks(false);
    setProperty("DmsHelperWindowType", DmsHelperWindowType::HW_UNKNOWN);

    find_shortcut = new QShortcut(QKeySequence(tr("Ctrl+F", "Find")), this);
    connect(find_shortcut, &QShortcut::activated, this, &QUpdatableBrowser::openFindWindow);

    if (m_handleAnchors)
        connect(this, &QTextBrowser::anchorClicked, this, &QUpdatableBrowser::onAnchorClicked);
}

void QUpdatableBrowser::restart_updating()
{
    m_Waiter.start(this);
    QPointer<QUpdatableBrowser> self = this;
    QTimer::singleShot(0, MainWindow::TheOne(),
        [self]()
        {
            if (self)
            {
                // update() (ValueInfo / Statistics HTML generation) runs DMS code that can throw.
                // Report and END the loop on failure: a thrown exception must not escape into the
                // timer dispatch, and we must not re-schedule (which would loop on a persistent error).
                try {
                    if (!self->update())
                        self->restart_updating();
                    else
                        self->m_Waiter.end();
                }
                catch (...) {
                    catchAndReportException();
                    self->m_Waiter.end();
                }
            }
        }
    );
}

void QUpdatableBrowser::GenerateDescription()
{
    auto pw = dynamic_cast<QMdiSubWindow*>(parentWidget());
    if (!pw)
        return;
    SetText(SharedStr(pw->windowTitle().toStdString().c_str()));
}

void QUpdatableBrowser::openFindWindow()
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
