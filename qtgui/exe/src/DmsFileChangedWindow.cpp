#include "DmsFileChangedWindow.h"
#include "DmsMainWindow.h"

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
    m_ignore->setMaximumSize(dms_params::default_push_button_maximum_size);

    m_reopen = new QPushButton(tr("&Reopen"), this);
    connect(m_ignore, &QPushButton::released, this, &DmsFileChangedWindow::ignore);
    connect(m_reopen, &QPushButton::released, this, &DmsFileChangedWindow::reopen);
    m_reopen->setAutoDefault(true);
    m_reopen->setDefault(true);
    m_reopen->setMaximumSize(dms_params::default_push_button_maximum_size);
    box_layout->addWidget(m_reopen);
    box_layout->addWidget(m_ignore);
    grid_layout->addLayout(box_layout, 14, 0, 1, 3);

    setWindowModality(Qt::ApplicationModal);
}

void DmsFileChangedWindow::setFileChangedMessage(std::string_view changed_files) {
    std::string file_changed_message_markdown = "The following files have been changed:\n\n";
    size_t curr_pos = 0;
    while (curr_pos < changed_files.size()) {
        auto curr_line_end = changed_files.find_first_of('\n', curr_pos);
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
    MainWindow::TheOne()->reopen();
}

void DmsFileChangedWindow::onAnchorClicked(const QUrl& link) {
    auto clicked_file_link = link.toString().toStdString();
    MainWindow::TheOne()->openConfigSourceDirectly(clicked_file_link, "0");
}