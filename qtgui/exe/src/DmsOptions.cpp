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
#include <QColorDialog>;

//======== BEGIN GUI OPTIONS WINDOW ========
DmsGuiOptionsWindow::DmsGuiOptionsWindow(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QString("Gui options"));
    setMinimumSize(800, 400);

    auto grid_layout = new QGridLayout(this);
    grid_layout->setVerticalSpacing(0);

    m_show_hidden_items = new QCheckBox("Show hidden items", this);
    m_show_thousand_separator = new QCheckBox("Show thousand separator", this);
    m_show_state_colors_in_treeview = new QCheckBox("Show state colors in treeview", this);
    grid_layout->addWidget(m_show_hidden_items, 0, 0, 1, 3);
    grid_layout->addWidget(m_show_thousand_separator, 1, 0, 1, 3);
    grid_layout->addWidget(m_show_state_colors_in_treeview, 2, 0, 1, 3);

    auto valid_color_text_ti = new QLabel("    Valid:", this);
    auto not_calculated_color_text_ti = new QLabel("    NotCalculated:", this);
    auto failed_color_text_ti = new QLabel("    Failed:", this);
    auto valid_color_ti_button = new QPushButton(this);
    auto not_calculated_color_ti_button = new QPushButton(this);
    auto failed_color_ti_button = new QPushButton(this);
    grid_layout->addWidget(valid_color_text_ti, 3, 0);
    grid_layout->addWidget(valid_color_ti_button, 3, 1);
    grid_layout->addWidget(not_calculated_color_text_ti, 4, 0);
    grid_layout->addWidget(not_calculated_color_ti_button, 4, 1);
    grid_layout->addWidget(failed_color_text_ti, 5, 0);
    grid_layout->addWidget(failed_color_ti_button, 5, 1);

    auto map_view_color_settings = new QLabel("Mapview color settings", this);
    auto background_color_text = new QLabel("    Background:", this);
    auto background_color_button = new QPushButton(this);
    grid_layout->addWidget(map_view_color_settings, 6, 0, 1, 3);
    grid_layout->addWidget(background_color_text, 7, 0);
    grid_layout->addWidget(background_color_button, 7, 1);


    auto default_classification_text = new QLabel("Default classification ramp colors", this);
    auto start_color_text = new QLabel("    Start:", this);
    auto start_color_button = new QPushButton(this);
    auto end_color_text = new QLabel("    End:", this);
    auto end_color_button = new QPushButton(this);
    grid_layout->addWidget(default_classification_text, 8, 0, 1, 3);
    grid_layout->addWidget(start_color_text, 9, 0);
    grid_layout->addWidget(start_color_button, 9, 1);
    grid_layout->addWidget(end_color_text, 10, 0);
    grid_layout->addWidget(end_color_button, 10, 1);

    QWidget* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    grid_layout->addWidget(spacer, 11, 0, 1, 3);

    // ok/apply/cancel buttons
    auto box_layout = new QHBoxLayout(this);
    m_ok = new QPushButton("Ok", this);
    m_ok->setMaximumSize(75, 30);
    m_ok->setAutoDefault(true);
    m_ok->setDefault(true);
    m_apply = new QPushButton("Apply", this);
    m_apply->setMaximumSize(75, 30);
    m_apply->setDisabled(true);

    m_undo = new QPushButton("Undo", this);
    m_undo->setDisabled(true);
    connect(m_ok, &QPushButton::released, this, &DmsGuiOptionsWindow::ok);
    connect(m_apply, &QPushButton::released, this, &DmsGuiOptionsWindow::apply);
    connect(m_undo, &QPushButton::released, this, &DmsGuiOptionsWindow::undo);
    m_undo->setMaximumSize(75, 30);
    box_layout->addWidget(m_ok);
    box_layout->addWidget(m_apply);
    box_layout->addWidget(m_undo);
    grid_layout->addLayout(box_layout, 12, 0, 1, 3);
}

void DmsGuiOptionsWindow::apply()
{
    /*SetGeoDmsRegKeyString("LocalDataDir", m_ld_input.data()->text().toStdString());
    SetGeoDmsRegKeyString("SourceDataDir", m_sd_input.data()->text().toStdString());
    SetGeoDmsRegKeyString("DmsEditor", m_editor_input.data()->text().toStdString());

    auto dms_reg_status_flags = GetRegStatusFlags();
    SetGeoDmsRegKeyDWord("StatusFlags", m_pp0->isChecked() ? dms_reg_status_flags |= RSF_SuspendForGUI : dms_reg_status_flags &= ~RSF_SuspendForGUI);
    SetGeoDmsRegKeyDWord("StatusFlags", m_pp1->isChecked() ? dms_reg_status_flags |= RSF_MultiThreading1 : dms_reg_status_flags &= ~RSF_MultiThreading1);
    SetGeoDmsRegKeyDWord("StatusFlags", m_pp2->isChecked() ? dms_reg_status_flags |= RSF_MultiThreading2 : dms_reg_status_flags &= ~RSF_MultiThreading2);
    SetGeoDmsRegKeyDWord("StatusFlags", m_pp3->isChecked() ? dms_reg_status_flags |= RSF_MultiThreading3 : dms_reg_status_flags &= ~RSF_MultiThreading3);
    SetGeoDmsRegKeyDWord("StatusFlags", m_tracelog->isChecked() ? dms_reg_status_flags |= RSF_TraceLogFile : dms_reg_status_flags &= ~RSF_TraceLogFile);
    SetGeoDmsRegKeyDWord("MemoryFlushThreshold", m_flush_treshold->value());*/
    m_changed = false;
    m_apply->setDisabled(true);
}


void DmsGuiOptionsWindow::undo()
{
    //restoreOptions();
    m_changed = false;
}

void DmsGuiOptionsWindow::ok()
{
    if (m_changed)
        apply();
    m_changed = false;
    done(QDialog::Accepted);
}

//======== BEGIN GUI OPTIONS WINDOW ========

//======== BEGIN ADVANCED OPTIONS WINDOW ========
DmsAdvancedOptionsWindow::DmsAdvancedOptionsWindow(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QString("Advanced options"));
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

    connect(path_ld_fldr, &QPushButton::clicked, this, &DmsAdvancedOptionsWindow::setLocalDataDirThroughDialog);
    connect(path_sd_fldr, &QPushButton::clicked, this, &DmsAdvancedOptionsWindow::setSourceDataDirThroughDialog);

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
    connect(m_flush_treshold, &QSlider::valueChanged, this, &DmsAdvancedOptionsWindow::onFlushTresholdValueChange);

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
    connect(m_pp0, &QCheckBox::stateChanged, this, &DmsAdvancedOptionsWindow::onStateChange);
    connect(m_pp1, &QCheckBox::stateChanged, this, &DmsAdvancedOptionsWindow::onStateChange);
    connect(m_pp2, &QCheckBox::stateChanged, this, &DmsAdvancedOptionsWindow::onStateChange);
    connect(m_pp3, &QCheckBox::stateChanged, this, &DmsAdvancedOptionsWindow::onStateChange);
    connect(m_tracelog, &QCheckBox::stateChanged, this, &DmsAdvancedOptionsWindow::onStateChange);
    connect(m_ld_input, &QLineEdit::textChanged, this, &DmsAdvancedOptionsWindow::onTextChange);
    connect(m_sd_input, &QLineEdit::textChanged, this, &DmsAdvancedOptionsWindow::onTextChange);

    // ok/apply/cancel buttons
    auto box_layout = new QHBoxLayout(this);
    m_ok = new QPushButton("Ok", this);
    m_ok->setMaximumSize(75, 30);
    m_ok->setAutoDefault(true);
    m_ok->setDefault(true);
    m_apply = new QPushButton("Apply", this);
    m_apply->setMaximumSize(75, 30);
    m_apply->setDisabled(true);

    m_undo = new QPushButton("Undo", this);
    m_undo->setDisabled(true);
    connect(m_ok, &QPushButton::released, this, &DmsAdvancedOptionsWindow::ok);
    connect(m_apply, &QPushButton::released, this, &DmsAdvancedOptionsWindow::apply);
    connect(m_undo, &QPushButton::released, this, &DmsAdvancedOptionsWindow::undo);
    m_undo->setMaximumSize(75, 30);
    box_layout->addWidget(m_ok);
    box_layout->addWidget(m_apply);
    box_layout->addWidget(m_undo);
    grid_layout->addLayout(box_layout, 14, 0, 1, 3);

    restoreOptions();

    setWindowModality(Qt::ApplicationModal);
}

void DmsAdvancedOptionsWindow::setInitialLocalDataDirValue()
{
    auto ld_reg_key = GetGeoDmsRegKey("LocalDataDir");
    if (ld_reg_key.empty())
    {
        SetGeoDmsRegKeyString("LocalDataDir", "C:\\LocalData");
        ld_reg_key = GetGeoDmsRegKey("LocalDataDir");
    }
    m_ld_input->setText(ld_reg_key.c_str());
}

void DmsAdvancedOptionsWindow::setInitialSourceDatDirValue()
{
    auto ld_reg_key = GetGeoDmsRegKey("SourceDataDir");
    if (ld_reg_key.empty())
    {
        SetGeoDmsRegKeyString("SourceDataDir", "C:\\SourceData");
        ld_reg_key = GetGeoDmsRegKey("SourceDataDir");
    }
    m_sd_input->setText(ld_reg_key.c_str());
}

void DmsAdvancedOptionsWindow::setInitialEditorValue()
{
    auto ld_reg_key = GetGeoDmsRegKey("DmsEditor");
    if (ld_reg_key.empty())
    {
        SetGeoDmsRegKeyString("DmsEditor", """%env:ProgramFiles%\\Notepad++\\Notepad++.exe"" ""%F"" -n%L");
        ld_reg_key = GetGeoDmsRegKey("DmsEditor");
    }
    m_editor_input->setText(ld_reg_key.c_str());
}

void DmsAdvancedOptionsWindow::setInitialMemoryFlushTresholdValue()
{
    auto flush_treshold = RTC_GetRegDWord(RegDWordEnum::MemoryFlushThreshold);
    m_flush_treshold->setValue(flush_treshold);
}

void DmsAdvancedOptionsWindow::restoreOptions()
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

void DmsAdvancedOptionsWindow::apply()
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

void DmsAdvancedOptionsWindow::ok()
{
    if (m_changed)
        apply();
    m_changed = false;
    done(QDialog::Accepted);
}

void DmsAdvancedOptionsWindow::undo()
{
    restoreOptions();
    m_changed = false;
}

void DmsAdvancedOptionsWindow::onStateChange(int state)
{
    m_changed = true;
    m_apply->setDisabled(false);
    m_undo->setDisabled(false);
}

void DmsAdvancedOptionsWindow::onTextChange(const QString& text)
{
    m_changed = true;
    m_apply->setDisabled(false);
    m_undo->setDisabled(false);
}

void DmsAdvancedOptionsWindow::setLocalDataDirThroughDialog()
{
    auto new_local_data_dir_folder = m_folder_dialog->QFileDialog::getExistingDirectory(this, tr("Open LocalData Directory"), m_ld_input->text(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (!new_local_data_dir_folder.isEmpty())
        m_ld_input->setText(new_local_data_dir_folder);
}

void DmsAdvancedOptionsWindow::setSourceDataDirThroughDialog()
{
    auto new_source_data_dir_folder = m_folder_dialog->QFileDialog::getExistingDirectory(this, tr("Open SourceData Directory"), m_sd_input->text(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!new_source_data_dir_folder.isEmpty())
        m_sd_input->setText(new_source_data_dir_folder);
}

void DmsAdvancedOptionsWindow::onFlushTresholdValueChange(int value)
{
    m_flush_treshold_text->setText(QString::number(value).rightJustified(3, ' ') + "%");
    m_apply->setDisabled(false);
    m_changed = true;
}
//======== END ADVANCED OPTIONS WINDOW ========

//======== BEGIN CONFIG OPTIONS WINDOW ========
DmsConfigOptionsWindow::DmsConfigOptionsWindow(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QString("Config options"));
    setMinimumSize(800, 400);
}
//======== BEGIN CONFIG OPTIONS WINDOW ========