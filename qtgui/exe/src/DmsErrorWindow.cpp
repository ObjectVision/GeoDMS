#include "DmsErrorWindow.h"
#include "DmsDetailPages.h"
#include "DmsMainWindow.h"

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
    MainWindow::TheOne()->onInternalLinkClick(link);
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
    m_ignore->setMaximumSize(dms_params::default_push_button_maximum_size);
    m_terminate = new QPushButton(tr("&Terminate"), this);
    m_terminate->setMaximumSize(dms_params::default_push_button_maximum_size);

    m_reopen = new QPushButton(tr("&Reopen"), this);
    m_reopen->setAutoDefault(true);
    m_reopen->setDefault(true);

    connect(m_ignore, &QPushButton::released, this, &DmsErrorWindow::ignore);
    connect(m_terminate, &QPushButton::released, this, &DmsErrorWindow::terminate);
    connect(m_reopen, &QPushButton::released, this, &DmsErrorWindow::reopen);
    m_reopen->setMaximumSize(dms_params::default_push_button_maximum_size);
    box_layout->addWidget(m_reopen);
    box_layout->addWidget(m_terminate);
    box_layout->addWidget(m_ignore);
    grid_layout->addLayout(box_layout, 14, 0, 1, 3);
    setWindowModality(Qt::ApplicationModal);
}