#include "DmsOptions.h"
#include "utl/Environment.h"
#include "Parallel.h"
#include "ptr/SharedStr.h"

#include <QCheckBox>;
#include <QPushButton>;
#include <QSlider>;
#include <QLabel>;
#include <QFileDialog>;
#include <QLineEdit>;
#include <QGridLayout>;

void DmsOptionsWindow::setInitialLocalDataDirValue()
{
    auto ld_reg_key = GetGeoDmsRegKey("LocalDataDir");
    if (ld_reg_key.empty())
    {
        SetGeoDmsRegKeyString("LocalDataDir", "C:\\LocalData");
        ld_reg_key = GetGeoDmsRegKey("LocalDataDir");
    }
    m_ld_input->setText(ld_reg_key.c_str());
}

void DmsOptionsWindow::setInitialSourceDatDirValue()
{
    auto ld_reg_key = GetGeoDmsRegKey("SourceDataDir");
    if (ld_reg_key.empty())
    {
        SetGeoDmsRegKeyString("SourceDataDir", "C:\\SourceData");
        ld_reg_key = GetGeoDmsRegKey("SourceDataDir");
    }
    m_sd_input->setText(ld_reg_key.c_str());
}

void DmsOptionsWindow::setInitialEditorValue()
{
    auto ld_reg_key = GetGeoDmsRegKey("DmsEditor");
    if (ld_reg_key.empty())
    {
        SetGeoDmsRegKeyString("DmsEditor", """%env:ProgramFiles%\\Notepad++\\Notepad++.exe"" ""%F"" -n%L");
        ld_reg_key = GetGeoDmsRegKey("DmsEditor");
    }
    m_editor_input->setText(ld_reg_key.c_str());
}

void DmsOptionsWindow::setInitialMemoryFlushTresholdValue()
{
    auto flush_treshold = RTC_GetRegDWord(RegDWordEnum::MemoryFlushThreshold);
    m_flush_treshold->setValue(flush_treshold);
}

void DmsOptionsWindow::restoreOptions()
{
    {
        const QSignalBlocker blocker1(m_ld_input);
        const QSignalBlocker blocker2(m_sd_input);
        const QSignalBlocker blocker3(m_editor_input);
        const QSignalBlocker blocker4(m_flush_treshold);
        const QSignalBlocker blocker5(m_pp0);
        const QSignalBlocker blocker6(m_pp1);
        const QSignalBlocker blocker7(m_pp2);
        const QSignalBlocker blocker8(m_pp3);
        const QSignalBlocker blocker9(m_tracelog);

        setInitialLocalDataDirValue();
        setInitialSourceDatDirValue();
        setInitialEditorValue();
        setInitialMemoryFlushTresholdValue();
        m_pp0->setChecked(IsMultiThreaded0());
        m_pp1->setChecked(IsMultiThreaded1());
        m_pp2->setChecked(IsMultiThreaded2());
        m_pp3->setChecked(IsMultiThreaded3());
        m_tracelog->setChecked(GetRegStatusFlags() & RSF_TraceLogFile);
    }
    m_apply->setDisabled(true);
    m_undo->setDisabled(true);
}

void DmsOptionsWindow::apply()
{
    SetGeoDmsRegKeyString("LocalDataDir", m_ld_input.data()->text().toStdString());
    SetGeoDmsRegKeyString("SourceDataDir", m_sd_input.data()->text().toStdString());
    SetGeoDmsRegKeyString("DmsEditor", m_editor_input.data()->text().toStdString());

    auto dms_reg_status_flags = GetRegStatusFlags();
    SetGeoDmsRegKeyDWord("StatusFlags", m_pp0->isChecked() ? dms_reg_status_flags |= RSF_SuspendForGUI : dms_reg_status_flags &= ~RSF_SuspendForGUI);
    SetGeoDmsRegKeyDWord("StatusFlags", m_pp1->isChecked() ? dms_reg_status_flags |= RSF_MultiThreading1 : dms_reg_status_flags &= ~RSF_MultiThreading1);
    SetGeoDmsRegKeyDWord("StatusFlags", m_pp2->isChecked() ? dms_reg_status_flags |= RSF_MultiThreading2 : dms_reg_status_flags &= ~RSF_MultiThreading2);
    SetGeoDmsRegKeyDWord("StatusFlags", m_pp3->isChecked() ? dms_reg_status_flags |= RSF_MultiThreading3 : dms_reg_status_flags &= ~RSF_MultiThreading3);
    SetGeoDmsRegKeyDWord("StatusFlags", m_tracelog->isChecked() ? dms_reg_status_flags |= RSF_TraceLogFile : dms_reg_status_flags &= ~RSF_TraceLogFile);
    SetGeoDmsRegKeyDWord("MemoryFlushThreshold", m_flush_treshold->value());
    m_changed = false;
    m_apply->setDisabled(true);
}

void DmsOptionsWindow::ok()
{
    if (m_changed)
        apply();
    m_changed = false;
    done(QDialog::Accepted);
}

void DmsOptionsWindow::undo()
{
    restoreOptions();
    m_changed = false;
}

void DmsOptionsWindow::onStateChange(int state)
{
    m_changed = true;
    m_apply->setDisabled(false);
    m_undo->setDisabled(false);
}

void DmsOptionsWindow::onTextChange(const QString& text)
{
    m_changed = true;
    m_apply->setDisabled(false);
    m_undo->setDisabled(false);
}

void DmsOptionsWindow::setLocalDataDirThroughDialog()
{
    auto new_local_data_dir_folder = m_folder_dialog->QFileDialog::getExistingDirectory(this, tr("Open LocalData Directory"), m_ld_input->text(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (!new_local_data_dir_folder.isEmpty())
        m_ld_input->setText(new_local_data_dir_folder);
}

void DmsOptionsWindow::setSourceDataDirThroughDialog()
{
    auto new_source_data_dir_folder = m_folder_dialog->QFileDialog::getExistingDirectory(this, tr("Open SourceData Directory"), m_sd_input->text(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!new_source_data_dir_folder.isEmpty())
        m_sd_input->setText(new_source_data_dir_folder);
}

DmsOptionsWindow::DmsOptionsWindow(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QString("Options"));
    setMinimumSize(800, 400);

    m_folder_dialog = new QFileDialog(this);
    m_folder_dialog->setFileMode(QFileDialog::FileMode::Directory);


    auto grid_layout = new QGridLayout(this);
    // path widgets
    auto path_ld = new QLabel("Local data:", this);
    auto path_sd = new QLabel("Source data:", this);
    m_ld_input = new QLineEdit(this);
    m_sd_input = new QLineEdit(this);
    auto path_ld_fldr = new QPushButton(QIcon(":/res/images/DP_explore.bmp"), "", this);
    auto path_sd_fldr = new QPushButton(QIcon(":/res/images/DP_explore.bmp"), "", this);

    connect(path_ld_fldr, &QPushButton::clicked, this, &DmsOptionsWindow::setLocalDataDirThroughDialog);
    connect(path_sd_fldr, &QPushButton::clicked, this, &DmsOptionsWindow::setSourceDataDirThroughDialog);

    grid_layout->addWidget(path_ld, 0, 0);
    grid_layout->addWidget(m_ld_input, 0, 1);
    grid_layout->addWidget(path_ld_fldr, 0, 2);
    grid_layout->addWidget(path_sd, 1, 0);
    grid_layout->addWidget(m_sd_input, 1, 1);
    grid_layout->addWidget(path_sd_fldr, 1, 2);

    auto path_line = new QFrame(this);
    path_line->setFrameShape(QFrame::HLine);
    path_line->setFrameShadow(QFrame::Plain);
    path_line->setLineWidth(1);
    path_line->setMidLineWidth(1);
    grid_layout->addWidget(path_line, 2, 0, 1, 3);

    // editor widgets
    auto editor_text = new QLabel("Editor:", this);
    m_editor_input = new QLineEdit(this);
    grid_layout->addWidget(editor_text, 3, 0);
    grid_layout->addWidget(m_editor_input, 3, 1);

    auto line_editor = new QFrame(this);
    line_editor->setFrameShape(QFrame::HLine);
    line_editor->setFrameShadow(QFrame::Plain);
    line_editor->setLineWidth(1);
    line_editor->setMidLineWidth(1);
    grid_layout->addWidget(line_editor, 4, 0, 1, 3);

    // parallel processing widgets
    auto pp_text = new QLabel("Parallel processing:", this);
    m_pp0 = new QCheckBox("0: Suspend view updates to favor gui", this);
    m_pp0->setChecked(IsMultiThreaded0());
    m_pp1 = new QCheckBox("1: Tile/segment tasks", this);
    m_pp1->setChecked(IsMultiThreaded1());
    m_pp2 = new QCheckBox("2: Multiple operations simultaneously", this);
    m_pp2->setChecked(IsMultiThreaded2());
    m_pp3 = new QCheckBox("3: Pipelined tile operations", this);
    m_pp3->setChecked(IsMultiThreaded3());

    grid_layout->addWidget(pp_text, 5, 0, 1, 3);
    grid_layout->addWidget(m_pp0, 6, 0, 1, 3);
    grid_layout->addWidget(m_pp1, 7, 0, 1, 3);
    grid_layout->addWidget(m_pp2, 8, 0, 1, 3);
    grid_layout->addWidget(m_pp3, 9, 0, 1, 3);

    auto pp_line = new QFrame(this);
    pp_line->setFrameShape(QFrame::HLine);
    pp_line->setFrameShadow(QFrame::Plain);
    pp_line->setLineWidth(1);
    pp_line->setMidLineWidth(1);
    grid_layout->addWidget(pp_line, 10, 0, 1, 3);

    // flush treshold
    auto ft_text = new QLabel("Flush treshold:", this);
    m_flush_treshold_text = new QLabel("100%", this);
    m_flush_treshold = new QSlider(Qt::Orientation::Horizontal, this);
    m_flush_treshold->setMinimum(50);
    m_flush_treshold->setMaximum(100);
    m_flush_treshold->setValue(100);
    connect(m_flush_treshold, &QSlider::valueChanged, this, &DmsOptionsWindow::onFlushTresholdValueChange);

    m_tracelog = new QCheckBox("Tracelog file", this);

    grid_layout->addWidget(ft_text, 11, 0);
    grid_layout->addWidget(m_flush_treshold, 11, 1);
    grid_layout->addWidget(m_flush_treshold_text, 11, 2);
    grid_layout->addWidget(m_tracelog, 12, 0);

    auto ft_line = new QFrame(this);
    ft_line->setFrameShape(QFrame::HLine);
    ft_line->setFrameShadow(QFrame::Plain);
    ft_line->setLineWidth(1);
    ft_line->setMidLineWidth(1);
    grid_layout->addWidget(ft_line, 13, 0, 1, 3);

    // change connections
    connect(m_pp0, &QCheckBox::stateChanged, this, &DmsOptionsWindow::onStateChange);
    connect(m_pp1, &QCheckBox::stateChanged, this, &DmsOptionsWindow::onStateChange);
    connect(m_pp2, &QCheckBox::stateChanged, this, &DmsOptionsWindow::onStateChange);
    connect(m_pp3, &QCheckBox::stateChanged, this, &DmsOptionsWindow::onStateChange);
    connect(m_tracelog, &QCheckBox::stateChanged, this, &DmsOptionsWindow::onStateChange);
    connect(m_ld_input, &QLineEdit::textChanged, this, &DmsOptionsWindow::onTextChange);
    connect(m_sd_input, &QLineEdit::textChanged, this, &DmsOptionsWindow::onTextChange);

    // ok/apply/cancel buttons
    auto box_layout = new QHBoxLayout(this);
    m_ok = new QPushButton("Ok");
    m_ok->setMaximumSize(75, 30);
    m_ok->setAutoDefault(true);
    m_ok->setDefault(true);
    m_apply = new QPushButton("Apply");
    m_apply->setMaximumSize(75, 30);
    m_apply->setDisabled(true);

    m_undo = new QPushButton("Undo");
    m_undo->setDisabled(true);
    connect(m_ok, &QPushButton::released, this, &DmsOptionsWindow::ok);
    connect(m_apply, &QPushButton::released, this, &DmsOptionsWindow::apply);
    connect(m_undo, &QPushButton::released, this, &DmsOptionsWindow::undo);
    m_undo->setMaximumSize(75, 30);
    box_layout->addWidget(m_ok);
    box_layout->addWidget(m_apply);
    box_layout->addWidget(m_undo);
    grid_layout->addLayout(box_layout, 14, 0, 1, 3);

    restoreOptions();

    setWindowModality(Qt::ApplicationModal);
}

void DmsOptionsWindow::onFlushTresholdValueChange(int value)
{
    m_flush_treshold_text->setText(QString::number(value).rightJustified(3, ' ') + "%");
    m_apply->setDisabled(false);
    m_changed = true;
}