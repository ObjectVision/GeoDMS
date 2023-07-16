#include "DmsOptions.h"
#include "utl/Environment.h"
#include "Parallel.h"
#include "ptr/SharedStr.h"
#include "StgBase.h"
#include "AbstrDataItem.h"

#include <QCheckBox>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QFileDialog>
#include <QLineEdit>
#include <QGridLayout>
#include <QColorDialog>

struct colorOptionAttr {

    CharPtr regKey;
    CharPtr descr;
    DmsColor color;
    UInt32  palette_index = 0;

    void apply(DmsColor clr)
    {
        clr &= 0xFFFFFF;
        CheckColor(clr);
        this->color = clr;
        if (this->palette_index)
            STG_Bmp_SetDefaultColor(this->palette_index, clr);
    }
};

static auto salmon = QColor(255, 128, 114);
static auto darkGrey = QColor(50, 50, 50);
static auto cool_blue = QColor(82, 136, 219);
static auto cool_green = QColor(0, 153, 51);
static auto white = QColor(255, 255, 255);

colorOptionAttr sColorOptionData[(int)color_option::count] =
{
    { "Valid", "Pick the TreeItem valid status color", cool_blue.rgb()},
    { "Invalidated", "Pick the color for not calcualate, status", salmon.rgb()},
    { "Failed", "Pick the TreeItem failed status color", 0xFF0000},
    { "Exogenic", "Pick the color for exogenic items", cool_green.rgb()},
    { "Operator", "Pick the color for template items", darkGrey.rgb()},
    { "Background", "Pick the Mapview background color", white.rgb(), 256},
    { "RampStart", "Pick the classification ramp start color", 0x0000FF, 257},
    { "RampEnd", "Pick the classification ramp end color", 0xFF0000, 258},
};

void setSF(bool value, UInt32& rsf, UInt32 flag)
{
    if (value)
        rsf |= flag;
    else
        rsf &= ~flag;
}

auto backgroundColor2StyleSheet(QColor clr) -> QString
{
    return QString("* { background-color: rgb(")
        + QString::number(qRed(clr.rgba())) + ","
        + QString::number(qGreen(clr.rgba())) + ","
        + QString::number(qBlue(clr.rgba()))
     + ") }";
}

void setBackgroundColor(QPushButton* btn, QColor clr)
{
    btn->setStyleSheet(backgroundColor2StyleSheet(clr));
}

auto getBackgroundColor(QPushButton* btn) -> QColor
{
    return btn->palette().color(QPalette::ColorRole::Button);
}

void saveBackgroundColor(QPushButton* btn, color_option co)
{
    auto clr = getBackgroundColor(btn).rgb();

    auto& colorOptionData = sColorOptionData[(int)co];
    SetGeoDmsRegKeyDWord(colorOptionData.regKey, clr, "Colors");
    colorOptionData.apply(clr);
}

void LoadColors()
{
    for (color_option co = color_option(0); co != color_option::count; reinterpret_cast<int&>(co)++)
    {
        auto& colorOptionData = sColorOptionData[(int)co];
        auto clr = GetGeoDmsRegKeyDWord(colorOptionData.regKey, colorOptionData.color, "Colors");
        colorOptionData.apply(clr);
    }
}

DmsColor GetUserColor(color_option co)
{
    assert(co < color_option::count);
    return sColorOptionData[(int)co].color;
}

void DmsGuiOptionsWindow::changeColor(QPushButton* btn, color_option co)
{
    auto old_color = getBackgroundColor(btn);
    auto new_color = QColorDialog::getColor(old_color, this, sColorOptionData[(int)co].descr);
    if (new_color == old_color)
        return;

    setBackgroundColor(btn, new_color);
    setChanged(true);
}


//======== BEGIN GUI OPTIONS WINDOW ========

void DmsGuiOptionsWindow::changeValidTreeItemColor()
{
    changeColor(m_valid_color_ti_button, color_option::tv_valid);
}

void DmsGuiOptionsWindow::changeNotCalculatedTreeItemColor()
{
    changeColor(m_not_calculated_color_ti_button, color_option::tv_not_calculated);
 }

void DmsGuiOptionsWindow::changeFailedTreeItemColor()
{
    changeColor(m_failed_color_ti_button, color_option::tv_failed);
}

void DmsGuiOptionsWindow::changeMapviewBackgroundColor()
{
    changeColor(m_background_color_button, color_option::mapview_background);
}

void DmsGuiOptionsWindow::changeClassificationStartColor()
{
    changeColor(m_start_color_button, color_option::mapview_ramp_start);
}

void DmsGuiOptionsWindow::changeClassificationEndColor()
{
    changeColor(m_end_color_button, color_option::mapview_ramp_end);
}

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
    connect(m_show_hidden_items, &QCheckBox::stateChanged, this, &DmsGuiOptionsWindow::hasChanged);
    connect(m_show_thousand_separator, &QCheckBox::stateChanged, this, &DmsGuiOptionsWindow::hasChanged);
    connect(m_show_state_colors_in_treeview, &QCheckBox::stateChanged, this, &DmsGuiOptionsWindow::hasChanged);

    auto valid_color_text_ti = new QLabel("    Valid:", this);
    auto not_calculated_color_text_ti = new QLabel("    NotCalculated:", this);
    auto failed_color_text_ti = new QLabel("    Failed:", this);
    m_valid_color_ti_button = new QPushButton(this);
    m_not_calculated_color_ti_button = new QPushButton(this);
    m_failed_color_ti_button = new QPushButton(this);
    grid_layout->addWidget(valid_color_text_ti, 3, 0);
    grid_layout->addWidget(m_valid_color_ti_button, 3, 1);
    grid_layout->addWidget(not_calculated_color_text_ti, 4, 0);
    grid_layout->addWidget(m_not_calculated_color_ti_button, 4, 1);
    grid_layout->addWidget(failed_color_text_ti, 5, 0);
    grid_layout->addWidget(m_failed_color_ti_button, 5, 1);
    connect(m_valid_color_ti_button, &QPushButton::released, this, &DmsGuiOptionsWindow::changeValidTreeItemColor);
    connect(m_not_calculated_color_ti_button, &QPushButton::released, this, &DmsGuiOptionsWindow::changeNotCalculatedTreeItemColor);
    connect(m_failed_color_ti_button, &QPushButton::released, this, &DmsGuiOptionsWindow::changeFailedTreeItemColor);

    auto map_view_color_settings = new QLabel("Mapview color settings", this);
    auto background_color_text = new QLabel("    Background:", this);
    m_background_color_button = new QPushButton(this);
    grid_layout->addWidget(map_view_color_settings, 6, 0, 1, 3);
    grid_layout->addWidget(background_color_text, 7, 0);
    grid_layout->addWidget(m_background_color_button, 7, 1);
    connect(m_background_color_button, &QPushButton::released, this, &DmsGuiOptionsWindow::changeMapviewBackgroundColor);

    auto default_classification_text = new QLabel("Default classification ramp colors", this);
    auto start_color_text = new QLabel("    Start:", this);
    m_start_color_button = new QPushButton(this);
    auto end_color_text = new QLabel("    End:", this);
    m_end_color_button = new QPushButton(this);
    grid_layout->addWidget(default_classification_text, 8, 0, 1, 3);
    grid_layout->addWidget(start_color_text, 9, 0);
    grid_layout->addWidget(m_start_color_button, 9, 1);
    grid_layout->addWidget(end_color_text, 10, 0);
    grid_layout->addWidget(m_end_color_button, 10, 1);
    connect(m_start_color_button, &QPushButton::released, this, &DmsGuiOptionsWindow::changeClassificationStartColor);
    connect(m_end_color_button, &QPushButton::released, this, &DmsGuiOptionsWindow::changeClassificationEndColor);

    QWidget* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    grid_layout->addWidget(spacer, 11, 0, 1, 3);

    // ok/apply/cancel buttons
    auto box_layout = new QHBoxLayout(this);
    m_ok = new QPushButton("Ok", this);
    m_ok->setMaximumSize(75, 30);
    m_ok->setAutoDefault(true);
    m_apply = new QPushButton("Apply", this);
    m_apply->setMaximumSize(75, 30);
    m_undo = new QPushButton("Undo", this);
    m_undo->setMaximumSize(75, 30);

    connect(m_ok, &QPushButton::released, this, &DmsGuiOptionsWindow::ok);
    connect(m_apply, &QPushButton::released, this, &DmsGuiOptionsWindow::apply);
    connect(m_undo, &QPushButton::released, this, &DmsGuiOptionsWindow::undo);

    box_layout->addWidget(m_ok);
    box_layout->addWidget(m_apply);
    box_layout->addWidget(m_undo);
    grid_layout->addLayout(box_layout, 12, 0, 1, 3);

    restoreOptions();

    setChanged(false);
    setWindowModality(Qt::ApplicationModal);
    setAttribute(Qt::WA_DeleteOnClose);
}

void DmsGuiOptionsWindow::setChanged(bool isChanged)
{
    m_changed = isChanged;
    m_apply->setEnabled(isChanged);
    m_undo->setEnabled(isChanged);
}

void DmsGuiOptionsWindow::apply()
{
    auto dms_reg_status_flags = GetRegStatusFlags();
    setSF(m_show_hidden_items->isChecked(), dms_reg_status_flags, RSF_AdminMode);
    setSF(m_show_thousand_separator->isChecked(), dms_reg_status_flags, RSF_ShowThousandSeparator);
    setSF(m_show_state_colors_in_treeview->isChecked(), dms_reg_status_flags, RSF_ShowStateColors);
    SetGeoDmsRegKeyDWord("StatusFlags", dms_reg_status_flags);

    saveBackgroundColor(m_valid_color_ti_button, color_option::tv_valid);
    saveBackgroundColor(m_not_calculated_color_ti_button, color_option::tv_not_calculated);
    saveBackgroundColor(m_failed_color_ti_button, color_option::tv_failed);

    saveBackgroundColor(m_background_color_button, color_option::mapview_background);
    saveBackgroundColor(m_start_color_button, color_option::mapview_ramp_start);
    saveBackgroundColor(m_end_color_button, color_option::mapview_ramp_end);

    setChanged(false);
}

void DmsGuiOptionsWindow::restoreOptions()
{   
    auto dms_reg_status_flags = GetRegStatusFlags();
    m_show_hidden_items->setChecked(dms_reg_status_flags & RSF_AdminMode);
    m_show_thousand_separator->setChecked(dms_reg_status_flags & RSF_ShowThousandSeparator);
    m_show_state_colors_in_treeview->setChecked(dms_reg_status_flags & RSF_ShowStateColors);

    setBackgroundColor(m_valid_color_ti_button, sColorOptionData[(int)color_option::tv_valid].color);
    setBackgroundColor(m_not_calculated_color_ti_button, sColorOptionData[(int)color_option::tv_not_calculated].color);
    setBackgroundColor(m_failed_color_ti_button, sColorOptionData[(int)color_option::tv_failed].color);
    setBackgroundColor(m_background_color_button, STG_Bmp_GetDefaultColor(CI_BACKGROUND));
    setBackgroundColor(m_start_color_button, STG_Bmp_GetDefaultColor(CI_RAMPSTART));
    setBackgroundColor(m_end_color_button, STG_Bmp_GetDefaultColor(CI_RAMPEND));

    setChanged(false);
}

void DmsGuiOptionsWindow::undo()
{
    restoreOptions();
}

void DmsGuiOptionsWindow::ok()
{
    if (m_changed)
        apply();
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
    setAttribute(Qt::WA_DeleteOnClose);
}

struct string_option_attr {
    CharPtr reg_key, default_value;
};

const string_option_attr sStringOptionsData[string_option::Count] = {
    {"LocalDataDir", "C:\\LocalData"},
    {"SourceDataDir", "C:\\SourceData"},
    {"DmsEditor", """%env:ProgramFiles%\\Notepad++\\Notepad++.exe"" ""%F"" -n%L"},
};

void setInitialStringValue(string_option so, QLineEdit* widget)
{
    const auto& stringOptionsData = sStringOptionsData[so];
    auto regKeyName = stringOptionsData.reg_key;
    auto regKey = GetGeoDmsRegKey(regKeyName);
    if (regKey.empty())
    {
        regKey = stringOptionsData.default_value;
        SetGeoDmsRegKeyString(regKeyName, regKey.c_str());
    }
    widget->setText(regKey.c_str());

}

void DmsAdvancedOptionsWindow::setInitialLocalDataDirValue()
{
    setInitialStringValue(string_option::LocalDataDir, m_ld_input);
}

void DmsAdvancedOptionsWindow::setInitialSourceDatDirValue()
{
    setInitialStringValue(string_option::SourceDataDir, m_sd_input);
}

void DmsAdvancedOptionsWindow::setInitialEditorValue()
{
    setInitialStringValue(string_option::StartEditorCmd, m_editor_input);
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
    setSF(m_pp0->isChecked(), dms_reg_status_flags, RSF_SuspendForGUI);
    setSF(m_pp1->isChecked(), dms_reg_status_flags, RSF_MultiThreading1);
    setSF(m_pp2->isChecked(), dms_reg_status_flags, RSF_MultiThreading2);
    setSF(m_pp3->isChecked(), dms_reg_status_flags, RSF_MultiThreading3);
    setSF(m_tracelog->isChecked(), dms_reg_status_flags, RSF_TraceLogFile);
    SetGeoDmsRegKeyDWord("StatusFlags", dms_reg_status_flags);

    SetGeoDmsRegKeyDWord("MemoryFlushThreshold", m_flush_treshold->value());

    setChanged(false);
}

void DmsAdvancedOptionsWindow::setChanged(bool isChanged)
{
    m_changed = isChanged;
    m_apply->setEnabled(isChanged);
    m_undo->setEnabled(isChanged);
}

void DmsAdvancedOptionsWindow::ok()
{
    if (m_changed)
        apply();
    setChanged(false);
    done(QDialog::Accepted);
}

void DmsAdvancedOptionsWindow::undo()
{
    restoreOptions();
    setChanged(false);
}

void DmsAdvancedOptionsWindow::onStateChange(int state)
{
    setChanged(true);
}

void DmsAdvancedOptionsWindow::onTextChange(const QString& text)
{
    setChanged(true);
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

#include "Unit.h"
#include "UnitClass.h"
#include "utl/Registry.h"
#include "DmsMainWindow.h"

//======== BEGIN CONFIG OPTIONS WINDOW ========
DmsConfigOptionsWindow::DmsConfigOptionsWindow(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QString("Config options"));
    setMinimumSize(800, 400);

    auto grid_layout = new QGridLayout(this);
    grid_layout->setVerticalSpacing(0);

    grid_layout->addWidget(new QLabel("Option", this), 0, 0);
    grid_layout->addWidget(new QLabel("Override", this), 0, 1);
    grid_layout->addWidget(new QLabel("Configured or override value", this), 0, 2);

    unsigned int nrRows = 1;
    static TokenID tConfigSettings = GetTokenID_mt("ConfigSettings");
    static TokenID tOverridable    = GetTokenID_mt("Overridable");

    auto tiCursor = MainWindow::TheOne()->getRootTreeItem();
    if (tiCursor) tiCursor = tiCursor->GetSubTreeItemByID(tConfigSettings);
    if (tiCursor) tiCursor = tiCursor->GetSubTreeItemByID(tOverridable);
    if (tiCursor) tiCursor = tiCursor->_GetFirstSubItem();
    if (tiCursor)
    {
        GuiReadLock lockHolder;
        for (; tiCursor; tiCursor = tiCursor->GetNextItem())
        {
            if (!IsDataItem(tiCursor))
                continue;
            auto adi = AsDataItem(tiCursor);
            auto avu = adi->GetAbstrValuesUnit();
            if (avu->GetUnitClass() != Unit<SharedStr>::GetStaticClass())
                continue;
            
//            auto box_layout = new QHBoxLayout(this);
            auto tiName = SharedStr(tiCursor->GetName());
            auto label= new QLabel(tiName.c_str(), this);

            auto option_input = new QLineEdit(this);
            auto option_cbx = new QCheckBox(this);
            connect(option_cbx, &QCheckBox::toggled, this, &DmsConfigOptionsWindow::checkbox_toggled);

            grid_layout->addWidget(label, nrRows, 0);
            grid_layout->addWidget(option_cbx, nrRows, 1);
            grid_layout->addWidget(option_input, nrRows, 2);


            SharedDataItemInterestPtr data_item = adi;

            PreparedDataReadLock drl(adi); // interestCount held by m_Options;
            auto configuredValue = data_item->GetRefObj()->AsString(0, lockHolder);

            m_Options.emplace_back(ConfigOption{ std::move(tiName), std::move(configuredValue), option_cbx, option_input });

            nrRows++;
        }
    }

    // ok/apply/cancel buttons
    auto box_layout = new QHBoxLayout(this);
    m_ok = new QPushButton("Ok", this);
    m_ok->setMaximumSize(75, 30);
    m_ok->setAutoDefault(true);
    m_ok->setDefault(true);
    m_apply = new QPushButton("Apply", this);
    m_apply->setMaximumSize(75, 30);
    m_undo = new QPushButton("Undo", this);
    m_undo->setMaximumSize(75, 30);

    connect(m_ok, &QPushButton::released, this, &DmsConfigOptionsWindow::ok);
    connect(m_apply, &QPushButton::released, this, &DmsConfigOptionsWindow::apply);
    connect(m_undo, &QPushButton::released, this, &DmsConfigOptionsWindow::resetValues);

    box_layout->addWidget(m_ok);
    box_layout->addWidget(m_apply);
    box_layout->addWidget(m_undo);
    grid_layout->addLayout(box_layout, nrRows+1, 0, 1, 3);

    setWindowModality(Qt::ApplicationModal);
    setAttribute(Qt::WA_DeleteOnClose);
    resetValues();
}

#include "DmsMainWindow.h"

void DmsConfigOptionsWindow::setChanged(bool isChanged)
{
    m_changed = isChanged;
    m_apply->setEnabled(isChanged);
    m_undo->setEnabled(isChanged);
}

void DmsConfigOptionsWindow::resetValues()
{
    if (m_Options.size())
    {
        RegistryHandleLocalMachineRO regLM;
        for (auto& option : m_Options)
        {
            bool valueExists = regLM.ValueExists(option.name.c_str());
            option.override_cbx->setChecked(valueExists);
        }
    }
    updateAccordingToCheckboxStates();

    setChanged(false);
}

void DmsConfigOptionsWindow::updateAccordingToCheckboxStates()
{
    if (m_Options.size())
    {
        RegistryHandleLocalMachineRO regLM;
        for (auto& option : m_Options)
        {
            bool valueExists = regLM.ValueExists(option.name.c_str());
            bool valueOverridden = option.override_cbx->isChecked();
            if (valueOverridden && valueExists)
                option.override_value->setText(regLM.ReadString(option.name.c_str()).c_str());
            else
                option.override_value->setText(option.configured_value.c_str());

            option.override_value->setEnabled(valueOverridden);
        }
    }
}

void DmsConfigOptionsWindow::checkbox_toggled()
{
    updateAccordingToCheckboxStates();
    setChanged(true);
}

void DmsConfigOptionsWindow::apply()
{
    if (m_Options.size())
    {
        RegistryHandleLocalMachineRW regLM;
        for (auto& option : m_Options)
        {
            bool valueExists = regLM.ValueExists(option.name.c_str());
            bool valueOverridden = option.override_cbx->isChecked();
            if (valueOverridden)
            {
                QByteArray overrideValue = option.override_value->text().toUtf8();
                regLM.WriteString(option.name.c_str(), CharPtrRange(overrideValue.cbegin(), overrideValue.cend()));
            }
            else if (valueExists)
                regLM.DeleteValue(option.name.c_str());
        }
    }

    setChanged(false);
}


void DmsConfigOptionsWindow::ok()
{
    if (m_changed)
        apply();
    done(QDialog::Accepted);
}


//======== END CONFIG OPTIONS WINDOW ========